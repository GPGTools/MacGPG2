# This is a template.  The dist target uses it to create the real file.
Summary: libgpg-error
Name: libgpg-error
Version: 1.9
Release: 1
URL: ftp://ftp.gnupg.org/gcrypt/alpha/libgpg-error/
Source: ftp://ftp.gnupg.org/gcrypt/alpha/libgpg-error/%{name}-%{version}.tar.gz
Group: Development/Libraries
Copyright: LGPL
BuildRoot: %{_tmppath}/%{name}-%{version}
BuildRequires: make
Prereq: /sbin/ldconfig

%description
This is a library that defines common error values for all GnuPG
components.  Among these are GPG, GPGSM, GPGME, GPG-Agent, libgcrypt,
pinentry, SmartCard Daemon and possibly more in the future.

%prep
%setup -q

%build
CFLAGS="$RPM_OPT_FLAGS"; export CFLAGS
./configure --prefix=/usr
make

%install
rm -fr $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT
make install prefix=$RPM_BUILD_ROOT/usr

%clean
rm -fr $RPM_BUILD_ROOT
make distclean

%post
/sbin/ldconfig

%postun
/sbin/ldconfig

%files
%defattr(-,root,root)
%doc COPYING COPYING.LIB AUTHORS README INSTALL NEWS ChangeLog
%attr(0755,root,root) %{_bindir}/gpg-error-config
%attr(0755,root,root) %{_bindir}/gpg-error
%attr(0755,root,root) %{_libdir}/*gpg-error.so*
%attr(0755,root,root) %{_libdir}/*gpg-error.la
%attr(0644,root,root) %{_libdir}/*gpg-error.a
%{_includedir}/gpg-error.h
%{_datadir}/aclocal/gpg-error.m4

%changelog
* Wed Sep  3 2003 Robert Schiele <rschiele@uni-mannheim.de>
- initial specfile.

# EOF
