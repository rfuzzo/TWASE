import SwiftUI

struct ContentView: View {
    @ObservedObject var model: AppModel

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            InstallationSection(model: model)
            TabView {
                PlayView(model: model).tabItem { Text("Play") }
                ModsView(model: model).tabItem { Text("Mods") }
                SavesView(model: model).tabItem { Text("Saves") }
                FoldersView(model: model).tabItem { Text("Folders") }
            }
            Text(model.backendStatus)
                .font(.caption)
                .foregroundStyle(.secondary)
                .textSelection(.enabled)
        }
        .padding()
        .frame(minWidth: 880, minHeight: 700)
        .alert(
            model.alert?.title ?? "",
            isPresented: alertBinding,
            presenting: model.alert
        ) { info in
            if let confirmTitle = info.confirmTitle {
                Button(confirmTitle) {
                    let action = info.onConfirm
                    // Run after the alert is dismissed so a follow-up alert is not swallowed.
                    Task { @MainActor in action?() }
                }
                Button("Cancel", role: .cancel) {}
            } else {
                Button("OK", role: .cancel) {}
            }
        } message: { info in
            Text(info.message)
        }
        .task { model.start() }
    }

    private var alertBinding: Binding<Bool> {
        let shown = model.alert?.id
        return Binding(
            get: { model.alert != nil },
            set: { presented in
                if !presented, model.alert?.id == shown { model.alert = nil }
            })
    }
}

struct InstallationSection: View {
    @ObservedObject var model: AppModel

    var body: some View {
        GroupBox("Game installation") {
            VStack(alignment: .leading, spacing: 8) {
                HStack {
                    Picker("Installation", selection: $model.selectedPath) {
                        if model.installs.isEmpty {
                            Text("None found").tag(String?.none)
                        }
                        ForEach(model.installs) { install in
                            Text(install.label).tag(String?.some(install.id))
                        }
                    }
                    .labelsHidden()
                    Button("Choose…") { model.chooseInstallation() }
                    Button("Rescan") { model.refreshInstalls() }
                    if model.isScanning {
                        ProgressView().controlSize(.small)
                    }
                }
                .disabled(model.isBusy)

                if let install = model.selected {
                    Grid(alignment: .leading, horizontalSpacing: 12, verticalSpacing: 4) {
                        GridRow {
                            Text("Type").foregroundStyle(.secondary)
                            Text(install.kind.title)
                        }
                        GridRow {
                            Text("Version").foregroundStyle(.secondary)
                            Text(install.version ?? "unknown")
                        }
                        GridRow {
                            Text("Build UUID").foregroundStyle(.secondary)
                            Text(install.uuid ?? "unknown").textSelection(.enabled)
                        }
                        GridRow {
                            Text("Location").foregroundStyle(.secondary)
                            Text(install.url.path).textSelection(.enabled)
                        }
                        if let note = install.note {
                            GridRow {
                                Text("Note").foregroundStyle(.secondary)
                                Text(note).foregroundStyle(.red)
                            }
                        }
                    }
                    .font(.callout)
                } else if !model.isScanning {
                    Text("No Total War ATTILA installation was found at the default Steam or App Store locations. Use Choose… to select the game.")
                        .foregroundStyle(.secondary)
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            .padding(4)
        }
    }
}
