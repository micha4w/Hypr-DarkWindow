#include <hyprland/src/render/pass/PassElement.hpp>

#define private public
#include <hyprland/src/render/pass/SurfacePassElement.hpp>
#undef private

#define m_failedPluginConfigValues    m_failedPluginConfigValues; friend struct ConfigManagerFriend;
#include <hyprland/src/config/ConfigManager.hpp>
#undef m_failedPluginConfigValues


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

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle)
{
    PHANDLE = handle;

    {
        auto& config = ConfigManagerFriend::GetConfig();
        config->addSpecialCategory(SHADER_CATEGORY, { .key = "id", });
        config->addSpecialConfigValue(SHADER_CATEGORY, "name", "");
        config->addSpecialConfigValue(SHADER_CATEGORY, "path", "");
        config->addSpecialConfigValue(SHADER_CATEGORY, "args", "");
    }

    g_Callbacks = {};
    g_Callbacks.push_back(HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "configReloaded",
        [&](void* self, SCallbackInfo&, std::any data) {
            std::lock_guard<std::mutex> lock(g_ShaderMutex);

            g_WindowShader = WindowShader();

            auto& config = ConfigManagerFriend::GetConfig();
            auto ids = config->listKeysForSpecialCategory(SHADER_CATEGORY);
            for (auto& id : std::set<std::string>(ids.begin(), ids.end())) {
                auto name = std::any_cast<Hyprlang::STRING>(
                    config->getSpecialConfigValue(SHADER_CATEGORY, "name", id.c_str()));
                auto path = std::any_cast<Hyprlang::STRING>(
                    config->getSpecialConfigValue(SHADER_CATEGORY, "path", id.c_str()));
                auto args = std::any_cast<Hyprlang::STRING>(
                    config->getSpecialConfigValue(SHADER_CATEGORY, "args", id.c_str()));

                auto nameOrPath = std::string(path) != "" ?
                    std::variant<std::string, std::string>{ std::in_place_index<1>, path} :
                    std::variant<std::string, std::string>{ std::in_place_index<0>, name};

                try
                {
                    g_WindowShader.AddShader(id, nameOrPath, args);
                }
                catch (const std::exception& ex)
                {
                    Debug::log(ERR, "Failed to load shader {}", ex.what());
                    HyprlandAPI::addNotification(
                        PHANDLE,
                        std::string("Failed to load ") + SHADER_CATEGORY + "[" + id + "]: " + ex.what(),
                        CHyprColor(0xFFFF0000),
                        25'000
                    );
                }
            }

            try {
                // Always add the invert shader if it doesnt already exist, because backwards compatibility!
                g_WindowShader.AddShader("invert", std::variant<std::string, std::string>{ std::in_place_index<0>, "invert" }, "");
            } catch (const std::exception& ex) {
                Debug::log(ERR, "Failed to load shader {}", ex.what());
            }

            Debug::log(INFO, "Compiled all shaders");
            g_WindowShader.ReshadeWindows();
        }
    ));
    g_Callbacks.push_back(HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "closeWindow",
        [&](void* self, SCallbackInfo&, std::any data) {
            std::lock_guard<std::mutex> lock(g_ShaderMutex);
            g_WindowShader.OnWindowClose(std::any_cast<PHLWINDOW>(data));
        }
    ));
    g_Callbacks.push_back(HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "windowUpdateRules",
        [&](void* self, SCallbackInfo&, std::any data) {
            std::lock_guard<std::mutex> lock(g_ShaderMutex);
            g_WindowShader.ShadeIfMatches(std::any_cast<PHLWINDOW>(data));
        }
    ));

    const auto findFunction = [&](const std::string& className, const std::string& name) {
        auto all = HyprlandAPI::findFunctionsByName(PHANDLE, name);
        auto found = std::find_if(all.begin(), all.end(), [&](const SFunctionMatch& line) {
            return line.demangled.starts_with(className + "::" + name);
        });
        if (found != all.end())
            return std::optional(*found);
        else
            return std::optional<SFunctionMatch>();
    };

    static const auto pDraw = findFunction("CSurfacePassElement", "draw");
    if (!pDraw) throw std::runtime_error("Failed to find CSurfacePassElement::draw");
    g_surfacePassDraw = HyprlandAPI::createFunctionHook(handle, pDraw->address, (void*)&hkSurfacePassDraw);
    g_surfacePassDraw->hook();

    // Keep these keywords because backwards compatibility
    HyprlandAPI::addDispatcherV2(PHANDLE, "invertwindow", [&](std::string args) {
        std::lock_guard<std::mutex> lock(g_ShaderMutex);
        g_WindowShader.ToggleShade(g_pCompositor->getWindowByRegex(args), "invert");
        return SDispatchResult{};
    });
    HyprlandAPI::addDispatcherV2(PHANDLE, "invertactivewindow", [&](std::string args) {
        std::lock_guard<std::mutex> lock(g_ShaderMutex);
        g_WindowShader.ToggleShade(g_pCompositor->m_lastWindow.lock(), "invert");
        return SDispatchResult{};
    });

    HyprlandAPI::addDispatcherV2(PHANDLE, "shadewindow", [&](std::string args) {
        std::lock_guard<std::mutex> lock(g_ShaderMutex);
        size_t space = args.find(" ");
        if (space == std::string::npos) throw std::runtime_error("Expected 2 Arguments: <WINDOW> <SHADER>");
        g_WindowShader.ToggleShade(g_pCompositor->getWindowByRegex(args.substr(0, space)), args.substr(space + 1));
        return SDispatchResult{};
    });
    HyprlandAPI::addDispatcherV2(PHANDLE, "shadeactivewindow", [&](std::string args) {
        std::lock_guard<std::mutex> lock(g_ShaderMutex);
        g_WindowShader.ToggleShade(g_pCompositor->m_lastWindow.lock(), args);
        return SDispatchResult{};
    });

    return {
        "Hypr-DarkWindow",
        "Allows you to set dark mode for only specific Windows",
        "micha4w",
        "3.0.1"
    };
}

APICALL EXPORT void PLUGIN_EXIT()
{
    std::lock_guard<std::mutex> lock(g_ShaderMutex);
    g_Callbacks = {};
    g_WindowShader.Unload();

    auto& config = ConfigManagerFriend::GetConfig();
    config->removeSpecialConfigValue(SHADER_CATEGORY, "name");
    config->removeSpecialConfigValue(SHADER_CATEGORY, "path");
    config->removeSpecialConfigValue(SHADER_CATEGORY, "args");
    config->removeSpecialCategory(SHADER_CATEGORY);
}

APICALL EXPORT std::string PLUGIN_API_VERSION()
{
    return HYPRLAND_API_VERSION;
}
