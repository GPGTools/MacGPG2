require 'formula'

class Libassuan < Formula
  url 'ftp://ftp.gnupg.org/gcrypt/libassuan/libassuan-2.0.3.tar.bz2'
  homepage 'http://www.gnupg.org/related_software/libassuan/index.en.html'
  sha1 '2bf4eba3b588758e349976a7eb9e8a509960c3b5'

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
    ENV.prepend 'LDFLAGS', '-headerpad_max_install_names'
    ENV.prepend 'LDFLAGS', "-Wl,-rpath,@loader_path/../lib -Wl,-rpath,#{HOMEBREW_PREFIX}/lib"
    
    system "./configure", "--disable-dependency-tracking", "--prefix=#{prefix}",
           "--with-gpg-error-prefix=#{HOMEBREW_PREFIX}",
           "--enable-static=no", "--disable-maintainer-mode"
    system "make check"
    system "make install"
  end
end

__END__

