require 'formula'

class Gettext < Formula
  url 'http://ftpmirror.gnu.org/gettext/gettext-0.18.1.1.tar.gz'
  mirror 'http://ftp.gnu.org/gnu/gettext/gettext-0.18.1.1.tar.gz'
  md5 '3dd55b952826d2b32f51308f2f91aa89'
  homepage 'http://www.gnu.org/software/gettext/'
  keep_install_names true
  # keg_only "OS X provides the BSD gettext library and some software gets confused if both are in the library path."

  def options
  [
    ['--with-examples', 'Keep example files.'],
    ['--universal', 'Build universal binaries.']
  ]
  end

  def patches
    # Patch to allow building with Xcode 4; safe for any compiler.
    p = {:p0 => ['https://trac.macports.org/export/79617/trunk/dports/devel/gettext/files/stpncpy.patch'] }

    unless ARGV.include? '--with-examples'
      # Use a MacPorts patch to disable building examples at all,
      # rather than build them and remove them afterwards.
      p[:p0] << 'https://trac.macports.org/export/79183/trunk/dports/devel/gettext/files/patch-gettext-tools-Makefile.in'
    end

    return p
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
                          "--without-included-gettext",
                          "--without-included-glib",
                          "--without-included-libcroco",
                          "--without-included-libxml",
                          "--without-emacs",
                          "--disable-csharp",
                          "--disable-native-java",
                          "--disable-java",
                          # Don't use VCS systems to create these archives
                          "--without-git",
                          "--without-cvs",
                          "--with-libiconv-prefix=#{HOMEBREW_PREFIX}"
    system "make"
    ENV.deparallelize # install doesn't support multiple make jobs
    system "make install"
  end
end
