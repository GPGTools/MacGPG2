require 'formula'

class Libksba < Formula
  url 'ftp://ftp.gnupg.org/gcrypt/libksba/libksba-1.2.0.tar.bz2'
  homepage 'http://www.gnupg.org/related_software/libksba/index.en.html'
  sha1 '0c4e593464b9dec6f53c728c375d54a095658230'

  depends_on 'libgpg-error'
  
  keep_install_names true
  
  def install
    ENV.universal_binary if ARGV.build_universal?
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
