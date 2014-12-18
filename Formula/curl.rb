require "formula"

class Curl < Formula
  homepage "http://curl.haxx.se/"
  url "http://curl.haxx.se/download/curl-7.39.0.tar.bz2"
  mirror "ftp://ftp.sunet.se/pub/www/utilities/curl/curl-7.39.0.tar.bz2"
  sha256 "b222566e7087cd9701b301dd6634b360ae118cc1cbc7697e534dc451102ea4e0"

  depends_on "pkg-config"
  depends_on "gnutls"
  
  keep_install_names true
  
  def patches
    { :p1 => ["#{HOMEBREW_PREFIX}/Library/Formula/Patches/curl/support-gnutls-keychain-ca-lookup.patch"] }
  end
  
  def install
    ENV.macosxsdk("10.6")
    
    # It's necessary to add the -rpath to the LDFLAGS, otherwise
    # programs can't link to libraries using @rpath.
    ENV.prepend 'LDFLAGS', '-headerpad_max_install_names'
    ENV.prepend 'LDFLAGS', "-Wl,-rpath,@loader_path/../lib -Wl,-rpath,#{HOMEBREW_PREFIX}/lib"
    
    args = %W[
      --disable-debug
      --disable-dependency-tracking
      --prefix=#{prefix}
    ]

    args << "--without-libssh2"
    args << "--without-libidn"
    args << "--without-libmetalink"
    args << "--without-gssapi"
    args << "--without-librtmp"
    args << "--disable-ares"
    args << "--with-ca-bundle=/usr/local/MacGPG2/share/sks-keyservers.netCA.pem"
    args << "--with-gnutls=#{HOMEBREW_PREFIX}"
    args << "--without-ssl"
    
    # We have to run configure three times. Once for 32bit to create a working
    # curlbuild.h file, then for 64bit to create a working curlbuild.h file
    # and after that, for the universal build, patching the curlbuild.h file
    # to build correctly.
    
    ENV.m32
    system "./configure", *args
    # Copy the 32bit curlbuild.h file
    system "cp ./include/curl/curlbuild.h ./include/curl/curlbuild-32bit.h"
    system "make distclean"
    
    ENV.remove 'LDFLAGS', "-arch i386"
    ENV.remove_from_cflags "-m32"
    ENV.remove_from_cflags "-arch i386"
    ENV.m64
    system "./configure", *args
    # Copy the 32bit curlbuild.h file
    system "cp ./include/curl/curlbuild.h ./include/curl/curlbuild-64bit.h"
    system "make distclean"
    
    ENV.remove 'LDFLAGS', "-arch x86_64"
    ENV.remove_from_cflags "-m64"
    ENV.remove_from_cflags "-arch x86_64"
      
    ENV.universal_binary if ARGV.build_universal?
    system "./configure", *args
    # Fix the curlbuild.h file to work on both 32bit builds and 64bit builds
    pn = Pathname.new "include/curl/curlbuild-64bit.h"
    pn2 = Pathname.new "include/curl/curlbuild-32bit.h"
    pn_final = Pathname.new "include/curl/curlbuild.h"
    pn_final.delete()
    pn_final.write(<<eof
#if defined(__i386__)
#{pn2.read}
#elif defined(__x86_64__)
#{pn.read}
#endif      
eof
    )
    
    system "make", "install"
  end

  test do
    # Fetch the curl tarball and see that the checksum matches.
    # This requires a network connection, but so does Homebrew in general.
    filename = (testpath/"test.tar.gz")
    system "#{bin}/curl", stable.url, "-o", filename
    filename.verify_checksum stable.checksum
  end
end
