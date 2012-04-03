require 'formula'

class Zlib < Formula
  url 'http://zlib.net/zlib-1.2.6.tar.gz'
  homepage ''
  md5 '618e944d7c7cd6521551e30b32322f4a'
  
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
    
    ENV.prepend 'LDFLAGS', '-headerpad_max_install_names'
    ENV.prepend 'LDFLAGS', "-Wl,-rpath,@loader_path/../lib -Wl,-rpath,#{HOMEBREW_PREFIX}/lib"
    system "./configure", "--prefix=#{prefix}"
    system "make check"
    system "make install"
  end

end
