require 'formula'

class LibusbCompat < Formula
  url 'http://downloads.sourceforge.net/project/libusb/libusb-compat-0.1/libusb-compat-0.1.3/libusb-compat-0.1.3.tar.bz2'
  homepage 'http://www.libusb.org/'
  sha1 'd5710d5bc4b67c5344e779475b76168c7ccc5e69'
  
  depends_on 'pkg-config' => :build
  depends_on 'libusb'
  
  keep_install_names true
  
  def install
    ENV.universal_binary if ARGV.build_universal?
    # Make sure that deployment target is 10.6+ so the lib works
    # on 10.6 and up not only on host system os x version.
    ENV.macosxsdk("10.6")
    
    # Otherwise homebrew fails to find libusb.
    dep = Formula.factory 'libusb'
    ENV.prepend 'PKG_CONFIG_PATH', dep.lib+'pkgconfig', ':'
    ENV.prepend 'LDFLAGS', '-headerpad_max_install_names'
    
    system "./configure", "--prefix=#{prefix}", 
           "--enable-static=no", "--disable-dependency-tracking"
    system "make check"
    system "make install"
  end
end

