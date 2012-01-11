require 'formula'

class Libksba < Formula
  url 'ftp://ftp.gnupg.org/gcrypt/libksba/libksba-1.2.0.tar.bz2'
  homepage 'http://www.gnupg.org/related_software/libksba/index.en.html'
  sha1 '0c4e593464b9dec6f53c728c375d54a095658230'

  depends_on 'libgpg-error'
  
  keep_install_names true
  
  # def patches
  #     { :p0 => DATA }
  #   end
  
  def install
    ENV.universal_binary if ARGV.build_universal?
    ENV.prepend 'LDFLAGS', '-headerpad_max_install_names'
    ENV.prepend 'LDFLAGS', "-Wl,-rpath,@loader_path/../lib -Wl,-rpath,#{HOMEBREW_PREFIX}/lib"
    
    system "./configure", "--disable-dependency-tracking",
           "--with-gpg-error-prefix=#{HOMEBREW_PREFIX}", 
           "--enable-static=no", "--disable-maintainer-mode",
           "--prefix=#{prefix}"
    system "make check"
    system "make install"
  end
end

__END__

diff --git ltmain.sh ltmain.sh
index 160c173..0fa9c3c 100644
--- ltmain.sh
+++ ltmain.sh
@@ -4343,7 +4343,7 @@ func_mode_link ()
 	rpath | xrpath)
 	  # We need an absolute path.
 	  case $arg in
-	  [\\/]* | [A-Za-z]:[\\/]*) ;;
+	  [\\/]* | [A-Za-z]:[\\/]* | @rpath* | @loader_path*) ;;
 	  *)
 	    func_fatal_error "only absolute run-paths are allowed"
 	    ;;
diff --git src/Makefile.in src/Makefile.in
index 8c2b493..77718f8 100644
--- src/Makefile.in
+++ src/Makefile.in
@@ -396,7 +396,7 @@ clean-libLTLIBRARIES:
 	  rm -f "$${dir}/so_locations"; \
 	done
 libksba.la: $(libksba_la_OBJECTS) $(libksba_la_DEPENDENCIES) 
-	$(libksba_la_LINK) -rpath $(libdir) $(libksba_la_OBJECTS) $(libksba_la_LIBADD) $(LIBS)
+	$(libksba_la_LINK) -rpath @rpath $(libksba_la_OBJECTS) $(libksba_la_LIBADD) $(LIBS)
 
 clean-noinstPROGRAMS:
 	@list='$(noinst_PROGRAMS)'; for p in $$list; do \
