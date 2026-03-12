# Convenience Makefile wrapper around CMake

BUILD_DIR ?= build
CMAKE_FLAGS ?=
PREFIX ?= /usr/local

.PHONY: all clean install uninstall package-deb package-rpm appimage

all:
	@mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake $(CMAKE_FLAGS) -DCMAKE_INSTALL_PREFIX=$(PREFIX) .. && $(MAKE)

static:
	@mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake $(CMAKE_FLAGS) -DSTATIC_BUILD=ON -DCMAKE_INSTALL_PREFIX=$(PREFIX) .. && $(MAKE)

clean:
	rm -rf $(BUILD_DIR)

install: all
	cd $(BUILD_DIR) && $(MAKE) install

uninstall:
	rm -f $(PREFIX)/bin/linuxkeeper
	rm -f $(PREFIX)/etc/linuxkeeper/config.toml.example
	rm -f /usr/lib/systemd/user/linuxkeeper.service

package-deb: all
	cd $(BUILD_DIR) && cpack -G DEB

package-rpm: all
	cd $(BUILD_DIR) && cpack -G RPM

appimage: all
	@echo "Building AppImage..."
	@mkdir -p $(BUILD_DIR)/AppDir/usr/bin
	cp $(BUILD_DIR)/linuxkeeper $(BUILD_DIR)/AppDir/usr/bin/
	@mkdir -p $(BUILD_DIR)/AppDir/usr/share/applications
	@mkdir -p $(BUILD_DIR)/AppDir/usr/share/icons/hicolor/256x256/apps
	@echo '[Desktop Entry]' > $(BUILD_DIR)/AppDir/usr/share/applications/linuxkeeper.desktop
	@echo 'Name=LinuxKeeper' >> $(BUILD_DIR)/AppDir/usr/share/applications/linuxkeeper.desktop
	@echo 'Exec=linuxkeeper' >> $(BUILD_DIR)/AppDir/usr/share/applications/linuxkeeper.desktop
	@echo 'Icon=linuxkeeper' >> $(BUILD_DIR)/AppDir/usr/share/applications/linuxkeeper.desktop
	@echo 'Type=Application' >> $(BUILD_DIR)/AppDir/usr/share/applications/linuxkeeper.desktop
	@echo 'Categories=AudioVideo;Audio;' >> $(BUILD_DIR)/AppDir/usr/share/applications/linuxkeeper.desktop
	cp $(BUILD_DIR)/AppDir/usr/share/applications/linuxkeeper.desktop $(BUILD_DIR)/AppDir/
	@# Generate a simple icon (1x1 transparent PNG as placeholder)
	@printf '\x89PNG\r\n\x1a\n' > $(BUILD_DIR)/AppDir/linuxkeeper.png
	@if command -v appimagetool >/dev/null 2>&1; then \
		ARCH=$$(uname -m) appimagetool $(BUILD_DIR)/AppDir $(BUILD_DIR)/linuxkeeper-$$(uname -m).AppImage; \
	else \
		echo "appimagetool not found. Install from https://github.com/AppImage/AppImageKit"; \
		exit 1; \
	fi

valgrind: all
	cd $(BUILD_DIR) && $(MAKE) valgrind

help:
	@echo "Targets:"
	@echo "  all          - Build linuxkeeper (default)"
	@echo "  static       - Build fully static binary"
	@echo "  clean        - Remove build directory"
	@echo "  install      - Install to PREFIX (default /usr/local)"
	@echo "  uninstall    - Remove installed files"
	@echo "  package-deb  - Build .deb package"
	@echo "  package-rpm  - Build .rpm package"
	@echo "  appimage     - Build AppImage"
	@echo "  valgrind     - Run valgrind memory check"
