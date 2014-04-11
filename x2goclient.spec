Name:           x2goclient
Version:        4.0.1.5
Release:        0.0x2go1%{?dist}
Summary:        X2Go Client application (Qt4)

Group:          Applications/Communications
License:        GPLv2+
URL:            http://www.x2go.org
Source0:        http://code.x2go.org/releases/source/%{name}/%{name}-%{version}.tar.gz

BuildRequires:  cups-devel
BuildRequires:  desktop-file-utils
#BuildRequires:  libssh-devel >= 0.6.1
BuildRequires:  libssh-devel
BuildRequires:  libXpm-devel
%if 0%{?fedora}
BuildRequires:  man2html-core
%else
BuildRequires:  man
%endif
BuildRequires:  openldap-devel
%if 0%{?el5} || 0%{?el6}
BuildRequires:  qt4-devel
%else
BuildRequires:  qt-devel
%endif
%if 0%{?fedora} >= 19 || 0%{?rhel} >= 6
BuildRequires:  qtbrowserplugin-static
%endif
Requires:       hicolor-icon-theme
Requires:       nxproxy
Requires:       openssh-clients, openssh-server

%if 0%{?el5}
# For compatibility with EPEL5
BuildRoot:      %(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)
%endif

%description
X2Go is a server-based computing environment with
    - session resuming
    - low bandwidth support
    - session brokerage support
    - client-side mass storage mounting support
    - client-side printing support
    - audio support
    - authentication by smartcard and USB stick

X2Go Client is a graphical client (Qt4) for the X2Go system.
You can use it to connect to running sessions and start new sessions.


%package -n x2goplugin
Summary: X2Go Client (Qt4) as browser plugin
Group:          Applications/Communications
Requires:       mozilla-filesystem
Requires:       nxproxy
Requires:       openssh-clients, openssh-server

%description -n x2goplugin
X2Go is a server-based computing environment with
    - session resuming
    - low bandwidth support
    - session brokerage support
    - client-side mass storage mounting support
    - client-side printing support
    - audio support
    - authentication by smartcard and USB stick

X2Go Client is a graphical client (qt4) for the X2Go system.
You can use it to connect to running sessions and start new sessions.

This package provides X2Go Client as QtBrowser-based Mozilla plugin.


%package -n x2goplugin-provider
Summary: Provide X2Go Plugin via Apache webserver
Group:          Applications/Communications
Requires: httpd

%description -n x2goplugin-provider
X2Go is a server-based computing environment with
    - session resuming
    - low bandwidth support
    - session brokerage support
    - client-side mass storage mounting support
    - client-side printing support
    - audio support
    - authentication by smartcard and USB stick

This package provides an example configuration for providing
the X2Go Plugin via an Apache webserver.

%prep
%setup -q
# Fix up install issues
sed -i -e 's/-o root -g root//' Makefile
sed -i -e '/^MOZPLUGDIR=/s/lib/%{_lib}/' Makefile
%if 0%{?el5}
sed -i -e '/^QMAKE_BINARY=/s@qmake-qt4@%{_libdir}/qt4/bin/qmake@' Makefile
sed -i -e '/^LRELEASE_BINARY=/s@lrelease-qt4@%{_libdir}/qt4/bin/lrelease@' Makefile
%endif
%if 0%{?fedora} >= 19 || 0%{?rhel} >= 6
# Use system qtbrowserplugin
sed -i -e '/CFGPLUGIN/aTEMPLATE=lib' x2goclient.pro
sed -i -e '/^LIBS /s/$/ -ldl/' x2goclient.pro
sed -i -e 's/include.*qtbrowserplugin.pri)/LIBS += -lqtbrowserplugin/' x2goclient.pro
rm -r qtbrowserplugin*
%endif


%build
export PATH=%{_qt4_bindir}:$PATH
make %{?_smp_mflags}


%install
make install DESTDIR=%{buildroot} PREFIX=%{_prefix}
desktop-file-validate %{buildroot}%{_datadir}/applications/%{name}.desktop

mkdir -p %{buildroot}%{_sysconfdir}/httpd/conf.d
ln -s ../../x2go/x2goplugin-apache.conf %{buildroot}%{_sysconfdir}/httpd/conf.d/x2goplugin-provider.conf

%post
/bin/touch --no-create %{_datadir}/icons/hicolor &>/dev/null || :

%postun
if [ $1 -eq 0 ] ; then
    /bin/touch --no-create %{_datadir}/icons/hicolor &>/dev/null
    /usr/bin/gtk-update-icon-cache %{_datadir}/icons/hicolor &>/dev/null || :
fi

%posttrans
/usr/bin/gtk-update-icon-cache %{_datadir}/icons/hicolor &>/dev/null || :


%files
%doc AUTHORS COPYING LICENSE 
%{_bindir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/128x128/apps/%{name}.png
%{_datadir}/icons/hicolor/16x16/apps/%{name}.png
%{_datadir}/icons/hicolor/32x32/apps/%{name}.png
%{_datadir}/icons/hicolor/64x64/apps/%{name}.png
%{_datadir}/%{name}/
%{_mandir}/man1/%{name}.1.gz


%files -n x2goplugin
%{_libdir}/mozilla/plugins/libx2goplugin.so

%files -n x2goplugin-provider
# Link
%{_sysconfdir}/httpd/conf.d/x2goplugin-provider.conf
%dir %{_sysconfdir}/x2go
%dir %{_sysconfdir}/x2go/plugin-provider
%config(noreplace) %{_sysconfdir}/x2go/plugin-provider/x2goplugin.html
%config(noreplace) %{_sysconfdir}/x2go/x2goplugin-apache.conf
%{_datadir}/x2go/


%changelog
