let
    program-name = "wiser";
in
{ lib
, stdenv
, pkg-config
, gtk3
, gtk-layer-shell
, jsoncpp
, wrapGAppsHook
}:
stdenv.mkDerivation {
  pname   = program-name;
  version = "1.0.0";

  src = ./.;

  nativeBuildInputs = [
    pkg-config
    # wrapGAppsHook   # auto-wraps the binary with GDK/GTK env vars
  ];

  buildInputs = [
    gtk3
    gtk-layer-shell
    jsoncpp
  ];

  buildPhase = ''
    g++ widget.cpp -o ${program-name} \
      $(pkg-config --cflags --libs gtk+-3.0 gtk-layer-shell-0 jsoncpp) \
      -std=c++17 -O2
  '';

  installPhase = ''
    install -Dm755 ${program-name} $out/bin/wizer
  '';

  meta = {
    description     = "Wayland layer-shell desktop quote/text widget";
    longDescription = ''
      A frameless, transparent desktop widget for Wayland compositors
      (Hyprland, Sway, niri, River). Uses wlr-layer-shell to place itself
      on the desktop layer. Configured via a JSON file. Supports drag,
      resize, background images, and custom fonts/colours.
    '';
    license     = lib.licenses.gpl3;
    platforms   = lib.platforms.linux;
    mainProgram = "wizer";
  };
}
