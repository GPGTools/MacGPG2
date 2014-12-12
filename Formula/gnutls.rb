require 'formula'

class Gnutls < Formula
  homepage 'http://gnutls.org'
  url 'ftp://ftp.gnutls.org/gcrypt/gnutls/v3.3/gnutls-3.3.11.tar.xz'
  sha1 '82db10dc9b10d03cacbb86b567ef692401f34add'

  depends_on 'pkg-config'
  depends_on 'libtasn1'
  depends_on 'nettle'

  fails_with :llvm do
    build 2326
    cause "Undefined symbols when linking"
  end

  def patches
    { :p1 => ["#{HOMEBREW_PREFIX}/Library/Formula/Patches/gnutls/gnutls-pkgconfig-osx.patch",
              "#{HOMEBREW_PREFIX}/Library/Formula/Patches/gnutls/mac-keychain-lookup.patch",
              "#{HOMEBREW_PREFIX}/Library/Formula/Patches/gnutls/read-file-limits.h.patch"] }
  end

  def install
    # Make sure that deployment target is 10.6+ so the lib works
    # on 10.6 and up not only on host system os x version.
    ENV.macosxsdk("10.6")
    ENV.prepend 'LDFLAGS', '-headerpad_max_install_names'
    ENV.prepend 'LDFLAGS', "-Wl,-rpath,@loader_path/../lib -Wl,-rpath,#{HOMEBREW_PREFIX}/lib"
    ENV.universal_binary if ARGV.build_universal?
    
    args = %W[
      --disable-dependency-tracking
      --disable-static
      --disable-gtk-doc
      --without-p11-kit
      --disable-cxx
      --disable-openssl-compatibility
      --disable-guile
      --disable-nls
      --without-libintl-prefix
      --prefix=#{prefix}
    ]
    
    # This instructions is incredibly important, otherwise gnutls will not build for 32bit.
    args << "--disable-hardware-acceleration"

    system "./configure", *args
    system "make install"

    # certtool shadows the OS X certtool utility
    mv bin+'certtool', bin+'gnutls-certtool'
    mv man1+'certtool.1', man1+'gnutls-certtool.1'
  end
end
