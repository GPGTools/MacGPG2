#!/usr/bin/python

"""Copies the homebrew MacGPG2 directory tree to a destination dir.

A simple copy doesn't work since symlinks aren't resolved. A deep
copy fails, since symlinks are resolved, but all lib files are also
copied, not only actual files.

Especially with lib files there are different version referencing the
same file.

This script takes care of creating the correct structure re-creating
all symlinks as symlinks. 
"""

import os, pprint, types, glob, re, sys, shutil, distutils.dir_util, subprocess
from optparse import OptionParser

class TerminalColor(object):
    @classmethod
    def blue(cls):
        return TerminalColor.bold(34)
    
    @classmethod
    def white(cls):
        return TerminalColor.bold(39)
    
    @classmethod
    def red(cls):
        return TerminalColor.underline(31)
    
    @classmethod
    def yellow(cls):
        return TerminalColor.underline(33)
    
    @classmethod
    def reset(cls):
        return TerminalColor.escape(0)
    
    @classmethod
    def em(cls):
        return TerminalColor.underline(39)
    
    @classmethod
    def green(cls):
        return TerminalColor.color(92)
    
    @classmethod
    def color(cls, s):
        return TerminalColor.escape("0;%s" % (s))
    
    @classmethod
    def bold(cls, s):
        return TerminalColor.escape("1;%s" % (s))
    
    @classmethod
    def underline(cls, s):
        return TerminalColor.escape("4;%s" % (s))
    
    @classmethod
    def escape(cls, s):
        return "\033[%sm" % (s)

def status(msg):
    print "%s==>%s - %s" % (TerminalColor.blue(), TerminalColor.reset(), msg)

def title(msg):
    print "%s==>%s %s%s" % (TerminalColor.blue(), TerminalColor.white(), msg, TerminalColor.reset())

def success(msg):
    print "%s==>%s %s%s" % (TerminalColor.green(), TerminalColor.white(), msg, TerminalColor.reset())

def error(msg):
    print "%sError%s: %s" % (TerminalColor.red(), TerminalColor.reset(), msg)
    sys.exit(2)

def version_from_config(config_file):
    command = ['bash', '-c', 'source %s && echo $MAJOR.$MINOR${REVISION:+.$REVISION}' % config_file]
    p = subprocess.Popen(command, stdout=subprocess.PIPE)
    version = p.communicate()[0].strip()

    p.returncode == 0 or sys.exit("Unable to get version from '%s'!" % config_file)
    version[:2] == '2.' or sys.exit("Invalid version '%s'" % version)
    return version

def xcopy(src, dst, makedirs=False):
    """
    Provides an extended copy version of shutil.copy2 also copying
    extended attributes in addition to group and owner, which shutil
    doesn't copy at all.
    
    If makedirs is set, create intermediate directories.
    """
    import shutil, xattr
    stat = os.stat(src)
    if makedirs and not os.path.isdir(os.path.dirname(dst)):
        os.makedirs(os.path.dirname(dst), 0755)
    # Copy the file
    shutil.copy2(src, dst)
    # Might fail on some files. simply ignore.
    try:
        os.lchown(dst, stat.st_uid, stat.st_gid)
    except:
        pass
    # And copy the extended attributes.
    src_attrs = xattr.xattr(src)
    dst_attrs = xattr.xattr(dst)
    for key, value in src_attrs.iteritems():
        dst_attrs[key] = value

def copy_from_homebrew(base_dir, dest_dir, files):
    # Stores a map of file names and symlinks pointing to the same file.
    symlinks = {}
    
    for path in files:
        # Fully resolved path contains the actual path for symlinks
        # even if there are more symlinks in between.
        fully_resolved_path = os.path.realpath(path)
        # Real path however points to the path item within the destination dir
        # which might still be a link. It's important to test for these links,
        # since they should stay links e.g. in the lib folder.
        real_path = os.path.join(os.path.dirname(fully_resolved_path), os.path.basename(path))
        # Destination path is where the file is copied to, in case of a real file
        # otherwise it's registered to be symlinked.
        dst_path = path.replace(base_dir, dest_dir)
        
        if fully_resolved_path not in symlinks:
            symlinks[fully_resolved_path] = []
        
        if not os.path.islink(real_path):
            xcopy(real_path, dst_path, makedirs=True)
        else:
            symlinks[fully_resolved_path].append(dst_path)
        
    # Link each symlink to its original path.
    for key, links in symlinks.iteritems():
        src_name = os.path.basename(key)
        
        # Change into the directory to create a relative link.
        for dst_path in links:
            # Change into the destination dir to create a relative
            # symlink.
            os.chdir(os.path.dirname(dst_path))
            os.symlink(src_name, os.path.basename(dst_path))

def wildcard_expander(file):
    if file.endswith("**"):
        return tree_files(file.replace("**", ""))
    
    return glob.glob(file.replace("**", ""))

def tree_files(directory):
    if not os.path.isdir(directory):
        return []
    return reduce(lambda l1,l2: l1 + l2,
                  map(lambda entry: map(lambda x: os.path.join(entry[0], x), entry[2]), 
                                        os.walk(directory, followlinks=True)))

def inverted_files(source_dir, base_path, files, pattern=None):
    """Returns all files from base_path tree which are not in files."""
    original_files = tree_files(os.path.join(source_dir, base_path))
    
    # Expand wildcards on files.
    files = reduce(lambda l1, l2: l1 + l2,
                   map(lambda x: wildcard_expander(os.path.join(source_dir, x)), files))
    
    # If a pattern is given, match the original files against
    # that pattern and include only those matching.
    if pattern:
        compiled_pattern = re.compile(pattern)
        files = filter(lambda x: (compiled_pattern.search(os.path.basename(x)) != None and 
                                  compiled_pattern.search(os.path.basename(x)).group() != ''), files)
    
    inverted = list(set(original_files).difference(files))
    
    return inverted

def prepend_path_component(path_component, files):
    return map(lambda x: os.path.join(path_component, x), files)

def append_path_component(path_component, files):
    if isinstance(path_component, types.StringTypes):
        path_component = [path_component]
    return map(lambda x: os.path.join(x, *path_component), files)

def resolve_files(base_path, files):
    """Resolves a list of file names containing patterns to actual file paths."""
    return reduce(lambda l1, l2: l1 + l2, 
                  map(lambda x: wildcard_expander(os.path.join(base_path, x)), files))


def parse_options():
    usage = "usage: %prog [-p] source_directory target_directory"
    parser = OptionParser(usage=usage)
    parser.add_option("-p", "--prune", dest="prune", action="store_true",
                      default=False,
                      help="Remove target directory prior to copying from source directory")
    
    return [parser] + list(parser.parse_args())

def main():
    (parser, options, args) = parse_options()
    
    if len(args) != 2:
        parser.print_usage()
        sys.exit(1)
    
    DIRS = ("bin", "sbin", "lib", "libexec", "share")
    BASE_DIR = os.path.realpath(args[0])
    DEST_DIR = os.path.realpath(args[1])
    CURRENT_DIR = os.path.dirname(os.path.abspath(__file__))
    
    BASE_DIR_ORIGINAL = args[0]
    DEST_DIR_ORIGINAL = args[1]
    
    # Check if BASE_DIR actually exists.
    if not os.path.isdir(BASE_DIR):
        error("Source directory doesn't exist: %s" % (BASE_DIR_ORIGINAL))
    
    # If prune is not set and the target directory already
    # exists, exit with an error.
    if os.path.isdir(DEST_DIR) and not options.prune:
       error("Target directory already exists: %s - use --prune to force removal." % 
             (DEST_DIR_ORIGINAL))
    
    # If prune is set, remove the target directory.
    if os.path.isdir(DEST_DIR) and options.prune:
        shutil.rmtree(DEST_DIR)
    
    # Reset the umask.
    os.makedirs(DEST_DIR, 0755)
    
    if not os.path.isdir(DEST_DIR):
        error("Failed to create target directory: %s" % (DEST_DIR_ORIGINAL))
    
    title("Prepare MacGPG2 files for the installer")
    
    status("Collect files to exclude")
    
    # Contains all files to exclude. If the files to include are known
    # simply use inverted_files which returns all other files.
    EXCLUDE_FILES = reduce(lambda l1, l2: l1 + l2, (
        # ./bin files to exclude.
        resolve_files(BASE_DIR, prepend_path_component("bin", 
            ["dumpsexp", "gpgparsemail", "gpgsm", "gpgsm-gencert.sh", 
             "kbxutil", "gpg-error", "gpgv2", "msg*", "*gettext*", "*-config", 
             "recode*", "iconv", "brew", "envsubst", "autopoint", "adns*", "asn1*", "lz*",
             "unlzma", "unxz", "xz*", "aclocal*", "auto*", "ifnames"])),
    
        # ./lib files to exclude
        resolve_files(BASE_DIR, prepend_path_component("lib", ["pkgconfig**", "gettext**"]) + 
            # Apparently for gpg gettext support we only need libintl.
            map(lambda x: os.path.join("lib", x), ["libgettextlib*", "libgettextpo*", "libgettextsrc*"])
        ),
    
        # ./sbin files to exclude.
        resolve_files(BASE_DIR, prepend_path_component("sbin", 
            ["applygnupgdefaults", "addgnupghome"])),
    
        # ./share files to exclude.
        resolve_files(BASE_DIR,
            prepend_path_component("share",  
                # Two stars denote that tree_files should be used to also exclude
                # any subfolders.
                append_path_component("**", ["aclocal", "aclocal-1.14", "autoconf", "automake-1.14", "common-lisp", "doc", "gettext", "emacs"]))),
        
        # ./share/gnupg files to include
        inverted_files(BASE_DIR, os.path.join("share", "gnupg"), map(lambda x: os.path.join("share", "gnupg", x),
            ["*"]), pattern="gpg-conf.skel"),
        
        # ./share/locale files to include.
        inverted_files(BASE_DIR, os.path.join("share", "locale"), map(lambda x: os.path.join("share", "locale", x), 
            map(lambda x: os.path.join(x, "*", "*"), 
                # en is not necessary since it's the default language.
                ["de", "fr", "es", "it", "uk"])), pattern="gnupg2\.mo"),
    
        # ./share/man files to include.
        inverted_files(BASE_DIR, os.path.join("share", "man"), map(lambda x: os.path.join("share", "man", "*", x), 
            ["gpg2.1", "gpgconf.1", "gpg-agent.1", "gpg-connect-agent.1", "watchgnupg.1", "scdaemon.1", "symcryptrun.1"])),
    ))

    # Record the current working dir to change
    # back to it at the end.
    working_dir = os.getcwd()
    
    
    status("Collect files to copy from %s" % (BASE_DIR_ORIGINAL))
    # Will contain every file of the base directory.
    all_files = reduce(lambda l1, l2: l1 + l2, 
                       map(lambda directory: tree_files(os.path.join(BASE_DIR, directory)), DIRS))

    # The subset of files which are actually necessary.
    # YES, we can dump a lot.
    distro_files = set(all_files).difference(EXCLUDE_FILES)
    
    status("Copy files from %s to %s" % (BASE_DIR_ORIGINAL, DEST_DIR_ORIGINAL))
    # Copy all necessary files.
    try:
        copy_from_homebrew(BASE_DIR, DEST_DIR, distro_files)
        # For homebrew compatibility create a file with the current version.
        version = version_from_config("%s/Version.config" % (working_dir))
        if os.path.isdir("%s/share/gnupg" % DEST_DIR) and version:
            status("Create version file for homebrew in %s/share/gnupg" % (DEST_DIR_ORIGINAL))
            fh = open("%s/share/gnupg/VERSION" % DEST_DIR, "w")
            fh.write(version)
            fh.close()
        
    except Exception, e:
        import traceback
        traceback.print_exc()
        error("Failed to copy files from %s - %s" % (BASE_DIR_ORIGINAL, e))
    
    

    # Copy the additional files from Payload/libexec.
    distutils.dir_util.copy_tree(os.path.join(CURRENT_DIR, "Payload/libexec/"), os.path.join(DEST_DIR, "libexec/"))
    distutils.dir_util.copy_tree(os.path.join(CURRENT_DIR, "Payload/share/"), os.path.join(DEST_DIR, "share/"))
    # Change back to the original directory
    os.chdir(working_dir)
    
    success("Preparing for installer succeeded")
    
    sys.exit(0)

if __name__ == "__main__":
    main()
