{
  description = "Wizer — desktop wize-quote/text widget";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      system  = "x86_64-linux";
      pkgs    = nixpkgs.legacyPackages.${system};
    in {

      packages.${system}.default = pkgs.callPackage ./default.nix {};

      apps.${system}.default = {
        type    = "app";
        program = "${self.packages.${system}.default}/bin/wizer";
      };

      devShells.${system}.default = pkgs.mkShell {
        nativeBuildInputs = with pkgs; [ pkg-config ];
        buildInputs       = with pkgs; [ gtk3 gtk-layer-shell jsoncpp ];
        shellHook = ''
          echo "wizer dev shell — run: make"
        '';
      };

    };
}
