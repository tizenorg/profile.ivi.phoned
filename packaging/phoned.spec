Name:       phoned
Summary:    A service to export OFono/Obex functionality over DBUS, to be used by WebRuntime plugin
Version:    0.0.0
Release:    1
Group:      Development/Libraries
License:    Apache-2.0
Source0:    %{name}-%{version}.tar.gz

BuildRequires:  pkgconfig(ewebkit2)
BuildRequires:  pkgconfig(dpl-efl)
BuildRequires:  pkgconfig(dpl-event-efl)
BuildRequires:  pkgconfig(wrt-plugins-commons)
BuildRequires:  pkgconfig(wrt-plugins-commons-javascript)

BuildRequires:  evolution-data-server-devel
BuildRequires:  wrt-plugins-tizen-devel
BuildRequires:  expat-devel
BuildRequires:  cmake
BuildRequires:  gettext-devel
BuildRequires:  pkgconfig(json-glib-1.0)

%description
A service to export OFono/Obex functionality over DBUS, to be used by WebRuntime plugin

%prep
%setup -q

%build

%define PREFIX "%{_libdir}/wrt-plugins"

export LDFLAGS+="-Wl,--rpath=%{PREFIX} -Wl,--as-needed"

cmake . -DCMAKE_INSTALL_PREFIX=%{_prefix} -DDPL_LOG="ON" -DENABLE_TIME_TRACER="OFF"

make %{?jobs:-j%jobs} VERBOSE=1

%install
rm -rf %{buildroot}
%make_install

%post

%postun

%files
%{_libdir}/pkgconfig/phoned.pc
%{_prefix}/sbin/phoned
%{_prefix}/share/dbus-1/services/org.tizen.phone.service
%{_prefix}/lib/systemd/user/phoned.service

