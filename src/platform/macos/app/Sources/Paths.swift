import Foundation

/// Every location the launcher reads or writes. Writable state lives below `root` (the TWASE root);
/// the normal Feral folders are only ever read from.
struct LauncherPaths: Sendable {
    static let steamUUID = "392D6F66-9183-329E-8A98-8C90FC828326"
    static let appStoreUUID = "196C74A0-71A2-3C28-80DF-BC3DB1C7FEB7"

    static let home = URL(fileURLWithPath: NSHomeDirectory(), isDirectory: true)
    static let steamDefaultApp = home.appendingPathComponent(
        "Library/Application Support/Steam/steamapps/common/Total War Attila/Total War ATTILA.app", isDirectory: true)
    static let appStoreOriginal = URL(fileURLWithPath: "/Applications/Total War ATTILA.app", isDirectory: true)
    static let userApplications = home.appendingPathComponent("Applications", isDirectory: true)
    static let preparedDefault = userApplications.appendingPathComponent("Total War ATTILA (TWASE).app", isDirectory: true)

    /// Feral's normal (Steam) preferences tree. Read only.
    static let normalFeral = home.appendingPathComponent(
        "Library/Application Support/Feral Interactive/Total War ATTILA", isDirectory: true)
    static let normalPrefs = normalFeral.appendingPathComponent("Preferences Data")
    static let normalLocalMods = feralLocalMods(in: normalFeral)
    static let normalSaves = feralSaves(in: normalFeral)

    static func feralLocalMods(in feral: URL) -> URL {
        feral.appendingPathComponent("VFS/Local/local_mods", isDirectory: true)
    }

    static func feralSaves(in feral: URL) -> URL {
        feral.appendingPathComponent(
            "VFS/User/AppData/Roaming/The Creative Assembly/Attila/save_games", isDirectory: true)
    }

    /// The TWASE root: the directory that contains `TWASE/`.
    let root: URL

    var twaseDir: URL { root.appendingPathComponent("TWASE", isDirectory: true) }
    var dylib: URL { twaseDir.appendingPathComponent("libTWASE.dylib") }
    var appStoreScript: URL { twaseDir.appendingPathComponent(BundledBackend.appStoreScriptName) }
    var steamScript: URL { twaseDir.appendingPathComponent(BundledBackend.steamScriptName) }
    var versionMarker: URL { twaseDir.appendingPathComponent(BundledBackend.versionMarkerName) }
    var logs: URL { twaseDir.appendingPathComponent("logs", isDirectory: true) }
    var config: URL { twaseDir.appendingPathComponent("config.ini") }

    /// HOME of the prepared App Store copy (set by twase-appstore.command).
    var privateHome: URL { twaseDir.appendingPathComponent("appstore-home", isDirectory: true) }
    var privateFeral: URL {
        privateHome.appendingPathComponent(
            "Library/Application Support/Feral Interactive/Total War ATTILA", isDirectory: true)
    }
    var privatePrefs: URL { privateFeral.appendingPathComponent("Preferences Data") }
    var privateLocalMods: URL { Self.feralLocalMods(in: privateFeral) }
    var privateSaves: URL { Self.feralSaves(in: privateFeral) }

    /// `TWASE_LAUNCHER_ROOT` (developer only) overrides `~/Library/Application Support/TWASE`.
    static func current() -> LauncherPaths {
        let root: URL
        if let override = ProcessInfo.processInfo.environment["TWASE_LAUNCHER_ROOT"], !override.isEmpty {
            root = URL(fileURLWithPath: override, isDirectory: true)
        } else {
            root = home.appendingPathComponent("Library/Application Support/TWASE", isDirectory: true)
        }
        return LauncherPaths(root: root.standardizedFileURL)
    }

    /// Creates `directory` (and missing parents) strictly below the TWASE root. Existing symlinks along the way are
    /// refused, so a link can never redirect writes into a real Feral preferences tree.
    func ensureDirectory(_ directory: URL) throws {
        let fm = FileManager.default
        if !fm.fileExists(atPath: root.path) {
            try fm.createDirectory(at: root, withIntermediateDirectories: true)
        }
        let rootComponents = root.standardizedFileURL.pathComponents
        let components = directory.standardizedFileURL.pathComponents
        guard components.count >= rootComponents.count, Array(components.prefix(rootComponents.count)) == rootComponents
        else {
            throw LauncherError("Refusing to create \(directory.path): it is outside the TWASE root \(root.path).")
        }
        var current = root
        for component in components.dropFirst(rootComponents.count) {
            current.appendPathComponent(component, isDirectory: true)
            switch FileKind.of(current) {
            case .directory:
                continue
            case .missing:
                try fm.createDirectory(at: current, withIntermediateDirectories: false)
            case .symlink:
                throw LauncherError("Refusing to use \(current.path): it is a symbolic link.")
            case .other:
                throw LauncherError("Refusing to use \(current.path): it exists and is not a folder.")
            }
        }
    }
}

/// `lstat` based classification (does not follow symlinks).
enum FileKind {
    case missing, directory, symlink, other

    static func of(_ url: URL) -> FileKind {
        var info = stat()
        guard lstat(url.path, &info) == 0 else { return .missing }
        switch info.st_mode & S_IFMT {
        case S_IFDIR: return .directory
        case S_IFLNK: return .symlink
        default: return .other
        }
    }
}

struct LauncherError: LocalizedError, Sendable {
    let message: String
    init(_ message: String) { self.message = message }
    var errorDescription: String? { message }
}

enum Quarantine {
    static func isQuarantined(_ url: URL) -> Bool {
        getxattr(url.path, "com.apple.quarantine", nil, 0, 0, XATTR_NOFOLLOW) >= 0
    }

    /// Guidance only; the launcher clears quarantine solely on the backend copies it installs from its own bundle.
    static func guidance(for urls: [URL]) -> String {
        let commands = urls.map { url -> String in
            let flag = FileKind.of(url) == .directory ? "-dr" : "-d"
            return "xattr \(flag) com.apple.quarantine '\(url.path.replacingOccurrences(of: "'", with: "'\\''"))'"
        }
        return """
            macOS marked these files as downloaded from the internet (quarantine), so the game would refuse to load \
            TWASE. The launcher only clears this on the files it installs itself. Only if you trust where you got \
            these files from, run in Terminal:

            \(commands.joined(separator: "\n"))
            """
    }
}
