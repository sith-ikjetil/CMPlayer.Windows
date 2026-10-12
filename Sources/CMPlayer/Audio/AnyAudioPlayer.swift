// Windows replacement for AnyAudioPlayer. Keep the existing CmpAudioPlayer wrapper,
// CmpAudioPlayerProtocol, CmpMetadata, and CmpError definitions in your application.
// Original source: Kjetil Kr Solberg, 2024. Windows adaptation, 2026.
import Foundation
import CCMPAudioSupport

internal final class AnyAudioPlayer: CmpAudioPlayerProtocol {
    private let filePath: URL
    private let lock = NSLock()
    private var handle: OpaquePointer?
    private var fadeTarget: Float = 1
    private var fadeDuration: UInt64 = 0
    private var baseVolume: Float = 1
    private var errorText: String?

    init(path: URL) { filePath = path }
    deinit { cwa_destroy(handle) }

    // Every handle access is under the same lock. Callers must keep the Swift
    // object alive during calls; do not use unsafe unowned references across tasks.
    private func locked<T>(_ body: () throws -> T) rethrows -> T {
        lock.lock()
        defer { lock.unlock() }
        return try body()
    }
    private static func message(_ error: cwa_error, code: Int32) -> String {
        var copy = error
        let text = withUnsafePointer(to: &copy.message) {
            $0.withMemoryRebound(to: CChar.self, capacity: 512) { String(cString: $0) }
        }
        return "Windows audio: \(text) (HRESULT 0x\(String(UInt32(bitPattern: code), radix: 16)))"
    }
    private func checked(_ code: Int32, _ error: cwa_error) throws {
        if code < 0 {
            let text = Self.message(error, code: code)
            errorText = text
            throw CmpError(message: text)
        }
    }
    private static func nativePath(_ url: URL) throws -> String {
        guard url.isFileURL else { throw CmpError(message: "Windows audio requires a file URL") }
        var path = url.path
        // Foundation can expose a drive URL as /C:/dir/file. C ABI expects C:/dir/file.
        let bytes = Array(path.utf8)
        if bytes.count >= 4 && bytes[0] == 47 && bytes[2] == 58 && bytes[3] == 47 {
            path.removeFirst()
        }
        if let host = url.host, !host.isEmpty && host.lowercased() != "localhost",
           !path.hasPrefix("\\\\") && !path.hasPrefix("//") {
            path = "\\\\" + host + path.replacingOccurrences(of: "/", with: "\\")
        }
        guard !path.utf8.contains(0) else { throw CmpError(message: "Path contains NUL") }
        return path
    }
    private func ensureOpen() throws -> OpaquePointer {
        if let handle { return handle }
        guard cwa_abi_version() == 1 else { throw CmpError(message: "Unsupported WindowsAudio ABI") }
        let path = try Self.nativePath(filePath)
        var created: OpaquePointer?
        var error = cwa_error()
        let code = path.withCString { cwa_create($0, &created, &error) }
        try checked(code, error)
        guard let created else { throw CmpError(message: "WindowsAudio returned a null player") }
        // Publish only after configuration succeeds; no leaked partially configured handle.
        do {
            try checked(cwa_set_volume(created, baseVolume, &error), error)
            try checked(cwa_set_end_fade(created, fadeTarget, fadeDuration, &error), error)
        } catch {
            cwa_destroy(created)
            throw error
        }
        handle = created
        return created
    }
    private func snapshot() -> cwa_snapshot {
        guard let handle else { return cwa_snapshot() }
        var result = cwa_snapshot()
        var error = cwa_error()
        let code = cwa_get_snapshot(handle, &result, &error)
        if code < 0 { errorText = Self.message(error, code: code) }
        if result.playback_error < 0 {
            errorText = "Asynchronous Windows audio failure (HRESULT 0x\(String(UInt32(bitPattern: result.playback_error), radix: 16)))"
        }
        return result
    }
    // Original implementation remains 'playing' while paused.
    var isPlaying: Bool { locked {
        let state = snapshot().state
        return state == UInt32(CWA_PLAYING) || state == UInt32(CWA_PAUSED)
    } }
    var isPaused: Bool { locked { snapshot().state == UInt32(CWA_PAUSED) } }
    var hasPlayed: Bool { locked { snapshot().has_played != 0 } }
    var timeElapsed: UInt64 { locked { snapshot().position_ms } }
    var duration: UInt64 { locked { snapshot().duration_ms } }
    // The protocol's stop/pause/resume/seek/fade methods cannot throw.
    // This extra property exposes their failures and asynchronous backend errors.
    var lastError: String? { locked { _ = snapshot(); return errorText } }

    func play() throws {
        try locked {
            do {
                let player = try ensureOpen()
                var error = cwa_error()
                try checked(cwa_play(player, &error), error)
                errorText = nil
            } catch {
                if errorText == nil { errorText = String(describing: error) }
                throw error
            }
        }
    }
    private func command(_ operation: (OpaquePointer, UnsafeMutablePointer<cwa_error>) -> Int32) {
        locked {
            guard let handle else { return }
            var error = cwa_error()
            let code = operation(handle, &error)
            errorText = code < 0 ? Self.message(error, code: code) : nil
        }
    }
    func stop() { command { cwa_stop($0, $1) } }
    func pause() { command { cwa_pause($0, $1) } }
    func resume() { command { cwa_resume($0, $1) } }
    func seekToPos(position: UInt64) { command { cwa_seek($0, position, $1) } }
    func setCrossfadeVolume(volume: Float, fadeDuration: UInt64) {
        locked {
            guard volume.isFinite && volume >= 0 && volume <= 1 else {
                errorText = "Fade target must be finite and between 0 and 1"
                return
            }
            self.fadeTarget = volume
            self.fadeDuration = fadeDuration
            if let handle {
                var error = cwa_error()
                let code = cwa_set_end_fade(handle, volume, fadeDuration, &error)
                errorText = code < 0 ? Self.message(error, code: code) : nil
            }
        }
    }
    // Optional extension beyond the original protocol: independent base volume.
    func setVolume(_ volume: Float) {
        locked {
            guard volume.isFinite && volume >= 0 && volume <= 1 else {
                errorText = "Volume must be finite and between 0 and 1"
                return
            }
            baseVolume = volume
            if let handle {
                var error = cwa_error()
                let code = cwa_set_volume(handle, volume, &error)
                errorText = code < 0 ? Self.message(error, code: code) : nil
            }
        }
    }
    static func gatherMetadata(path: URL) throws -> CmpMetadata {        
        guard cwa_abi_version() == 1 else { throw CmpError(message: "Unsupported WindowsAudio ABI") }
        let pathString = try nativePath(path)
        var metadata: OpaquePointer?
        var error = cwa_error()
        let code = pathString.withCString { cwa_read_metadata($0, &metadata, &error) }
        guard code >= 0, let metadata else { throw CmpError(message: message(error, code: code)) }
        defer { cwa_metadata_destroy(metadata) }
        var numbers = cwa_metadata_numbers()
        let numericCode = cwa_metadata_get_numbers(metadata, &numbers, &error)
        guard numericCode >= 0 else { throw CmpError(message: message(error, code: numericCode)) }
        func tag(_ field: Int) -> String {
            guard let p = cwa_metadata_string(metadata, UInt32(field)) else { return "" }
            return String(cString: p) // Copy while metadata owns the buffer.
        }
        let result = CmpMetadata()
        result.artist = tag(CWA_ARTIST)
        result.title = tag(CWA_TITLE)
        result.albumName = tag(CWA_ALBUM)
        result.genre = tag(CWA_GENRE)
        result.trackNo = Int(numbers.track)
        result.recordingYear = Int(numbers.year)
        result.duration = numbers.duration_ms
        // songNo is an application playlist number, not a file tag; retain default 0.        
        return result
    }
}
