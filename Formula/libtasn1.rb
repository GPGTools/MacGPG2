require "formula"

class Libtasn1 < Formula
  homepage "https://www.gnu.org/software/libtasn1/"
  url "http://ftpmirror.gnu.org/libtasn1/libtasn1-4.2.tar.gz"
  mirror "https://ftp.gnu.org/gnu/libtasn1/libtasn1-4.2.tar.gz"
  sha1 "d2fe4bf12dbdc4d6765a04abbf8ddaf7e9163afa"

  option :universal
  
  keep_install_names true

  def install
    ENV.universal_binary if build.universal?
    # Make sure that deployment target is 10.6+ so the lib works
    # on 10.6 and up not only on host system os x version.
    ENV.macosxsdk("10.6")
    ENV.prepend 'LDFLAGS', '-headerpad_max_install_names'
    ENV.prepend 'LDFLAGS', "-Wl,-rpath,@loader_path/../lib -Wl,-rpath,#{HOMEBREW_PREFIX}/lib"
    
    system "./configure", "--prefix=#{prefix}", "--disable-dependency-tracking"
    system "make install"
  end
end
