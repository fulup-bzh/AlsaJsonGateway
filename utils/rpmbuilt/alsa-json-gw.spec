#
# spec file for package alsa-json-gw
#
# Copyright (c) 2015 SUSE LINUX Products GmbH, Nuernberg, Germany.
#
# All modifications and additions to the file contributed by third parties
# remain the property of their copyright owners, unless otherwise agreed
# upon. The license for this file, and modifications and additions to the
# file, is the same license as for the pristine package itself (unless the
# license for the pristine package is not an Open Source License, in which
# case the license is the MIT License). An "Open Source License" is a
# license that conforms to the Open Source Definition (Version 1.9)
# published by the Open Source Initiative.

# Please submit bugfixes or comments via http://bugs.opensuse.org/
#

Name:    AlsaJsonGateway
Version: master
Release: 1
License: Apache
Summary: HTTP REST/JSON Gateway to ALSA mixer service for HTML5 UI
Url:     https://github.com/fulup-bzh/AlsaJsonGateway
Source:  https://github.com/fulup-bzh/AlsaJsonGateway/archive/master.tar.gz
#BuildRequires: libjson-c-devel, libmicrohttpd-devel, alsa-lib-devel

BuildRoot:      %{_tmppath}/%{name}-%{version}-build
Provides: ajg-daemon
Prefix: /opt/ajg-daemon

%if 0%{?suse_version}
BuildRequires: pkg-config, libjson-c-devel, libmicrohttpd-devel, alsa-lib-devel
%else
BuildRequires:pkg-config, json-c-devel, libmicrohttpd-devel, alsa-lib-devel
%endif

%description 
Provides an HTTP REST interface to ALSA mixer for HTML5 UI support. The main objective of AJG is to decouple ALSA from UI, this especially for Music Oriented Sound boards like Scarlett Focurite and others.

%prep
%setup -q

%build
(cd src; make %{?_smp_mflags})

%install
(cd src; make built DESTDIR=%{buildroot} %{?_smp_mflags})

%post
ln -sf /opt/ajg-daemon/bin/ajg-daemon /usr/local/bin/ajg-daemon


%files
/opt/ajg-daemon
/opt/ajg-daemon/*
