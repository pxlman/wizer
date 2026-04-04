# wizer

A lightweight Wayland-compatible desktop text/quote widget daemon written in C++ with GTK3.

## Features

- **Zero heavy deps** — only GTK3 (standard on most Linux distros)
- **Wayland compatible** — uses GDK/GTK3 which works on both X11 and Wayland (via XWayland if needed)
- **Transparent, frameless window** — blends into your desktop
- **Stays below all windows** — true desktop widget behavior
- **Hover interactions** — subtle highlight + resize hint dots appear on hover
- **Right-click context menu** — drag, resize, save, quit
- **Auto-saves** layout (position + size) to config on drag/resize end
- **Background image support** — use any image as the widget background
- **Rounded corners** with a decorative accent line
- **JSON config file** — human-readable, no external parser library needed

---

## Dependencies

```
gtk3 (libgtk-3-dev on Debian/Ubuntu, gtk3-devel on Fedora)
g++ with C++17 support
pkg-config
```

Install on Nix shell:
```bash
nix-shell -p gtk3 gtk3.dev gtk-layer-shell gtk-layer-shell.dev jsoncpp pkg-config gcc gnumake
```

Install on Debian/Ubuntu:
```bash
sudo apt install libgtk-3-dev g++ pkg-config
```

Install on Fedora/RHEL:
```bash
sudo dnf install gtk3-devel gcc-c++ pkgconfig
```

Install on Arch:
```bash
sudo pacman -S gtk3 base-devel
```

---

## Build

```bash
make
```

Or manually:
```bash
g++ widget.cpp -o wizer $(pkg-config --cflags --libs gtk+-3.0) -std=c++17 -O2
```

---

## Run

```bash
# Use default config (~/.config/wizer/config.json)
./wizer

# Use a specific config file
./wizer /path/to/my-widget.json

# Help
./wizer --help
```

---

## Config File (`config.json`)

```json
{
  "text": "\"The only way to do great work\nis to love what you do.\"\n\n— Steve Jobs",
  "font": "Serif Italic 20",
  "fg_color": "#F0E6D3",
  "bg_color": "#1A1A2E",
  "bg_image": "",
  "bg_opacity": 0.88,
  "x": 100,
  "y": 100,
  "width": 400,
  "height": 180
}
```

| Key         | Type   | Description                                        |
|-------------|--------|----------------------------------------------------|
| `text`      | string | The quote or text to display. Use `\n` for newlines |
| `font`      | string | Pango font string (e.g. `"Serif Italic 18"`, `"Monospace Bold 14"`, `"Sans 16"`) |
| `fg_color`  | string | Text color as hex (`#RRGGBB`)                      |
| `bg_color`  | string | Background fill color as hex (`#RRGGBB`)           |
| `bg_image`  | string | Path to a background image; overrides `bg_color` if set |
| `bg_opacity`| float  | Window opacity 0.0 (invisible) – 1.0 (opaque)     |
| `x`, `y`   | int    | Screen position in pixels                          |
| `width`, `height` | int | Widget dimensions in pixels               |

### Font String Examples

```
"Serif Italic 18"
"Monospace Bold 14"
"Sans 16"
"DejaVu Serif Italic 20"
"Noto Serif 18"
```

---

## Mouse Interactions

| Action        | How                                          |
|---------------|----------------------------------------------|
| **Drag**      | Right-click → "Enable Drag", then click+drag |
| **Resize**    | Right-click → "Enable Resize", then click+drag (SE corner effect) |
| **Save**      | Right-click → "Save Layout" (or auto-saves after drag/resize) |
| **Quit**      | Right-click → "Quit"                         |

----

## Run Multiple Widgets

Launch with different config files:

```bash
./wizer ~/widgets/motivation.json &
./wizer ~/widgets/clock-quote.json &
./wizer ~/widgets/todo.json &
```

---

## Autostart as a Systemd User Service

```bash
# Install binary
install -Dm755 wizer ~/.local/bin/wizer

# Install service
mkdir -p ~/.config/systemd/user
cp wizer.service ~/.config/systemd/user/

# Enable and start
systemctl --user enable --now wizer.service

# Check status
systemctl --user status wizer.service
```

---

## Wayland Notes

GTK3 runs natively on Wayland via the `GDK_BACKEND=wayland` environment variable (usually set automatically). The `GDK_WINDOW_TYPE_HINT_DESKTOP` hint tells the compositor to treat this as a desktop-layer window.

On some Wayland compositors (especially pure wlroots-based ones like Sway/Hyprland), the "keep below" hint may not be honored. In that case you can use **wlr-layer-shell** for true desktop layer placement — this would require `libwayland-client` and a small protocol extension. The current implementation works best on:

- **GNOME** (Mutter + XWayland)
- **KDE Plasma** (KWin)  
- **X11** (all WMs)
- **Hyprland / Sway** (via XWayland with `DISPLAY` set)

---

## Multiple Quote Rotation (Shell Trick)

Create a wrapper script that picks a random quote:

```bash
#!/bin/bash
QUOTES=(
  "Be the change you wish to see."
  "Stay hungry, stay foolish."
  "In the middle of difficulty lies opportunity."
)
IDX=$(( RANDOM % ${#QUOTES[@]} ))
# Update the config
jq --arg t "${QUOTES[$IDX]}" '.text = $t' ~/.config/wizer/config.json > /tmp/wq.json
mv /tmp/wq.json ~/.config/wizer/config.json
exec wizer
```

---

## License

MIT — do whatever you want with it.
