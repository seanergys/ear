%define _prefix /hpc/base/EAR/ear5.1.JAMMY-Neo2

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

%install
rm -rf $RPM_BUILD_ROOT

# Creating bin/tools
mkdir -p %{buildroot}%{_bindir}/tools

# Installing all tools' binaries
install -m 0770 bin/tools/* %{buildroot}%{_bindir}/tools

# Installing erun into bin
install -m 0775 bin/erun %{buildroot}%{_bindir}

# Instaling ear-info and econtrol
install -m 2771 bin/ear-info bin/econtrol %{buildroot}%{_bindir}

# Installing eacct and ereport if they were installed
if [ -f bin/eacct ]; then
install -m 2771 bin/eacct %{buildroot}%{_bindir}
fi
if [ -f bin/ereport ]; then
install -m 2771 bin/eacct %{buildroot}%{_bindir}
fi

mkdir -p %{buildroot}%{_sbindir}
install -m 0770 sbin/* %{buildroot}%{_sbindir}

# Creating etc, etc/ear and etc/ear/coeffs
mkdir -p %{buildroot}%{_sysconfdir}/ear/coeffs
if [ -n "$(ls -A etc/ear/coeffs)" ]; then
install -m 0775 etc/ear/coeffs/* %{buildroot}%{_sysconfdir}/ear/coeffs
fi

install -m 0660 etc/ear/defines.log etc/ear/ear.conf* etc/ear/edcmon* %{buildroot}%{_sysconfdir}/ear

# Creating and installing modules
mkdir -p %{buildroot}%{_sysconfdir}/etc/module
install -m 0775 etc/module/* %{buildroot}%{_sysconfdir}/etc/module

mkdir -p %{buildroot}%{_sysconfdir}/etc/slurm
install -m 0644 etc/slurm/ear.plugstack.conf %{buildroot}%{_sysconfdir}/etc/slurm

mkdir -p %{buildroot}%{_sysconfdir}/etc/systemd
install -m 0660 etc/systemd/* %{buildroot}%{_sysconfdir}/etc/systemd

# Creating and installing include dir
mkdir -p %{buildroot}%{_includedir}
install -m 0664 include/ear.h include/earld.h %{buildroot}%{_includedir}

# Creating and install lib
mkdir -p %{buildroot}%{_libdir}
install -m 0664 lib/libearld_dummy.a lib/libear_api.a %{buildroot}%{_libdir}
install -m 0775 lib/libearld* %{buildroot}%{_libdir}
install -m 0755 lib/libear*.so %{buildroot}%{_libdir}

mkdir -p %{buildroot}%{_libdir}/pbs
install -m 0775 lib/pbs/* %{buildroot}%{_libdir}/pbs

mkdir -p %{buildroot}%{_libdir}/plugins/policies
install -m 0775 lib/plugins/policies/* %{buildroot}%{_libdir}/plugins/policies

mkdir -p %{buildroot}%{_libdir}/plugins/tracer
install -m 0775 lib/plugins/tracer/* %{buildroot}%{_libdir}/plugins/tracer

mkdir -p %{buildroot}%{_libdir}/plugins/models
install -m 0775 lib/plugins/models/* %{buildroot}%{_libdir}/plugins/models

mkdir -p %{buildroot}%{_libdir}/plugins/reports
install -m 0775 lib/plugins/reports/* %{buildroot}%{_libdir}/plugins/reports

mkdir -p %{buildroot}%{_libdir}/plugins/energy
install -m 0775 lib/plugins/energy/* %{buildroot}%{_libdir}/plugins/energy

mkdir -p %{buildroot}%{_libdir}/plugins/eard_policies
install -m 0775 lib/plugins/eard_policies/* %{buildroot}%{_libdir}/plugins/eard_policies

mkdir -p %{buildroot}%{_libdir}/plugins/powercap
install -m 0775 lib/plugins/powercap/* %{buildroot}%{_libdir}/plugins/powercap

mkdir -p %{buildroot}%{_libdir}/plugins/monitoring
install -m 0775 lib/plugins/monitoring/* %{buildroot}%{_libdir}/plugins/monitoring

%files
# %license LICENSE
%{_bindir}
%{_includedir}
%{_sysconfdir}
%{_sbindir}
%{_libdir}

%changelog
* Fri Jun 14 Oriol Vidal <oriol.vidal@bsc.es> - 5.0-1
-
