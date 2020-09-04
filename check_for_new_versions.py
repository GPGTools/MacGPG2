#!/usr/bin/env python

import os, sys, re, json, subprocess, hashlib
import urllib.request as request


baseDir = os.path.dirname(os.path.realpath(__file__))
downloadDir = '/Library/Caches/Homebrew/'



with request.urlopen('https://gnupg.org/download/index.html') as response:
   html = response.read().decode("utf-8")


regex = re.compile(r"""
	<tr>\n
	<td\ class="left"><a\ href="[^"]+">  (?P<name>[^<]+)  </a></td>\n 	# Name of the Component
	<td\  class="left">  (?P<version>[^<]+)  </td>\n  						# Version of the Component
	[^\n]+\n  													# No interesting values in this line.
	[^\n]+\n  													# No interesting values in this line.
	<td\ class="left"><a\ href="  (?P<url>[^"]+)  ">[^<]+</a></td>\n 	# URL of archive
	<td\ class="left"><a\ href="  (?P<sig_url>[^"]+)  ">[^<]+</a></td>(\n) 	# URL of signature
""", re.X | re.S)

latestLibs = {}
for match in re.finditer(regex, html):
	g=match.groupdict()
	name=g['name'].lower()
	latestLibs[name] = g

#print (libs)



with open(baseDir + '/libs.json', 'r') as j: 
	libs = json.load(j)



validSigRegex = re.compile(r"""\[GNUPG:\]\ TRUST_FULLY""", re.X | re.M)

for lib in libs:
	name = lib['name']
	if (name in latestLibs):
		latestLib = latestLibs[name]
		if (lib['version'] != latestLib['version']):
			url = 'https://gnupg.org' + latestLib['url']
			sig_url = 'https://gnupg.org' + latestLib['sig_url']
			archive = os.path.basename(url)
			signature = os.path.basename(sig_url)
			archive_path = downloadDir + archive
			signature_path = downloadDir + signature
			request.urlretrieve(url, archive_path)
			request.urlretrieve(sig_url, signature_path)
			
			proc = subprocess.Popen(['/usr/local/bin/gpg', '--status-fd', '1', '--verify', signature_path, archive_path], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
			(out, err) = proc.communicate()
			out = out.decode("utf-8")
			if (validSigRegex.search(out)):
				# Valid signed. get the sha256 hash.
				
				sha256 = hashlib.sha256()
				with open(archive_path, 'rb') as f:
					for block in iter(lambda: f.read(65536), b''):
						sha256.update(block)
				hashString = sha256.hexdigest()
				
				print ('New version of', name, latestLib['version'])
				print ('Hash:', hashString)
				print ()




