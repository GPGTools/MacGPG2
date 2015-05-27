require 'formula'

class Pth < Formula
  url 'http://ftpmirror.gnu.org/pth/pth-2.0.7.tar.gz'
  mirror 'http://ftp.gnu.org/gnu/pth/pth-2.0.7.tar.gz'
  homepage 'http://www.gnu.org/software/pth/'
  sha1 '9a71915c89ff2414de69fe104ae1016d513afeee'
  
  keep_install_names true
  
  def patches
    { :p0 => DATA }
  end
  
  def install
    ENV.deparallelize
    ENV.universal_binary if ARGV.build_universal?
    # Make sure that deployment target is 10.6+ so the lib works
    # on 10.6 and up not only on host system os x version.
    ENV.macosxsdk("10.6")
    
    ENV.prepend 'LDFLAGS', "-Wl,-rpath,@loader_path/../lib -Wl,-rpath,#{HOMEBREW_PREFIX}/lib"
    ENV.prepend 'LDFLAGS', '-headerpad_max_install_names'
    
    # Note: shared library will not be build with --disable-debug, so don't add that flag
    system "./configure", "--disable-dependency-tracking",
                          "--with-mctx-mth=sjlj",
                          "--with-mctx-dsp=ssjlj",
                          "--with-mctx-stk=sas",
                          "--enable-static=no", "--disable-maintainer-mode",
                          "--prefix=#{prefix}",
                          "--mandir=#{man}"
    system "make test"
    system "make install"
  end
end

__END__
--- Makefile.in	2006-06-08 19:54:01.000000000 +0200
+++ Makefile.in	2011-12-22 20:26:32.000000000 +0100
@@ -168,10 +168,10 @@
 
 #   build the static and possibly shared libraries
 libpth.la: $(LOBJS)
-	$(LIBTOOL) --mode=link --quiet $(CC) -o libpth.la $(LOBJS) \
+	$(LIBTOOL) --mode=link --quiet $(CC) $(CFLAGS) -o libpth.la $(LOBJS) \
 	-rpath $(libdir) -version-info `$(SHTOOL) version -lc -dlibtool $(_VERSION_FILE)`
 libpthread.la: pthread.lo $(LOBJS)
-	$(LIBTOOL) --mode=link --quiet $(CC) -o libpthread.la pthread.lo $(LOBJS) \
+	$(LIBTOOL) --mode=link --quiet $(CC) $(CFLAGS) -o libpthread.la pthread.lo $(LOBJS) \
 	-rpath $(libdir) -version-info `$(SHTOOL) version -lc -dlibtool $(_VERSION_FILE)`
 
 #   build the manual pages
@@ -194,25 +194,25 @@
 
 #   build test program
 test_std: test_std.o test_common.o libpth.la
-	$(LIBTOOL) --mode=link --quiet $(CC) $(LDFLAGS) -o test_std test_std.o test_common.o libpth.la $(LIBS)
+	$(LIBTOOL) --mode=link --quiet $(CC) $(CFLAGS) $(LDFLAGS) -o test_std test_std.o test_common.o libpth.la $(LIBS)
 test_httpd: test_httpd.o test_common.o libpth.la
-	$(LIBTOOL) --mode=link --quiet $(CC) $(LDFLAGS) -o test_httpd test_httpd.o test_common.o libpth.la $(LIBS)
+	$(LIBTOOL) --mode=link --quiet $(CC) $(CFLAGS) $(LDFLAGS) -o test_httpd test_httpd.o test_common.o libpth.la $(LIBS)
 test_misc: test_misc.o test_common.o libpth.la
-	$(LIBTOOL) --mode=link --quiet $(CC) $(LDFLAGS) -o test_misc test_misc.o test_common.o libpth.la $(LIBS)
+	$(LIBTOOL) --mode=link --quiet $(CC) $(CFLAGS) $(LDFLAGS) -o test_misc test_misc.o test_common.o libpth.la $(LIBS)
 test_mp: test_mp.o test_common.o libpth.la
-	$(LIBTOOL) --mode=link --quiet $(CC) $(LDFLAGS) -o test_mp test_mp.o test_common.o libpth.la $(LIBS)
+	$(LIBTOOL) --mode=link --quiet $(CC) $(CFLAGS) $(LDFLAGS) -o test_mp test_mp.o test_common.o libpth.la $(LIBS)
 test_philo: test_philo.o test_common.o libpth.la
-	$(LIBTOOL) --mode=link --quiet $(CC) $(LDFLAGS) -o test_philo test_philo.o test_common.o libpth.la $(LIBS)
+	$(LIBTOOL) --mode=link --quiet $(CC) $(CFLAGS) $(LDFLAGS) -o test_philo test_philo.o test_common.o libpth.la $(LIBS)
 test_sig: test_sig.o test_common.o libpth.la
-	$(LIBTOOL) --mode=link --quiet $(CC) $(LDFLAGS) -o test_sig test_sig.o test_common.o libpth.la $(LIBS)
+	$(LIBTOOL) --mode=link --quiet $(CC) $(CFLAGS) $(LDFLAGS) -o test_sig test_sig.o test_common.o libpth.la $(LIBS)
 test_select: test_select.o test_common.o libpth.la
-	$(LIBTOOL) --mode=link --quiet $(CC) $(LDFLAGS) -o test_select test_select.o test_common.o libpth.la $(LIBS)
+	$(LIBTOOL) --mode=link --quiet $(CC) $(CFLAGS) $(LDFLAGS) -o test_select test_select.o test_common.o libpth.la $(LIBS)
 test_sfio: test_sfio.o test_common.o libpth.la
-	$(LIBTOOL) --mode=link --quiet $(CC) $(LDFLAGS) -o test_sfio test_sfio.o test_common.o libpth.la $(LIBS)
+	$(LIBTOOL) --mode=link --quiet $(CC) $(CFLAGS) $(LDFLAGS) -o test_sfio test_sfio.o test_common.o libpth.la $(LIBS)
 test_uctx: test_uctx.o test_common.o libpth.la
-	$(LIBTOOL) --mode=link --quiet $(CC) $(LDFLAGS) -o test_uctx test_uctx.o test_common.o libpth.la $(LIBS)
+	$(LIBTOOL) --mode=link --quiet $(CC) $(CFLAGS) $(LDFLAGS) -o test_uctx test_uctx.o test_common.o libpth.la $(LIBS)
 test_pthread: test_pthread.o test_common.o libpthread.la
-	$(LIBTOOL) --mode=link --quiet $(CC) $(LDFLAGS) -o test_pthread test_pthread.o test_common.o libpthread.la $(LIBS)
+	$(LIBTOOL) --mode=link --quiet $(CC) $(CFLAGS) $(LDFLAGS) -o test_pthread test_pthread.o test_common.o libpthread.la $(LIBS)
 
 #   install the package
 install: all-for-install
