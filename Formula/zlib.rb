require 'formula'

class Zlib < Formula
  url 'http://zlib.net/zlib-1.2.7.tar.gz'
  homepage ''
  sha1 '4aa358a95d1e5774603e6fa149c926a80df43559'
  
  # keg_only :provided_by_osx
  # depends_on 'cmake' => :build
  
  keep_install_names true
  
  def options
    [
      ['--universal', 'Build universal binaries.']
    ]
  end
  
  def install
    ENV.universal_binary if ARGV.build_universal?
    # Make sure that deployment target is 10.6+ so the lib works
    # on 10.6 and up not only on host system os x version.
    ENV.macosxsdk("10.6")
    
    ENV.prepend 'LDFLAGS', '-headerpad_max_install_names'
    ENV.prepend 'LDFLAGS', "-Wl,-rpath,@loader_path/../lib -Wl,-rpath,#{HOMEBREW_PREFIX}/lib"
    system "./configure", "--prefix=#{prefix}"
    system "make check"
    system "make install"
  end

end
