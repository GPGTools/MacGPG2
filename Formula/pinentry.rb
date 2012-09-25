require 'formula'

class Pinentry < Formula
  homepage 'http://gpgtools.org'
  url 'https://github.com/GPGTools/pinentry-mac.git'
  md5 '2095fbe7cf556062427e04c156503f27'

  # depends_on 'cmake' => :build
  
  def patches
    { :p0 => DATA }
  end
  
  def install
    ENV.universal_binary if ARGV.build_universal?
    
    target = "compile"
    build_dir = "Release"
    xconfig = "homebrew.xconfig"
    if ARGV.build_ppc?
      target = "compile_with_ppc"
      build_dir = "Release with ppc"
      xconfig = "homebrew-ppc.xconfig"
      build_env = ARGV.build_env.gsub ' ', '\\\ '
      inreplace 'homebrew-ppc.xconfig' do |s|
         s.gsub! '#SDKROOT#', "#{build_env}/SDKs/MacOSX10.5.sdk"
      end
    end
    
    inreplace xconfig do |s|
      s.gsub! '#HOMEBREW_LIB#', "#{HOMEBREW_PREFIX}/lib"
    end
    
    # Use the homebrew.xconfig file to force using GGC_VERSION specified.
    inreplace 'Makefile' do |s|
      s.gsub! /@xcodebuild/, "@xcodebuild -xcconfig #{xconfig}"
    end
    
    system "make #{target}" # if this fails, try separate make/make install steps
    
    # Homebrew doesn't like touching libexec for some reason.
    # That's why we have to manually symlink.
    # Also uninstalling wouldn't take care of libexec, so I've pachted keg.rb
    libexec.install "build/#{build_dir}/pinentry-mac.app"
    Pathname.new("#{HOMEBREW_PREFIX}/libexec/pinentry-mac.app").make_relative_symlink("#{prefix}/libexec/pinentry-mac.app")
  end
end

__END__

diff --git homebrew.xconfig homebrew.xconfig
new file mode 100644
index 0000000..fdb4290
--- /dev/null
+++ homebrew.xconfig
@@ -0,0 +1,2 @@
+GCC_VERSION = com.apple.compilers.llvmgcc42
+OTHER_LDFLAGS = $OTHER_LDFLAGS -L#HOMEBREW_LIB#

diff --git homebrew-ppc.xconfig homebrew-ppc.xconfig
new file mode 100644
index 0000000..9515ac6
--- /dev/null
+++ homebrew-ppc.xconfig
@@ -0,0 +1,3 @@
+GCC_VERSION = com.apple.compilers.llvmgcc42
+SDKROOT = #SDKROOT#
+OTHER_LDFLAGS = $OTHER_LDFLAGS -L#HOMEBREW_LIB#
