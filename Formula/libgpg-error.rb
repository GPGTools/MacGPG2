require 'formula'

class LibgpgError < Formula
  url 'ftp://ftp.gnupg.org/gcrypt/libgpg-error/libgpg-error-1.10.tar.bz2'
  homepage 'http://www.gnupg.org/'
  sha1 '95b324359627fbcb762487ab6091afbe59823b29'
  
  keep_install_names true
  
  def patches
    { :p0 => DATA }
  end
  
  def install
    ENV.j1
    ENV.universal_binary if ARGV.build_universal? # build fat so wine can use it
    # It's necessary to add the -rpath to the LDFLAGS, otherwise
    # programs can't link to libraries using @rpath.
    ENV.prepend 'LDFLAGS', '-headerpad_max_install_names'
    ENV.prepend 'LDFLAGS', "-Wl,-rpath,@loader_path/../lib -Wl,-rpath,#{HOMEBREW_PREFIX}/lib"
    
    system "./configure", "--disable-dependency-tracking",
                          "--prefix=#{prefix}",
                          "--enable-static=no", "--disable-maintainer-mode"
    
    system "make check"
    system "make install"
  end
end

__END__
diff -ur src/err-codes.h src/err-codes.h
--- src/err-codes.h     2010-09-30 17:32:38.000000000 +0200
+++ src/err-codes.h     2011-12-24 12:33:57.000000000 +0100
@@ -522,7 +522,7 @@
     4746
   };
 
-static inline int
+static int
 msgidxof (int code)
 {
   return (0 ? 0
diff -ur src/err-sources.h src/err-sources.h
--- src/err-sources.h   2010-09-16 15:16:33.000000000 +0200
+++ src/err-sources.h   2011-12-24 12:34:08.000000000 +0100
@@ -72,7 +72,7 @@
     209
   };
 
-static inline int
+static int
 msgidxof (int code)
 {
   return (0 ? 0
diff -ur src/mkstrtable.awk src/mkstrtable.awk
--- src/mkstrtable.awk  2010-01-21 12:09:02.000000000 +0100
+++ src/mkstrtable.awk  2011-12-24 12:34:24.000000000 +0100
@@ -157,7 +157,7 @@
   print "    " pos[coded_msgs];
   print "  };";
   print "";
-  print "static inline int";
+  print "static int";
   print namespace "msgidxof (int code)";
   print "{";
   print "  return (0 ? 0";
