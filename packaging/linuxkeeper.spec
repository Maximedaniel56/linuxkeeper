Name:           linuxkeeper
Version:        1.0.0
Release:        1%{?dist}
Summary:        Linux audio keep-alive daemon

License:        MIT
URL:            https://github.com/linuxkeeper/linuxkeeper
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake >= 3.16
BuildRequires:  gcc
BuildRequires:  pkg-config
BuildRequires:  alsa-lib-devel
BuildRequires:  pulseaudio-libs-devel
BuildRequires:  pipewire-devel
BuildRequires:  systemd-devel

Requires:       alsa-lib
Recommends:     pulseaudio-libs
Recommends:     pipewire

%description
LinuxKeeper keeps Bluetooth and USB audio devices alive by continuously
playing inaudible silent audio, preventing automatic disconnection or
power-saving suspend on audio interfaces. The Linux equivalent of
Windows SoundKeeper.

%prep
%autosetup

%build
%cmake
%cmake_build

%install
%cmake_install

mkdir -p %{buildroot}%{_userunitdir}
install -m 644 linuxkeeper.service %{buildroot}%{_userunitdir}/

mkdir -p %{buildroot}%{_sysconfdir}/linuxkeeper
install -m 644 linuxkeeper.toml.example %{buildroot}%{_sysconfdir}/linuxkeeper/config.toml.example

%files
%license LICENSE
%doc README.md
%{_bindir}/linuxkeeper
%{_userunitdir}/linuxkeeper.service
%dir %{_sysconfdir}/linuxkeeper
%config(noreplace) %{_sysconfdir}/linuxkeeper/config.toml.example

%changelog
* Thu Jan 01 2024 LinuxKeeper Maintainers <maintainers@linuxkeeper.dev> - 1.0.0-1
- Initial release
