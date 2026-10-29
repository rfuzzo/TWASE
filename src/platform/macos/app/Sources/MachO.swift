import Foundation

/// Reads the LC_UUID of the arm64 slice of a thin or universal Mach-O executable without loading the whole file.
enum MachO {
    private static let fatMagic: UInt32 = 0xCAFE_BABE
    private static let fatMagic64: UInt32 = 0xCAFE_BABF
    private static let machMagic64: UInt32 = 0xFEED_FACF
    private static let cpuTypeARM64: UInt32 = 0x0100_000C
    private static let loadCommandUUID: UInt32 = 0x1B
    private static let header64Size = 32

    /// Returns the uppercase UUID string, or nil if the file has no arm64 slice or no LC_UUID.
    static func arm64UUID(of executable: URL) throws -> String? {
        let handle = try FileHandle(forReadingFrom: executable)
        defer { try? handle.close() }

        let start = try read(handle, offset: 0, count: 8)
        let magicBE = bigEndian32(start, 0)
        var sliceOffset: UInt64

        if magicBE == fatMagic || magicBE == fatMagic64 {
            let is64 = magicBE == fatMagic64
            let count = Int(bigEndian32(start, 4))
            guard count > 0, count <= 64 else { throw LauncherError("Malformed universal binary: \(executable.path)") }
            let entrySize = is64 ? 32 : 20
            let table = try read(handle, offset: 8, count: count * entrySize)
            var found: UInt64?
            for index in 0..<count {
                let base = index * entrySize
                guard bigEndian32(table, base) == cpuTypeARM64 else { continue }
                found = is64 ? bigEndian64(table, base + 8) : UInt64(bigEndian32(table, base + 8))
                break
            }
            guard let offset = found else { return nil }
            sliceOffset = offset
        } else {
            sliceOffset = 0
        }

        let header = try read(handle, offset: sliceOffset, count: header64Size)
        guard littleEndian32(header, 0) == machMagic64 else {
            throw LauncherError("Not a 64-bit Mach-O executable: \(executable.path)")
        }
        guard littleEndian32(header, 4) == cpuTypeARM64 else { return nil }
        let commandCount = Int(littleEndian32(header, 16))
        let commandsSize = Int(littleEndian32(header, 20))
        guard commandsSize <= 16 * 1024 * 1024 else { throw LauncherError("Malformed Mach-O: \(executable.path)") }
        let commands = try read(handle, offset: sliceOffset + UInt64(header64Size), count: commandsSize)

        var position = 0
        for _ in 0..<commandCount {
            guard position + 8 <= commands.count else { break }
            let command = littleEndian32(commands, position)
            let size = Int(littleEndian32(commands, position + 4))
            guard size >= 8, position + size <= commands.count else { break }
            if command == loadCommandUUID, size >= 24 {
                return formatUUID(Array(commands[(position + 8)..<(position + 24)]))
            }
            position += size
        }
        return nil
    }

    private static func read(_ handle: FileHandle, offset: UInt64, count: Int) throws -> [UInt8] {
        try handle.seek(toOffset: offset)
        let data = try handle.read(upToCount: count) ?? Data()
        guard data.count == count else { throw LauncherError("Unexpected end of Mach-O file.") }
        return [UInt8](data)
    }

    private static func bigEndian32(_ bytes: [UInt8], _ at: Int) -> UInt32 {
        UInt32(bytes[at]) << 24 | UInt32(bytes[at + 1]) << 16 | UInt32(bytes[at + 2]) << 8 | UInt32(bytes[at + 3])
    }

    private static func bigEndian64(_ bytes: [UInt8], _ at: Int) -> UInt64 {
        UInt64(bigEndian32(bytes, at)) << 32 | UInt64(bigEndian32(bytes, at + 4))
    }

    private static func littleEndian32(_ bytes: [UInt8], _ at: Int) -> UInt32 {
        UInt32(bytes[at]) | UInt32(bytes[at + 1]) << 8 | UInt32(bytes[at + 2]) << 16 | UInt32(bytes[at + 3]) << 24
    }

    private static func formatUUID(_ bytes: [UInt8]) -> String {
        let hex = bytes.map { String(format: "%02X", $0) }
        return [hex[0..<4], hex[4..<6], hex[6..<8], hex[8..<10], hex[10..<16]]
            .map { $0.joined() }
            .joined(separator: "-")
    }
}
