require 'formula'

class Macgpg2Updater < Formula
  homepage 'http://gpgtools.org'
  url 'https://github.com/GPGTools/MacGPG2.git', :branch => 'sparkle-test', :revision => 'ad5eab4a3a' 
  sha1 ''                                                                                      
  version '0.1'
  keep_install_names true
  
  def install
    ENV.universal_binary if ARGV.build_universal?
    # Make sure that deployment target is 10.6+ so the lib works
    # on 10.6 and up not only on host system os x version.
    ENV.macosxsdk("10.6")
    # Force clang, otherwise it won't build.
    ENV['CC'] = "clang"
    
    # It's necessary to add the -rpath to the LDFLAGS, otherwise
    # programs can't link to libraries using @rpath.
    ENV.prepend 'LDFLAGS', '-headerpad_max_install_names'
    ENV.prepend 'LDFLAGS', "-Wl,-rpath,@loader_path/../../../../lib -Wl,-rpath,#{HOMEBREW_PREFIX}/lib -Wl,-rpath,/usr/local/lib/MacGPG2"
    
    system "make help"
    
    # Now the GPGTools_Core repository should be available and we can apply the patches necessary.
    inreplace "Dependencies/GPGTools_Core/newBuildSystem/versioning.sh" do |s|
      s.gsub! "git symbolic-ref -q HEAD", "git symbolic-ref -q HEAD 2>/dev/null"
      s.gsub! "git symbolic-ref -q HEAD", "git symbolic-ref -q HEAD 2>/dev/null"
      s.gsub! "git status --porcelain", "git status --porcelain 2>/dev/null"
      s.gsub! "git rev-parse --short HEAD", "git rev-parse --short HEAD 2>/dev/null"
    end
    
    system "make updater"
    
    # Homebrew doesn't like touching libexec for some reason.
    # That's why we have to manually symlink.
    # Also uninstalling wouldn't take care of libexec, so I've pachted keg.rb
    libexec.install "build/Release/MacGPG2_Updater.app"
    Pathname.new("#{HOMEBREW_PREFIX}/libexec/MacGPG2_Updater.app").make_relative_symlink("#{libexec}/MacGPG2_Updater.app")
  end
end
