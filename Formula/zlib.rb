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
  
  def patches
    { :p0 => DATA }
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

__END__

diff --git configure configure
index bd9edd2..0b8ca4e 100755
--- configure
+++ configure
@@ -146,7 +146,7 @@ if test "$gcc" -eq 1 && ($cc -c $cflags $test.c) 2>/dev/null; then
              SHAREDLIB=libz$shared_ext
              SHAREDLIBV=libz.$VER$shared_ext
              SHAREDLIBM=libz.$VER1$shared_ext
-             LDSHARED=${LDSHARED-"$cc -dynamiclib -install_name $libdir/$SHAREDLIBM -compatibility_version $VER1 -current_version $VER3"} ;;
+             LDSHARED=${LDSHARED-"$cc -dynamiclib -install_name @rpath/$SHAREDLIBM -compatibility_version $VER1 -current_version $VER3"} ;;
   *)             LDSHARED=${LDSHARED-"$cc -shared"} ;;
   esac
 else
