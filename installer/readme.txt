Please use the detached signature to confirm the integrity of your download 
prior to install.  Public key needed available from http://www.gpgtools.org/

Unzip the archive and then run the MacGPG2 installer.

 * MD5 (MacGPG2-2.0.17.zip) = 

 * 121,836 downloads of MacGPG2 from 165 countries in two years!


What's New
=========
 * Supports 32- and 64-bit Intel Macs running OS X Leopard (10.5) and higher.

 * Core upgraded to GnuPG v2.0.17
  = Configured to use standard socket and daemonise gpg agent on the fly if 
    required.

 * Maximum key size increased to 8192 bits; recommended for expert users only.

 * Includes GPGTools gpg-agent cache-id option patch.

 * Pinentry updated by GPGTools team and includes keychain support

 * Installs exclusively under /usr/local/MacGPG2/ removing previous v2.0.16 install.

 * Libksba upgraded to v1.1.0

 * Libusb upgraded to v1.0.8


Credits
=====

 * Werner Koch and the GnuPG Project, http://www.gnupg.org/

 * Stéphane Corthésy for the launchd patches.

 * Charly Avital for his patient testing.

 * Dr Alun J Carr for his kind donation.


Noteworthy changes in GnuPG version 2.0.17 (2011-01-13)
-------------------------------------------------

 * Allow more hash algorithms with the OpenPGP v2 card.

 * The gpg-agent now tests for a new gpg-agent.conf on a HUP.

 * Fixed output of "gpgconf --check-options".

 * Fixed a bug where Scdaemon sends a signal to Gpg-agent running in
  non-daemon mode.

 * Fixed TTY management for pinentries and session variable update
  problem.

 * Minor bug fixes.
