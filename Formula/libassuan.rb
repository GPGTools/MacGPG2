require 'formula'

class Libassuan < Formula
  homepage 'http://www.gnupg.org/related_software/libassuan/index.en.html'
  url 'ftp://ftp.gnupg.org/gcrypt/libassuan/libassuan-2.4.2.tar.bz2'
  sha1 'ac1047f9764fd4a4db7dafe47640643164394db9'

  depends_on 'libgpg-error'
  
  keep_install_names true
  
  # def patches
  #     # Patch to allow building with Xcode 4; safe for any compiler.
  #     p = {:p1 => DATA}
  # 
  #     return p
  #   end
  
  def install
    ENV.universal_binary if ARGV.build_universal?
    # Make sure that deployment target is 10.6+ so the lib works
    # on 10.6 and up not only on host system os x version.
    ENV.macosxsdk("10.6")
    ENV.prepend 'LDFLAGS', '-headerpad_max_install_names'
    ENV.prepend 'LDFLAGS', "-Wl,-rpath,@loader_path/../lib -Wl,-rpath,#{HOMEBREW_PREFIX}/lib"
    
    system "./configure", "--disable-dependency-tracking", "--prefix=#{prefix}",
           "--with-gpg-error-prefix=#{HOMEBREW_PREFIX}",
           "--enable-static=no", "--disable-maintainer-mode"
    system "make check"
    system "make install"
  end
end

