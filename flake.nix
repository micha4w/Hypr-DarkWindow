{
  inputs = {
    hyprland.url = "github:hyprwm/Hyprland";
    nix-filter.url = "github:numtide/nix-filter";
  };

  outputs = {
    self,
    hyprland,
    nix-filter,
    ...
  }: let
    inherit (hyprland.inputs) nixpkgs;
    forHyprlandSystems = fn: nixpkgs.lib.genAttrs (builtins.attrNames hyprland.packages) (system: fn system nixpkgs.legacyPackages.${system});
  in {
    packages = forHyprlandSystems (system: pkgs: rec {
      Hypr-DarkWindow = pkgs.gcc13Stdenv.mkDerivation {
        pname = "Hypr-DarkWindow";
        version = "0.1";
        src = nix-filter.lib {
          root = ./.;
          include = [
            "src"
            ./Makefile
          ];
        };

        # allow overriding xwayland support
        BUILT_WITH_NOXWAYLAND = false;

        nativeBuildInputs = with pkgs; [gnumake pkg-config];

        buildInputs = with pkgs;
          [
            hyprland.packages.${system}.hyprland.dev
            pango
            cairo
          ]
          ++ hyprland.packages.${system}.hyprland.buildInputs;

        meta = with pkgs.lib; {
          homepage = "https://github.com/micha4w/Hypr-DarkWindow";
          description = "Invert the colors of specific Windows";
          # license = licenses.bsd3;
          platforms = platforms.linux;
        };
      };

      default = Hypr-DarkWindow;
    });

    devShells = forHyprlandSystems (system: pkgs: {
      default = pkgs.mkShell {
        name = "Hypr-DarkWindow";

        nativeBuildInputs = with pkgs; [
          clang-tools_16
        ];

        inputsFrom = [self.packages.${system}.Hypr-DarkWindow];
      };
    });
  };
}
