# Hypr-DarkWindow
Hyprland plugin that adds the possibility to modify the fragment shader of specific windows.

![preview](./res/preview.png)

> [!IMPORTANT]
> The main branch is always™ up to date with Hyprlands main branch,
> if you are using a release version of Hyprland check the Readme of the specific tag.

## Shaders

> [!NOTE]
> You can only have one shader applied at the same time.
> Applying a shader to a window which already has one applied will override the first one.
> Shaders that were applied using a dispatcher take priority over windowrule shaders.

There are few shaders already included in this plugin.
All of them get loaded with the plugin, if you want to only load specific ones you can limit the shaders that are loaded.

```ini
plugin:darkwindow:load_shaders = invert,tint # defaults to 'all'
plugin:darkwindow:load_shaders = # dont load any default shaders
```

| **Name**      | **Description**                                                                                                                                                                                                                                                                                                                                                              |
| ------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **invert**    | _No uniforms_ <br> Applies smart color inversion                                                                                                                                                                                                                                                                                                                             |
| **tint**      | <ul><li>_tintStrength_ = `float` (0-1) </li><li>_tintColor_ = `vec3`</li></ul> Tints the Window <br>                                                                                                                                                                                                                                                                         |
| **chromakey** | <ul> <li>_bkg_ = `vec3` <br> The background color of the Window </li> <li>_similarity_ = `float` <br> How many similar colors should be affected</li> <li>_amount_ = `float` <br> How much similar colors should be changed</li> <li>_targetOpacity_ = `float` <br> Target opacity for similar colors</li> </ul> Applies opacity changes to pixels similar to one color <br> |

If you want to use your own Shaders, check [this](#custom-shaders) out.

## Configuration

> [!IMPORTANT]
> BREAKING:
>
> - WindowRules `invertwindow` and `shadewindow` were removed, use `darkwindow:shade [invert]` instead
> - Shader definition was moved from `darkwindow:shader` to `plugin:darkwindow:shader`
>
> Deprecated:
>
> - Dispatchers `invert[active]window` and `shade[active]window` will be removed soon, use `darkwindow:shade[active] [invert]` instead

```ini
# hyprland.conf

plugin:darkwindow {
  # To modify the uniforms of an already existing shader, create a new shader and set the uniforms you want
  shader[tintRed] {
      from = tint
      args = tintColor=[1 0 0] tintStrength=0.1
  }

  # Use a custom shader from a file
  shader[cool] {
      path = /path/to/shader.glsl # see the section below (#custom-shaders) for the content of this file
      args = wow=[1.0 0 0]
      introduces_transparency = true # if you modify the alpha value make sure to set this value to true so hyprland knows it should enable blur
  }
}

# Then to apply the shader to a window you can use window rules
windowrule = darkwindow:shade invert, match:class (pb170.exe)
# Uniforms can also be passed on the fly, but make sure to not use commas inside the arrays
windowrule = darkwindow:shade tint tintColor=[0 1 0], match:fullscreen true

# Or use a dispatcher
bind = $mainMod, T, darkwindow:shadeactive, tint tintColor=[0 0.5 1] tintStrength=0.3
# There is also a `darkwindow:shade WINDOW_REGEX SHADER_NAME` available (see window in https://wiki.hypr.land/Configuring/Dispatchers/#parameter-explanation)
```

### Custom Shaders

To add custom shaders use the `plugin:darkwindow:shader` config category (see example above).
The file at `.path` is a glsl file that should contain a `void windowShader(inout vec4 color)` function and
uniform declarations for your `.args`.
It can also contain more functions but be careful to not clash with names that are already used by hyprland.

The custom shader code will then be injected into the fragment shader used by hyprland.
You can see examples of shaders by looking at the [predefined shaders](./src/PredefinedShaders.cpp).
Feel free to make a pull request to add your own shaders!

#### Special Variables

You can use these variables anywhere in your shader code.
Do not add the uniform declarations.
This plugin will automatically detect the used variables and set them at each render.

| **Name**            | **Type**                        | **Description**                                                                                                                                      |
| ------------------- | ------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------- |
| **x_Time**          | `float`                         | Current time in seconds<br>_using this will cause the entire window to rerender every frame_                                                         |
| **x_PixelPos**      | `vec2`                          | Position of the current pixel in window space<br>(top left of the window is [0,0], monitor scaling already applied)                                  |
| **x_CursorPos**     | `vec2`                          | Position of the cursor, same coordinate space as `x_PixelPos`<br>_using this will cause the entire window to rerender every time the mouse is moved_ |
| **x_WindowSize**    | `vec2`                          | Size of the current window, same scaling as `x_PixelPos`                                                                                             |
| **x_MonitorScale**  | `float`                         | Scaling of the monitor as seen in `hyprctl monitors`                                                                                                 |
| **x_Texture**       | `fn (vec2 texCoord) -> vec4`    | Gets the color of a pixel<br>(the difference of using this vs. using `texture(x_Tex, texCoord)` is that this function handles opaque windows)        |
| **x_TextureOffset** | `fn (vec2 pixelOffset) -> vec4` | Gets the color at a pixel offset to the currently drawn pixel                                                                                        |
| **x_Tex**           | `sampler2D`                     | The texture that gets sampled from                                                                                                                   |
| **x_TexCoord**      | `vec2`                          | The coordinate that was used to get the current pixel color                                                                                          |

## Installation

### hyprpm

```sh
hyprpm add https://github.com/micha4w/Hypr-DarkWindow
hyprpm enable Hypr-DarkWindow
hyprpm reload
```

### NixOS (home-manager)

You should already have a fully working home-manager setup before adding this plugin.

```nix
#flake.nix
inputs = {
    home-manager = { ... };
    hyprland = { ... };
    ...
    hypr-darkwindow = {
      url = "github:micha4w/Hypr-DarkWindow/tags/v0.36.0"; # Make sure to change the tag to match your hyprland version
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
