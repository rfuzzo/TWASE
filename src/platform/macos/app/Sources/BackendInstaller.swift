import Foundation

/// The backend scripts and dylib shipped in the launcher's Resources. They are always copied to a stable location
/// before use, so it does not matter where the launcher itself runs from (including App Translocation).
struct BundledBackend: Sendable {
    static let appStoreScriptName = "twase-appstore.command"
    static let steamScriptName = "twase-launch.command"
    static let dylibName = "libTWASE.dylib"
    static let versionMarkerName = "launcher-backend-version"

    let dylib: URL
    let appStoreScript: URL
    let steamScript: URL
    let version: String

    static func locate() throws -> BundledBackend {
        guard let resources = Bundle.main.resourceURL else {
            throw LauncherError("The launcher has no Resources folder.")
        }
        let backend = BundledBackend(
            dylib: resources.appendingPathComponent(dylibName),
            appStoreScript: resources.appendingPathComponent(appStoreScriptName),
            steamScript: resources.appendingPathComponent(steamScriptName),
            version: Bundle.main.object(forInfoDictionaryKey: "TWASEVersion") as? String ?? "unknown")
        for file in [backend.dylib, backend.appStoreScript, backend.steamScript]
        where !FileManager.default.isReadableFile(atPath: file.path) {
            throw LauncherError("The launcher bundle is incomplete, missing \(file.lastPathComponent). Rebuild or re-download it.")
        }
        return backend
    }
}

enum SteamBackendState: Sendable, Equatable {
    case notInstalled
    case outdated
    case current
}

enum BackendInstaller {
    /// Brings `<TWASE root>/TWASE/` up to date with the bundled backend. Unchanged files are left alone.
    static func installIntoRoot(_ bundled: BundledBackend, paths: LauncherPaths) throws {
        try paths.ensureDirectory(paths.twaseDir)
        try paths.ensureDirectory(paths.logs)
        try install(bundled.dylib, to: paths.dylib)
        try install(bundled.appStoreScript, to: paths.appStoreScript, executable: true)
        try install(bundled.steamScript, to: paths.steamScript, executable: true)
        try writeMarker(bundled.version, to: paths.versionMarker)
    }

    static func installedVersion(at marker: URL) -> String? {
        (try? String(contentsOf: marker, encoding: .utf8))?.trimmingCharacters(in: .whitespacesAndNewlines)
    }

    static func steamState(_ bundled: BundledBackend, gameRoot: URL) -> SteamBackendState {
        let script = gameRoot.appendingPathComponent(BundledBackend.steamScriptName)
        let dylib = gameRoot.appendingPathComponent("TWASE").appendingPathComponent(BundledBackend.dylibName)
        let fm = FileManager.default
        guard fm.fileExists(atPath: script.path), fm.fileExists(atPath: dylib.path) else { return .notInstalled }
        return sameContents(bundled.steamScript, script) && sameContents(bundled.dylib, dylib) ? .current : .outdated
    }

    /// Explicit user action only: installs `twase-launch.command` and `TWASE/libTWASE.dylib` into the Steam game root.
    static func installIntoSteam(_ bundled: BundledBackend, gameRoot: URL) throws {
        let twase = gameRoot.appendingPathComponent("TWASE", isDirectory: true)
        switch FileKind.of(twase) {
        case .missing: try FileManager.default.createDirectory(at: twase, withIntermediateDirectories: false)
        case .directory: break
        case .symlink, .other: throw LauncherError("\(twase.path) exists but is not a folder.")
        }
        try install(bundled.dylib, to: twase.appendingPathComponent(BundledBackend.dylibName))
        try install(bundled.steamScript, to: gameRoot.appendingPathComponent(BundledBackend.steamScriptName), executable: true)
        try writeMarker(bundled.version, to: twase.appendingPathComponent(BundledBackend.versionMarkerName))
    }

    /// Copies to a temporary file in the destination folder and renames it over the destination. A dylib must never be
    /// overwritten in place: the kernel caches code signatures per inode, and a running game may still map the old one.
    private static func install(_ source: URL, to destination: URL, executable: Bool = false) throws {
        if FileKind.of(destination) == .other, sameContents(source, destination) {
            try clearQuarantine(destination)
            return
        }
        if FileKind.of(destination) == .directory || FileKind.of(destination) == .symlink {
            throw LauncherError("Refusing to replace \(destination.path): it is a folder or symbolic link.")
        }
        let fm = FileManager.default
        let temporary = destination.deletingLastPathComponent()
            .appendingPathComponent(".\(destination.lastPathComponent).\(UUID().uuidString).tmp")
        do {
            try fm.copyItem(at: source, to: temporary)
            try fm.setAttributes([.posixPermissions: executable ? 0o755 : 0o644], ofItemAtPath: temporary.path)
            try clearQuarantine(temporary)
            guard rename(temporary.path, destination.path) == 0 else {
                throw LauncherError("Cannot replace \(destination.path): \(String(cString: strerror(errno)))")
            }
        } catch {
            try? fm.removeItem(at: temporary)
            throw error
        }
    }

    /// The source files are sealed resources of this signed launcher, which the user already approved through
    /// Gatekeeper; copies inherit the download's quarantine flag, which the backends refuse. Clear it on these copies
    /// only, never on the game or on files the launcher did not install.
    private static func clearQuarantine(_ url: URL) throws {
        if removexattr(url.path, "com.apple.quarantine", XATTR_NOFOLLOW) != 0 && errno != ENOATTR {
            throw LauncherError("Cannot clear the download quarantine on \(url.path): \(String(cString: strerror(errno)))")
        }
    }

    private static func writeMarker(_ version: String, to marker: URL) throws {
        if installedVersion(at: marker) == version { return }
        try Data((version + "\n").utf8).write(to: marker, options: .atomic)
    }

    static func sameContents(_ a: URL, _ b: URL) -> Bool {
        let fm = FileManager.default
        guard let sizeA = (try? fm.attributesOfItem(atPath: a.path))?[.size] as? NSNumber,
              let sizeB = (try? fm.attributesOfItem(atPath: b.path))?[.size] as? NSNumber,
              sizeA == sizeB
        else { return false }
        return fm.contentsEqual(atPath: a.path, andPath: b.path)
    }
}
