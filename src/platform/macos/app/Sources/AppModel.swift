import AppKit
import Foundation
import UniformTypeIdentifiers

struct AlertInfo: Identifiable {
    let id = UUID()
    let title: String
    let message: String
    var confirmTitle: String?
    var onConfirm: (@MainActor () -> Void)?
}

struct LogLine: Identifiable {
    let id: Int
    let text: String
}

@MainActor
final class AppModel: ObservableObject {
    private enum Keys {
        static let selectedInstall = "selectedInstall"
        static let preparedCopy = "preparedCopyPath"
        static let customInstalls = "customInstalls"
        static let modSources = "modSources"
        static let showFeralOptions = "showFeralOptions"
    }

    let paths = LauncherPaths.current()
    private let defaults = UserDefaults.standard
    private(set) var bundled: BundledBackend?

    @Published private(set) var installs: [GameInstall] = []
    @Published var selectedPath: String? {
        didSet {
            defaults.set(selectedPath, forKey: Keys.selectedInstall)
            if oldValue != selectedPath { selectionChanged() }
        }
    }
    @Published private(set) var preparedCopy: GameInstall?
    @Published private(set) var isScanning = false

    @Published private(set) var backendStatus = "Installing the TWASE backend…"
    @Published private(set) var backendReady = false
    @Published private(set) var steamState: SteamBackendState?
    @Published private(set) var steamRunning: Bool?
    @Published private(set) var privatePrefsExist = false
    @Published private(set) var normalPrefsExist = false

    @Published var showFeralOptions: Bool {
        didSet { defaults.set(showFeralOptions, forKey: Keys.showFeralOptions) }
    }
    @Published private(set) var logLines: [LogLine] = []
    @Published private(set) var activity: String?
    @Published private(set) var lastResult: String?
    @Published var alert: AlertInfo?

    @Published private(set) var modSources: [String]
    @Published private(set) var packs: [PackEntry] = []
    @Published var selectedPacks = Set<String>()
    @Published private(set) var saves: [SaveEntry] = []
    @Published var selectedSaves = Set<String>()

    private var nextLogID = 0
    private var started = false

    init() {
        showFeralOptions = defaults.object(forKey: Keys.showFeralOptions) as? Bool ?? true
        modSources = defaults.stringArray(forKey: Keys.modSources) ?? []
        selectedPath = defaults.string(forKey: Keys.selectedInstall)
    }

    var selected: GameInstall? { installs.first { $0.id == selectedPath } }
    var isAppStoreMode: Bool { selected?.kind.isAppStore == true }
    var isBusy: Bool { activity != nil }

    /// The vendor-signed original used as the preparation source.
    var appStoreSource: GameInstall? {
        if let selected, selected.kind == .appStoreOriginal { return selected }
        return installs.first { $0.kind == .appStoreOriginal && $0.url.path == LauncherPaths.appStoreOriginal.path }
            ?? installs.first { $0.kind == .appStoreOriginal }
    }

    /// Play needs the backend for the selected kind: in the Steam game folder, or a prepared copy for the App Store.
    var canPlay: Bool {
        guard !isBusy, let selected else { return false }
        switch selected.kind {
        case .steam: return steamState == .current || steamState == .outdated
        case .appStoreOriginal, .appStorePrepared: return backendReady && preparedCopy != nil
        case .unsupported: return false
        }
    }

    /// Whether the Feral preferences that --skip-launcher needs exist for the selected kind.
    var feralOptionsToggleAvailable: Bool {
        switch selected?.kind {
        case .steam?: return normalPrefsExist
        case .appStoreOriginal?, .appStorePrepared?: return privatePrefsExist
        default: return false
        }
    }

    var steamDataFolder: URL? {
        installs.first { $0.kind == .steam }?.gameRoot.appendingPathComponent("TotalWarAttilaData/data", isDirectory: true)
    }

    var allModSources: [URL] {
        [LauncherPaths.normalLocalMods] + modSources.map { URL(fileURLWithPath: $0, isDirectory: true) }
    }

    // MARK: - Startup and refresh

    func start() {
        guard !started else { return }
        started = true
        installBackend()
        refreshInstalls()
    }

    private func installBackend() {
        let bundled: BundledBackend
        do {
            bundled = try BundledBackend.locate()
        } catch {
            backendStatus = error.localizedDescription
            showError("TWASE backend missing", error)
            return
        }
        self.bundled = bundled
        let paths = paths
        Task {
            let failure: String? = await Task.detached {
                do {
                    try BackendInstaller.installIntoRoot(bundled, paths: paths)
                    return nil
                } catch {
                    return error.localizedDescription
                }
            }.value
            if let failure {
                backendStatus = "Backend install failed: \(failure)"
                alert = AlertInfo(title: "Cannot install the TWASE backend", message: failure)
            } else {
                backendReady = true
                backendStatus = "TWASE \(bundled.version) backend installed in \(paths.twaseDir.path)"
                if Quarantine.isQuarantined(paths.dylib) {
                    backendStatus += " (quarantined: see Play)"
                }
            }
            refreshState()
        }
    }

    func refreshInstalls() {
        isScanning = true
        let remembered = (defaults.stringArray(forKey: Keys.customInstalls) ?? [])
            + [defaults.string(forKey: Keys.preparedCopy)].compactMap { $0 }
        Task {
            let found = await Task.detached {
                InstallInspector.discover(remembered: remembered)
            }.value
            installs = found
            isScanning = false
            if selected == nil {
                selectedPath = (found.first { $0.kind == .steam }
                    ?? found.first { $0.kind == .appStorePrepared }
                    ?? found.first { $0.kind == .appStoreOriginal }
                    ?? found.first)?.id
            }
            updatePreparedCopy()
            refreshState()
        }
    }

    private func selectionChanged() {
        updatePreparedCopy()
        refreshState()
    }

    private func updatePreparedCopy() {
        if let selected, selected.kind == .appStorePrepared {
            preparedCopy = selected
        } else if let remembered = defaults.string(forKey: Keys.preparedCopy),
                  let copy = installs.first(where: { $0.id == remembered && $0.kind == .appStorePrepared }) {
            preparedCopy = copy
        } else {
            preparedCopy = installs.first { $0.kind == .appStorePrepared }
        }
        if let preparedCopy {
            defaults.set(preparedCopy.url.path, forKey: Keys.preparedCopy)
        }
    }

    /// Re-reads everything that depends on the file system (cheap enough to do after every action).
    func refreshState() {
        privatePrefsExist = FileKind.of(paths.privatePrefs) == .other
        normalPrefsExist = FileManager.default.fileExists(atPath: LauncherPaths.normalPrefs.path)
        if let selected, selected.kind == .steam, let bundled {
            steamState = BackendInstaller.steamState(bundled, gameRoot: selected.gameRoot)
            Task {
                steamRunning = await Task.detached { ProcessRunner.capture("/usr/bin/pgrep", ["-x", "steam_osx"]).status == 0 }.value
            }
        } else {
            steamState = nil
            steamRunning = nil
        }
        if isAppStoreMode {
            refreshPacks()
            refreshSaves()
        }
    }

    // MARK: - Installations

    func chooseInstallation() {
        let panel = NSOpenPanel()
        panel.title = "Choose Total War: ATTILA"
        panel.message = "Choose the Total War ATTILA app (Steam, App Store or a prepared TWASE copy)."
        panel.allowedContentTypes = [.applicationBundle]
        panel.canChooseDirectories = false
        panel.treatsFilePackagesAsDirectories = false
        guard panel.runModal() == .OK, let url = panel.url else { return }
        Task {
            let install = await Task.detached { InstallInspector.inspect(url) }.value
            var custom = defaults.stringArray(forKey: Keys.customInstalls) ?? []
            if !custom.contains(install.id) {
                custom.append(install.id)
                defaults.set(custom, forKey: Keys.customInstalls)
            }
            if !installs.contains(where: { $0.id == install.id }) {
                installs.append(install)
            } else {
                installs = installs.map { $0.id == install.id ? install : $0 }
            }
            selectedPath = install.id
            if install.kind == .unsupported {
                alert = AlertInfo(title: "Unsupported game build", message: install.note ?? "This build is not supported.")
            }
        }
    }

    func selectPreparedCopy() {
        let panel = NSOpenPanel()
        panel.title = "Select prepared copy"
        panel.message = "Choose a private copy previously created by \"Prepare private copy\"."
        panel.allowedContentTypes = [.applicationBundle]
        panel.directoryURL = LauncherPaths.userApplications
        guard panel.runModal() == .OK, let url = panel.url else { return }
        Task {
            let install = await Task.detached { InstallInspector.inspect(url) }.value
            guard install.kind == .appStorePrepared else {
                alert = AlertInfo(
                    title: "Not a prepared copy",
                    message: "\(url.path) is \(install.kind.title.lowercased()). A prepared copy has the App Store "
                        + "build \(LauncherPaths.appStoreUUID) and an ad-hoc signature.")
                return
            }
            defaults.set(install.url.path, forKey: Keys.preparedCopy)
            if !installs.contains(where: { $0.id == install.id }) { installs.append(install) }
            updatePreparedCopy()
            refreshState()
        }
    }

    // MARK: - App Store preparation

    func choosePreparationDestination() {
        let panel = NSSavePanel()
        panel.title = "Private copy destination"
        panel.message = "Choose a new name for the private copy. Existing apps are never overwritten."
        panel.allowedContentTypes = [.applicationBundle]
        panel.nameFieldStringValue = LauncherPaths.preparedDefault.lastPathComponent
        if FileManager.default.fileExists(atPath: LauncherPaths.userApplications.path) {
            panel.directoryURL = LauncherPaths.userApplications
        }
        guard panel.runModal() == .OK, let url = panel.url else { return }
        confirmPrepare(destination: url.pathExtension == "app" ? url : url.appendingPathExtension("app"))
    }

    func confirmPrepare(destination: URL = LauncherPaths.preparedDefault) {
        guard let source = appStoreSource else {
            alert = AlertInfo(title: "App Store game not found", message: "Select the App Store Total War ATTILA first.")
            return
        }
        guard FileKind.of(destination) == .missing else {
            alert = AlertInfo(
                title: "Destination already exists",
                message: "\(destination.path) already exists and will not be overwritten. Use \"Select existing copy…\" "
                    + "if it is a prepared copy, or choose another destination.")
            return
        }
        alert = AlertInfo(
            title: "Prepare a private copy?",
            message: """
                An APFS clone of \(source.url.path) will be created at \(destination.path).
                The copy is UNSANDBOXED and NO LONGER VENDOR SIGNED (ad-hoc signed). The original stays untouched; \
                no receipt or licence check is bypassed. This can take several minutes.
                """,
            confirmTitle: "Prepare",
            onConfirm: { [weak self] in self?.prepare(source: source.url, destination: destination) })
    }

    private func prepare(source: URL, destination: URL) {
        guard backendReady else { return backendNotReady() }
        let parent = destination.deletingLastPathComponent()
        if parent.standardizedFileURL == LauncherPaths.userApplications.standardizedFileURL,
           FileKind.of(parent) == .missing {
            do {
                try FileManager.default.createDirectory(at: parent, withIntermediateDirectories: false)
            } catch {
                return showError("Cannot create \(parent.path)", error)
            }
        }
        runBackend(
            "Preparing private copy",
            script: paths.appStoreScript,
            arguments: ["--prepare", destination.path, "--source", source.path],
            environment: appStoreEnvironment()
        ) { [weak self] exit in
            guard let self, exit.succeeded else { return }
            defaults.set(destination.standardizedFileURL.path, forKey: Keys.preparedCopy)
            refreshInstalls()
        }
    }

    // MARK: - Steam backend

    func installSteamBackend() {
        guard let bundled, let selected, selected.kind == .steam else { return }
        let gameRoot = selected.gameRoot
        Task {
            let failure: String? = await Task.detached {
                do {
                    try BackendInstaller.installIntoSteam(bundled, gameRoot: gameRoot)
                    return nil
                } catch {
                    return error.localizedDescription
                }
            }.value
            if let failure {
                alert = AlertInfo(title: "Cannot install TWASE into the Steam game folder", message: failure)
            } else {
                appendLog("Installed TWASE \(bundled.version) into \(gameRoot.path)")
            }
            refreshState()
        }
    }

    // MARK: - Play

    func play() {
        guard !isBusy else { return }
        guard let selected else {
            alert = AlertInfo(title: "No game selected", message: "Choose a Total War ATTILA installation first.")
            return
        }
        let skipLauncher = feralOptionsToggleAvailable && !showFeralOptions
        switch selected.kind {
        case .steam:
            guard steamState != nil, steamState != .notInstalled else {
                alert = AlertInfo(
                    title: "TWASE is not installed for Steam",
                    message: "Use \"Install/Update TWASE\" to put the backend into \(selected.gameRoot.path).")
                return
            }
            let dylib = selected.gameRoot.appendingPathComponent("TWASE/libTWASE.dylib")
            guard !refuseQuarantined([dylib]) else { return }
            Task {
                let running = await Task.detached {
                    ProcessRunner.capture("/usr/bin/pgrep", ["-x", "steam_osx"]).status == 0
                }.value
                steamRunning = running
                if running {
                    launchSteam(selected, skipLauncher: skipLauncher)
                } else {
                    alert = AlertInfo(
                        title: "Steam does not seem to be running",
                        message: "The Steam game needs the Steam client. Start Steam and sign in first, or launch anyway.",
                        confirmTitle: "Launch Anyway",
                        onConfirm: { self.launchSteam(selected, skipLauncher: skipLauncher) })
                }
            }
        case .appStoreOriginal, .appStorePrepared:
            guard backendReady else { return backendNotReady() }
            guard let copy = preparedCopy else {
                alert = AlertInfo(
                    title: "No prepared copy",
                    message: "The App Store original is never modified or injected. Prepare a private copy first.")
                return
            }
            guard !refuseQuarantined([paths.dylib, copy.url]) else { return }
            var arguments: [String] = skipLauncher ? ["--skip-launcher"] : []
            arguments += ["--", copy.url.path]
            runBackend("Playing (App Store copy)", script: paths.appStoreScript, arguments: arguments,
                       environment: appStoreEnvironment(), completion: nil)
        case .unsupported:
            alert = AlertInfo(title: "Unsupported game build", message: selected.note ?? "This build is not supported.")
        }
    }

    private func launchSteam(_ install: GameInstall, skipLauncher: Bool) {
        guard !isBusy else { return }
        var environment = cleanEnvironment()
        environment.removeValue(forKey: "TWASE_ROOT")
        runBackend(
            "Playing (Steam)",
            script: install.gameRoot.appendingPathComponent(BundledBackend.steamScriptName),
            arguments: skipLauncher ? ["--skip-launcher"] : [],
            environment: environment,
            completion: nil)
    }

    /// Never a Steam-started process: drop Steam and injection variables the launcher might have inherited.
    private func cleanEnvironment() -> [String: String] {
        ProcessInfo.processInfo.environment.filter { key, _ in
            !(key.hasPrefix("DYLD_") || key.hasPrefix("Steam") || key.hasPrefix("STEAM_")
                || key == "TWASE_LAUNCHER_ROOT")
        }
    }

    private func appStoreEnvironment() -> [String: String] {
        var environment = cleanEnvironment()
        environment["TWASE_ROOT"] = paths.root.path
        return environment
    }

    private func refuseQuarantined(_ urls: [URL]) -> Bool {
        let quarantined = urls.filter(Quarantine.isQuarantined)
        guard !quarantined.isEmpty else { return false }
        alert = AlertInfo(title: "Files are quarantined", message: Quarantine.guidance(for: quarantined))
        return true
    }

    private func runBackend(
        _ title: String,
        script: URL,
        arguments: [String],
        environment: [String: String],
        completion: (@MainActor (ProcessRunner.Exit) -> Void)?
    ) {
        guard !isBusy else { return }
        activity = title
        lastResult = nil
        let firstLine = nextLogID
        appendLog("── \(title) ──")
        appendLog("$ /bin/sh \(([script.path] + arguments).map(quoted).joined(separator: " "))")
        Task {
            do {
                let exit = try await ProcessRunner.stream(
                    executable: URL(fileURLWithPath: "/bin/sh"),
                    arguments: [script.path] + arguments,
                    environment: environment,
                    onLine: { line in self.appendLog(line) })
                appendLog("── \(title): \(exit.description) ──")
                lastResult = "\(title) — \(exit.description)"
                if !exit.succeeded {
                    reportFailure(title, exit: exit, since: firstLine)
                }
                completion?(exit)
            } catch {
                lastResult = "\(title) — could not start: \(error.localizedDescription)"
                showError("Could not start \(script.lastPathComponent)", error)
            }
            activity = nil
            refreshState()
        }
    }

    private func reportFailure(_ title: String, exit: ProcessRunner.Exit, since firstLine: Int) {
        let output = logLines.filter { $0.id >= firstLine }.map(\.text)
        if output.contains(where: { $0.contains("quarantined") }) {
            let candidates = [paths.dylib, preparedCopy?.url].compactMap { $0 }
            let quarantined = candidates.filter(Quarantine.isQuarantined)
            alert = AlertInfo(
                title: "macOS quarantine blocked TWASE",
                message: Quarantine.guidance(for: quarantined.isEmpty ? candidates : quarantined))
            return
        }
        let errors = output.filter { $0.contains("error:") }.suffix(3)
        alert = AlertInfo(
            title: "\(title) failed",
            message: (errors.isEmpty ? "The backend \(exit.description)." : errors.joined(separator: "\n"))
                + "\n\nSee the log for details.")
    }

    // MARK: - Log

    func appendLog(_ text: String) {
        logLines.append(LogLine(id: nextLogID, text: text))
        nextLogID += 1
        if logLines.count > 5000 { logLines.removeFirst(logLines.count - 5000) }
    }

    func clearLog() {
        logLines.removeAll()
    }

    // MARK: - Mods

    func refreshPacks() {
        let sources = allModSources
        let privateMods = paths.privateLocalMods
        Task {
            let found = await Task.detached { ModLinker.scan(sources: sources, privateLocalMods: privateMods) }.value
            packs = found
            selectedPacks = selectedPacks.intersection(found.map(\.id))
        }
    }

    func addModSource() {
        let panel = NSOpenPanel()
        panel.title = "Add mod folder"
        panel.message = "Packs directly in this folder or one level below it will be listed."
        panel.canChooseDirectories = true
        panel.canChooseFiles = false
        guard panel.runModal() == .OK, let url = panel.url else { return }
        addModSource(url)
    }

    func addModSource(_ url: URL) {
        let path = url.standardizedFileURL.path
        guard !allModSources.contains(where: { $0.standardizedFileURL.path == path }) else { return }
        modSources.append(path)
        defaults.set(modSources, forKey: Keys.modSources)
        refreshPacks()
    }

    func removeModSource(_ path: String) {
        modSources.removeAll { $0 == path }
        defaults.set(modSources, forKey: Keys.modSources)
        refreshPacks()
    }

    func linkSelectedPacks() {
        let chosen = packs.filter { selectedPacks.contains($0.id) }
        guard !chosen.isEmpty else { return }
        let paths = paths
        Task {
            let report = await Task.detached { ModLinker.link(chosen, paths: paths) }.value
            alert = AlertInfo(title: "Link packs", message: report.summary(verb: "Linked"))
            refreshPacks()
        }
    }

    func unlinkSelectedPacks() {
        let chosen = packs.filter { selectedPacks.contains($0.id) }
        guard !chosen.isEmpty else { return }
        let paths = paths
        Task {
            let report = await Task.detached { ModLinker.unlink(chosen, paths: paths) }.value
            alert = AlertInfo(title: "Unlink packs", message: report.summary(verb: "Removed links"))
            refreshPacks()
        }
    }

    // MARK: - Saves

    func refreshSaves() {
        let privateSaves = paths.privateSaves
        Task {
            let found = await Task.detached {
                SaveCopier.list(normal: LauncherPaths.normalSaves, privateSaves: privateSaves)
            }.value
            saves = found
            selectedSaves = selectedSaves.intersection(found.map(\.id))
        }
    }

    func copySelectedSaves() {
        let chosen = saves.filter { selectedSaves.contains($0.id) }
        guard !chosen.isEmpty else { return }
        let paths = paths
        Task {
            let report = await Task.detached { SaveCopier.copy(chosen, paths: paths) }.value
            alert = AlertInfo(title: "Copy saves", message: report.summary(verb: "Copied"))
            refreshSaves()
        }
    }

    // MARK: - Folders

    /// Opens a folder below the TWASE root, creating it if needed.
    func openRootFolder(_ url: URL) {
        do {
            try paths.ensureDirectory(url)
            NSWorkspace.shared.open(url)
        } catch {
            showError("Cannot open \(url.path)", error)
        }
    }

    func openConfig() {
        if FileManager.default.fileExists(atPath: paths.config.path) {
            NSWorkspace.shared.open(paths.config)
        } else {
            openRootFolder(paths.twaseDir)
            alert = AlertInfo(
                title: "No config.ini yet",
                message: "\(paths.config.path) does not exist yet. The TWASE folder was opened instead.")
        }
    }

    /// Opens an existing folder outside the TWASE root (never created).
    func openExisting(_ url: URL) {
        guard FileManager.default.fileExists(atPath: url.path) else {
            alert = AlertInfo(title: "Folder not found", message: "\(url.path) does not exist.")
            return
        }
        NSWorkspace.shared.open(url)
    }

    // MARK: - Helpers

    private func backendNotReady() {
        alert = AlertInfo(title: "TWASE backend not installed", message: backendStatus)
    }

    private func showError(_ title: String, _ error: Error) {
        alert = AlertInfo(title: title, message: error.localizedDescription)
    }

    private func quoted(_ text: String) -> String {
        text.contains(where: { $0 == " " || $0 == "'" || $0 == "\"" })
            ? "'\(text.replacingOccurrences(of: "'", with: "'\\''"))'"
            : text
    }
}
