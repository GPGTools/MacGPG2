require 'formula'

class Pinentry < Formula
  homepage 'http://gpgtools.org'
  url 'https://github.com/GPGTools/pinentry-mac.git'
  md5 '2095fbe7cf556062427e04c156503f27'

  # depends_on 'cmake' => :build

  def install
    ENV.universal_binary if ARGV.build_universal?
    ENV.build_ppc if ARGV.build_ppc?
    
    target = "compile"
    build_dir = "Release"
    if ARGV.build_ppc?
      target = "compile_with_ppc"
      build_dir = "Release with ppc"
      inreplace 'pinentry-mac.xcodeproj/project.pbxproj' do |s|
        s.gsub! /SDKROOT\s*=\s*.*;/, "SDKROOT = #{ARGV.build_env}/SDKs/MacOSX10.5.sdk;"
        s.gsub! /GCC_VERSION\s*=\s*.*;/, "GCC_VERSION = com.apple.compilers.llvmgcc42;"
      end
    end
    
    system "make #{target}" # if this fails, try separate make/make install steps
    
    # Homebrew doesn't like touching libexec for some reason.
    # That's why we have to manually symlink.
    # Also uninstalling wouldn't take care of libexec, so I've pachted keg.rb
    libexec.install "build/#{build_dir}/pinentry-mac.app"
    Pathname.new("#{HOMEBREW_PREFIX}/libexec/pinentry-mac.app").make_relative_symlink("#{prefix}/libexec/pinentry-mac.app")
  end
end
