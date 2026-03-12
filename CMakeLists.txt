cmake_minimum_required(VERSION 3.16)
project(linuxkeeper VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Options
option(STATIC_BUILD "Build a fully static binary" OFF)
option(WITH_PIPEWIRE "Enable PipeWire backend" ON)
option(WITH_PULSEAUDIO "Enable PulseAudio backend" ON)
option(WITH_ALSA "Enable ALSA backend" ON)
option(WITH_OSS "Enable OSS backend" ON)
option(WITH_SYSTEMD "Enable systemd journal logging" ON)

# Compiler flags
add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)

if(STATIC_BUILD)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
endif()

# Find packages via pkg-config
find_package(PkgConfig)

set(BACKEND_SOURCES "")
set(BACKEND_LIBS "")
set(BACKEND_DEFS "")

# PipeWire detection
if(WITH_PIPEWIRE AND PkgConfig_FOUND)
    pkg_check_modules(PIPEWIRE libpipewire-0.3)
    if(PIPEWIRE_FOUND)
        list(APPEND BACKEND_SOURCES src/backend_pipewire.c)
        list(APPEND BACKEND_DEFS WITH_PIPEWIRE)
        list(APPEND BACKEND_LIBS ${PIPEWIRE_LIBRARIES})
        include_directories(${PIPEWIRE_INCLUDE_DIRS})
        link_directories(${PIPEWIRE_LIBRARY_DIRS})
        message(STATUS "PipeWire backend: ENABLED")
    else()
        message(STATUS "PipeWire backend: DISABLED (libpipewire-0.3 not found)")
    endif()
else()
    message(STATUS "PipeWire backend: DISABLED")
endif()

# PulseAudio detection
if(WITH_PULSEAUDIO AND PkgConfig_FOUND)
    pkg_check_modules(PULSE libpulse libpulse-simple)
    if(PULSE_FOUND)
        list(APPEND BACKEND_SOURCES src/backend_pulseaudio.c)
        list(APPEND BACKEND_DEFS WITH_PULSEAUDIO)
        list(APPEND BACKEND_LIBS ${PULSE_LIBRARIES})
        include_directories(${PULSE_INCLUDE_DIRS})
        link_directories(${PULSE_LIBRARY_DIRS})
        message(STATUS "PulseAudio backend: ENABLED")
    else()
        message(STATUS "PulseAudio backend: DISABLED (libpulse not found)")
    endif()
else()
    message(STATUS "PulseAudio backend: DISABLED")
endif()

# ALSA detection
if(WITH_ALSA AND PkgConfig_FOUND)
    pkg_check_modules(ALSA alsa)
    if(ALSA_FOUND)
        list(APPEND BACKEND_SOURCES src/backend_alsa.c)
        list(APPEND BACKEND_DEFS WITH_ALSA)
        list(APPEND BACKEND_LIBS ${ALSA_LIBRARIES})
        include_directories(${ALSA_INCLUDE_DIRS})
        link_directories(${ALSA_LIBRARY_DIRS})
        message(STATUS "ALSA backend: ENABLED")
    else()
        message(STATUS "ALSA backend: DISABLED (libasound not found)")
    endif()
else()
    message(STATUS "ALSA backend: DISABLED")
endif()

# OSS detection (always available on Linux if header exists)
if(WITH_OSS)
    include(CheckIncludeFile)
    check_include_file("linux/soundcard.h" HAVE_LINUX_SOUNDCARD_H)
    check_include_file("sys/soundcard.h" HAVE_SYS_SOUNDCARD_H)
    if(HAVE_LINUX_SOUNDCARD_H OR HAVE_SYS_SOUNDCARD_H)
        list(APPEND BACKEND_SOURCES src/backend_oss.c)
        list(APPEND BACKEND_DEFS WITH_OSS)
        message(STATUS "OSS backend: ENABLED")
    else()
        message(STATUS "OSS backend: DISABLED (soundcard.h not found)")
    endif()
else()
    message(STATUS "OSS backend: DISABLED")
endif()

# systemd journal
if(WITH_SYSTEMD AND PkgConfig_FOUND)
    pkg_check_modules(SYSTEMD libsystemd)
    if(SYSTEMD_FOUND)
        list(APPEND BACKEND_DEFS HAVE_SYSTEMD)
        list(APPEND BACKEND_LIBS ${SYSTEMD_LIBRARIES})
        include_directories(${SYSTEMD_INCLUDE_DIRS})
        message(STATUS "systemd journal: ENABLED")
    else()
        message(STATUS "systemd journal: DISABLED (libsystemd not found)")
    endif()
else()
    message(STATUS "systemd journal: DISABLED")
endif()

# Main executable
add_executable(linuxkeeper
    src/main.c
    src/config.c
    src/audio.c
    src/daemon.c
    src/device_monitor.c
    src/log.c
    ${BACKEND_SOURCES}
)

target_compile_definitions(linuxkeeper PRIVATE ${BACKEND_DEFS})
target_link_libraries(linuxkeeper PRIVATE ${BACKEND_LIBS})

# Install targets
include(GNUInstallDirs)
install(TARGETS linuxkeeper RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES linuxkeeper.toml.example DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/linuxkeeper RENAME config.toml.example)
install(FILES linuxkeeper.service DESTINATION lib/systemd/user)

# Valgrind target
find_program(VALGRIND valgrind)
if(VALGRIND)
    add_custom_target(valgrind
        COMMAND ${VALGRIND} --leak-check=full --show-leak-kinds=all
                --track-origins=yes --error-exitcode=1
                $<TARGET_FILE:linuxkeeper> --help
        DEPENDS linuxkeeper
        COMMENT "Running valgrind memory check"
    )
endif()

# CPack for packaging
set(CPACK_PACKAGE_NAME "linuxkeeper")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Linux audio keep-alive daemon")
set(CPACK_PACKAGE_CONTACT "linuxkeeper maintainers")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libasound2 (>= 1.1), libpulse0 (>= 12.0)")
set(CPACK_DEBIAN_PACKAGE_SECTION "sound")
set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Multimedia")
include(CPack)
