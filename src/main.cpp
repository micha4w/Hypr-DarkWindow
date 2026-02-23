#include "State.h"

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle)
{
    g.Init(handle);

    Log::logger->log(Log::INFO, "[Hypr-DarkWindow] Loading Plugin");

    g.AddConfigValues();
    g.HookFunctions();

    const auto shade = g.HandleError([&](std::string args) {
        g.Manager.ApplyDispatchedShader(Desktop::focusState()->window(), args);
    });
    const auto shadeSpecific = g.HandleError([&](std::string args) {
        size_t space = args.find(" ");
        if (space == std::string::npos)
            throw g.Efmt("Expected 2 Arguments: <WINDOW> <SHADER>");
        g.Manager.ApplyDispatchedShader(g_pCompositor->getWindowByRegex(args.substr(0, space)), args.substr(space + 1));
    });

    HyprlandAPI::addDispatcherV2(g.Handle, "darkwindow:shade", shadeSpecific);
    HyprlandAPI::addDispatcherV2(g.Handle, "darkwindow:shadeactive", shade);

    // Keep these keywords because backwards compatibility :)
    HyprlandAPI::addDispatcherV2(g.Handle, "shadeactivewindow", shade);
    HyprlandAPI::addDispatcherV2(g.Handle, "shadewindow", shadeSpecific);
    HyprlandAPI::addDispatcherV2(g.Handle, "invertactivewindow", [&](std::string args) { return shade("invert"); });
    HyprlandAPI::addDispatcherV2(g.Handle, "invertwindow", [&](std::string args) { return shadeSpecific(args + " invert"); });

    g.Listeners.push_back(Event::bus()->m_events.config.reloaded.listen([&] {
        g.Manager = ShadeManager();
#ifdef WATCH_SHADERS
        additionalWatchPaths.clear();
#endif

        for (auto& name : g.Config_LoadedShaders())
        {
            try
            {
                Log::logger->log(Log::INFO, "[Hypr-DarkWindow] Loading predefined shader with id: {}", name);
                g.Manager.LoadPredefinedShader(std::string(name));
            }
            catch (const std::exception& ex)
            {
                g.NotifyError("Failed to load predefined shader " + std::string(name) + ": " + ex.what());
            }
        }

        for (auto& shader : g.Config_UserShaders())
        {
            try
            {
                Log::logger->log(Log::INFO, "[Hypr-DarkWindow] Loading custom shader with id: {}", shader.Id);
                g.Manager.AddShader(ShaderDefinition{
                    .ID = shader.Id,
                    .From = shader.From,
                    .Path = shader.Path,
                    .Args = ShaderDefinition::ParseArgs(shader.Args),
                    .Transparency = IntroducesTransparency{ shader.IntroducesTransparency },
                    });
#ifdef WATCH_SHADERS
                additionalWatchPaths.push_back(shader.Path);
#endif
            }
            catch (const std::exception& ex)
            {
                g.NotifyError(std::string("Failed to load custom shader ") + g.USER_SHADER_CATEGORY + "[" + shader.Id + "]: " + ex.what());
            }
        }

        Log::logger->log(Log::INFO, "[Hypr-DarkWindow] Compiled all shaders");
        try
        {
            g.Manager.RecheckWindowRules();
        }
        catch (const std::exception& ex)
        {
            g.NotifyError(std::string("Failed to apply window rule shader: ") + ex.what());
        }

#ifdef WATCH_SHADERS
        g_pConfigManager->updateWatcher();
#endif
    }));

    g_pConfigManager->reload();


    g.Listeners.push_back(Event::bus()->m_events.window.updateRules.listen([&](PHLWINDOW window) {
        try
        {
            g.Manager.ApplyWindowRuleShader(window);
        }
        catch (const std::exception& ex)
        {
            g.NotifyError(std::string("Failed to apply window rule shader: ") + ex.what());
        }
    }));

    g.Listeners.push_back(Event::bus()->m_events.window.destroy.listen([&](PHLWINDOW window) {
        g.Manager.ForgetWindow(window);
    }));

    g.Listeners.push_back(Event::bus()->m_events.render.pre.listen([&](PHLMONITOR monitor) {
        g.Manager.PreRenderMonitor(monitor);
    }));

    g.Listeners.push_back(Event::bus()->m_events.input.mouse.move.listen([&] {
        g.Manager.MouseMove();
    }));

    return {
        "Hypr-DarkWindow",
        "Allows you to modify the fragment shader of specific windows",
        "micha4w",
        "5.1.0"
    };
}

APICALL EXPORT void PLUGIN_EXIT()
{
    g.RemoveConfigValues();
    g = State();
}

APICALL EXPORT std::string PLUGIN_API_VERSION()
{
    return HYPRLAND_API_VERSION;
}
