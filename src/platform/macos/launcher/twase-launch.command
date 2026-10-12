#!/bin/sh
# Starts Total War: ATTILA with TWASE injected. Steam must be running.
# Lives in the game root, next to "Total War ATTILA.app" and the TWASE folder.
#
# Usage: twase-launch.command [--skip-launcher] [--mods <mod_list.txt>] [--load <save>] [--extra "<statements>"] [<game> [args]]
#
#   --skip-launcher   start the game directly, without Feral's pre-launcher
#   --mods <file>     load exactly the mods in a Runcher style mod list (lines like: mod "@my_mod.pack";)
#                     instead of the mods enabled in Feral's mod manager
#   --load <save>     load a campaign save on startup, e.g. --load "Saxons 395 AD Spring.save" (implies --skip-launcher)
#   --extra <text>    append raw statements to the game's command line
#   <game> [args]     the game to start (the .app or its executable), for Steam's launch options:
#                     "<game folder>/twase-launch.command" %command%
#
# The game ignores its process arguments on macOS, so these options go through Feral's preferences
# (GameOptionsDialogShouldShow, DisableAllMods, ExtraCommandLine). They are set for this launch only, and the changed
# keys are put back when the game exits. Packs from --mods that aren't in TotalWarAttilaData/data are looked up in the
# Steam Workshop folder and linked into data for the duration of the launch.
#
# Steam's own libraries (overlay) stay injected: macOS strips DYLD_* variables on the way through /bin/sh, so they are
# taken from STEAM_DYLD_INSERT_LIBRARIES, which Steam sets for exactly that. Each launch is logged to
# TWASE/logs/twase-launch.log.

root="$(cd "$(dirname "$0")" && pwd)"
game="$root/Total War ATTILA.app/Contents/MacOS/Total War ATTILA"
dylib="$root/TWASE/libTWASE.dylib"
prefs="$HOME/Library/Application Support/Feral Interactive/Total War ATTILA/Preferences Data"

log()
{
    echo "twase-launch: $*"
    mkdir -p "$root/TWASE/logs" 2>/dev/null
    printf '%s %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$*" >> "$root/TWASE/logs/twase-launch.log" 2>/dev/null
}

die()
{
    log "error: $*" >&2
    exit 1
}

skip_launcher=0
mods_file=""
save=""
extra=""

while [ $# -gt 0 ]; do
    case "$1" in
        --skip-launcher) skip_launcher=1 ;;
        --mods) [ $# -ge 2 ] || die "--mods needs a file"; mods_file="$2"; shift ;;
        --load) [ $# -ge 2 ] || die "--load needs a save name"; save="$2"; skip_launcher=1; shift ;;
        --extra) [ $# -ge 2 ] || die "--extra needs statements"; extra="$2"; shift ;;
        -h|--help) sed -n '1d; /^#/!q; s/^# \{0,1\}//p' "$0"; exit 0 ;;
        -*) die "unknown option: $1 (see --help)" ;;
        *) break ;;
    esac
    shift
done

# the rest is the game command (Steam's %command%), anything after the game is passed to it
log "started (args: $*, from Steam: $([ -n "$SteamAppId" ] && echo yes || echo no))"
if [ $# -gt 0 ]; then
    case "$1" in
        *.app|*.app/) game="${1%/}/Contents/MacOS/Total War ATTILA" ;;
        *) game="$1" ;;
    esac
    shift
fi

[ -x "$game" ] || die "Total War ATTILA not found at: $game"
[ -f "$dylib" ] || die "TWASE not found at: $dylib"

# macOS refuses to load a downloaded (quarantined) dylib ("library load disallowed by system policy")
if xattr "$dylib" 2>/dev/null | grep -q com.apple.quarantine; then
    echo "twase-launch: removing the download quarantine from $root/TWASE"
    xattr -dr com.apple.quarantine "$root/TWASE" || die "could not remove the quarantine, run: xattr -dr com.apple.quarantine \"$root/TWASE\""
fi

# Feral writes its preferences when the game quits, never edit them while it runs
if pgrep -f "$game" >/dev/null 2>&1; then
    die "Total War ATTILA is already running"
fi

# --- Feral preferences (XML, one <value name="..." type="...">text</value> per line) ---

pref_exists()
{
    grep -q "<value name=\"$1\" type=" "$prefs"
}

# prints the stored (XML escaped) text of a value
pref_get()
{
    sed -n "s|.*<value name=\"$1\" type=\"[a-z]*\">\([^<]*\)</value>.*|\1|p" "$prefs" | head -n 1
}

# sets a value to already XML escaped text
pref_set()
{
    replacement=$(printf '%s' "$2" | sed 's/[\\|&]/\\&/g')
    sed -i '' "s|\(<value name=\"$1\" type=\"[a-z]*\">\)[^<]*\(</value>\)|\1$replacement\2|" "$prefs"
}

xml_escape()
{
    printf '%s' "$1" | sed 's/&/\&amp;/g; s/</\&lt;/g; s/>/\&gt;/g'
}

changed_keys=""

# remembers the original value once, then sets the new one
override()
{
    pref_exists "$1" || die "'$1' not found in Feral's preferences, start the game once without options first"

    case " $changed_keys " in
        *" $1 "*) ;;
        *)
            eval "original_$1=\$(pref_get \"\$1\")"
            changed_keys="$changed_keys $1"
            ;;
    esac

    pref_set "$1" "$2"
}

restore()
{
    for key in $changed_keys; do
        eval "pref_set \"\$key\" \"\$original_$key\""
    done
    changed_keys=""
}

# --- packs ---
#
# With DisableAllMods Feral empties its own mods folder, so the packs from the mod list are linked into the game's data
# folder (always searched for "mod" entries) and the links are removed again when the game exits.

data="$root/TotalWarAttilaData/data"
workshop="$(cd "$root/../../workshop/content/325610" 2>/dev/null && pwd)"
created_links=""

link_pack()
{
    # already there (game pack, manually installed mod or a link from an earlier launch)
    if [ -e "$data/$1" ] || [ -L "$data/$1" ]; then
        return 0
    fi

    source=$(find "$workshop" -mindepth 2 -maxdepth 2 -iname "$1" 2>/dev/null | head -n 1)
    [ -n "$source" ] || die "pack '$1' not found in $data or the Steam Workshop folder"

    ln -s "$source" "$data/$1" || die "could not link '$1' into $data"
    created_links="$created_links$1
"
}

unlink_packs()
{
    printf '%s' "$created_links" | while IFS= read -r pack; do
        [ -n "$pack" ] && [ -L "$data/$pack" ] && rm "$data/$pack"
    done
    created_links=""
}

statements=""

if [ -n "$mods_file" ]; then
    [ -f "$mods_file" ] || die "mod list not found: $mods_file"
    # one line per statement, joined with spaces
    statements=$(tr '\r\n' '  ' < "$mods_file" | sed 's/  */ /g; s/^ //; s/ $//')

    # mod "name"; or mod name;
    packs=$(tr ';' '\n' < "$mods_file" | sed -n 's/^[[:space:]]*mod[[:space:]][[:space:]]*"\{0,1\}\([^"]*\)"\{0,1\}[[:space:]]*$/\1/p')
    [ -n "$packs" ] || die "no 'mod' entries in $mods_file"
fi

if [ -n "$save" ]; then
    statements="$statements game_startup_mode campaign_load \"$save\";"
fi

if [ -n "$extra" ]; then
    statements="$statements $extra"
fi

if [ "$skip_launcher" -eq 1 ] || [ -n "$mods_file" ] || [ -n "$statements" ]; then
    [ -f "$prefs" ] || die "Feral's preferences not found, start the game once without options first"
    trap 'restore; unlink_packs; exit 1' INT TERM HUP
    trap 'restore; unlink_packs' EXIT

    if [ -n "$mods_file" ]; then
        # a here-document keeps the loop in this shell, so created_links survives
        while IFS= read -r pack; do
            link_pack "$pack"
        done <<EOF
$packs
EOF
    fi

    if [ "$skip_launcher" -eq 1 ]; then
        override GameOptionsDialogShouldShow 0
    fi

    # only the mods from the list, not the ones enabled in Feral's mod manager
    if [ -n "$mods_file" ]; then
        override DisableAllMods 1
    fi

    if [ -n "$statements" ]; then
        override ExtraCommandLineEnabled 1
        override ExtraCommandLine "$(xml_escape "${statements# }")"
        echo "twase-launch: extra command line: ${statements# }"
    fi
fi

# keep Steam's libraries (steamloader, overlay) in front of TWASE
steam_libs="${STEAM_DYLD_INSERT_LIBRARIES:-$DYLD_INSERT_LIBRARIES}"
case "$steam_libs" in
    *libTWASE.dylib*) inject="$steam_libs" ;;
    "") inject="$dylib" ;;
    *) inject="$steam_libs:$dylib" ;;
esac
log "starting $game with DYLD_INSERT_LIBRARIES=$inject"

cd "$root" || exit 1

if [ -z "$changed_keys" ]; then
    DYLD_INSERT_LIBRARIES="$inject" exec "$game" "$@"
fi

DYLD_INSERT_LIBRARIES="$inject" "$game" "$@"
status=$?

# restore and unlink_packs run from the EXIT trap
exit $status
