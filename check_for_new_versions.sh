#!/bin/bash

BASE_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")"; pwd -P)

# Helper functions.
function doFail {
	msg="\n** ERROR at $* ** - build failed (${BASH_SOURCE[1]##*/}: line ${BASH_LINENO[0]})"
	if [[ "$HAVE_TERMINAL" == "1" ]] ;then
		echo -e "\033[1;31m$msg\033[0m" >&2
	else
		echo -e "$msg" >&2
	fi
	exit 1
}
function pycmd {
	python -c "c=$config
$1"
}


# Read libs.json
config=$(python -c "import sys,json; print(json.loads(sys.stdin.read()))" < "$BASE_DIR/libs.json" 2>/dev/null) || doFail "read libs.json"
count=$(pycmd "print(len(c))") || doFail "process libs.json" 

# Loop over all libs
for (( i=0; i<count; i++ )); do
	# Set the variables lib_name, lib_url, etc.
	unset $(compgen -A variable | grep "^lib_") 
	eval $(pycmd "for k, v in c[$i].iteritems(): print('lib_%s=\'%s\'' % (k, v))")
	
	if [[ ! "$lib_url" =~ ^ftp:// ]]; then
		# Can only check via ftp for new versions.
		continue
	fi
	
	dirUrl=${lib_url%/*}/
	fileName=${lib_url##*/}
	prefix=${fileName%\$\{VERSION\}*}
	suffix=${fileName#*\$\{VERSION\}}
	
	newVersion=$(curl -sl "$dirUrl" | sed -En 's/^'"$prefix"'([0-9\.]+)'"$suffix"'$/\1/p' | sort -t. -k1,1 -k2,2n -k3,3n -k4,4n -k5,5n | tail -n1)
	
	if [[ -n "$newVersion" && "$newVersion" != "$lib_version" ]]; then
		echo "New $lib_name version: $newVersion > $lib_version   $dirUrl$prefix$newVersion$suffix"
	fi
	
done

