require 'formula'

class Gettext < Formula
  url 'http://ftpmirror.gnu.org/gettext/gettext-0.18.3.1.tar.gz'
  mirror 'http://ftp.gnu.org/gnu/gettext/gettext-0.18.3.1.tar.gz'
  sha256 '0d8f9a33531b77776b3dc473e7940019ca19bfca5b4c06db6e96065eeb07245d'
  homepage 'http://www.gnu.org/software/gettext/'
  keep_install_names true
  # keg_only "OS X provides the BSD gettext library and some software gets confused if both are in the library path."

  depends_on 'libiconv'
  
  def options
  [
    ['--with-examples', 'Keep example files.'],
    ['--universal', 'Build universal binaries.']
  ]
  end

  def patches
    unless ARGV.include? '--with-examples'
      # Use a MacPorts patch to disable building examples at all,
      # rather than build them and remove them afterwards.
      {:p0 => [ "#{HOMEBREW_PREFIX}/Library/Formula/Patches/gettext/patch-gettext-tools-Makefile.in"]}
    end
  end

  def install
    ENV.libxml2
    ENV.O3 # Issues with LLVM & O4 on Mac Pro 10.6
    
    ENV.universal_binary if ARGV.build_universal?
    # Make sure that deployment target is 10.6+ so the lib works
    # on 10.6 and up not only on host system os x version.
    ENV.macosxsdk("10.6")
    # gettext checks for iconv using a test-script. that only executes
    # if the correct @rpath is set for the binary, this prepend the loader_path.
    ENV.prepend 'LDFLAGS', "-Wl,-rpath,@loader_path/../lib -Wl,-rpath,#{HOMEBREW_PREFIX}/lib"
    ENV.prepend 'LDFLAGS', '-headerpad_max_install_names'
    
    system "./configure", "--disable-dependency-tracking", "--disable-debug",
                          "--prefix=#{prefix}",
                          "--enable-static=no", "--disable-maintainer-mode",
                          "--enable-threads",
                          "--with-included-gettext",
                          "--with-included-glib",
                          "--with-included-libcroco",
                          "--with-included-libunistring",
                          "--with-included-libxml",
                          "--without-emacs",
                          "--disable-csharp",
                          "--disable-native-java",
                          "--disable-java",
                          # Don't use VCS systems to create these archives
                          "--without-git",
                          "--without-cvs",
                          "--with-libiconv-prefix=#{HOMEBREW_PREFIX}"
    
    # Disable HAVE_PTHREAD_MUTEX_RECURSIVE to force the use of glthread,
    # otherwise gettext will crash on 10.6
    inreplace "gettext-runtime/config.h", "HAVE_PTHREAD_MUTEX_RECURSIVE 1", "HAVE_PTHREAD_MUTEX_RECURSIVE 0"
    inreplace "gettext-tools/config.h", "HAVE_PTHREAD_MUTEX_RECURSIVE 1", "HAVE_PTHREAD_MUTEX_RECURSIVE 0"
    
    system "make"
    ENV.deparallelize # install doesn't support multiple make jobs
    system "make install"
  end
end
