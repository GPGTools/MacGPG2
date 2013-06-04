require 'formula'

class Pinentry < Formula
  homepage 'http://gpgtools.org'
  url 'https://github.com/GPGTools/pinentry-mac.git', :revision => 'HEAD'
  sha1 ''
  version '0.8.1'
  # depends_on 'cmake' => :build
  
  
  def install
    ENV.universal_binary if ARGV.build_universal?
    
    ldflags = "-headerpad_max_install_names -Wl,-rpath,@loader_path/../../../../lib -Wl,-rpath,#{HOMEBREW_PREFIX}/lib -Wl,-rpath,/usr/local/lib/MacGPG2"
    
    ENV.prepend 'LDFLAGS', ldflags
    
	
	xconfig = "GCC_VERSION=com.apple.compilers.llvmgcc42 " +
			  "OTHER_LDFLAGS=\"#{ldflags} \"'$$'OTHER_LDFLAGS\" -L#{HOMEBREW_PREFIX}/lib\" "

    target = "compile"
    build_dir = "Release"
    if ARGV.build_ppc?
      target = "compile_with_ppc"
      build_dir = "Release with ppc"
      build_env = ARGV.build_env
	  
      xconfig += "SDKROOT=\"#{build_env}/SDKs/MacOSX10.5.sdk\" " +
				 "MACOSX_DEPLOYMENT_TARGET=10.5 "
    end
    
	puts "CODE_SIGN=" + ENV['CODE_SIGN']
	
    # Use xconfig to force using GGC_VERSION specified.
    inreplace 'Makefile' do |s|
      s.gsub! "@xcodebuild", "@xcodebuild #{xconfig}"
    end
    
    system "export CODE_SIGN=" + ENV['CODE_SIGN'] + "; make #{target}" # if this fails, try separate make/make install steps
    
    # Homebrew doesn't like touching libexec for some reason.
    # That's why we have to manually symlink.
    # Also uninstalling wouldn't take care of libexec, so I've pachted keg.rb
    libexec.install "build/#{build_dir}/pinentry-mac.app"
    Pathname.new("#{HOMEBREW_PREFIX}/libexec/pinentry-mac.app").make_relative_symlink("#{prefix}/libexec/pinentry-mac.app")
  end
end

