// This file contains static functions that were yoinked from hyprland/src/render/OpenGL.cpp
#pragma once

#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/shaders/Shaders.hpp>
#include <hyprland/src/helpers/fs/FsUtils.hpp>

#include <hyprutils/string/String.hpp>
#include <hyprutils/path/Path.hpp>

const std::vector<const char*> ASSET_PATHS = {
#ifdef DATAROOTDIR
    DATAROOTDIR,
#endif
    "/usr/share",
    "/usr/local/share",
};

static std::string loadShader(const std::string& filename) {
    const auto home = Hyprutils::Path::getHome();
    if (home.has_value()) {
        const auto src = NFsUtils::readFileAsString(home.value() + "/hypr/shaders/" + filename);
        if (src.has_value())
            return src.value();
    }
    for (auto& e : ASSET_PATHS) {
        const auto src = NFsUtils::readFileAsString(std::string{e} + "/hypr/shaders/" + filename);
        if (src.has_value())
            return src.value();
    }
    if (SHADERS.contains(filename))
        return SHADERS.at(filename);
    throw std::runtime_error(std::format("Couldn't load shader {}", filename));
}

static void loadShaderInclude(const std::string& filename, std::map<std::string, std::string>& includes) {
    includes.insert({filename, loadShader(filename)});
}

static void processShaderIncludes(std::string& source, const std::map<std::string, std::string>& includes) {
    for (auto it = includes.begin(); it != includes.end(); ++it) {
        Hyprutils::String::replaceInString(source, "#include \"" + it->first + "\"", it->second);
    }
}

static std::string processShader(const std::string& filename, const std::map<std::string, std::string>& includes) {
    auto source = loadShader(filename);
    processShaderIncludes(source, includes);
    return source;
}
