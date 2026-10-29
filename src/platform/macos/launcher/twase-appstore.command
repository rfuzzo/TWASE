#!/bin/sh
# App Store launcher: only a private, explicitly prepared copy may be injected.
# Only preferences in TWASE's private development home may be changed.

set -u
umask 077

mas_uuid="196C74A0-71A2-3C28-80DF-BC3DB1C7FEB7"
original="/Applications/Total War ATTILA.app"
launcher_dir="$(CDPATH= cd -P -- "$(dirname -- "$0")" && pwd)" || exit 1
source_app="$original"
destination=""
app=""
dylib=""
logfile=""
temporary=""
source_set=0
skip_launcher=0
save=""
lock_directory=""
preferences_changed=0
game_pid=""

help()
{
    cat <<'HELP'
Usage:
  twase-appstore.command --prepare <new-copy.app> [--source <source.app>]
  twase-appstore.command [--dylib <libTWASE.dylib>] [--skip-launcher]
                        [--load <save-name>] <prepared-copy.app>

Prepare a private copy of the supported Mac App Store ATTILA build. The default
source is /Applications/Total War ATTILA.app. The destination must not exist.
Preparation uses cp -cR (an APFS clone; no full-copy fallback), preserves the
App Store receipt, and ad-hoc deep-signs ONLY the new copy with hardened runtime
and these entitlements:
  com.apple.security.cs.allow-dyld-environment-variables
  com.apple.security.cs.disable-library-validation
  com.apple.security.cs.allow-unsigned-executable-memory
WARNING: the private copy is UNSANDBOXED and NO LONGER VENDOR SIGNED. Its original
sandbox, application/team identity and app-group entitlements are not retained.
No receipt or licence validation is patched or bypassed. Keep the installed
original intact; use a new destination for each preparation attempt.

Launching requires an explicit prepared .app path, the supported arm64 UUID,
a valid ad-hoc hardened-runtime signature, and the injection entitlements above.
The default dylib is TWASE/libTWASE.dylib beside this launcher. TWASE_ROOT may
select a different EXISTING root directory containing TWASE (also changes the
default dylib). Logs are TWASE/logs/twase-appstore-launch.log; TWASE reads its
config under TWASE. No Steam libraries are injected or Workshop paths used.

HOME and CFFIXED_USER_HOME are set to TWASE/appstore-home under that root, with
an initially empty Feral preferences/save-data directory. Existing Steam and
App Store preferences are not copied or edited. Run once through the normal
Feral UI first to create PRIVATE preferences. --skip-launcher then suppresses
that UI; --load <save-name> loads a save from the private home's save directory
and implies --skip-launcher. These options temporarily change only private
GameOptionsDialogShouldShow / ExtraCommandLineEnabled / ExtraCommandLine keys,
restoring their original values when the game exits. Save names cannot contain
quotes, backslashes or newlines. Existing saves must be copied into the private
home separately; no Steam/container save lookup or migration is performed.
Game data must be inside the copy at Contents/Resources/Data/data. The working
directory is the copy's Contents/MacOS, never the launcher's directory.

Requires macOS on Apple Silicon, Apple command-line tools (dwarfdump or otool),
codesign, and writable storage for the copy/TWASE directory. Preparation requires
APFS clone support between source and destination. Quarantine is not silently
removed; only remove it from files you trust. --help prints this text.
HELP
}

log()
{
    printf 'twase-appstore: %s\n' "$*"
    if [ -n "$logfile" ]; then
        printf '%s %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$*" >> "$logfile" || exit 1
    fi
}

die()
{
    log "error: $*" >&2
    exit 1
}

cleanup()
{
    cleanup_status=$?
    if [ "$preferences_changed" -eq 1 ]; then
        if ! restore_preferences; then
            printf '%s\n' "twase-appstore: error: could not restore private Feral preferences: $prefs" >&2
            cleanup_status=1
        fi
    fi
    if [ -n "$lock_directory" ]; then
        /bin/rmdir -- "$lock_directory"
    fi
    if [ -n "$temporary" ]; then
        /bin/rm -rf -- "$temporary"
    fi
    trap - EXIT
    exit "$cleanup_status"
}
trap cleanup EXIT
stop_game()
{
    if [ -n "$game_pid" ]; then
        kill -TERM "$game_pid" 2>/dev/null
        wait "$game_pid" 2>/dev/null
        game_pid=""
    fi
}
trap 'stop_game; exit 130' INT
trap 'stop_game; exit 143' TERM
trap 'stop_game; exit 129' HUP

while [ "$#" -gt 0 ]; do
    case "$1" in
        --prepare)
            [ "$#" -ge 2 ] || die "--prepare needs a new .app destination"
            [ -z "$destination" ] || die "--prepare may only be supplied once"
            destination="$2"; shift ;;
        --source)
            [ "$#" -ge 2 ] || die "--source needs a .app source"
            [ "$source_set" -eq 0 ] || die "--source may only be supplied once"
            source_app="$2"; source_set=1; shift ;;
        --dylib)
            [ "$#" -ge 2 ] || die "--dylib needs a dylib path"
            [ -z "$dylib" ] || die "--dylib may only be supplied once"
            dylib="$2"; shift ;;
        --skip-launcher) skip_launcher=1 ;;
        --load)
            [ "$#" -ge 2 ] && [ -n "$2" ] || die "--load needs a save name"
            [ -z "$save" ] || die "--load may only be supplied once"
            save="$2"; skip_launcher=1; shift ;;
        -h|--help) help; exit 0 ;;
        --)
            shift
            [ "$#" -eq 1 ] && [ -z "$app" ] || die "expected exactly one prepared .app path"
            app="$1"; shift; break ;;
        -*) die "unknown option: $1 (see --help)" ;;
        *)
            [ -z "$app" ] || die "expected one prepared .app path, not game arguments"
            app="$1" ;;
    esac
    shift
done

case "$save" in
    *\"*|*\\*|*'
'*|*"$(printf '\r')"*) die "save names cannot contain quotes, backslashes or newlines" ;;
esac

[ "$(/usr/bin/uname -s)" = Darwin ] || die "this launcher requires macOS"
[ "$(/usr/bin/uname -m)" = arm64 ] || die "run this launcher natively on Apple Silicon, not under Rosetta"
[ -x /usr/bin/codesign ] || die "codesign is required"
[ -x /usr/libexec/PlistBuddy ] || die "PlistBuddy is required"
temporary="$(/usr/bin/mktemp -d /tmp/twase-appstore.XXXXXX)" || die "could not create a temporary directory"

# Canonical directory paths prevent aliases of the installed original being used.
resolve_app()
{
    case "${1%/}" in
        *.app) ;;
        *) die "expected an .app bundle: $1" ;;
    esac
    [ -d "$1" ] || die "app bundle not found: $1"
    (CDPATH= cd -P -- "$1" && pwd) || die "cannot resolve app bundle: $1"
}

original_physical="$original"
if [ -d "$original" ]; then
    original_physical="$(resolve_app "$original")" || exit 1
fi

# This allowlist is intentionally exact; version strings are not build identities.
validate_bundle()
{
    bundle="$1"
    executable="$bundle/Contents/MacOS/Total War ATTILA"
    [ -f "$executable" ] && [ -x "$executable" ] || die "game executable not found: $executable"
    [ ! -L "$executable" ] || die "game executable must not be a symlink"
    [ -d "$bundle/Contents/Resources/Data/data" ] || die "App Store data directory not found: $bundle/Contents/Resources/Data/data"
    [ -f "$bundle/Contents/_MASReceipt/receipt" ] || die "App Store receipt is missing; no validation bypass is provided"

    uuid=""
    if [ -x /usr/bin/dwarfdump ] && /usr/bin/dwarfdump --uuid "$executable" > "$temporary/uuid" 2> "$temporary/uuid-error"; then
        uuid="$(/usr/bin/awk '$1 == "UUID:" && $3 == "(arm64)" { print toupper($2) }' "$temporary/uuid")"
    elif [ -x /usr/bin/otool ] && /usr/bin/otool -arch arm64 -l "$executable" > "$temporary/uuid" 2> "$temporary/uuid-error"; then
        uuid="$(/usr/bin/awk '$1 == "cmd" { in_uuid = ($2 == "LC_UUID") } in_uuid && $1 == "uuid" { print toupper($2) }' "$temporary/uuid")"
    else
        die "cannot read the executable UUID; install Apple command-line tools (dwarfdump or otool)"
    fi
    [ "$uuid" = "$mas_uuid" ] || die "unsupported executable UUID: ${uuid:-no arm64 UUID} (expected $mas_uuid)"
}

validate_signature()
{
    signed_app="$1"
    /usr/bin/codesign --verify --deep --strict "$signed_app" || die "prepared copy has an invalid signature; prepare a fresh copy"
    /usr/bin/codesign -dvv "$signed_app" > "$temporary/signature" 2>&1 || die "cannot inspect the app signature"
    /usr/bin/grep -qx 'Signature=adhoc' "$temporary/signature" || die "launch requires an ad-hoc signed private copy, not a vendor-signed app"
    /usr/bin/grep -Eq '^CodeDirectory .*flags=.*\(.*runtime.*\)' "$temporary/signature" || die "prepared copy is missing hardened runtime signing"
    /usr/bin/codesign -d --entitlements - --xml "$signed_app" > "$temporary/actual-entitlements.plist" 2> "$temporary/entitlements-error" || die "cannot inspect app entitlements"
    for entitlement in \
        com.apple.security.cs.allow-dyld-environment-variables \
        com.apple.security.cs.disable-library-validation \
        com.apple.security.cs.allow-unsigned-executable-memory
    do
        value="$(/usr/libexec/PlistBuddy -c "Print :$entitlement" "$temporary/actual-entitlements.plist" 2>/dev/null)" || die "missing injection entitlement: $entitlement"
        [ "$value" = true ] || die "injection entitlement is not true: $entitlement"
    done
    # Match the deliberately de-sandboxed preparation contract; never claim these
    # identity-bound entitlements survive ad-hoc re-signing.
    for entitlement in \
        com.apple.security.app-sandbox \
        com.apple.application-identifier \
        application-identifier \
        com.apple.developer.team-identifier \
        com.apple.security.application-groups
    do
        if /usr/libexec/PlistBuddy -c "Print :$entitlement" "$temporary/actual-entitlements.plist" >/dev/null 2>&1; then
            die "unexpected retained entitlement ($entitlement); prepare a fresh de-sandboxed copy"
        fi
    done
}

if [ -n "$destination" ]; then
    [ -z "$app" ] && [ -z "$dylib" ] && [ "$skip_launcher" -eq 0 ] || die "--prepare cannot be combined with launch options or a launch path"
    source_app="$(resolve_app "$source_app")" || exit 1
    validate_bundle "$source_app"
    destination="${destination%/}"
    case "$destination" in
        *.app) ;;
        *) die "preparation destination must end in .app" ;;
    esac
    [ ! -e "$destination" ] && [ ! -L "$destination" ] || die "destination already exists; it will not be overwritten: $destination"
    destination_parent="$(CDPATH= cd -P -- "$(dirname -- "$destination")" && pwd)" || die "destination parent directory must already exist"
    destination="$destination_parent/$(basename -- "$destination")"
    case "$destination" in
        "$original_physical"|"$original_physical"/*|"$source_app"|"$source_app"/*)
            die "destination must be outside the source and installed original" ;;
    esac
    log "WARNING: preparing an UNSANDBOXED private copy; it will NO LONGER BE VENDOR SIGNED. Receipt/licence checks are not bypassed."
    # Reserve the destination atomically. On failure leave it in place for the
    # user to inspect; never delete or overwrite an existing app on their behalf.
    /bin/mkdir -- "$destination" || die "cannot reserve destination (it may already exist): $destination"
    /bin/cp -cR "$source_app/." "$destination" || die "APFS clone failed; partial destination left at $destination. Use a new destination on a clone-compatible APFS volume."
    cat > "$temporary/entitlements.plist" <<'ENTITLEMENTS'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>com.apple.security.cs.allow-dyld-environment-variables</key><true/>
<key>com.apple.security.cs.disable-library-validation</key><true/>
<key>com.apple.security.cs.allow-unsigned-executable-memory</key><true/>
</dict></plist>
ENTITLEMENTS
    /usr/bin/codesign --force --deep --sign - --options runtime --entitlements "$temporary/entitlements.plist" "$destination" || die "re-signing failed; incomplete private copy left at $destination"
    /usr/bin/cmp -s "$source_app/Contents/_MASReceipt/receipt" "$destination/Contents/_MASReceipt/receipt" || die "receipt was not preserved; do not launch this copy"
    validate_bundle "$destination"
    validate_signature "$destination"
    log "prepared: $destination"
    log "launch explicitly: $0 \"$destination\""
    exit 0
fi

[ "$source_set" -eq 0 ] || die "--source is only valid with --prepare"
[ -n "$app" ] || die "an explicit prepared-copy .app path is required (see --help)"
app="$(resolve_app "$app")" || exit 1
case "$app" in
    "$original_physical"|"$original_physical"/*) die "the installed original must never be used as the injected copy" ;;
esac
validate_bundle "$app"
validate_signature "$app"

root="${TWASE_ROOT:-$launcher_dir}"
root="$(CDPATH= cd -P -- "$root" && pwd)" || die "TWASE_ROOT must name an existing directory containing TWASE"
case "$root" in
    "$original_physical"|"$original_physical"/*|"$app"|"$app"/*)
        die "TWASE_ROOT must be outside the original and prepared app bundles" ;;
esac

# Do not follow pre-existing symlinks into a real user's preference tree.
private_directory()
{
    [ ! -L "$1" ] || die "private writable directory must not be a symlink: $1"
    if [ ! -e "$1" ]; then
        /bin/mkdir -- "$1" || die "cannot create private directory: $1"
    fi
    [ -d "$1" ] && [ -w "$1" ] || die "directory is not writable: $1"
}

private_directory "$root/TWASE"
private_directory "$root/TWASE/logs"
candidate_logfile="$root/TWASE/logs/twase-appstore-launch.log"
[ ! -L "$candidate_logfile" ] || die "launcher log must not be a symlink: $candidate_logfile"
[ ! -e "$candidate_logfile" ] || [ -f "$candidate_logfile" ] || die "launcher log is not a regular file: $candidate_logfile"
logfile="$candidate_logfile"
[ -n "$dylib" ] || dylib="$root/TWASE/libTWASE.dylib"
[ -f "$dylib" ] && [ -r "$dylib" ] || die "TWASE dylib not found or unreadable: $dylib"
dylib_parent="$(CDPATH= cd -P -- "$(dirname -- "$dylib")" && pwd)" || die "cannot resolve dylib directory"
dylib="$dylib_parent/$(basename -- "$dylib")"
case "$dylib" in
    *:*) die "dylib path cannot contain ':' (DYLD_INSERT_LIBRARIES separator)" ;;
esac
if /usr/bin/xattr -p com.apple.quarantine "$dylib" >/dev/null 2>&1; then
    die "dylib is quarantined: $dylib. Review its source and remove quarantine manually only if trusted."
fi
if /usr/bin/xattr -p com.apple.quarantine "$app" >/dev/null 2>&1; then
    die "prepared copy is quarantined: $app. Review it and remove quarantine manually only if trusted."
fi

private_home="$root/TWASE/appstore-home"
private_directory "$private_home"
private_directory "$private_home/Library"
private_directory "$private_home/Library/Application Support"
private_directory "$private_home/Library/Application Support/Feral Interactive"
private_directory "$private_home/Library/Application Support/Feral Interactive/Total War ATTILA"
pending_lock="$private_home/.twase-launch.lock"
/bin/mkdir -- "$pending_lock" 2>/dev/null || die "another launcher owns $pending_lock. If no game is running, remove that stale empty lock directory manually."
lock_directory="$pending_lock"
prefs="$private_home/Library/Application Support/Feral Interactive/Total War ATTILA/Preferences Data"

# The format and keys follow the existing Steam launcher, but the parent path
# was verified with an actual App Store run under HOME + CFFIXED_USER_HOME.
pref_get()
{
    count="$(/usr/bin/grep -c "<value name=\"$1\" type=\"$2\">" "$prefs")"
    [ "$count" = 1 ] || return 1
    /usr/bin/sed -n "s|.*<value name=\"$1\" type=\"$2\">\\([^<]*\\)</value>.*|\\1|p" "$prefs"
}

pref_set()
{
    replacement="$(printf '%s' "$2" | /usr/bin/sed 's/[\\|&]/\\&/g')"
    /usr/bin/sed -i '' "s|\\(<value name=\"$1\" type=\"[a-z]*\">\\)[^<]*\\(</value>\\)|\\1$replacement\\2|" "$prefs"
}

restore_preferences()
{
    restore_failed=0
    pref_set GameOptionsDialogShouldShow "$original_dialog" || restore_failed=1
    if [ -n "$save" ]; then
        pref_set ExtraCommandLineEnabled "$original_extra_enabled" || restore_failed=1
        pref_set ExtraCommandLine "$original_extra" || restore_failed=1
    fi
    [ "$restore_failed" -eq 0 ]
}

if [ "$skip_launcher" -eq 1 ]; then
    [ -f "$prefs" ] && [ ! -L "$prefs" ] || die "private Feral preferences are missing or symlinked; launch once without --skip-launcher/--load and quit normally first: $prefs"
    original_dialog="$(pref_get GameOptionsDialogShouldShow integer)" || die "private GameOptionsDialogShouldShow is missing or ambiguous; use the normal Feral UI first"
    if [ -n "$save" ]; then
        original_extra_enabled="$(pref_get ExtraCommandLineEnabled integer)" || die "private ExtraCommandLineEnabled is missing or ambiguous; use the normal Feral UI first"
        original_extra="$(pref_get ExtraCommandLine string)" || die "private ExtraCommandLine is missing or ambiguous; use the normal Feral UI first"
    fi
    preferences_changed=1
    pref_set GameOptionsDialogShouldShow 0 || die "cannot update private Feral preferences"
    if [ -n "$save" ]; then
        statements="game_startup_mode campaign_load \"$save\";"
        escaped_statements="$(printf '%s' "$statements" | /usr/bin/sed 's/&/\&amp;/g; s/</\&lt;/g; s/>/\&gt;/g')"
        pref_set ExtraCommandLineEnabled 1 || die "cannot enable private Feral command line"
        pref_set ExtraCommandLine "$escaped_statements" || die "cannot set private Feral command line"
    fi
fi

log "WARNING: launching an UNSANDBOXED, ad-hoc signed private copy (not vendor signed)"
log "app: $app; UUID: $mas_uuid"
log "TWASE_ROOT: $root; HOME and CFFIXED_USER_HOME: $private_home"
log "data: $app/Contents/Resources/Data/data"
log "injecting only: $dylib; skip launcher: $skip_launcher; only private preferences may be changed"

CDPATH= cd -- "$app/Contents/MacOS" || die "cannot enter the app executable directory"
# Clear inherited Steam/injection values: this is never a Steam launch. Set the
# DYLD variable in the shell immediately before exec, not through /usr/bin/env
# (a protected intermediary can strip it). Keep this shell to clean temporary
# files and record the game's actual exit status.
unset STEAM_DYLD_INSERT_LIBRARIES SteamAppId SteamGameId SteamOverlayGameId DYLD_LIBRARY_PATH DYLD_FRAMEWORK_PATH DYLD_FALLBACK_LIBRARY_PATH DYLD_FALLBACK_FRAMEWORK_PATH
HOME="$private_home" CFFIXED_USER_HOME="$private_home" TWASE_ROOT="$root" \
    DYLD_INSERT_LIBRARIES="$dylib" "$app/Contents/MacOS/Total War ATTILA" &
game_pid=$!
wait "$game_pid"
status=$?
game_pid=""
log "game exited with status $status"
exit "$status"
