# Hypr-DarkWindow

Hyprland plugin that adds possibility to modify the fragment shader of a specific windows.

![preview](./res/preview.png)

## Shaders

There are few shaders already included in this plugin.
All of them get loaded with the plugin, if you want to only load specific ones you can limit the shaders that are loaded.

```ini
plugin:darkwindow:load_shaders = invert,tint,... # defaults to 'all'
```

| **Name**   | **Description**                                                                                              |
| ---------- | ------------------------------------------------------------------------------------------------------------ |
| **invert** | _No uniforms_ <br> Applies smart color inversion                                                             |
| **tint**   | <ul><li>_tintStrength_ = `float` (0-1) </li><li>_tintColor_ = `vec3`</li></ul> Tints the Window <br> |
| **chromakey**   | <ul> <li>_bkg_ = `vec3` <br> The background color of the Window </li> <li>_similarity_ = `float` <br> How many similar colors should be affected</li> <li>_amount_ = `float` <br> How much similar colors should be changed</li> <li>_targetOpacity_ = `float` <br> Target opacity for similar colors</li> </ul> Applies opacity changes to pixels similar to one color <br> |

Feel free to make a pull request if you want to add more shaders ([look here](./src/WindowShader.cpp:7)).

## Configuration

> [!NOTE]
> You can only have one shader applied at the same time.
> Applying a shader to a window which already has one applied will override the first one.

```ini
# hyprland.conf

# To modify the uniforms of an already existing shader, create a new shader and set the uniforms you want
darkwindow:shader[tintRed] {
    from = tint
    args = tintColor=[1 0 0] tintStrength=0.1
}

# Use a custom shader from a file, check out ./src/WindowShader.cpp:7 to see examples for the files content
darkwindow:shader[cool] {
    path = /path/to/shader.glsl
    args = wow=[1.0 0 0]
    introduces_transparency = true # if you modify the alpha value make sure to set this value to true so hyprland knows it should enable blur
}

# Then to apply the shader to a window you can use window rules
windowrulev2 = plugin:shadewindow invert,class:(pb170.exe)
# Uniforms can also be passed on the fly
windowrulev2 = plugin:shadewindow tint tintColor=[0 1 0],fullscreen:1

# Or use a dispatcher
bind = $mainMod, T, shadeactivewindow, tint tintColor=[0 0.5 1] tintStrength=0.3
# There is also a `shadewindow WINDOW_REGEX SHADER_NAME` available (see window in https://wiki.hypr.land/Configuring/Dispatchers/#parameter-explanation)
```

## Installation

### Hyprland >= v0.36.0

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
> In this example `inputs.hypr-darkwindow.url` sets the tag, Make sure that tag matches your Hyprland version.

### Hyprland >= v0.34.0

Install using `hyprpm`

```sh
hyprpm add https://github.com/micha4w/Hypr-DarkWindow
hyprpm enable Hypr-DarkWindow
hyprpm reload
```

### Hyprland >= v0.28.0

Installable using [Hyprload](https://github.com/duckonaut/hyprload)

```toml
# hyprload.toml
plugins = [
  "micha4w/Hypr-DarkWindow",
]
```
