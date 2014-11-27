require 'formula'

class Libksba < Formula
  url 'ftp://ftp.gnupg.org/gcrypt/libksba/libksba-1.3.2.tar.bz2'
  homepage 'http://www.gnupg.org/related_software/libksba/index.en.html'
  sha1 '37d0893a587354af2b6e49f6ae701ca84f52da67'

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
