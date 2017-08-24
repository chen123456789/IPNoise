%define packages_path   ipnoise/IPNoise/packages/
%define package_name    kernel
%define package_path    %{packages_path}/ipnoise-%{package_name}

%define name            ipnoise-kernel
%define version         3.13.1
%define release         3

Name:           %{name}
Summary:        IPNoise kernel
Version:        %{version}
Release:        %{release}
URL:            http://ipnoise.ru/

Group:          Applications/Multimedia
BuildRoot:      %{_tmppath}/ipnoise-buildroot
License:        Copyright Roman. E. Chechnev

%description
IPNoise kernel
TODO add description here

%prep
# Look at http://rpm.org/max-rpm/s1-rpm-inside-macros.html
# for help with %setup
%setup -T -D -c -n ipnoise
%{git_clone}
cd ipnoise
%{git_checkout}
%{git_pull}

%build
mkdir -p %buildroot
make -C %{package_path} lin32

%install
cd %{package_path}
make install DESTDIR=%buildroot
ipnoise-devel-rpms-find_files --buildroot "%buildroot" --out .files.list

%files -f %{package_path}/.files.list

%clean
rm -rf $RPM_BUILD_ROOT

%post
    /sbin/new-kernel-pkg --package kernel --mkinitrd --depmod --install %{version} --make-default || exit $?

%postun
    /sbin/new-kernel-pkg --rminitrd --rmmoddep --remove %{version} || exit $?

%changelog
* Mon Oct 20 2014 - morik@
- New version 3.13.1-3
- Global variable debug_level renamed in g_debug_level
- Removing compiler's warnings
