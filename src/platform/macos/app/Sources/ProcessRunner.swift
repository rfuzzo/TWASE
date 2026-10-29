import Foundation

enum ProcessRunner {
    struct Captured: Sendable {
        let status: Int32
        let output: String
    }

    struct Exit: Sendable {
        let status: Int32
        let signaled: Bool

        var succeeded: Bool { !signaled && status == 0 }
        var description: String { signaled ? "terminated by signal \(status)" : "exited with status \(status)" }
    }

    /// Runs a short helper (codesign, pgrep) synchronously and returns its combined stdout/stderr.
    /// Call it off the main actor.
    static func capture(_ executable: String, _ arguments: [String]) -> Captured {
        let process = Process()
        process.executableURL = URL(fileURLWithPath: executable)
        process.arguments = arguments
        process.standardInput = FileHandle.nullDevice
        let pipe = Pipe()
        process.standardOutput = pipe
        process.standardError = pipe
        do {
            try process.run()
        } catch {
            return Captured(status: -1, output: error.localizedDescription)
        }
        let data = pipe.fileHandleForReading.readDataToEndOfFile()
        process.waitUntilExit()
        return Captured(status: process.terminationStatus, output: String(decoding: data, as: UTF8.self))
    }

    /// Runs a backend script and delivers its merged stdout/stderr line by line on the main actor while it runs.
    @MainActor
    static func stream(
        executable: URL,
        arguments: [String],
        environment: [String: String],
        onLine: @escaping @MainActor (String) -> Void
    ) async throws -> Exit {
        let process = Process()
        process.executableURL = executable
        process.arguments = arguments
        process.environment = environment
        process.currentDirectoryURL = URL(fileURLWithPath: NSTemporaryDirectory(), isDirectory: true)
        process.standardInput = FileHandle.nullDevice
        let pipe = Pipe()
        process.standardOutput = pipe
        process.standardError = pipe

        let (chunks, chunkSink) = AsyncStream<Data>.makeStream()
        let reading = pipe.fileHandleForReading
        reading.readabilityHandler = { handle in
            let data = handle.availableData
            if data.isEmpty {
                handle.readabilityHandler = nil
                chunkSink.finish()
            } else {
                chunkSink.yield(data)
            }
        }

        let (exits, exitSink) = AsyncStream<Exit>.makeStream()
        process.terminationHandler = { finished in
            exitSink.yield(Exit(status: finished.terminationStatus, signaled: finished.terminationReason == .uncaughtSignal))
            exitSink.finish()
        }

        do {
            try process.run()
        } catch {
            reading.readabilityHandler = nil
            chunkSink.finish()
            throw error
        }
        // Only the child keeps the write end open, so EOF arrives when it (and anything it spawned) exits.
        try? pipe.fileHandleForWriting.close()

        let reader = LineReader()
        let readerTask = Task { @MainActor in
            var buffer = Data()
            for await chunk in chunks {
                buffer.append(chunk)
                while let newline = buffer.firstIndex(of: 0x0A) {
                    onLine(decodeLine(buffer[buffer.startIndex..<newline]))
                    buffer = Data(buffer[buffer.index(after: newline)...])
                }
            }
            if !buffer.isEmpty {
                onLine(decodeLine(buffer[...]))
            }
            reader.finished = true
        }

        var exit = Exit(status: -1, signaled: false)
        for await value in exits {
            exit = value
        }

        // A detached grandchild may keep the pipe open; give remaining output a moment, then stop reading.
        for _ in 0..<20 where !reader.finished {
            try? await Task.sleep(nanoseconds: 50_000_000)
        }
        reading.readabilityHandler = nil
        chunkSink.finish()
        await readerTask.value
        return exit
    }

    private static func decodeLine(_ bytes: Data.SubSequence) -> String {
        var line = String(decoding: bytes, as: UTF8.self)
        if line.hasSuffix("\r") { line.removeLast() }
        return line
    }

    @MainActor
    private final class LineReader {
        var finished = false
    }
}
