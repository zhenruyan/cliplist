<p align="center">
  <img src="assets/icons/cliplist.svg" width="96" height="96" alt="Cliplist">
</p>

# Cliplist

A lightweight clipboard history manager for Linux (X11/XFCE4).

## Features

- **Clipboard monitoring** — automatically saves text and image clipboard history
- **System tray** — XFCE4 panel integration via AppIndicator
- **Global hotkey** — configurable `Ctrl+Shift+V` popup with search
- **Search** — find clips by keyword
- **Favorites** — pin important clips
- **Image support** — saves screenshots and image copies
- **Auto-paste** — selects a clip, closes popup, and pastes into the last window
- **CLI interface** — manage clips from the terminal
- **Auto-start** — XDG autostart and Systemd user service support

## Requirements

```bash
# Debian/Ubuntu
sudo apt install libx11-dev libxfixes-dev libsqlite3-dev \
    libgtk-3-dev libayatana-appindicator3-dev xclip xdotool

# Go 1.21+
```

## Build & Install

```bash
make build          # Build both daemon and CLI
make install        # Install to /usr/local/bin + autostart + systemd
make deb            # Build .deb package (build/ directory)
```

### Enable Autostart (Systemd)

Recommended way to run Cliplist on modern distributions:

```bash
systemctl --user enable --now cliplist.service
```

## Usage

### Start the daemon

```bash
cliplistd &
```

### Global Hotkey

Press `Ctrl+Shift+V` (default) to open the popup:
- Type to search
- Up/Down to navigate
- Enter to select and auto-paste
- Escape to close

### CLI commands

```bash
cliplist list [n]       # Show last n clips (default 20)
cliplist search <q>    # Search clips by keyword
cliplist copy <id>     # Copy clip to clipboard
cliplist fav <id>      # Toggle favorite
cliplist delete <id>   # Delete a clip
cliplist clear         # Clear all non-favorite clips
```

Aliases: `ls`, `s`, `c`, `f`, `rm`

## Configuration

Config file: `~/.config/cliplist/config.toml`

```toml
max_history = 100
poll_interval = "500ms"
db_path = "~/.config/cliplist/cliplist.db"
image_dir = "~/.local/share/cliplist/images"
paste_delay = "80ms"
auto_start = true
ignore_whitespace = true
max_content_length = 1048576

[hotkey]
modifiers = ["ctrl", "shift"]
key = "v"
```

### Supported hotkey modifiers

`ctrl`, `shift`, `alt`, `super`

### Supported hotkey keys

Letters `a`-`z`, numbers `0`-`9`, `space`, `return`, `tab`, `escape`, `f1`-`f12`

## Architecture

```
cliplistd (daemon)
├── X11 Monitor     — polls CLIPBOARD via xclip (text + images)
├── Store           — SQLite database for clip history
├── Tray            — AppIndicator system tray (GTK3/CGo)
├── Popup           — GTK3 popup window with search
├── Hotkey          — X11 XGrabKey global shortcut (xgb)
└── IPC Server      — Unix socket for CLI communication

cliplist (CLI)
└── IPC Client      — sends commands to daemon via Unix socket
```

## Author

pkold.com — [github.com/zhenruyan](https://github.com/zhenruyan)

## License

MIT
