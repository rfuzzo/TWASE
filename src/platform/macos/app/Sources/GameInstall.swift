import Foundation

enum InstallKind: Sendable, Hashable {
    case steam
    case appStoreOriginal
    case appStorePrepared
    case unsupported

    var title: String {
        switch self {
        case .steam: return "Steam"
        case .appStoreOriginal: return "App Store (original)"
        case .appStorePrepared: return "App Store (prepared TWASE copy)"
        case .unsupported: return "Unsupported"
        }
    }

    var isAppStore: Bool { self == .appStoreOriginal || self == .appStorePrepared }
}

struct GameInstall: Sendable, Hashable, Identifiable {
    let url: URL
    let kind: InstallKind
    let uuid: String?
    let version: String?
    /// Why the install is unsupported, or a short note.
    let note: String?

    var id: String { url.path }
    /// Steam: the folder that contains the .app, the TotalWarAttilaData folder and the TWASE backend.
    var gameRoot: URL { url.deletingLastPathComponent() }

    var label: String {
        "\(kind.title)\(version.map { " \($0)" } ?? "") — \(url.path)"
    }
}

enum InstallInspector {
    /// Classifies a game bundle by the exact arm64 LC_UUID of its executable. Blocking; call off the main actor.
    static func inspect(_ url: URL) -> GameInstall {
        let url = url.standardizedFileURL
        func unsupported(_ note: String, uuid: String? = nil, version: String? = nil) -> GameInstall {
            GameInstall(url: url, kind: .unsupported, uuid: uuid, version: version, note: note)
        }

        var isDirectory: ObjCBool = false
        guard FileManager.default.fileExists(atPath: url.path, isDirectory: &isDirectory), isDirectory.boolValue else {
            return unsupported("Not found.")
        }
        let info = NSDictionary(contentsOf: url.appendingPathComponent("Contents/Info.plist")) as? [String: Any] ?? [:]
        let version = versionString(info)
        let executableName = info["CFBundleExecutable"] as? String ?? "Total War ATTILA"
        let executable = url.appendingPathComponent("Contents/MacOS").appendingPathComponent(executableName)

        let uuid: String?
        do {
            uuid = try MachO.arm64UUID(of: executable)
        } catch {
            return unsupported("Cannot read the game executable: \(error.localizedDescription)", version: version)
        }
        switch uuid {
        case LauncherPaths.steamUUID?:
            return GameInstall(url: url, kind: .steam, uuid: uuid, version: version, note: nil)
        case LauncherPaths.appStoreUUID?:
            if isAdHocSigned(url) {
                return GameInstall(url: url, kind: .appStorePrepared, uuid: uuid, version: version, note: nil)
            }
            return GameInstall(url: url, kind: .appStoreOriginal, uuid: uuid, version: version, note: nil)
        case nil:
            return unsupported("The executable has no arm64 build identity (UUID).", version: version)
        case let other?:
            return unsupported(
                "Unknown build \(other). TWASE supports only Steam \(LauncherPaths.steamUUID) and "
                    + "App Store \(LauncherPaths.appStoreUUID).",
                uuid: other, version: version)
        }
    }

    /// A prepared copy is re-signed ad hoc by twase-appstore.command; the App Store original is vendor signed.
    static func isAdHocSigned(_ app: URL) -> Bool {
        let result = ProcessRunner.capture("/usr/bin/codesign", ["-dv", app.path])
        guard result.status == 0 else { return false }
        return result.output.split(whereSeparator: \.isNewline).contains { $0 == "Signature=adhoc" }
    }

    private static func versionString(_ info: [String: Any]) -> String? {
        let short = info["CFBundleShortVersionString"] as? String
        let build = info["CFBundleVersion"] as? String
        switch (short, build) {
        case let (short?, build?) where short != build: return "\(short) (\(build))"
        case let (short?, _): return short
        case let (nil, build?): return build
        default: return nil
        }
    }

    /// Inspects the default locations and remembered ones, plus prepared copies found in ~/Applications
    /// (other bundles there, such as Steam shortcuts, are ignored). Blocking; call off the main actor.
    static func discover(remembered: [String]) -> [GameInstall] {
        let fm = FileManager.default
        let explicit = [LauncherPaths.steamDefaultApp, LauncherPaths.appStoreOriginal, LauncherPaths.preparedDefault]
            + remembered.map { URL(fileURLWithPath: $0, isDirectory: true) }
        var scanned: [URL] = []
        if let names = try? fm.contentsOfDirectory(atPath: LauncherPaths.userApplications.path) {
            for name in names.sorted() where name.hasPrefix("Total War ATTILA") && name.hasSuffix(".app") {
                scanned.append(LauncherPaths.userApplications.appendingPathComponent(name, isDirectory: true))
            }
        }

        var seen = Set<String>()
        var result: [GameInstall] = []
        for url in explicit.map(\.standardizedFileURL)
        where fm.fileExists(atPath: url.path) && seen.insert(url.path).inserted {
            result.append(inspect(url))
        }
        for url in scanned.map(\.standardizedFileURL) where seen.insert(url.path).inserted {
            let install = inspect(url)
            if install.kind == .appStorePrepared { result.append(install) }
        }
        return result
    }
}
