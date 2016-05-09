
DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")"; pwd -P)

PLIST_PATH="$DIR/build/MacGPG2/share/gnupg/Version.plist"

if [[ -d "$DEPLOY_RESOURCES_DIR" ]] ;then
    source "$DEPLOY_RESOURCES_DIR/config/versioning.sh"
else	
    source "$DIR/Version.config"
fi
[[ -n "$VERSION" ]] || { echo "Missing Version!"; exit 1; }

rm -f "$PLIST_PATH"

/usr/libexec/PlistBuddy \
  -c "Add CommitHash String '${COMMIT_HASH:--}'" \
  -c "Add BuildNumber String '${BUILD_NUMBER:-0}'" \
  -c "Add CFBundleVersion String '${BUILD_VERSION:-0n}'" \
  -c "Add CFBundleShortVersionString String '$VERSION'" \
  "$PLIST_PATH" >/dev/null || exit 2
