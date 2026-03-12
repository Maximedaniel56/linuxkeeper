# LinuxKeeper

**Linux audio keep-alive daemon** — the Linux equivalent of [Windows SoundKeeper](https://veg.by/projects/soundkeeper/).

## The Problem

Bluetooth headphones, USB DACs, and some audio interfaces automatically disconnect or enter power-saving mode when no audio is playing. This causes:

- **Bluetooth reconnection delays** — 2-5 seconds of silence before audio starts playing again
- **Audio pops and clicks** — when the device wakes up from sleep
- **Complete disconnection** — some BT devices disconnect entirely after idle timeout
- **USB DAC sleep** — some DACs enter a low-power mode that causes the first second of audio to be lost

## The Solution

LinuxKeeper continuously plays **true silence** (zero-filled PCM buffers) to your audio devices, keeping them in an active state. The stream is completely inaudible — it uses zero-amplitude samples, not software mute — so your audio hardware stays powered and connected with zero audible side effects.

## Features

- **Multi-backend**: PipeWire, PulseAudio, ALSA, OSS — auto-detected at runtime
- **Multi-device**: Keep multiple devices alive simultaneously
- **Bluetooth-aware**: Optional mode to auto-detect and keep all BT sinks alive
- **Zero dependencies**: Single static binary option for maximum portability
- **Systemd integration**: Ships as a user service with journal logging
- **Watchdog**: Auto-reconnects with exponential backoff on stream errors
- **Hotplug-aware**: Detects device connect/disconnect events
- **Configurable**: TOML config file with sensible defaults

## Quick Install

```bash
# Build and install (auto-detects your distro and installs dependencies)
git clone https://github.com/linuxkeeper/linuxkeeper.git
cd linuxkeeper
./install.sh
```

## Manual Build

### Dependencies

| Distro | Command |
|--------|---------|
| **Ubuntu/Debian** | `sudo apt install cmake gcc pkg-config libasound2-dev libpulse-dev libpipewire-0.3-dev libsystemd-dev` |
| **Fedora/RHEL** | `sudo dnf install cmake gcc pkg-config alsa-lib-devel pulseaudio-libs-devel pipewire-devel systemd-devel` |
| **Arch/Manjaro** | `sudo pacman -S cmake gcc pkg-config alsa-lib libpulse pipewire` |
| **openSUSE** | `sudo zypper install cmake gcc pkg-config alsa-devel libpulse-devel pipewire-devel systemd-devel` |
| **Alpine** | `sudo apk add cmake gcc musl-dev pkgconfig alsa-lib-dev pulseaudio-dev pipewire-dev` |

### Build

```bash
# Standard build (dynamically linked, uses available backends)
make

# Static build (no runtime .so dependencies)
make static

# Build with specific backends only
mkdir build && cd build
cmake -DWITH_PIPEWIRE=OFF -DWITH_OSS=OFF ..
make
```

### Install

```bash
# User install (to ~/.local/bin)
./install.sh

# System-wide install (to /usr/local/bin)
sudo ./install.sh --system

# Or manually via CMake
cd build && sudo make install
```

### Packaging

```bash
make package-deb    # Build .deb package
make package-rpm    # Build .rpm package
make appimage       # Build AppImage
```

For Arch Linux, copy `packaging/PKGBUILD` to a build directory and run `makepkg -si`.

## Usage

```bash
# Run in foreground (useful for testing)
linuxkeeper

# Run as daemon
linuxkeeper --daemon

# Target a specific device
linuxkeeper -D "bluez_sink.AA_BB_CC_DD_EE_FF.a2dp_sink"

# Force a specific backend
linuxkeeper --backend alsa

# List available audio devices
linuxkeeper --list-devices

# Check if daemon is running
linuxkeeper --status

# Stop the daemon
linuxkeeper --kill

# Verbose output for debugging
linuxkeeper -v
```

## Systemd Setup

The installer automatically sets up a systemd user service. You can also do it manually:

```bash
# Copy service file
mkdir -p ~/.config/systemd/user
cp linuxkeeper.service ~/.config/systemd/user/

# Enable and start
systemctl --user daemon-reload
systemctl --user enable --now linuxkeeper

# Check status
systemctl --user status linuxkeeper

# View logs
journalctl --user -u linuxkeeper -f
```

## Configuration

Config file locations (checked in order):
1. Path from `--config` flag
2. `$XDG_CONFIG_HOME/linuxkeeper/config.toml` (default: `~/.config/linuxkeeper/config.toml`)
3. `/etc/linuxkeeper/config.toml`

### Full Config Reference

```toml
[audio]
# Audio backend: auto | pipewire | pulseaudio | alsa | oss
backend = "auto"

# Sample rate in Hz
sample_rate = 44100

# Number of channels (1=mono, 2=stereo)
channels = 2

# Buffer size in frames
buffer_size = 2048

# Target devices. Use "default", specific names, or "all"
target_devices = ["default"]

[daemon]
# Log level: debug | info | warn | error
log_level = "info"

# Log file path (empty = stderr/journald)
log_file = ""

# PID file path
pid_file = "/run/user/1000/linuxkeeper.pid"

# Max reconnect backoff interval (seconds)
watchdog_max_interval = 30

[stream]
# Reconnect when audio device changes
reconnect_on_device_change = true

# Keep ALL Bluetooth sinks alive automatically
keep_all_bt_devices = false
```

## Troubleshooting

### Bluetooth device still disconnects

1. Check that linuxkeeper is actually running: `linuxkeeper --status`
2. Try targeting the device explicitly: `linuxkeeper -D "bluez_sink.XX_XX_XX_XX_XX_XX.a2dp_sink"`
3. Find your device name: `linuxkeeper --list-devices` or `pactl list sinks short`
4. Enable debug logging: `linuxkeeper -v`

### ALSA "Device or resource busy"

Another application may have exclusive access to the ALSA device. Solutions:
- Use PulseAudio or PipeWire backend instead (they handle sharing)
- Close the other application using the device
- Use `dmix` plugin in ALSA config for software mixing

### PipeWire permission errors

Ensure your user has access to the PipeWire socket:
```bash
ls -la /run/user/$(id -u)/pipewire-0
```
If missing, check that PipeWire is running: `systemctl --user status pipewire`

### PulseAudio "Connection refused"

```bash
# Check if PulseAudio is running
pulseaudio --check
# Or for PipeWire's PulseAudio compatibility
systemctl --user status pipewire-pulse
```

### No backends available

Ensure at least one audio development library is installed. ALSA is the most universal:
```bash
# Ubuntu/Debian
sudo apt install libasound2-dev
# Then rebuild
make clean && make
```

### High CPU usage

This should not happen with normal operation. If it does:
1. Increase `buffer_size` in config (e.g., 4096 or 8192)
2. Check if the audio device is repeatedly failing and triggering reconnects
3. Run with `-v` to see what's happening

## How It Works

LinuxKeeper generates a continuous stream of PCM silence (zero-filled buffers) and writes it to your audio output device. The audio data is true silence — all sample values are zero — which means:

- The audio hardware DAC receives a valid PCM stream and stays active
- No audible sound is produced (zero amplitude = silence)
- Bluetooth A2DP/aptX/LDAC codecs stay connected because the stream is active
- USB DACs maintain their active state

The daemon monitors the audio stream for errors and automatically reconnects using exponential backoff (1s, 2s, 4s, 8s, up to the configured maximum).

## Contributing

1. Fork the repository
2. Create a feature branch: `git checkout -b my-feature`
3. Make your changes
4. Test with valgrind: `make valgrind`
5. Ensure clean compilation: `gcc -Wall -Wextra -std=c11`
6. Submit a pull request

### Code Style

- C11 standard
- 4-space indentation
- `lk_` prefix for all public symbols
- Each backend in its own file behind the `AudioBackend` vtable interface

## License

MIT License. See [LICENSE](LICENSE) for details.
