Name:       phoned
Summary:    OFono/Obex business logic for phone web APIs
Version:    0.0.0
Release:    1
Group:      Automotive/Modello
License:    Apache-2.0
Source0:    %{name}-%{version}.tar.gz

BuildRequires:  cmake
BuildRequires:  gettext-devel
BuildRequires:  pkgconfig(libebook-contacts-1.2)
BuildRequires:  pkgconfig(expat)
BuildRequires:  pkgconfig(json-glib-1.0)
BuildRequires:  pkgconfig(dbus-1)

%description
A service to export OFono/Obex functionality over DBUS, to be used by WebRuntime plugin

%prep
%setup -q

%build

%define PREFIX "%{_libdir}/wrt-plugins"

export LDFLAGS+="-Wl,--rpath=%{PREFIX} -Wl,--as-needed"

cmake . -DCMAKE_INSTALL_PREFIX=%{_prefix} -DENABLE_TIME_TRACER="OFF"

make %{?jobs:-j%jobs} VERBOSE=1

%install
rm -rf %{buildroot}
%make_install

%install_service ../user/weston.target.wants phoned.service

%files
%{_libdir}/pkgconfig/phoned.pc
%{_prefix}/bin/phoned
%{_prefix}/share/dbus-1/services/org.tizen.phone.service
%{_unitdir_user}/phoned.service
%{_unitdir_user}/weston.target.wants/phoned.service

