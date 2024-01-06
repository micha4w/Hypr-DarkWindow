# hypr-darkwindow
Hyprland plugin that adds possibility to invert the color of specific windows.

![preview](./res/preview.png)

## Configuration
This Branch adds config, that uses the rulev2 syntax:
```conf
# hyprland.conf
darkwindow_invert = class:(pb170.exe)
darkwindow_invert = fullscreen:1
```

Also adds 2 Dispatches `invertwindow WINDOW` and `invertactivewindow`

## Installation

### Hpyrland >= v0.34.0
Install using `hyprpm`
```sh
hyprpm add https://github.com/micha4w/Hypr-DarkWindow
hyprpm enable Hypr-DarkWindow
hyprpm reload
```

### Hpyrland >= v0.28.0
Installable using [Hyprload](https://github.com/duckonaut/hyprload)
```toml
# hyprload.toml
plugins = [
  "micha4w/Hypr-DarkWindow",
]
```