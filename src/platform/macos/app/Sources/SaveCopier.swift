import Foundation

struct SaveEntry: Sendable, Hashable, Identifiable {
    let url: URL
    let size: Int64
    let modified: Date?
    let inPrivate: Bool

    var id: String { url.path }
    var name: String { url.lastPathComponent }
}

/// Copies selected normal Feral saves into the prepared copy's private save folder. Never overwrites.
enum SaveCopier {
    static func list(normal: URL, privateSaves: URL) -> [SaveEntry] {
        let keys: [URLResourceKey] = [.fileSizeKey, .contentModificationDateKey, .isRegularFileKey]
        guard let entries = try? FileManager.default.contentsOfDirectory(
            at: normal, includingPropertiesForKeys: keys, options: [.skipsHiddenFiles])
        else { return [] }
        return entries.compactMap { url -> SaveEntry? in
            guard url.pathExtension.lowercased() == "save",
                  let values = try? url.resourceValues(forKeys: Set(keys)), values.isRegularFile == true
            else { return nil }
            return SaveEntry(
                url: url,
                size: Int64(values.fileSize ?? 0),
                modified: values.contentModificationDate,
                inPrivate: FileKind.of(privateSaves.appendingPathComponent(url.lastPathComponent)) != .missing)
        }
        .sorted { ($0.modified ?? .distantPast) > ($1.modified ?? .distantPast) }
    }

    static func copy(_ saves: [SaveEntry], paths: LauncherPaths) -> OperationReport {
        var report = OperationReport()
        do {
            try paths.ensureDirectory(paths.privateSaves)
        } catch {
            report.problems.append(error.localizedDescription)
            return report
        }
        for save in saves {
            let destination = paths.privateSaves.appendingPathComponent(save.name)
            guard FileKind.of(destination) == .missing else {
                report.problems.append("\(save.name): already exists in the private saves; not overwritten.")
                continue
            }
            do {
                try FileManager.default.copyItem(at: save.url, to: destination)
                report.done.append(save.name)
            } catch {
                report.problems.append("\(save.name): \(error.localizedDescription)")
            }
        }
        return report
    }
}
