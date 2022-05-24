MacGPG
=======

MacGPG is a version of GnuPG 2 with specific patches for macOS.


Updates
-------

The latest releases of MacGPG can be found on our [official website](https://gpgtools.org/).

For the latest news and updates check our [Twitter](https://twitter.com/gpgtools).

Visit our [support page](https://support.gpgtools.org) if you have questions or need help setting up your system and using MacGPG.


Build
-----

#### Clone the repository
```bash
git clone https://github.com/GPGTools/MacGPG2.git
cd MacGPG2
```

#### Build
```bash
make
```

**Note**: It might be necessary to install lzip if you don't already have it (e.g. `brew install lzip`)

#### Install

Copy ./build/MacGPG2 to /usr/local/MacGPG2

System Requirements
-------------------

* Mac OS X >= 10.12
