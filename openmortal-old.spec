Name:           openmortal-old
Version:        0.7.26
Release:        1%{?dist}
Summary:        OpenMortal — open-source Mortal Kombat parody fighting game (SDL1 branch)
License:        GPL-2.0-or-later
URL:            https://github.com/KAMI911/openmortal-old
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  gcc-c++
BuildRequires:  make
BuildRequires:  automake
BuildRequires:  autoconf
BuildRequires:  libtool
BuildRequires:  pkgconf-m4
BuildRequires:  pkgconf
BuildRequires:  SDL-devel >= 1.2
BuildRequires:  SDL_image-devel
BuildRequires:  SDL_mixer-devel
BuildRequires:  SDL_net-devel
BuildRequires:  freetype-devel
BuildRequires:  giflib-devel
BuildRequires:  perl-devel

%description
OpenMortal is a parody of the famous Mortal Kombat arcade game. The
characters are random digitized fighters. This package tracks the
SDL 1.x legacy branch of the game.

%prep
%autosetup

%build
autoreconf -i -f
%configure
%make_build

%install
%make_install

%files
%license COPYING
%doc ChangeLog README
%{_bindir}/openmortal
%{_datadir}/openmortal/

%changelog
* Thu Mar 12 2026 Kalman Szalai <kami@kami.hu> - 0.7.26-1
- Initial package
