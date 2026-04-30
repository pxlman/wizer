let 
pkgs = import <nixpkgs> {};
wizer = pkgs.callPackage ./default.nix {};
in
pkgs.mkShell {
  buildInputs = [
    wizer
    ];
}

