import SwiftUI

private func byteString(_ size: Int64) -> String {
    ByteCountFormatter.string(fromByteCount: size, countStyle: .file)
}

struct ModsView: View {
    @ObservedObject var model: AppModel

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            if model.isAppStoreMode {
                Text("The private copy only sees packs in its own mod folder. Link packs there (symbolic links, nothing is copied), then enable and order them in the Mod Manager tab of the Feral options window.")
                    .fixedSize(horizontal: false, vertical: true)
                sources
                Table(model.packs, selection: $model.selectedPacks) {
                    TableColumn("Pack", value: \.name)
                    TableColumn("Folder", value: \.folder)
                    TableColumn("Size") { pack in Text(byteString(pack.size)) }
                        .width(min: 60, ideal: 80)
                    TableColumn("State") { pack in
                        Text(pack.state.title)
                            .foregroundStyle(pack.state == .conflict ? Color.red : (pack.state == .linked ? Color.green : Color.primary))
                    }
                    .width(min: 70, ideal: 90)
                    TableColumn("Location") { pack in
                        Text(pack.realURL.deletingLastPathComponent().path).help(pack.realURL.path)
                    }
                }
                HStack {
                    Button("Link selected") { model.linkSelectedPacks() }
                        .disabled(model.selectedPacks.isEmpty)
                    Button("Unlink selected") { model.unlinkSelectedPacks() }
                        .disabled(model.selectedPacks.isEmpty)
                    Spacer()
                    Button("Open private mods") { model.openRootFolder(model.paths.privateLocalMods) }
                    Button("Refresh") { model.refreshPacks() }
                }
                Text("Links go to \(model.paths.privateLocalMods.path)/<folder>/. Existing different files are never replaced; Unlink only removes symbolic links.")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            } else {
                Text("Steam uses Feral's normal mod folder. Manage mods in the Mod Manager tab of the Feral options window.")
                    .foregroundStyle(.secondary)
                Spacer()
            }
        }
        .padding()
    }

    private var sources: some View {
        GroupBox("Pack sources") {
            VStack(alignment: .leading, spacing: 4) {
                Text("\(LauncherPaths.normalLocalMods.path) (Feral local mods)")
                    .font(.callout)
                ForEach(model.modSources, id: \.self) { path in
                    HStack {
                        Text(path).font(.callout)
                        Spacer()
                        Button("Remove") { model.removeModSource(path) }
                            .controlSize(.small)
                    }
                }
                HStack {
                    Button("Add folder…") { model.addModSource() }
                    if let data = model.steamDataFolder, !model.modSources.contains(data.standardizedFileURL.path) {
                        Button("Add Steam game data folder") { model.addModSource(data) }
                            .help(data.path)
                    }
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            .padding(4)
        }
    }
}

struct SavesView: View {
    @ObservedObject var model: AppModel

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            if model.isAppStoreMode {
                Text("The private copy has its own saves. Copy selected saves from Feral's normal save folder; existing private saves are never overwritten.")
                    .fixedSize(horizontal: false, vertical: true)
                Text("From: \(LauncherPaths.normalSaves.path)")
                    .font(.caption)
                    .foregroundStyle(.secondary)
                Table(model.saves, selection: $model.selectedSaves) {
                    TableColumn("Save", value: \.name)
                    TableColumn("Modified") { save in
                        Text(save.modified.map { $0.formatted(date: .abbreviated, time: .shortened) } ?? "")
                    }
                    .width(min: 120, ideal: 160)
                    TableColumn("Size") { save in Text(byteString(save.size)) }
                        .width(min: 60, ideal: 80)
                    TableColumn("In private saves") { save in Text(save.inPrivate ? "Yes" : "—") }
                        .width(min: 90, ideal: 110)
                }
                HStack {
                    Button("Copy to private saves") { model.copySelectedSaves() }
                        .disabled(model.selectedSaves.isEmpty)
                    Spacer()
                    Button("Open private saves") { model.openRootFolder(model.paths.privateSaves) }
                    Button("Refresh") { model.refreshSaves() }
                }
            } else {
                Text("Steam uses Feral's normal save folder; nothing to copy.")
                    .foregroundStyle(.secondary)
                Spacer()
            }
        }
        .padding()
    }
}

struct FoldersView: View {
    @ObservedObject var model: AppModel

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            GroupBox("TWASE (App Store copy and launcher backend)") {
                Grid(alignment: .leading, horizontalSpacing: 12, verticalSpacing: 6) {
                    row("TWASE root", model.paths.root) { model.openRootFolder(model.paths.root) }
                    row("Logs", model.paths.logs) { model.openRootFolder(model.paths.logs) }
                    row("Config", model.paths.config) { model.openConfig() }
                    row("Private saves", model.paths.privateSaves) { model.openRootFolder(model.paths.privateSaves) }
                    row("Private mods", model.paths.privateLocalMods) { model.openRootFolder(model.paths.privateLocalMods) }
                }
                .frame(maxWidth: .infinity, alignment: .leading)
                .padding(4)
            }
            if let steam = model.installs.first(where: { $0.kind == .steam }) {
                GroupBox("Steam") {
                    Grid(alignment: .leading, horizontalSpacing: 12, verticalSpacing: 6) {
                        let twase = steam.gameRoot.appendingPathComponent("TWASE", isDirectory: true)
                        row("TWASE folder", twase) { model.openExisting(twase) }
                        row("Feral data", LauncherPaths.normalFeral) { model.openExisting(LauncherPaths.normalFeral) }
                    }
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .padding(4)
                }
            }
            Spacer()
        }
        .padding()
    }

    private func row(_ title: String, _ url: URL, open: @escaping () -> Void) -> some View {
        GridRow {
            Button("Open \(title)", action: open)
            Text(url.path)
                .font(.caption)
                .foregroundStyle(.secondary)
                .textSelection(.enabled)
        }
    }
}
