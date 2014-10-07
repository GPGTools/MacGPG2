require 'formula'

class LibgpgError < Formula
  url 'ftp://ftp.gnupg.org/gcrypt/libgpg-error/libgpg-error-1.16.tar.bz2'
  homepage 'http://www.gnupg.org/'
  sha1 '059c40a2b78c3ac2b4cbec0e0481faba5af332fe'
  
  keep_install_names true
  
  def install
    ENV.j1
    ENV.universal_binary if ARGV.build_universal? # build fat so wine can use it
    # Make sure that deployment target is 10.6+ so the lib works
    # on 10.6 and up not only on host system os x version.
    ENV.macosxsdk("10.6")
    # It's necessary to add the -rpath to the LDFLAGS, otherwise
    # programs can't link to libraries using @rpath.
    ENV.prepend 'LDFLAGS', '-headerpad_max_install_names'
    ENV.prepend 'LDFLAGS', "-Wl,-rpath,@loader_path/../lib -Wl,-rpath,#{HOMEBREW_PREFIX}/lib"

    system "./configure", "--disable-dependency-tracking",
                          "--prefix=#{prefix}",
                          "--enable-static=no", "--disable-maintainer-mode"
    
    system "make check"
    system "make install"
  end
end
