{
  description = "desktop-widget — Wayland layer-shell desktop quote/text widget";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      system  = "x86_64-linux";
      pkgs    = nixpkgs.legacyPackages.${system};
    in {

      packages.${system}.default = pkgs.callPackage ./default.nix {};

      apps.${system}.default = {
        type    = "app";
        program = "${self.packages.${system}.default}/bin/desktop-widget";
      };

      devShells.${system}.default = pkgs.mkShell {
        nativeBuildInputs = with pkgs; [ pkg-config ];
        buildInputs       = with pkgs; [ gtk3 gtk-layer-shell jsoncpp ];
        shellHook = ''
          echo "desktop-widget dev shell — run: make"
        '';
      };

    };
}
