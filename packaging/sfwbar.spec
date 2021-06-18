Name:           sfwbar
Version:        0.9.8
Release:        1%{?dist}
Summary:        Sway Floating Window Bar
License:        GPL-3.0
URL:            https://github.com/LBCrion/sfwbar
Source0:        https://github.com/LBCrion/sfwbar/archive/refs/tags/v%{version}.tar.gz

BuildRequires:  make
BuildRequires:  meson
BuildRequires:  gcc
BuildRequires:  gtk3-devel 
BuildRequires:  libucl-devel
BuildRequires:  gtk-layer-shell-devel

%global debug_package %{nil}

%description
SFWBar (Sway Floating Window Bar) is a flexible taskbar application for Sway wayland compositor, designed with a stacking layout in mind.

%prep
%autosetup -p1


%build
%meson
%meson_build

%install
%meson_install

%files
%{_bindir}/*
%{_mandir}/man*/*
%{_datadir}/*
%license LICENSE


%changelog
* Fri Jun 18 2021 Lev Babiev <harley@hosers.org> 0.9.8-1
- Fix SPEC

* Tue Jun 08 2021 Adam Thiede <me@adamthiede.com> 0.9.8-1
- Initial specfile
