Name:           ear
Version:        5.0
Release:        1%{?dist}
Summary:        Energy Aware Runtime - %{version} ready to be deployed.

License: BSD-3 license for individual/non-commercial use and EPL-1.0 license for commercial use.
URL: https://gitlab.bsc.es/ear_team/ear_private
Source0: %{name}-%{version}.tar.gz

%description
EAR rpm package includes the daemons, configuration files and tools required to make EAR working in your cluster.

%prep
%setup

%define _prefix /opt/ear-5.0

%install
rm -rf $RPM_BUILD_ROOT

mkdir -p %{buildroot}%{_bindir}
install -m 0755 %{name} %{buildroot}%{_bindir}/%{name}

%files
%license LICENSE
%{_bindir}/%{name}

%changelog
* Tue May 31 2016 Adam Miller <maxamillion@fedoraproject.org> - 0.1-1
-
