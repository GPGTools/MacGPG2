require 'formula'

class Gmp < Formula
  homepage 'http://gmplib.org/'
  url 'http://ftpmirror.gnu.org/gmp/gmp-6.0.0a.tar.bz2'
  mirror 'ftp://ftp.gmplib.org/pub/gmp/gmp-6.0.0a.tar.bz2'
  mirror 'http://ftp.gnu.org/gnu/gmp/gmp-6.0.0a.tar.bz2'
  sha1 '360802e3541a3da08ab4b55268c80f799939fddc'
  
  resource "32-bit" do
    url "http://ftpmirror.gnu.org/gmp/gmp-6.0.0a.tar.bz2"
    sha1 "360802e3541a3da08ab4b55268c80f799939fddc"
  end
  
  keep_install_names true
  
  def install
    # Make sure that deployment target is 10.6+ so the lib works
    # on 10.6 and up not only on host system os x version.
    ENV.macosxsdk("10.6")
    ENV.prepend 'LDFLAGS', '-headerpad_max_install_names'
    ENV.prepend 'LDFLAGS', "-Wl,-rpath,@loader_path/../lib -Wl,-rpath,#{HOMEBREW_PREFIX}/lib"
    
    args = ["--prefix=#{prefix}/32-bit"]
    
    resource("32-bit").stage {
      ENV.prepend 'LDFLAGS', "-Wl,-rpath,#{prefix}/lib"
      build_variant("32bit", args)
      system "make check"
    }
    
    args[0] = "--prefix=#{prefix}"
    build_variant("64bit", args) {
      time = Time.now.to_i
      patchfile = "/tmp/gpgtools-#{time}-gmp.h.patch"
      pn = Pathname.new patchfile
      pn.write(
  <<-eos
  --- gmp-6.0.0/gmp.h.orig	2014-12-12 00:14:49.000000000 +0100
  +++ gmp-6.0.0/gmp.h	2014-12-12 00:40:49.000000000 +0100
  @@ -40,9 +40,14 @@
   #if ! defined (__GMP_WITHIN_CONFIGURE)
   #define __GMP_HAVE_HOST_CPU_FAMILY_power   0
   #define __GMP_HAVE_HOST_CPU_FAMILY_powerpc 0
  +#if defined(__i386__)
  +#define GMP_LIMB_BITS                      32
  +#define GMP_NAIL_BITS                      0
  +#elif defined(__x86_64__)
   #define GMP_LIMB_BITS                      64
   #define GMP_NAIL_BITS                      0
   #endif
  +#endif
   #define GMP_NUMB_BITS     (GMP_LIMB_BITS - GMP_NAIL_BITS)
   #define GMP_NUMB_MASK     ((~ __GMP_CAST (mp_limb_t, 0)) >> GMP_NAIL_BITS)
   #define GMP_NUMB_MAX      GMP_NUMB_MASK
  eos
      )
    
      safe_system "/usr/bin/patch", "-f", "-p1", "-i", "#{patchfile}"
      system "rm #{patchfile}"
    }
    system "make check"
    
    combine_archs "libgmp.10.dylib", "32-bit"
  end
end
