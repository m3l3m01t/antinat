
%define enable_ipv6 0

Summary: A SOCKS server and client library for SOCKS4 and SOCKS5
Name: antinat
Version: 0.90
Release: 1
License: GPL
URL: http://yallara.cs.rmit.edu.au/~malsmith/products/antinat/
Packager: Malcolm Smith <malxau@users.sourceforge.net>
Group: System Environment/Daemons
Source: %{name}-%{version}.tar.bz2
Requires: antinat-libs >= %{version}
Buildroot: %{_tmppath}/%{name}-%{version}-%{release}-root

%description
A SOCKS server for SOCKS4 and SOCKS5, and client library for SOCKS4, SOCKS5
and https proxies.

%clean
rm -fR $RPM_BUILD_ROOT

%prep
%setup -q

%build
%if %{enable_ipv6}
%configure --with-ipv6
%else
%configure
%endif

make

%install
%makeinstall LOGDIR="$RPM_BUILD_ROOT/%{_localstatedir}/log/antinat-%{version}"

%files
%defattr(-,root,root)
%doc COPYING README TODO
%config(noreplace) %{_sysconfdir}/antinat.xml
%{_bindir}/antinat
%{_mandir}/man1/antinat*
%dir %{_localstatedir}/log/antinat-%{version}
%{_mandir}/man4/antinat*

%package libs
Group: System Environment/Libraries
Summary: Client library for antinat applications

%description libs
Support for antinat-based applications that can run through SOCKS4, SOCKS5
and https proxies.

%files libs
%defattr(-,root,root)
%{_libdir}/libantinat.so*

%package libs-devel
Group: Development/Libraries
Summary: Development for antinat applications

%description libs-devel
Create antinat-based applications that can run through SOCKS4, SOCKS5
and https proxies.

%files libs-devel
%defattr(-,root,root)
%{_libdir}/libantinat.a
%{_libdir}/libantinat.la
%{_includedir}/antinat.h
%{_mandir}/man3/an_*
%{_mandir}/man3/AN_*
%{_bindir}/antinat-config

