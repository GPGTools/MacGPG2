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

__END__

# diff --git a/build-aux/ltmain.sh b/build-aux/ltmain.sh
# index 0dd6f3a..c1f985c 100644
# --- a/build-aux/ltmain.sh
# +++ b/build-aux/ltmain.sh
# @@ -4479,7 +4479,7 @@ func_mode_link ()
#   rpath | xrpath)
#     # We need an absolute path.
#     case $arg in
# -   [\\/]* | [A-Za-z]:[\\/]*) ;;
# +   [\\/]* | [A-Za-z]:[\\/]* | @rpath* | @loader_path*) ;;
#     *)
#       func_fatal_error "only absolute run-paths are allowed"
#       ;;
# diff --git a/gettext-runtime/intl/Makefile.in b/gettext-runtime/intl/Makefile.in
# index 2e53ccc..e05e03e 100644
# --- a/gettext-runtime/intl/Makefile.in
# +++ b/gettext-runtime/intl/Makefile.in
# @@ -223,7 +223,7 @@ libintl.la libgnuintl.la: $(OBJECTS) $(OBJECTS_RES_@WOE32@)
#     $(OBJECTS) @LTLIBICONV@ @INTL_MACOSX_LIBS@ $(LIBS) @LTLIBTHREAD@ @LTLIBC@ \
#     $(OBJECTS_RES_@WOE32@) \
#     -version-info $(LTV_CURRENT):$(LTV_REVISION):$(LTV_AGE) \
# -   -rpath $(libdir) \
# +   -rpath @rpath \
#     -no-undefined
#  
#  # Libtool's library version information for libintl.
# diff --git a/gettext-runtime/libasprintf/Makefile.in b/gettext-runtime/libasprintf/Makefile.in
# index 62a90a6..2df5046 100644
# --- a/gettext-runtime/libasprintf/Makefile.in
# +++ b/gettext-runtime/libasprintf/Makefile.in
# @@ -1140,8 +1140,8 @@ dist-hook:
#   rm -f $(distdir)/autosprintf.h
#  lib-asprintf.lo: $(lib_asprintf_EXTRASOURCES)
#  libasprintf.la: $(libasprintf_la_OBJECTS) $(libasprintf_la_DEPENDENCIES)
# - $(AM_V_GEN)$(CXXLINK) -rpath $(libdir) $(libasprintf_la_LDFLAGS) $(libasprintf_la_OBJECTS) $(libasprintf_la_LIBADD) $(LIBS) || \
# - $(LINK) -rpath $(libdir) $(libasprintf_la_LDFLAGS) $(libasprintf_la_OBJECTS) $(libasprintf_la_LIBADD) $(LIBS)
# + $(AM_V_GEN)$(CXXLINK) -rpath @rpath $(libasprintf_la_LDFLAGS) $(libasprintf_la_OBJECTS) $(libasprintf_la_LIBADD) $(LIBS) || \
# + $(LINK) -rpath @rpath $(libasprintf_la_LDFLAGS) $(libasprintf_la_OBJECTS) $(libasprintf_la_LIBADD) $(LIBS)
#  
#  # We need the following in order to create <alloca.h> when the system
#  # doesn't have one that works with the given compiler.
# diff --git a/gettext-runtime/src/Makefile.in b/gettext-runtime/src/Makefile.in
# index 3563601..1537d82 100644
# --- a/gettext-runtime/src/Makefile.in
# +++ b/gettext-runtime/src/Makefile.in
# @@ -154,21 +154,21 @@ am__v_lt_ = $(am__v_lt_$(AM_DEFAULT_VERBOSITY))
#  am__v_lt_0 = --silent
#  envsubst_LINK = $(LIBTOOL) $(AM_V_lt) --tag=CC $(AM_LIBTOOLFLAGS) \
#   $(LIBTOOLFLAGS) --mode=link $(CCLD) $(envsubst_CFLAGS) \
# - $(CFLAGS) $(envsubst_LDFLAGS) $(LDFLAGS) -o $@
# + $(CFLAGS) -Wl,-rpath,@loader_path/../lib $(envsubst_LDFLAGS) $(LDFLAGS) -o $@
#  am_gettext_OBJECTS = gettext-gettext.$(OBJEXT)
#  gettext_OBJECTS = $(am_gettext_OBJECTS)
#  gettext_LDADD = $(LDADD)
#  gettext_DEPENDENCIES = ../gnulib-lib/libgrt.a $(am__DEPENDENCIES_1)
#  gettext_LINK = $(LIBTOOL) $(AM_V_lt) --tag=CC $(AM_LIBTOOLFLAGS) \
#   $(LIBTOOLFLAGS) --mode=link $(CCLD) $(gettext_CFLAGS) \
# - $(CFLAGS) $(gettext_LDFLAGS) $(LDFLAGS) -o $@
# + $(CFLAGS) -Wl,-rpath,@loader_path/../lib $(gettext_LDFLAGS) $(LDFLAGS) -o $@
#  am_ngettext_OBJECTS = ngettext-ngettext.$(OBJEXT)
#  ngettext_OBJECTS = $(am_ngettext_OBJECTS)
#  ngettext_LDADD = $(LDADD)
#  ngettext_DEPENDENCIES = ../gnulib-lib/libgrt.a $(am__DEPENDENCIES_1)
#  ngettext_LINK = $(LIBTOOL) $(AM_V_lt) --tag=CC $(AM_LIBTOOLFLAGS) \
#   $(LIBTOOLFLAGS) --mode=link $(CCLD) $(ngettext_CFLAGS) \
# - $(CFLAGS) $(ngettext_LDFLAGS) $(LDFLAGS) -o $@
# + $(CFLAGS) -Wl,-rpath,@loader_path/../lib $(ngettext_LDFLAGS) $(LDFLAGS) -o $@
#  am__vpath_adj_setup = srcdirstrip=`echo "$(srcdir)" | sed 's|.|.|g'`;
#  am__vpath_adj = case $$p in \
#      $(srcdir)/*) f=`echo "$$p" | sed "s|^$$srcdirstrip/||"`;; \
# @@ -209,7 +209,7 @@ am__v_at_0 = @
#  CCLD = $(CC)
#  LINK = $(LIBTOOL) $(AM_V_lt) --tag=CC $(AM_LIBTOOLFLAGS) \
#   $(LIBTOOLFLAGS) --mode=link $(CCLD) $(AM_CFLAGS) $(CFLAGS) \
# - $(AM_LDFLAGS) $(LDFLAGS) -o $@
# + -Wl,-rpath,@loader_path/../lib $(AM_LDFLAGS) $(LDFLAGS) -o $@
#  AM_V_CCLD = $(am__v_CCLD_$(V))
#  am__v_CCLD_ = $(am__v_CCLD_$(AM_DEFAULT_VERBOSITY))
#  am__v_CCLD_0 = @echo "  CCLD  " $@;
# diff --git a/gettext-tools/gnulib-lib/Makefile.in b/gettext-tools/gnulib-lib/Makefile.in
# index f98d285..a530cbf 100644
# --- a/gettext-tools/gnulib-lib/Makefile.in
# +++ b/gettext-tools/gnulib-lib/Makefile.in
# @@ -2154,7 +2154,7 @@ libxml/xmlwriter.lo: libxml/$(am__dirstamp)
#  libxml/xpath.lo: libxml/$(am__dirstamp)
#  libxml/xpointer.lo: libxml/$(am__dirstamp)
#  libgettextlib.la: $(libgettextlib_la_OBJECTS) $(libgettextlib_la_DEPENDENCIES) 
# - $(AM_V_GEN)$(libgettextlib_la_LINK) -rpath $(libdir) $(libgettextlib_la_OBJECTS) $(libgettextlib_la_LIBADD) $(LIBS)
# + $(AM_V_GEN)$(libgettextlib_la_LINK) -rpath @rpath $(libgettextlib_la_OBJECTS) $(libgettextlib_la_LIBADD) $(LIBS)
#  glib/libglib_rpl_la-ghash.lo: glib/$(am__dirstamp)
#  glib/libglib_rpl_la-glist.lo: glib/$(am__dirstamp)
#  glib/libglib_rpl_la-gmessages.lo: glib/$(am__dirstamp)
# diff --git a/gettext-tools/libgettextpo/Makefile.in b/gettext-tools/libgettextpo/Makefile.in
# index 7488d45..3ed6a07 100644
# --- a/gettext-tools/libgettextpo/Makefile.in
# +++ b/gettext-tools/libgettextpo/Makefile.in
# @@ -1490,7 +1490,7 @@ LTV_AGE = 5
#  # define an uncontrolled amount of symbols.
#  libgettextpo_la_LIBADD = libgnu.la $(WOE32_LIBADD) $(LTLIBUNISTRING)
#  libgettextpo_la_LDFLAGS = -version-info \
# - $(LTV_CURRENT):$(LTV_REVISION):$(LTV_AGE) -rpath $(libdir) \
# + $(LTV_CURRENT):$(LTV_REVISION):$(LTV_AGE) -rpath @rpath \
#   @LTLIBINTL@ @LTLIBICONV@ -lc @LTNOUNDEF@ $(am__append_2)
#  @WOE32_FALSE@WOE32_LIBADD = 
#  @WOE32_TRUE@WOE32_LIBADD = libgettextpo.res.lo
# @@ -1607,7 +1607,7 @@ clean-noinstLTLIBRARIES:
#     rm -f "$${dir}/so_locations"; \
#   done
#  libgettextpo.la: $(libgettextpo_la_OBJECTS) $(libgettextpo_la_DEPENDENCIES) 
# - $(AM_V_CCLD)$(libgettextpo_la_LINK) -rpath $(libdir) $(libgettextpo_la_OBJECTS) $(libgettextpo_la_LIBADD) $(LIBS)
# + $(AM_V_CCLD)$(libgettextpo_la_LINK) -rpath @rpath $(libgettextpo_la_OBJECTS) $(libgettextpo_la_LIBADD) $(LIBS)
#  libgnu.la: $(libgnu_la_OBJECTS) $(libgnu_la_DEPENDENCIES) 
#   $(AM_V_CCLD)$(libgnu_la_LINK)  $(libgnu_la_OBJECTS) $(libgnu_la_LIBADD) $(LIBS)
#  
# diff --git a/gettext-tools/src/Makefile.in b/gettext-tools/src/Makefile.in
# index f9842d2..ab53e9b 100644
# --- a/gettext-tools/src/Makefile.in
# +++ b/gettext-tools/src/Makefile.in
# @@ -367,7 +367,7 @@ am_msgcmp_OBJECTS = msgcmp-msgcmp.$(OBJEXT) \
#   msgcmp-msgl-fsearch.$(OBJEXT)
#  msgcmp_OBJECTS = $(am_msgcmp_OBJECTS)
#  msgcmp_LINK = $(LIBTOOL) $(AM_V_lt) --tag=CC $(AM_LIBTOOLFLAGS) \
# - $(LIBTOOLFLAGS) --mode=link $(CCLD) $(AM_CFLAGS) $(CFLAGS) \
# + $(LIBTOOLFLAGS) --mode=link $(CCLD) $(AM_CFLAGS) $(CFLAGS) -Wl,-rpath,@loader_path/../lib \
#   $(msgcmp_LDFLAGS) $(LDFLAGS) -o $@
#  am__msgcomm_SOURCES_DIST = msgcomm.c ../woe32dll/c++msgcomm.cc
#  @WOE32DLL_FALSE@am_msgcomm_OBJECTS = msgcomm-msgcomm.$(OBJEXT)
# @@ -384,7 +384,7 @@ msgen_OBJECTS = $(am_msgen_OBJECTS)
#  am_msgexec_OBJECTS = msgexec-msgexec.$(OBJEXT)
#  msgexec_OBJECTS = $(am_msgexec_OBJECTS)
#  msgexec_LINK = $(LIBTOOL) $(AM_V_lt) --tag=CC $(AM_LIBTOOLFLAGS) \
# - $(LIBTOOLFLAGS) --mode=link $(CCLD) $(AM_CFLAGS) $(CFLAGS) \
# + $(LIBTOOLFLAGS) --mode=link $(CCLD) $(AM_CFLAGS) $(CFLAGS) -Wl,-rpath,@loader_path/../lib  \
#   $(msgexec_LDFLAGS) $(LDFLAGS) -o $@
#  am__msgfilter_SOURCES_DIST = msgfilter.c filter-sr-latin.c \
#   ../woe32dll/c++msgfilter.cc
# @@ -401,7 +401,7 @@ am_msgfmt_OBJECTS = msgfmt-msgfmt.$(OBJEXT) msgfmt-write-mo.$(OBJEXT) \
#  msgfmt_OBJECTS = $(am_msgfmt_OBJECTS)
#  msgfmt_LINK = $(LIBTOOL) $(AM_V_lt) --tag=CC $(AM_LIBTOOLFLAGS) \
#   $(LIBTOOLFLAGS) --mode=link $(CCLD) $(AM_CFLAGS) $(CFLAGS) \
# - $(msgfmt_LDFLAGS) $(LDFLAGS) -o $@
# + -Wl,-rpath,@loader_path/../lib $(msgfmt_LDFLAGS) $(LDFLAGS) -o $@
#  am__msggrep_SOURCES_DIST = msggrep.c ../woe32dll/c++msggrep.cc
#  @WOE32DLL_FALSE@am_msggrep_OBJECTS = msggrep-msggrep.$(OBJEXT)
#  @WOE32DLL_TRUE@am_msggrep_OBJECTS = msggrep-c++msggrep.$(OBJEXT)
# @@ -412,7 +412,7 @@ am_msginit_OBJECTS = msginit-msginit.$(OBJEXT) \
#  msginit_OBJECTS = $(am_msginit_OBJECTS)
#  msginit_LINK = $(LIBTOOL) $(AM_V_lt) --tag=CC $(AM_LIBTOOLFLAGS) \
#   $(LIBTOOLFLAGS) --mode=link $(CCLD) $(AM_CFLAGS) $(CFLAGS) \
# - $(msginit_LDFLAGS) $(LDFLAGS) -o $@
# + -Wl,-rpath,@loader_path/../lib $(msginit_LDFLAGS) $(LDFLAGS) -o $@
#  am__msgmerge_SOURCES_DIST = msgmerge.c msgl-fsearch.c lang-table.c \
#   plural-count.c ../woe32dll/c++msgmerge.cc
#  @WOE32DLL_FALSE@am_msgmerge_OBJECTS = msgmerge-msgmerge.$(OBJEXT) \
# @@ -431,7 +431,7 @@ am_msgunfmt_OBJECTS = msgunfmt-msgunfmt.$(OBJEXT) \
#  msgunfmt_OBJECTS = $(am_msgunfmt_OBJECTS)
#  msgunfmt_LINK = $(LIBTOOL) $(AM_V_lt) --tag=CC $(AM_LIBTOOLFLAGS) \
#   $(LIBTOOLFLAGS) --mode=link $(CCLD) $(AM_CFLAGS) $(CFLAGS) \
# - $(msgunfmt_LDFLAGS) $(LDFLAGS) -o $@
# + -Wl,-rpath,@loader_path/../lib $(msgunfmt_LDFLAGS) $(LDFLAGS) -o $@
#  am__msguniq_SOURCES_DIST = msguniq.c ../woe32dll/c++msguniq.cc
#  @WOE32DLL_FALSE@am_msguniq_OBJECTS = msguniq-msguniq.$(OBJEXT)
#  @WOE32DLL_TRUE@am_msguniq_OBJECTS = msguniq-c++msguniq.$(OBJEXT)
# @@ -443,7 +443,7 @@ recode_sr_latin_OBJECTS = $(am_recode_sr_latin_OBJECTS)
#  recode_sr_latin_LDADD = $(LDADD)
#  recode_sr_latin_LINK = $(LIBTOOL) $(AM_V_lt) --tag=CC \
#   $(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) --mode=link $(CCLD) \
# - $(AM_CFLAGS) $(CFLAGS) $(recode_sr_latin_LDFLAGS) $(LDFLAGS) \
# + $(AM_CFLAGS) $(CFLAGS) $(recode_sr_latin_LDFLAGS) -Wl,-rpath,@loader_path/../lib $(LDFLAGS) \
#   -o $@
#  am_urlget_OBJECTS = urlget-urlget.$(OBJEXT)
#  urlget_OBJECTS = $(am_urlget_OBJECTS)
# @@ -510,7 +510,7 @@ am__v_at_0 = @
#  CCLD = $(CC)
#  LINK = $(LIBTOOL) $(AM_V_lt) --tag=CC $(AM_LIBTOOLFLAGS) \
#   $(LIBTOOLFLAGS) --mode=link $(CCLD) $(AM_CFLAGS) $(CFLAGS) \
# - $(AM_LDFLAGS) $(LDFLAGS) -o $@
# + -Wl,-rpath,@loader_path/../lib $(AM_LDFLAGS) $(LDFLAGS) -o $@
#  AM_V_CCLD = $(am__v_CCLD_$(V))
#  am__v_CCLD_ = $(am__v_CCLD_$(AM_DEFAULT_VERBOSITY))
#  am__v_CCLD_0 = @echo "  CCLD  " $@;
# @@ -526,7 +526,7 @@ am__v_CXX_0 = @echo "  CXX   " $@;
#  CXXLD = $(CXX)
#  CXXLINK = $(LIBTOOL) $(AM_V_lt) --tag=CXX $(AM_LIBTOOLFLAGS) \
#   $(LIBTOOLFLAGS) --mode=link $(CXXLD) $(AM_CXXFLAGS) \
# - $(CXXFLAGS) $(AM_LDFLAGS) $(LDFLAGS) -o $@
# + -Wl,-rpath,@loader_path/../lib $(CXXFLAGS) $(AM_LDFLAGS) $(LDFLAGS) -o $@
#  AM_V_CXXLD = $(am__v_CXXLD_$(V))
#  am__v_CXXLD_ = $(am__v_CXXLD_$(AM_DEFAULT_VERBOSITY))
#  am__v_CXXLD_0 = @echo "  CXXLD " $@;
# @@ -1711,7 +1711,7 @@ urlget_CPPFLAGS = $(AM_CPPFLAGS) -DINSTALLDIR=\"$(pkglibdir)\"
#  
#  @WOE32DLL_FALSE@msgattrib_LINK = $(LIBTOOL) $(AM_V_lt) --tag=CC \
#  @WOE32DLL_FALSE@ $(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) --mode=link \
# -@WOE32DLL_FALSE@ $(CCLD) $(AM_CFLAGS) $(CFLAGS) $(msgattrib_LDFLAGS) $(LDFLAGS) \
# +@WOE32DLL_FALSE@ $(CCLD) $(AM_CFLAGS) $(CFLAGS) -Wl,-rpath,@loader_path/../lib $(msgattrib_LDFLAGS) $(LDFLAGS) \
#  @WOE32DLL_FALSE@ -o $@
#  
#  @WOE32DLL_TRUE@msgattrib_LINK = $(LIBTOOL) $(AM_V_lt) --tag=CXX \
# @@ -1721,7 +1721,7 @@ urlget_CPPFLAGS = $(AM_CPPFLAGS) -DINSTALLDIR=\"$(pkglibdir)\"
#  
#  @WOE32DLL_FALSE@msgcat_LINK = $(LIBTOOL) $(AM_V_lt) --tag=CC \
#  @WOE32DLL_FALSE@ $(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) --mode=link \
# -@WOE32DLL_FALSE@ $(CCLD) $(AM_CFLAGS) $(CFLAGS) $(msgcat_LDFLAGS) $(LDFLAGS) \
# +@WOE32DLL_FALSE@ $(CCLD) $(AM_CFLAGS) $(CFLAGS) -Wl,-rpath,@loader_path/../lib $(msgcat_LDFLAGS) $(LDFLAGS) \
#  @WOE32DLL_FALSE@ -o $@
#  
#  @WOE32DLL_TRUE@msgcat_LINK = $(LIBTOOL) $(AM_V_lt) --tag=CXX \
# @@ -1731,7 +1731,7 @@ urlget_CPPFLAGS = $(AM_CPPFLAGS) -DINSTALLDIR=\"$(pkglibdir)\"
#  
#  @WOE32DLL_FALSE@msgcomm_LINK = $(LIBTOOL) $(AM_V_lt) --tag=CC \
#  @WOE32DLL_FALSE@ $(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) --mode=link \
# -@WOE32DLL_FALSE@ $(CCLD) $(AM_CFLAGS) $(CFLAGS) $(msgcomm_LDFLAGS) $(LDFLAGS) \
# +@WOE32DLL_FALSE@ $(CCLD) $(AM_CFLAGS) $(CFLAGS) -Wl,-rpath,@loader_path/../lib $(msgcomm_LDFLAGS) $(LDFLAGS) \
#  @WOE32DLL_FALSE@ -o $@
#  
#  @WOE32DLL_TRUE@msgcomm_LINK = $(LIBTOOL) $(AM_V_lt) --tag=CXX \
# @@ -1741,7 +1741,7 @@ urlget_CPPFLAGS = $(AM_CPPFLAGS) -DINSTALLDIR=\"$(pkglibdir)\"
#  
#  @WOE32DLL_FALSE@msgconv_LINK = $(LIBTOOL) $(AM_V_lt) --tag=CC \
#  @WOE32DLL_FALSE@ $(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) --mode=link \
# -@WOE32DLL_FALSE@ $(CCLD) $(AM_CFLAGS) $(CFLAGS) $(msgconv_LDFLAGS) $(LDFLAGS) \
# +@WOE32DLL_FALSE@ $(CCLD) $(AM_CFLAGS) $(CFLAGS) -Wl,-rpath,@loader_path/../lib $(msgconv_LDFLAGS) $(LDFLAGS) \
#  @WOE32DLL_FALSE@ -o $@
#  
#  @WOE32DLL_TRUE@msgconv_LINK = $(LIBTOOL) $(AM_V_lt) --tag=CXX \
# @@ -1751,7 +1751,7 @@ urlget_CPPFLAGS = $(AM_CPPFLAGS) -DINSTALLDIR=\"$(pkglibdir)\"
#  
#  @WOE32DLL_FALSE@msgen_LINK = $(LIBTOOL) $(AM_V_lt) --tag=CC \
#  @WOE32DLL_FALSE@ $(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) --mode=link \
# -@WOE32DLL_FALSE@ $(CCLD) $(AM_CFLAGS) $(CFLAGS) $(msgen_LDFLAGS) $(LDFLAGS) \
# +@WOE32DLL_FALSE@ $(CCLD) $(AM_CFLAGS) $(CFLAGS) -Wl,-rpath,@loader_path/../lib $(msgen_LDFLAGS) $(LDFLAGS) \
#  @WOE32DLL_FALSE@ -o $@
#  
#  @WOE32DLL_TRUE@msgen_LINK = $(LIBTOOL) $(AM_V_lt) --tag=CXX \
# @@ -1761,7 +1761,7 @@ urlget_CPPFLAGS = $(AM_CPPFLAGS) -DINSTALLDIR=\"$(pkglibdir)\"
#  
#  @WOE32DLL_FALSE@msgfilter_LINK = $(LIBTOOL) $(AM_V_lt) --tag=CC \
#  @WOE32DLL_FALSE@ $(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) --mode=link \
# -@WOE32DLL_FALSE@ $(CCLD) $(AM_CFLAGS) $(CFLAGS) $(msgfilter_LDFLAGS) $(LDFLAGS) \
# +@WOE32DLL_FALSE@ $(CCLD) $(AM_CFLAGS) $(CFLAGS) -Wl,-rpath,@loader_path/../lib $(msgfilter_LDFLAGS) $(LDFLAGS) \
#  @WOE32DLL_FALSE@ -o $@
#  
#  @WOE32DLL_TRUE@msgfilter_LINK = $(LIBTOOL) $(AM_V_lt) --tag=CXX \
# @@ -1771,7 +1771,7 @@ urlget_CPPFLAGS = $(AM_CPPFLAGS) -DINSTALLDIR=\"$(pkglibdir)\"
#  
#  @WOE32DLL_FALSE@msggrep_LINK = $(LIBTOOL) $(AM_V_lt) --tag=CC \
#  @WOE32DLL_FALSE@ $(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) --mode=link \
# -@WOE32DLL_FALSE@ $(CCLD) $(AM_CFLAGS) $(CFLAGS) $(msggrep_LDFLAGS) $(LDFLAGS) \
# +@WOE32DLL_FALSE@ $(CCLD) $(AM_CFLAGS) $(CFLAGS) -Wl,-rpath,@loader_path/../lib $(msggrep_LDFLAGS) $(LDFLAGS) \
#  @WOE32DLL_FALSE@ -o $@
#  
#  @WOE32DLL_TRUE@msggrep_LINK = $(LIBTOOL) $(AM_V_lt) --tag=CXX \
# @@ -1781,7 +1781,7 @@ urlget_CPPFLAGS = $(AM_CPPFLAGS) -DINSTALLDIR=\"$(pkglibdir)\"
#  
#  @WOE32DLL_FALSE@msgmerge_LINK = $(LIBTOOL) $(AM_V_lt) --tag=CC \
#  @WOE32DLL_FALSE@ $(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) --mode=link \
# -@WOE32DLL_FALSE@ $(CCLD) $(msgmerge_CFLAGS) $(CFLAGS) $(msgmerge_LDFLAGS) $(LDFLAGS) \
# +@WOE32DLL_FALSE@ $(CCLD) $(msgmerge_CFLAGS) $(CFLAGS) -Wl,-rpath,@loader_path/../lib $(msgmerge_LDFLAGS) $(LDFLAGS) \
#  @WOE32DLL_FALSE@ -o $@
#  
#  @WOE32DLL_TRUE@msgmerge_LINK = $(LIBTOOL) $(AM_V_lt) --tag=CXX \
# @@ -1791,7 +1791,7 @@ urlget_CPPFLAGS = $(AM_CPPFLAGS) -DINSTALLDIR=\"$(pkglibdir)\"
#  
#  @WOE32DLL_FALSE@msguniq_LINK = $(LIBTOOL) $(AM_V_lt) --tag=CC \
#  @WOE32DLL_FALSE@ $(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) --mode=link \
# -@WOE32DLL_FALSE@ $(CCLD) $(AM_CFLAGS) $(CFLAGS) $(msguniq_LDFLAGS) $(LDFLAGS) \
# +@WOE32DLL_FALSE@ $(CCLD) $(AM_CFLAGS) $(CFLAGS) -Wl,-rpath,@loader_path/../lib $(msguniq_LDFLAGS) $(LDFLAGS) \
#  @WOE32DLL_FALSE@ -o $@
#  
#  @WOE32DLL_TRUE@msguniq_LINK = $(LIBTOOL) $(AM_V_lt) --tag=CXX \
# @@ -1801,7 +1801,7 @@ urlget_CPPFLAGS = $(AM_CPPFLAGS) -DINSTALLDIR=\"$(pkglibdir)\"
#  
#  @WOE32DLL_FALSE@xgettext_LINK = $(LIBTOOL) $(AM_V_lt) --tag=CC \
#  @WOE32DLL_FALSE@ $(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) --mode=link \
# -@WOE32DLL_FALSE@ $(CCLD) $(AM_CFLAGS) $(CFLAGS) $(xgettext_LDFLAGS) $(LDFLAGS) \
# +@WOE32DLL_FALSE@ $(CCLD) $(AM_CFLAGS) $(CFLAGS) $(xgettext_LDFLAGS) -Wl,-rpath,@loader_path/../lib $(LDFLAGS) \
#  @WOE32DLL_FALSE@ -o $@
#  
#  @WOE32DLL_TRUE@xgettext_LINK = $(LIBTOOL) $(AM_V_lt) --tag=CXX \
# @@ -1897,7 +1897,7 @@ clean-libLTLIBRARIES:
#     rm -f "$${dir}/so_locations"; \
#   done
#  libgettextsrc.la: $(libgettextsrc_la_OBJECTS) $(libgettextsrc_la_DEPENDENCIES) 
# - $(AM_V_GEN)$(libgettextsrc_la_LINK) -rpath $(libdir) $(libgettextsrc_la_OBJECTS) $(libgettextsrc_la_LIBADD) $(LIBS)
# + $(AM_V_GEN)$(libgettextsrc_la_LINK) -rpath @rpath $(libgettextsrc_la_OBJECTS) $(libgettextsrc_la_LIBADD) $(LIBS)
#  install-binPROGRAMS: $(bin_PROGRAMS)
#   @$(NORMAL_INSTALL)
#   test -z "$(bindir)" || $(MKDIR_P) "$(DESTDIR)$(bindir)"
