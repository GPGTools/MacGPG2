require 'formula'

class Libgcrypt < Formula
  url 'ftp://ftp.gnupg.org/gcrypt/libgcrypt/libgcrypt-1.5.3.tar.bz2'
  sha1 '2c6553cc17f2a1616d512d6870fe95edf6b0e26e'
  homepage 'http://directory.fsf.org/project/libgcrypt/'

  depends_on 'pth'
  depends_on 'libgpg-error'
  
  keep_install_names true
  
  def patches
    if ENV.compiler == :clang
      { :p0 => ["https://trac.macports.org/export/85232/trunk/dports/devel/libgcrypt/files/clang-asm.patch"]}
    else
      { :p0 => []}
    end
  end

  def install
    ENV.universal_binary if ARGV.build_universal?
    ENV.build_32_bit
    # Make sure that deployment target is 10.6+ so the lib works
    # on 10.6 and up not only on host system os x version.
    ENV.macosxsdk("10.6")
    
    ENV.prepend 'LDFLAGS', '-headerpad_max_install_names'
    ENV.prepend 'LDFLAGS', "-Wl,-rpath,@loader_path/../lib -Wl,-rpath,#{HOMEBREW_PREFIX}/lib"
    ENV.prepend 'CFLAGS', "-fheinous-gnu-extensions"
    
    system "./configure", "--disable-dependency-tracking",
                          "--prefix=#{prefix}",
                          "--disable-asm",
                          "--enable-m-guard",
                          "--with-gpg-error-prefix=#{HOMEBREW_PREFIX}",
                          "--with-pth-prefix=#{HOMEBREW_PREFIX}",
                          "--enable-static=no", "--disable-maintainer-mode"
    # Separate steps, or parallel builds fail
    system "make"
    system "make check"
    system "make install"
  end
end

