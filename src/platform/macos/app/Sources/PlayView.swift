import SwiftUI

struct PlayView: View {
    @ObservedObject var model: AppModel

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            switch model.selected?.kind {
            case .steam?:
                SteamCard(model: model)
            case .appStoreOriginal?, .appStorePrepared?:
                AppStoreCard(model: model)
            case .unsupported?:
                Text(model.selected?.note ?? "This game build is not supported by TWASE.")
                    .foregroundStyle(.red)
            case nil:
                Text("Select a Total War ATTILA installation above.")
                    .foregroundStyle(.secondary)
            }
            PlayControls(model: model)
            LogPane(model: model)
        }
        .padding()
    }
}

private struct SteamCard: View {
    @ObservedObject var model: AppModel

    var body: some View {
        GroupBox("Steam") {
            VStack(alignment: .leading, spacing: 6) {
                HStack {
                    Text(stateText)
                    Spacer()
                    Button(model.steamState == .current ? "Reinstall TWASE" : "Install/Update TWASE") {
                        model.installSteamBackend()
                    }
                    .disabled(model.isBusy || model.bundled == nil)
                }
                Text("Installs twase-launch.command and TWASE/libTWASE.dylib into \(model.selected?.gameRoot.path ?? "the game folder"). Mods are managed in Feral's normal Mod Manager.")
                    .font(.caption)
                    .foregroundStyle(.secondary)
                switch model.steamRunning {
                case false?:
                    Text("The Steam client does not seem to be running. Start Steam and sign in before playing.")
                        .foregroundStyle(.orange)
                case true?:
                    Text("Steam is running.").foregroundStyle(.secondary)
                case nil:
                    EmptyView()
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            .padding(4)
        }
    }

    private var stateText: String {
        let version = model.bundled?.version ?? "?"
        switch model.steamState {
        case .current?: return "TWASE \(version) is installed in the game folder."
        case .outdated?: return "The TWASE in the game folder differs from this launcher's TWASE \(version). Update recommended."
        case .notInstalled?: return "TWASE is not installed in the game folder yet."
        case nil: return "Checking…"
        }
    }
}

private struct AppStoreCard: View {
    @ObservedObject var model: AppModel

    var body: some View {
        GroupBox("App Store: private TWASE copy") {
            VStack(alignment: .leading, spacing: 8) {
                Text("The App Store game is never modified or injected. TWASE runs in a private copy: an APFS clone (little extra disk space) that is unsandboxed and no longer vendor signed (ad-hoc signed). The original stays untouched, and no licence or receipt check is bypassed. The copy uses its own private preferences, mods and saves under the TWASE folder.")
                    .font(.callout)
                    .fixedSize(horizontal: false, vertical: true)
                if let copy = model.preparedCopy {
                    Text("Prepared copy: \(copy.url.path)\(copy.version.map { " (\($0))" } ?? "")")
                        .textSelection(.enabled)
                } else {
                    Text("No prepared copy found.").foregroundStyle(.orange)
                }
                HStack {
                    Button("Prepare private copy") { model.confirmPrepare() }
                        .help("Destination: \(LauncherPaths.preparedDefault.path)")
                    Button("Choose destination…") { model.choosePreparationDestination() }
                    Button("Select existing copy…") { model.selectPreparedCopy() }
                }
                .disabled(model.isBusy)
                Text("Default destination: \(LauncherPaths.preparedDefault.path)")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            .padding(4)
        }
    }
}

private struct PlayControls: View {
    @ObservedObject var model: AppModel

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack(spacing: 16) {
                Toggle("Show Feral options window", isOn: toggleBinding)
                    .disabled(!model.feralOptionsToggleAvailable || model.isBusy)
                Spacer()
                if let activity = model.activity {
                    ProgressView().controlSize(.small)
                    Text(activity)
                }
                Button("Play") { model.play() }
                    .keyboardShortcut(.defaultAction)
                    .controlSize(.large)
                    .disabled(!model.canPlay)
            }
            if model.selected != nil, !model.feralOptionsToggleAvailable {
                Text(model.isAppStoreMode
                    ? "The Feral options window is always shown until the private copy has been started and quit normally once (it then has its own preferences)."
                    : "The Feral options window is always shown until the game has been started and quit normally once.")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
            if let lastResult = model.lastResult {
                Text(lastResult).font(.caption)
            }
        }
    }

    private var toggleBinding: Binding<Bool> {
        Binding(
            get: { model.showFeralOptions || !model.feralOptionsToggleAvailable },
            set: { model.showFeralOptions = $0 })
    }
}

private struct LogPane: View {
    @ObservedObject var model: AppModel

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            HStack {
                Text("Log").font(.headline)
                Spacer()
                Button("Clear") { model.clearLog() }
                    .disabled(model.logLines.isEmpty)
            }
            ScrollViewReader { proxy in
                ScrollView {
                    LazyVStack(alignment: .leading, spacing: 1) {
                        ForEach(model.logLines) { line in
                            Text(line.text)
                                .font(.system(.caption, design: .monospaced))
                                .textSelection(.enabled)
                                .frame(maxWidth: .infinity, alignment: .leading)
                                .id(line.id)
                        }
                    }
                    .padding(6)
                }
                .background(Color(nsColor: .textBackgroundColor))
                .border(Color(nsColor: .separatorColor))
                .onChange(of: model.logLines.last?.id) { _, last in
                    if let last { proxy.scrollTo(last, anchor: .bottom) }
                }
            }
        }
        .frame(minHeight: 160, maxHeight: .infinity)
    }
}
