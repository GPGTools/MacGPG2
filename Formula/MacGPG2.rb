require 'formula'

class Macgpg2 < Formula
  url 'ftp://ftp.gnupg.org/gcrypt/gnupg/gnupg-2.0.18.tar.bz2'
  homepage 'http://www.gnupg.org/'
  sha1 '5ec2f718760cc3121970a140aeea004b64545c46'
  
  depends_on 'libiconv'
  depends_on 'gettext'
  depends_on 'pth'
  depends_on 'libusb-compat'
  depends_on 'libgpg-error'
  depends_on 'libassuan'
  depends_on 'libgcrypt'
  depends_on 'libksba'
  depends_on 'zlib'
  
  keep_install_names true
  
  def patches
    { :p1 => [DATA],
      :p0 => ["#{HOMEBREW_PREFIX}/Library/Formula/Patches/gnupg2/IDEA.patch",
              "#{HOMEBREW_PREFIX}/Library/Formula/Patches/gnupg2/cacheid.patch",
              "#{HOMEBREW_PREFIX}/Library/Formula/Patches/gnupg2/keysize.patch",
              "#{HOMEBREW_PREFIX}/Library/Formula/Patches/gnupg2/launchd.patch",
              "#{HOMEBREW_PREFIX}/Library/Formula/Patches/gnupg2/MacGPG2VersionString.patch",
              "#{HOMEBREW_PREFIX}/Library/Formula/Patches/gnupg2/options.skel.patch"] }
  end

  def install
    (var+'run').mkpath
    ENV.build_32_bit
    
    # so we don't use Clang's internal stdint.h
    ENV['gl_cv_absolute_stdint_h'] = '/usr/include/stdint.h'
    # It's necessary to add the -rpath to the LDFLAGS, otherwise
    # programs can't link to libraries using @rpath.
    ENV.prepend 'LDFLAGS', '-headerpad_max_install_names'
    ENV.prepend 'LDFLAGS', "-Wl,-rpath,@loader_path/../lib -Wl,-rpath,#{HOMEBREW_PREFIX}/lib"
    system "./configure", "--prefix=#{prefix}",
                          "--disable-maintainer-mode",
                          "--disable-dependency-tracking",
                          "--disable-gpgtar",
                          "--enable-standard-socket",
                          "--with-pinentry-pgm=#{HOMEBREW_PREFIX}/libexec/pinentry-mac.app/Contents/MacOS/pinentry-mac",
                          "--with-gpg-error-prefix=#{HOMEBREW_PREFIX}",
                          "--with-libgcrypt-prefix=#{HOMEBREW_PREFIX}",
                          "--with-libassuan-prefix=#{HOMEBREW_PREFIX}",
                          "--with-ksba-prefix=#{HOMEBREW_PREFIX}",
                          "--with-pth-prefix=#{HOMEBREW_PREFIX}",
                          "--with-zlib=#{HOMEBREW_PREFIX}",
                          "--with-libiconv-prefix=#{HOMEBREW_PREFIX}",
                          "--with-libintl-prefix=#{HOMEBREW_PREFIX}"
    
    system "make"
    system "make check"
    system "make install"

    # conflicts with a manpage from the 1.x formula, and
    # gpg-zip isn't installed by this formula anyway
    #rm man1+'gpg-zip.1'
  end
end

__END__
# fix configure's failure to detect libcurl
# http://git.gnupg.org/cgi-bin/gitweb.cgi?p=gnupg.git;a=commit;h=57ef0d6
diff --git a/configure b/configure
index 3df3900..35c474f 100755
--- a/configure
+++ b/configure
@@ -9384,7 +9384,7 @@ else
 
            cat confdefs.h - <<_ACEOF >conftest.$ac_ext
 /* end confdefs.h.  */
-include <curl/curl.h>
+#include <curl/curl.h>
 int
 main ()
 {

# fix runtime data location
# http://git.gnupg.org/cgi-bin/gitweb.cgi?p=gnupg.git;a=commitdiff;h=c3f08dc
diff --git a/common/homedir.c b/common/homedir.c
index 5f2e31e..d797b68 100644
--- a/common/homedir.c
+++ b/common/homedir.c
@@ -365,7 +365,7 @@ dirmngr_socket_name (void)
     }
   return name;
 #else /*!HAVE_W32_SYSTEM*/
-  return "/var/run/dirmngr/socket";
+  return "HOMEBREW_PREFIX/var/run/dirmngr/socket";
 #endif /*!HAVE_W32_SYSTEM*/
 }
 

# rename package to avoid conflicts with our gnupg 1.x formula
diff --git a/configure b/configure
index 3df3900..b102aec 100755
--- a/configure
+++ b/configure
@@ -558,8 +558,8 @@ MFLAGS=
 MAKEFLAGS=
 
 # Identity of this package.
-PACKAGE_NAME='gnupg'
-PACKAGE_TARNAME='gnupg'
+PACKAGE_NAME='gnupg2'
+PACKAGE_TARNAME='gnupg2'
 PACKAGE_VERSION='2.0.18'
 PACKAGE_STRING='gnupg 2.0.18'
 PACKAGE_BUGREPORT='http://bugs.gnupg.org'

