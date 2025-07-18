#include <hyprland/src/render/pass/PassElement.hpp>

#define private public
#include <hyprland/src/render/pass/SurfacePassElement.hpp>
#undef private

#define m_failedPluginConfigValues    m_failedPluginConfigValues; friend struct ConfigManagerFriend;
#include <hyprland/src/config/ConfigManager.hpp>
#undef m_failedPluginConfigValues

#include <hyprutils/string/ConstVarList.hpp>
#include <hyprutils/string/String.hpp>


#include "WindowShader.h"

#include <mutex>
#include <vector>

#include <dlfcn.h>

#include <hyprlang.hpp>

struct ConfigManagerFriend {
    static auto& GetConfig() {
        return g_pConfigManager->m_config;
    }
};


inline HANDLE PHANDLE = nullptr;

inline WindowShader g_WindowShader;
inline std::mutex g_ShaderMutex;

inline std::vector<SP<HOOK_CALLBACK_FN>> g_Callbacks;
CFunctionHook* g_surfacePassDraw;

// TODO check out transformers

void hkSurfacePassDraw(CSurfacePassElement* thisptr, const CRegion& damage) {
    {
        std::lock_guard<std::mutex> lock(g_ShaderMutex);
        g_WindowShader.OnRenderWindowPre(thisptr->m_data.pWindow);
    }

    ((decltype(&hkSurfacePassDraw))g_surfacePassDraw->m_original)(thisptr, damage);

    {
        std::lock_guard<std::mutex> lock(g_ShaderMutex);
        g_WindowShader.OnRenderWindowPost();
    }
}


const char* SHADER_CATEGORY = "darkwindow:shader";
const char* LOAD_SHADERS_KEY = "plugin:darkwindow:load_shaders";

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle)
{
    Debug::log(INFO, "[Hypr-DarkWindow] Loading Plugin");
    PHANDLE = handle;

    {
        auto& config = ConfigManagerFriend::GetConfig();
        config->addSpecialCategory(SHADER_CATEGORY, { .key = "id", });
        config->addSpecialConfigValue(SHADER_CATEGORY, "from", "");
        config->addSpecialConfigValue(SHADER_CATEGORY, "path", "");
        config->addSpecialConfigValue(SHADER_CATEGORY, "args", "");
    }

    HyprlandAPI::addConfigValue(PHANDLE, LOAD_SHADERS_KEY, Hyprlang::STRING("all"));

    g_Callbacks = {};
    g_Callbacks.push_back(HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "configReloaded",
        [&](void* self, SCallbackInfo&, std::any data) {
            std::lock_guard<std::mutex> lock(g_ShaderMutex);

            g_WindowShader = WindowShader();

            Hyprutils::String::CConstVarList list(
                (Hyprlang::STRING) HyprlandAPI::getConfigValue(PHANDLE, LOAD_SHADERS_KEY)->dataPtr()
            );
            for (auto& _name : list)
            {
                std::string name(_name);
                try
                {
                    g_WindowShader.AddPredefinedShader(name);
                }
                catch (const std::exception& ex)
                {
                    notifyError(PHANDLE, std::string("Failed to load predefined shader ") + name + ": " + ex.what());
                }
            }

            auto& config = ConfigManagerFriend::GetConfig();

            auto ids = config->listKeysForSpecialCategory(SHADER_CATEGORY);
            for (auto& id : std::set<std::string>(ids.begin(), ids.end()))
            {
                auto name = std::any_cast<Hyprlang::STRING>(
                    config->getSpecialConfigValue(SHADER_CATEGORY, "from", id.c_str()));
                auto path = std::any_cast<Hyprlang::STRING>(
                    config->getSpecialConfigValue(SHADER_CATEGORY, "path", id.c_str()));
                auto args = std::any_cast<Hyprlang::STRING>(
                    config->getSpecialConfigValue(SHADER_CATEGORY, "args", id.c_str()));

                try
                {
                    g_WindowShader.AddShader({ id, name, path, args });
                }
                catch (const std::exception& ex)
                {
                    notifyError(PHANDLE, std::string("Failed to load custom shader ") + SHADER_CATEGORY + "[" + id + "]: " + ex.what());
                }
            }

            Debug::log(INFO, "[Hypr-DarkWindow] Compiled all shaders");
            try
            {
                g_WindowShader.ReshadeWindows();
            }
            catch (const std::exception& ex)
            {
                notifyError(PHANDLE, std::string("Failed to apply window rule shader: ") + ex.what());
            }
        }
    ));

    g_pConfigManager->reload();

    g_Callbacks.push_back(HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "closeWindow",
        [&](void* self, SCallbackInfo&, std::any data) {
            std::lock_guard<std::mutex> lock(g_ShaderMutex);
            g_WindowShader.ForgetWindow(std::any_cast<PHLWINDOW>(data));
        }
    ));
    g_Callbacks.push_back(HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "windowUpdateRules",
        [&](void* self, SCallbackInfo&, std::any data) {
            std::lock_guard<std::mutex> lock(g_ShaderMutex);
            try
            {
                g_WindowShader.ShadeIfMatches(std::any_cast<PHLWINDOW>(data));
            }
            catch (const std::exception& ex)
            {
                notifyError(PHANDLE, std::string("Failed to apply window rule shader: ") + ex.what());
            }
        }
    ));

    static const auto pDraw = findFunction(PHANDLE, "CSurfacePassElement", "draw");
    if (!pDraw) throw efmt("Failed to find CSurfacePassElement::draw");
    g_surfacePassDraw = HyprlandAPI::createFunctionHook(handle, pDraw->address, (void*)&hkSurfacePassDraw);
    g_surfacePassDraw->hook();

    const auto rescue = [&](auto f) {
        return [&](std::string args) {
            std::lock_guard<std::mutex> lock(g_ShaderMutex);
            try
            {
                f(args);
                return SDispatchResult{};
            }
            catch (const std::exception& ex)
            {
                notifyError(PHANDLE, std::format("Exception in dispatcher: {}", ex.what()));
                return SDispatchResult{
                    .success = false,
                    .error = ex.what(),
                };
            }
        };
    };

    HyprlandAPI::addDispatcherV2(PHANDLE, "shadewindow", rescue([&](std::string args) {
        size_t space = args.find(" ");
        if (space == std::string::npos) throw efmt("Expected 2 Arguments: <WINDOW> <SHADER>");
        g_WindowShader.ToggleShade(g_pCompositor->getWindowByRegex(args.substr(0, space)), args.substr(space + 1));
    }));
    HyprlandAPI::addDispatcherV2(PHANDLE, "shadeactivewindow", rescue([&](std::string args) {
        g_WindowShader.ToggleShade(g_pCompositor->m_lastWindow.lock(), args);
    }));

    // Keep these keywords because backwards compatibility
    HyprlandAPI::addDispatcherV2(PHANDLE, "invertwindow", rescue([&](std::string args) {
        g_WindowShader.ToggleShade(g_pCompositor->getWindowByRegex(args), "invert");
    }));
    HyprlandAPI::addDispatcherV2(PHANDLE, "invertactivewindow", rescue([&](std::string args) {
        g_WindowShader.ToggleShade(g_pCompositor->m_lastWindow.lock(), "invert");
    }));

    return {
        "Hypr-DarkWindow",
        "Allows you to set dark mode for only specific Windows",
        "micha4w",
        "4.0.0"
    };
}

APICALL EXPORT void PLUGIN_EXIT()
{
    std::lock_guard<std::mutex> lock(g_ShaderMutex);
    g_Callbacks = {};
    g_WindowShader.Unload();

    auto& config = ConfigManagerFriend::GetConfig();
    config->removeSpecialConfigValue(SHADER_CATEGORY, "from");
    config->removeSpecialConfigValue(SHADER_CATEGORY, "path");
    config->removeSpecialConfigValue(SHADER_CATEGORY, "args");
    config->removeSpecialCategory(SHADER_CATEGORY);
}

APICALL EXPORT std::string PLUGIN_API_VERSION()
{
    return HYPRLAND_API_VERSION;
}
