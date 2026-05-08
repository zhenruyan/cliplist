#!/bin/bash
set -e

# Configuration
PACKAGE="cliplist"
VERSION="${1:-1.0.0}"
ARCH="amd64"
MAINTAINER="Cliplist Contributors"
DESCRIPTION="Clipboard history manager for Linux (XFCE4 + Xorg)"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build"
DEB_DIR="$BUILD_DIR/${PACKAGE}_${VERSION}_${ARCH}"

cd "$PROJECT_DIR"

echo "==> Building binaries..."
make build

echo "==> Creating deb structure..."
rm -rf "$DEB_DIR"
mkdir -p "$DEB_DIR/DEBIAN"
mkdir -p "$DEB_DIR/usr/bin"
mkdir -p "$DEB_DIR/usr/share/applications"
mkdir -p "$DEB_DIR/usr/share/doc/$PACKAGE"
mkdir -p "$DEB_DIR/etc/xdg/autostart"

echo "==> Installing files..."
install -m 755 cliplistd  "$DEB_DIR/usr/bin/cliplistd"
install -m 755 cliplist   "$DEB_DIR/usr/bin/cliplist"
install -m 644 assets/cliplist.desktop "$DEB_DIR/usr/share/applications/cliplist.desktop"
install -m 644 assets/cliplist.desktop "$DEB_DIR/etc/xdg/autostart/cliplist.desktop"
install -m 644 assets/config.toml.example "$DEB_DIR/usr/share/doc/$PACKAGE/config.toml.example"
install -m 644 README.md "$DEB_DIR/usr/share/doc/$PACKAGE/README.md"

echo "==> Generating DEBIAN/control..."
INSTALLED_SIZE=$(du -sk "$DEB_DIR/usr" "$DEB_DIR/etc" 2>/dev/null | awk '{s+=$1} END {print s}')

cat > "$DEB_DIR/DEBIAN/control" << EOF
Package: $PACKAGE
Version: $VERSION
Section: utils
Priority: optional
Architecture: $ARCH
Depends: libgtk-3-0, libayatana-appindicator3-1, xclip
Installed-Size: $INSTALLED_SIZE
Maintainer: $MAINTAINER
Description: $DESCRIPTION
 A lightweight clipboard history manager that sits in the system tray.
 Features include search, favorites, image clipboard support, and
 configurable global hotkey.
EOF

echo "==> Generating postinst..."
cat > "$DEB_DIR/DEBIAN/postinst" << 'EOF'
#!/bin/sh
set -e

case "$1" in
    configure)
        echo "Cliplist installed. Run 'cliplistd' to start the daemon."
        echo "Run 'cliplist settings' or click Settings in the tray menu to configure."
        ;;
esac

#DEBHELPER#
exit 0
EOF
chmod 755 "$DEB_DIR/DEBIAN/postinst"

echo "==> Generating prerm..."
cat > "$DEB_DIR/DEBIAN/prerm" << 'EOF'
#!/bin/sh
set -e

case "$1" in
    remove|upgrade)
        # Stop the daemon if running
        pkill -f cliplistd 2>/dev/null || true
        ;;
esac

#DEBHELPER#
exit 0
EOF
chmod 755 "$DEB_DIR/DEBIAN/prerm"

echo "==> Building deb package..."
dpkg-deb --root-owner-group --build "$DEB_DIR"

DEB_FILE="$BUILD_DIR/${PACKAGE}_${VERSION}_${ARCH}.deb"
echo ""
echo "==> Done: $DEB_FILE"
echo "    Install with: sudo dpkg -i $DEB_FILE"
