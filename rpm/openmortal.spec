%global sum	Parody of Mortal Kombat

Name:           openmortal
Version:        0.7.30
Release:        1%{?dist}

Summary:        Parody fighting game based on Mortal Kombat
License:        GPLv2
URL:            https://github.com/KAMI911/openmortal-old
Source0:        https://github.com/KAMI911/openmortal-old/archive/refs/heads/upgrade.tar.gz


%if 0%{?fedora}
# Fedora dropped the plain SDL-devel package; sdl12-compat-devel (SDL 1.2
# API on top of SDL2) is what SDL_image/mixer/net-devel depend on there,
# and all three of those are installable as-is.
BuildRequires:	sdl12-compat-devel
BuildRequires:	SDL_image-devel
BuildRequires:	SDL_mixer-devel
BuildRequires:	SDL_net-devel
%endif

%if 0%{?rhel} == 8
# EL8 ships a real SDL-devel, SDL_image-devel and SDL_net-devel, but no
# SDL_mixer 1.2 devel package at all — CI builds just that one from
# source into /usr (see .github/workflows/unified-build.yml).
BuildRequires:	SDL-devel
BuildRequires:	SDL_image-devel
BuildRequires:	SDL_net-devel
%endif

# EL9 (0%{?rhel} == 9): none of SDL/SDL_image/SDL_mixer/SDL_net-devel are
# installable — EPEL9's *-devel packages all depend on an SDL-devel that
# EPEL9 doesn't ship. CI builds the whole stack from source directly into
# /usr before rpmbuild runs (sdl_mode: source), so nothing to declare here.

BuildRequires:	freetype-devel
BuildRequires:	perl-devel
BuildRequires:	perl-ExtUtils-Embed

%description
OpenMortal is a spoof of the original Mortal Kombat fighting game. The
game is playable in two-player mode and features 16+ characters. New
characters can be added, allowing players to create themselves as fighters.
The game includes network play capability through MortalNet.

Key features:
- Two-player fighting game with multiple characters
- Network multiplayer support (port 14882/TCP)
- Customizable characters
- Various game modes and difficulty levels
- Team mode support

%prep
%autosetup -n %{name}-old-upgrade

# Fix any permissions issues
find . -type f -name "*.sh" -exec chmod +x {} \;

%build
# Regenerate build system if needed
autoreconf -fi || :

%configure \
    --bindir=%{_bindir} \
    --datadir=%{_datadir}

%make_build

%install
%make_install

# Create desktop file if it doesn't exist
mkdir -p %{buildroot}%{_datadir}/applications
cat > %{buildroot}%{_datadir}/applications/%{name}.desktop << EOF
[Desktop Entry]
Name=OpenMortal
Comment=Mortal Kombat parody fighting game
Exec=%{name}
Icon=%{name}
Terminal=false
Type=Application
Categories=Game;ArcadeGame;
EOF

# Install icon if available
if [ -f data/gfx/icon.png ]; then
    mkdir -p %{buildroot}%{_datadir}/pixmaps
    install -m 644 data/gfx/icon.png %{buildroot}%{_datadir}/pixmaps/%{name}.png
elif [ -f data/gfx/openmortal.png ]; then
    mkdir -p %{buildroot}%{_datadir}/pixmaps
    install -m 644 data/gfx/openmortal.png %{buildroot}%{_datadir}/pixmaps/%{name}.png
fi

%files
%license COPYING
%doc README AUTHORS ChangeLog TODO INSTALL PACKAGERS
%{_bindir}/%{name}
%{_datadir}/%{name}/
%{_datadir}/applications/%{name}.desktop
%if 0%{?fedora} || 0%{?rhel} >= 8
%{_datadir}/pixmaps/%{name}.png
%endif

%changelog
* Sun Jan 25 2026 Package Builder <builder@localhost> - 0.7.1-1
- Initial RPM package for OpenMortal 0.7.1
- Based on upgrade branch from GitHub
- Added desktop integration
- Updated for Fedora 41 with sdl12-compat
- Configured SDL dependencies for modern Fedora

* Thu Jun 15 2006 Original Developer - 0.7.1-0
- Added additional backgrounds from KAMI
- Bug fixes and improvements

* Sat May 01 2004 Original Developer - 0.7-0
- High/true color video mode support
- Transparent shadows
- Team mode added
- Network play improvements
