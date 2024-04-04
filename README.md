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

### Hpyrland >= v0.36.0
We now support Nix, wooo!

You should already have a fully working home-manager setup before adding this plugin.
```nix
#flake.nix
inputs = {
    home-manager = { ... };
    hyprland = { ... };
    ...
    hypr-darkwindow = {
      url = "github:micha4w/Hypr-DarkWindow/tags/v0.36.0";
      inputs.hyprland.follows = "hyprland";
    };
};

outputs = {
  home-manager,
  hypr-darkwindow,
  ...
}: {
  ... = {
    home-manager.users.micha4w = {
      wayland.windowManager.hyprland.plugins = [
        hypr-darkwindow.packages.${pkgs.system}.Hypr-DarkWindow
      ];
    };
  };
}
```

> [!NOTE]
> In this example `inputs.hpyr-darkwindow.url` sets the tag, Make sure that tag matches your Hyprland version.


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
