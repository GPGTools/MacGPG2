require 'formula'

class Libiconv < Formula
  url 'http://ftpmirror.gnu.org/libiconv/libiconv-1.14.tar.gz'
  mirror 'http://ftp.gnu.org/gnu/libiconv/libiconv-1.14.tar.gz'
  homepage 'http://www.gnu.org/software/libiconv/'
  md5 'e34509b1623cec449dfeb73d7ce9c6c6'

  # keg_only :provided_by_osx, <<-EOS.undent
  #       A few software packages require this newer version of libiconv.
  #       Please use this dependency very sparingly.
  #     EOS
  
  keep_install_names true

  def patches
    { :p1 => [
      'http://svn.macports.org/repository/macports/trunk/dports/textproc/libiconv/files/patch-Makefile.devel',
      'http://svn.macports.org/repository/macports/trunk/dports/textproc/libiconv/files/patch-utf8mac.diff',
      DATA
    ]}
  end

  def options
    [[ '--universal', 'Build a universal library.' ]]
  end

  def install
    ENV.universal_binary if ARGV.build_universal?
    # Make sure that deployment target is 10.6+ so the lib works
    # on 10.6 and up not only on host system os x version.
    ENV.macosxsdk("10.6")
    
    ENV.j1
    ENV.prepend 'LDFLAGS', '-headerpad_max_install_names'
    ENV.prepend 'LDFLAGS', "-Wl,-rpath,@loader_path/../lib -Wl,-rpath,#{HOMEBREW_PREFIX}/lib"
    
    system "./configure", "--disable-debug", "--disable-dependency-tracking",
                          "--prefix=#{prefix}",
                          "--enable-static=no", "--disable-maintainer-mode",
                          "--enable-extra-encodings"
    
    system "make"
    system "make check"
    system "make install"
  end
end


__END__
diff --git a/lib/flags.h b/lib/flags.h
index d7cda21..4cabcac 100644
--- a/lib/flags.h
+++ b/lib/flags.h
@@ -14,6 +14,7 @@
 
 #define ei_ascii_oflags (0)
 #define ei_utf8_oflags (HAVE_ACCENTS | HAVE_QUOTATION_MARKS | HAVE_HANGUL_JAMO)
+#define ei_utf8mac_oflags (HAVE_ACCENTS | HAVE_QUOTATION_MARKS | HAVE_HANGUL_JAMO)
 #define ei_ucs2_oflags (HAVE_ACCENTS | HAVE_QUOTATION_MARKS | HAVE_HANGUL_JAMO)
 #define ei_ucs2be_oflags (HAVE_ACCENTS | HAVE_QUOTATION_MARKS | HAVE_HANGUL_JAMO)
 #define ei_ucs2le_oflags (HAVE_ACCENTS | HAVE_QUOTATION_MARKS | HAVE_HANGUL_JAMO)

