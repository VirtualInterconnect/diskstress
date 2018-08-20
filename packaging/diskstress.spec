Name:           diskstress
Version:        0.1
Release:        1%{?dist}
Summary:        Seek to physically wear out mechanical disks

License:        Apache-2.0
URL:            https://github.com/VirtualInterconnect/diskstress
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  automake
BuildRequires:  autoconf
BuildRequires:  autoconf-archive
BuildRequires:  gcc-c++

%description
diskstress seeks to physically wear out mechanical disks, bypassing any caches present when it can. This may be useful for burn-in and stress-testing disks prior to deploying them in production environments.

%prep
%setup -q
./bootstrap.sh

%build
%configure
make %{?_smp_mflags}

%check
make check

%install
rm -rf $RPM_BUILD_ROOT
%make_install

gzip %{buildroot}%{_mandir}/man8/diskstress.8

%files
%doc README.md NEWS CHANGELOG AUTHORS
%doc %{_mandir}/man8/diskstress.8.gz
%{_bindir}/diskstress

%changelog
* Mon Aug 8 2018 Kyle Maas <opensource@virtualinterconnect.com> 0.1
- Initial release
