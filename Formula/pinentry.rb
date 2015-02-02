require 'formula'

class Pinentry < Formula
  homepage 'http://gpgtools.org'
  head 'https://github.com/GPGTools/pinentry-mac.git', :revision => 'HEAD'
  sha1 ''
  version '0.8.1'
  # depends_on 'cmake' => :build
  
  
  def install
    ENV.universal_binary if ARGV.build_universal?
    
    ldflags = "-headerpad_max_install_names -Wl,-rpath,@loader_path/../../../../lib -Wl,-rpath,#{HOMEBREW_PREFIX}/lib -Wl,-rpath,/usr/local/lib/MacGPG2"
    
    ENV.prepend 'LDFLAGS', ldflags
    
	

    target = "compile"
    build_dir = "Release"
    
    system "make #{target}" # if this fails, try separate make/make install steps
    
    # Homebrew doesn't like touching libexec for some reason.
    # That's why we have to manually symlink.
    # Also uninstalling wouldn't take care of libexec, so I've pachted keg.rb
    libexec.install "build/#{build_dir}/pinentry-mac.app"
    Pathname.new("#{HOMEBREW_PREFIX}/libexec/pinentry-mac.app").make_relative_symlink("#{prefix}/libexec/pinentry-mac.app")
  end
end

