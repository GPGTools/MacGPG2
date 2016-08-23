require 'formula'

class Libksba < Formula
  url 'ftp://ftp.gnupg.org/gcrypt/libksba/libksba-1.3.4.tar.bz2'
  homepage 'http://www.gnupg.org/related_software/libksba/index.en.html'
  sha256 'f6c2883cebec5608692d8730843d87f237c0964d923bbe7aa89c05f20558ad4f'

  depends_on 'libgpg-error'
  
  keep_install_names true
  
  def install
    ENV.universal_binary if ARGV.build_universal?
    # Make sure that deployment target is 10.6+ so the lib works
    # on 10.6 and up not only on host system os x version.
    ENV.macosxsdk("10.6")
    
    ENV.prepend 'LDFLAGS', '-headerpad_max_install_names'
    ENV.prepend 'LDFLAGS', "-Wl,-rpath,@loader_path/../lib -Wl,-rpath,#{HOMEBREW_PREFIX}/lib"
    
    system "./configure", "--disable-dependency-tracking",
           "--with-gpg-error-prefix=#{HOMEBREW_PREFIX}", 
           "--enable-static=no", "--disable-maintainer-mode",
           "--prefix=#{prefix}"
    system "make check"
    system "make install"
  end
end
