import Foundation

enum LinkState: Sendable, Hashable {
    case notLinked
    case linked
    /// Something else already occupies the private entry.
    case conflict

    var title: String {
        switch self {
        case .notLinked: return "Not linked"
        case .linked: return "Linked"
        case .conflict: return "Conflict"
        }
    }
}

struct PackEntry: Sendable, Hashable, Identifiable {
    /// The pack file with all symlinks resolved; links always point here.
    let realURL: URL
    let name: String
    /// Name of the folder the pack was found in; the private link goes to `local_mods/<folder>/<name>`.
    let folder: String
    let source: URL
    let size: Int64
    let state: LinkState

    var id: String { realURL.path }
}

struct OperationReport: Sendable {
    var done: [String] = []
    var problems: [String] = []

    func summary(verb: String) -> String {
        var text = "\(verb): \(done.count)"
        if !problems.isEmpty {
            text += "\n\nNot done:\n" + problems.joined(separator: "\n")
        }
        return text
    }
}

/// Makes packs from other folders visible to the prepared App Store copy by symlinking them into its private
/// local_mods folder. Packs are never copied, and only symlinks are ever removed.
enum ModLinker {
    static func scan(sources: [URL], privateLocalMods: URL) -> [PackEntry] {
        var seen = Set<String>()
        var result: [PackEntry] = []
        for source in sources {
            for found in packs(in: source) {
                let real = found.resolvingSymlinksInPath()
                guard seen.insert(real.path).inserted else { continue }
                let folder = found.deletingLastPathComponent().lastPathComponent
                let size = (try? FileManager.default.attributesOfItem(atPath: real.path))?[.size] as? NSNumber
                result.append(PackEntry(
                    realURL: real,
                    name: found.lastPathComponent,
                    folder: folder,
                    source: source,
                    size: size?.int64Value ?? 0,
                    state: state(of: privateLocalMods, folder: folder, name: found.lastPathComponent, real: real)))
            }
        }
        return result.sorted { ($0.folder, $0.name) < ($1.folder, $1.name) }
    }

    /// `<source>/*.pack` and `<source>/*/*.pack`.
    private static func packs(in source: URL) -> [URL] {
        let fm = FileManager.default
        guard let entries = try? fm.contentsOfDirectory(
            at: source, includingPropertiesForKeys: nil, options: [.skipsHiddenFiles])
        else { return [] }
        var result: [URL] = []
        for entry in entries {
            if isPack(entry) {
                result.append(entry)
            } else if isDirectory(entry) {
                let nested = (try? fm.contentsOfDirectory(
                    at: entry, includingPropertiesForKeys: nil, options: [.skipsHiddenFiles])) ?? []
                result += nested.filter(isPack)
            }
        }
        return result
    }

    private static func isPack(_ url: URL) -> Bool {
        guard url.pathExtension.lowercased() == "pack" else { return false }
        var isDir: ObjCBool = false
        return FileManager.default.fileExists(atPath: url.path, isDirectory: &isDir) && !isDir.boolValue
    }

    private static func isDirectory(_ url: URL) -> Bool {
        var isDir: ObjCBool = false
        return FileManager.default.fileExists(atPath: url.path, isDirectory: &isDir) && isDir.boolValue
    }

    private static func state(of privateLocalMods: URL, folder: String, name: String, real: URL) -> LinkState {
        let entry = privateLocalMods.appendingPathComponent(folder).appendingPathComponent(name)
        switch FileKind.of(entry) {
        case .missing: return .notLinked
        case .symlink: return entry.resolvingSymlinksInPath().path == real.path ? .linked : .conflict
        case .directory, .other: return .conflict
        }
    }

    static func link(_ packs: [PackEntry], paths: LauncherPaths) -> OperationReport {
        var report = OperationReport()
        for pack in packs {
            guard pack.folder != ".", pack.folder != "..", !pack.folder.isEmpty else {
                report.problems.append("\(pack.name): unusable folder name \"\(pack.folder)\".")
                continue
            }
            let folder = paths.privateLocalMods.appendingPathComponent(pack.folder, isDirectory: true)
            let entry = folder.appendingPathComponent(pack.name)
            switch FileKind.of(entry) {
            case .missing:
                break
            case .symlink where entry.resolvingSymlinksInPath().path == pack.realURL.path:
                report.done.append("\(pack.folder)/\(pack.name) (already linked)")
                continue
            default:
                report.problems.append(
                    "\(pack.folder)/\(pack.name): a different file or link already exists at \(entry.path); not replaced.")
                continue
            }
            do {
                try paths.ensureDirectory(folder)
                try FileManager.default.createSymbolicLink(at: entry, withDestinationURL: pack.realURL)
                report.done.append("\(pack.folder)/\(pack.name)")
            } catch {
                report.problems.append("\(pack.folder)/\(pack.name): \(error.localizedDescription)")
            }
        }
        return report
    }

    static func unlink(_ packs: [PackEntry], paths: LauncherPaths) -> OperationReport {
        var report = OperationReport()
        for pack in packs {
            let entry = paths.privateLocalMods.appendingPathComponent(pack.folder).appendingPathComponent(pack.name)
            switch FileKind.of(entry) {
            case .missing:
                continue
            case .symlink:
                do {
                    try FileManager.default.removeItem(at: entry)
                    report.done.append("\(pack.folder)/\(pack.name)")
                } catch {
                    report.problems.append("\(pack.folder)/\(pack.name): \(error.localizedDescription)")
                }
            case .directory, .other:
                report.problems.append("\(pack.folder)/\(pack.name): \(entry.path) is not a symbolic link; left alone.")
            }
        }
        return report
    }
}
