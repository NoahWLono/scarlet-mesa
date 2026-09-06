#!/usr/bin/env bash
# Build first with -DSCARLET_BUNDLE_SDL=ON, then package the native executable.
set -euo pipefail
project_dir="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="${1:-$project_dir/build}"
output_dir="${2:-$project_dir/dist}"
build_dir="$(cd "$build_dir" && pwd)"
mkdir -p "$output_dir"
output_dir="$(cd "$output_dir" && pwd)"
version="${SCARLET_VERSION:-1.0.0}"
if [[ ! "$version" =~ ^[A-Za-z0-9._-]+$ ]]; then
    echo "SCARLET_VERSION must contain only letters, digits, dots, underscores, or hyphens." >&2
    exit 1
fi
machine="$(uname -m)"
case "$(uname -s)" in
    Linux) platform=linux ;;
    Darwin) platform=macos ;;
    *) echo "This packager supports Linux and macOS." >&2; exit 1 ;;
esac
staging="$(mktemp -d "$output_dir/.scarlet-package.XXXXXX")"
trap 'rm -rf "$staging"' EXIT
cmake --install "$build_dir" --prefix "$staging/install" --config Release
executable="$staging/install/scarlet-mesa"
if [[ ! -x "$executable" ]]; then
    echo "No scarlet-mesa executable found; build the Release target first." >&2
    exit 1
fi
if [[ ! -f "$staging/install/SDL-LICENSE.txt" ]]; then
    echo "A distributable must bundle SDL. Reconfigure with -DSCARLET_BUNDLE_SDL=ON." >&2
    exit 1
fi

if [[ "$platform" == macos ]]; then
    architectures="$(lipo -archs "$executable")"
    if [[ "$architectures" == *arm64* && "$architectures" == *x86_64* ]]; then
        machine=universal
    fi
fi
name="scarlet-mesa-$version-$platform-$machine"

mkdir -p "$staging/$name"
if [[ "$platform" == macos ]]; then
    if otool -L "$executable" | grep -Eq 'SDL[23].*(dylib|framework)'; then
        echo "External SDL detected. Reconfigure with -DSCARLET_BUNDLE_SDL=ON." >&2
        exit 1
    fi
    app="$staging/$name/Scarlet Mesa.app"
    mkdir -p "$app/Contents/MacOS" "$app/Contents/Resources"
    cp "$executable" "$app/Contents/MacOS/scarlet-mesa"
    cp -R "$staging/install/assets" "$app/Contents/Resources/assets"
    for file in "$staging/install/"*.md "$staging/install/"*LICENSE*; do
        [[ -f "$file" ]] && cp "$file" "$staging/$name/"
    done
    [[ ! -d "$staging/install/docs" ]] || cp -R "$staging/install/docs" "$staging/$name/docs"
    cat > "$app/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>CFBundleName</key><string>Scarlet Mesa</string>
<key>CFBundleDisplayName</key><string>Scarlet Mesa</string>
<key>CFBundleIdentifier</key><string>games.scarletmesa.fangame</string>
<key>CFBundleExecutable</key><string>scarlet-mesa</string>
<key>CFBundlePackageType</key><string>APPL</string>
<key>CFBundleShortVersionString</key><string>$version</string>
<key>CFBundleVersion</key><string>$version</string>
<key>LSMinimumSystemVersion</key><string>11.0</string>
<key>NSHighResolutionCapable</key><true/>
<key>NSHumanReadableCopyright</key><string>Free Touhou fan game. Touhou Project belongs to Team Shanghai Alice.</string>
</dict></plist>
PLIST
    # Ad-hoc signing supplies a valid local signature; this is not notarization.
    codesign --force --deep --sign - "$app"
    codesign --verify --deep --strict "$app"
else
    if ldd "$executable" | grep -Eq 'libSDL[23]'; then
        echo "External SDL detected. Reconfigure with -DSCARLET_BUNDLE_SDL=ON." >&2
        exit 1
    fi
    cp -R "$staging/install/." "$staging/$name/"
    strip "$staging/$name/scarlet-mesa"
fi

archive="$output_dir/$name.tar.gz"
tar -czf "$archive" -C "$staging" "$name"
(
    cd "$output_dir"
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$name.tar.gz" > "$name.tar.gz.sha256"
    else
        shasum -a 256 "$name.tar.gz" > "$name.tar.gz.sha256"
    fi
)
printf 'Package: %s\n' "$archive"
