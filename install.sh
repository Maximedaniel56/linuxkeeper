#!/usr/bin/env bash
set -euo pipefail

# LinuxKeeper Installer
# Usage: ./install.sh [--system] [--uninstall]

SYSTEM_INSTALL=0
UNINSTALL=0
PREFIX="$HOME/.local"
SYSTEMD_DIR="$HOME/.config/systemd/user"
CONFIG_DIR="$HOME/.config/linuxkeeper"
BINARY_NAME="linuxkeeper"

for arg in "$@"; do
    case "$arg" in
        --system)  SYSTEM_INSTALL=1; PREFIX="/usr/local" ;;
        --uninstall) UNINSTALL=1 ;;
        --help)
            echo "Usage: $0 [--system] [--uninstall]"
            echo "  --system     Install system-wide to /usr/local (requires sudo)"
            echo "  --uninstall  Remove linuxkeeper"
            exit 0
            ;;
        *) echo "Unknown option: $arg"; exit 1 ;;
    esac
done

if [ "$SYSTEM_INSTALL" -eq 1 ]; then
    CONFIG_DIR="/etc/linuxkeeper"
    SYSTEMD_DIR="/usr/lib/systemd/user"
fi

# Detect distro
detect_distro() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        echo "$ID"
    elif [ -f /etc/arch-release ]; then
        echo "arch"
    elif [ -f /etc/debian_version ]; then
        echo "debian"
    elif [ -f /etc/redhat-release ]; then
        echo "fedora"
    else
        echo "unknown"
    fi
}

DISTRO=$(detect_distro)
echo "Detected distribution: $DISTRO"

# Uninstall
if [ "$UNINSTALL" -eq 1 ]; then
    echo "Uninstalling linuxkeeper..."

    # Stop service if running
    systemctl --user stop linuxkeeper 2>/dev/null || true
    systemctl --user disable linuxkeeper 2>/dev/null || true

    rm -f "$PREFIX/bin/$BINARY_NAME"
    rm -f "$SYSTEMD_DIR/linuxkeeper.service"
    echo "Removed binary and service file."
    echo "Config directory ($CONFIG_DIR) was NOT removed. Delete manually if desired."
    exit 0
fi

# Check for binary
BINARY=""
if [ -f "./build/$BINARY_NAME" ]; then
    BINARY="./build/$BINARY_NAME"
elif [ -f "./$BINARY_NAME" ]; then
    BINARY="./$BINARY_NAME"
else
    echo "Binary not found. Building from source..."

    # Install build dependencies based on distro
    case "$DISTRO" in
        ubuntu|debian|linuxmint|pop)
            echo "Installing build dependencies..."
            sudo apt-get update -qq
            sudo apt-get install -y -qq cmake gcc pkg-config \
                libasound2-dev libpulse-dev libpipewire-0.3-dev libsystemd-dev
            ;;
        fedora|rhel|centos)
            echo "Installing build dependencies..."
            sudo dnf install -y cmake gcc pkg-config \
                alsa-lib-devel pulseaudio-libs-devel pipewire-devel systemd-devel
            ;;
        arch|manjaro)
            echo "Installing build dependencies..."
            sudo pacman -S --needed --noconfirm cmake gcc pkg-config \
                alsa-lib libpulse pipewire
            ;;
        opensuse*|suse*)
            echo "Installing build dependencies..."
            sudo zypper install -y cmake gcc pkg-config \
                alsa-devel libpulse-devel pipewire-devel systemd-devel
            ;;
        alpine)
            echo "Installing build dependencies..."
            sudo apk add cmake gcc musl-dev pkgconfig \
                alsa-lib-dev pulseaudio-dev pipewire-dev
            ;;
        *)
            echo "Unknown distro. Please install cmake, gcc, pkg-config, and audio dev libs manually."
            ;;
    esac

    mkdir -p build
    cd build
    cmake ..
    make -j"$(nproc)"
    cd ..
    BINARY="./build/$BINARY_NAME"
fi

# Install binary
echo "Installing $BINARY_NAME to $PREFIX/bin/"
mkdir -p "$PREFIX/bin"
if [ "$SYSTEM_INSTALL" -eq 1 ]; then
    sudo install -m 755 "$BINARY" "$PREFIX/bin/$BINARY_NAME"
else
    install -m 755 "$BINARY" "$PREFIX/bin/$BINARY_NAME"
fi

# Install config example
mkdir -p "$CONFIG_DIR"
if [ ! -f "$CONFIG_DIR/config.toml" ]; then
    if [ "$SYSTEM_INSTALL" -eq 1 ]; then
        sudo cp linuxkeeper.toml.example "$CONFIG_DIR/config.toml"
    else
        cp linuxkeeper.toml.example "$CONFIG_DIR/config.toml"
    fi
    echo "Installed example config to $CONFIG_DIR/config.toml"
else
    echo "Config file already exists at $CONFIG_DIR/config.toml (not overwritten)"
fi

# Install systemd service
mkdir -p "$SYSTEMD_DIR"
if [ "$SYSTEM_INSTALL" -eq 1 ]; then
    sudo install -m 644 linuxkeeper.service "$SYSTEMD_DIR/"
else
    install -m 644 linuxkeeper.service "$SYSTEMD_DIR/"
fi

# Enable and start service
systemctl --user daemon-reload
systemctl --user enable linuxkeeper.service
systemctl --user start linuxkeeper.service

echo ""
echo "============================================"
echo " LinuxKeeper installed successfully!"
echo "============================================"
echo ""
echo "  Binary:  $PREFIX/bin/$BINARY_NAME"
echo "  Config:  $CONFIG_DIR/config.toml"
echo "  Service: $SYSTEMD_DIR/linuxkeeper.service"
echo ""
echo "  Status:    systemctl --user status linuxkeeper"
echo "  Logs:      journalctl --user -u linuxkeeper -f"
echo "  Stop:      systemctl --user stop linuxkeeper"
echo "  Uninstall: $0 --uninstall"
echo ""
echo "  Make sure $PREFIX/bin is in your PATH."
echo ""
