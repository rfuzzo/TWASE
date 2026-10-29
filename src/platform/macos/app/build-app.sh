#!/bin/sh
# Builds "TWASE Launcher.app" (arm64, macOS 14+) with swiftc, bundles the backend scripts and libTWASE.dylib, and
# ad-hoc signs it. No Xcode project is needed; Xcode or the Command Line Tools must provide swiftc and the macOS SDK.
#
# Usage: build-app.sh --dylib <libTWASE.dylib> --output <directory> [--version <x.y.z>]
#   --version defaults to TWASE_VERSION_STR from src/core/Version.hpp.

set -eu

die()
{
    printf 'build-app: error: %s\n' "$*" >&2
    exit 1
}

usage()
{
    sed -n '2,7s/^# \{0,1\}//p' "$0"
}

dylib=""
output=""
version=""
while [ "$#" -gt 0 ]; do
    case "$1" in
        --dylib) [ "$#" -ge 2 ] || die "--dylib needs a path"; dylib="$2"; shift ;;
        --output) [ "$#" -ge 2 ] || die "--output needs a directory"; output="$2"; shift ;;
        --version) [ "$#" -ge 2 ] || die "--version needs a value"; version="$2"; shift ;;
        -h|--help) usage; exit 0 ;;
        *) die "unknown argument: $1 (see --help)" ;;
    esac
    shift
done

[ -n "$dylib" ] || die "--dylib <libTWASE.dylib> is required"
[ -n "$output" ] || die "--output <directory> is required"
[ -f "$dylib" ] || die "dylib not found: $dylib"

app_dir="$(CDPATH= cd -P -- "$(dirname -- "$0")" && pwd)"
repo="$(CDPATH= cd -P -- "$app_dir/../../../.." && pwd)"
launcher_dir="$repo/src/platform/macos/launcher"
for script in twase-appstore.command twase-launch.command; do
    [ -f "$launcher_dir/$script" ] || die "backend script not found: $launcher_dir/$script"
done
[ -f "$app_dir/Info.plist.in" ] || die "Info.plist template not found: $app_dir/Info.plist.in"

if [ -z "$version" ]; then
    version_header="$repo/src/core/Version.hpp"
    [ -f "$version_header" ] || die "cannot find $version_header; pass --version"
    version="$(sed -n 's/^#define TWASE_VERSION_STR "\(.*\)"$/\1/p' "$version_header")"
    [ -n "$version" ] || die "TWASE_VERSION_STR not found in $version_header; pass --version"
fi
case "$version" in
    *[!0-9A-Za-z.+-]*) die "version may only contain letters, digits, '.', '+' and '-': $version" ;;
esac

command -v xcrun >/dev/null 2>&1 || die "xcrun not found; install Xcode or the Command Line Tools"
swiftc="$(xcrun --find swiftc 2>/dev/null)" || die "swiftc not found; install Xcode or the Command Line Tools"
[ -x /usr/bin/codesign ] || die "codesign not found"

set -- "$app_dir"/Sources/*.swift
[ -f "$1" ] || die "no Swift sources in $app_dir/Sources"

mkdir -p "$output" || die "cannot create output directory: $output"
output="$(CDPATH= cd -P -- "$output" && pwd)"
app="$output/TWASE Launcher.app"
contents="$app/Contents"
work="$(mktemp -d "${TMPDIR:-/tmp}/twase-launcher-build.XXXXXX")"
trap 'rm -rf "$work"' EXIT

# Only ever replace a previous build of this bundle.
if [ -e "$app" ]; then
    [ -f "$contents/Info.plist" ] && /usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$contents/Info.plist" 2>/dev/null \
        | grep -qx 'io.github.rfuzzo.twase.launcher' || die "refusing to replace $app: it is not a TWASE Launcher build"
    rm -rf "$app"
fi
mkdir -p "$contents/MacOS" "$contents/Resources"

echo "build-app: compiling TWASE Launcher $version"
"$swiftc" -O -swift-version 5 -target arm64-apple-macos14 -parse-as-library \
    -sdk "$(xcrun --sdk macosx --show-sdk-path)" \
    -module-name TWASELauncher -module-cache-path "$work/module-cache" \
    -o "$contents/MacOS/TWASE Launcher" "$@" || die "swiftc failed"

sed "s/@TWASE_VERSION@/$version/g" "$app_dir/Info.plist.in" > "$contents/Info.plist"
plutil -lint -s "$contents/Info.plist" || die "generated Info.plist is invalid"
printf 'APPL????' > "$contents/PkgInfo"

for script in twase-appstore.command twase-launch.command; do
    cp "$launcher_dir/$script" "$contents/Resources/$script"
    chmod 755 "$contents/Resources/$script"
done
cp "$dylib" "$contents/Resources/libTWASE.dylib"
chmod 755 "$contents/Resources/libTWASE.dylib"

# --deep only signs nested code in code locations, not Resources, so sign the dylib explicitly first.
/usr/bin/codesign --force --sign - "$contents/Resources/libTWASE.dylib" || die "signing libTWASE.dylib failed"
/usr/bin/codesign --force --deep --sign - "$app" || die "signing the app failed"
/usr/bin/codesign --verify --deep --strict "$app" || die "signature verification failed"

echo "build-app: built $app"
