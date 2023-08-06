#define WLR_USE_UNSTABLE

#include <hyprland/src/Compositor.hpp>

// This function should be as lightweight as possible, since it gets called multiple times every frame
inline bool invertWindow(HANDLE PHANDLE, const CWindow& window)
{
    return window.m_szInitialClass == "pb170.exe";
}