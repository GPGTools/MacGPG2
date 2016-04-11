require 'formula'

class Libgcrypt < Formula
  homepage 'http://gnupg.org/'
  url 'ftp://ftp.gnupg.org/gcrypt/libgcrypt/libgcrypt-1.6.5.tar.bz2'
  sha1 'c3a5a13e717f7b3e3895650afc1b6e0d3fe9c726'

  depends_on 'libgpg-error'
  
  keep_install_names true

  option :universal

  resource 'config.h.ed' do
    url 'http://trac.macports.org/export/113198/trunk/dports/devel/libgcrypt/files/config.h.ed'
    version '113198'
    sha1 '136f636673b5c9d040f8a55f59b430b0f1c97d7a'
  end if build.universal?

  fails_with :clang do
    build 77
    cause "basic test fails"
  end

  # NOTE: the patch for building with clang and "universal" doesn't
  # apply anymore

  def install
    ENV.universal_binary if build.universal?
    #ENV.build_32_bit
    ENV.macosxsdk("10.6")
    
    ENV.append 'CFLAGS', '-std=gnu89 -fheinous-gnu-extensions' if ENV.compiler == :clang

    system "./configure", "--disable-dependency-tracking",
                          "--prefix=#{prefix}",
                          "--disable-asm",
                          "--with-gpg-error-prefix=#{HOMEBREW_PREFIX}"

    if build.universal?
      buildpath.install resource('config.h.ed')
      system "ed -s - config.h <config.h.ed"
    end

    # Parallel builds work, but only when run as separate steps
    system "make", "CFLAGS=#{ENV.cflags}"
    #system "make check"
    system "make install"
  end
end