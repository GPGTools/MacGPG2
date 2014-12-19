require 'formula'

class Nettle < Formula
  homepage 'http://www.lysator.liu.se/~nisse/nettle/'
  url 'http://www.lysator.liu.se/~nisse/archive/nettle-2.7.1.tar.gz'
  sha1 'e7477df5f66e650c4c4738ec8e01c2efdb5d1211'

  resource "32-bit" do
    url "http://www.lysator.liu.se/~nisse/archive/nettle-2.7.1.tar.gz"
    sha1 "e7477df5f66e650c4c4738ec8e01c2efdb5d1211"
  end
  
  depends_on 'gmp'

  keep_install_names true

  def install
    # Make sure that deployment target is 10.6+ so the lib works
    # on 10.6 and up not only on host system os x version.
    ENV.macosxsdk("10.6")
    ENV.prepend 'LDFLAGS', '-headerpad_max_install_names'
    ENV.prepend 'LDFLAGS', "-Wl,-rpath,@loader_path/../lib -Wl,-rpath,#{HOMEBREW_PREFIX}/lib"
    
    args = ["--prefix=#{prefix}/32-bit", "--disable-dependency-tracking", "--enable-shared"]
    
    resource("32-bit").stage {
      build_variant("32bit", args)
      system "make check"
    }
    
    args[0] = "--prefix=#{prefix}"
    
    build_variant("64bit", args)
    system "make check"
    
    # Create the universal lib using lipo and combining the two.
    combine_archs "libnettle.4.7.dylib", "32-bit"
    combine_archs "libhogweed.2.5.dylib", "32-bit"
    
    system "rm -rf #{prefix}/32-bit"
  end
end
