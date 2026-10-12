#pragma once
#include "WindowsAudio.h"
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <climits>
#include <cstring>
#include <exception>
#include <limits>
#include <new>
#include <string>
#include <string_view>
#include <utility>
#include <cstdint>
/*
extern "C" {
    /// <summary>
    /// Metadata for CMPlayer.Windows.
    /// </summary>
    typedef struct CmpAudioMetadata
    {
        char* title;
        char* artist;
        char* album;
        char* genre;
        char* comment;

        uint32_t year;
        uint32_t trackNumber;
        uint32_t bitrate;
        uint32_t sampleRate;
        uint32_t channels;

        int64_t durationMs;

    } CmpAudioMetadata;

    __declspec(dllexport)
    int32_t CMPAS_ReadMetadata(
        const char* filename,
        CmpAudioMetadata* metadata
    );

    __declspec(dllexport)
    void CMPAS_FreeMetadata(
        CmpAudioMetadata* metadata
    );



}*/
/*
namespace cwa {
    using Microsoft::WRL::ComPtr;
    struct Failure : std::exception {
        HRESULT code;
        std::string text;
        Failure(HRESULT h, std::string t) : code(h), text(std::move(t)) {}
        const char* what() const noexcept override { return text.c_str(); }
    };
    inline void check(HRESULT h, const char* operation) {
        if (FAILED(h)) throw Failure(h, operation);
    }
    inline void require(bool condition, const char* reason) {
        if (!condition) throw Failure(E_INVALIDARG, reason);
    }
    /* Every exported entry point contains exceptions; no C++ exception crosses C ABI. */
    template<class F> int32_t boundary(cwa_error* error, F&& f) noexcept {
        if (error) *error = {};
        auto fail = [error](HRESULT hr, const char* text) {
            if (error) {
                error->code = static_cast<int32_t>(hr);
                const auto n = (std::min)(std::strlen(text), sizeof(error->message) - 1);
                std::memcpy(error->message, text, n);
                error->message[n] = '\0';
            }
            return static_cast<int32_t>(hr);
            };
        try { f(); return 0; }
        catch (const Failure& e) { return fail(e.code, e.what()); }
        catch (const std::bad_alloc&) { return fail(E_OUTOFMEMORY, "Allocation failed"); }
        catch (const std::exception& e) { return fail(E_FAIL, e.what()); }
        catch (...) { return fail(E_FAIL, "Unknown C++ exception"); }
    }
    struct Runtime {
        Runtime() {
            check(CoInitializeEx(nullptr, COINIT_MULTITHREADED), "CoInitializeEx");
            const auto h = MFStartup(MF_VERSION, MFSTARTUP_FULL);
            if (FAILED(h)) { CoUninitialize(); check(h, "MFStartup (Media Feature Pack may be missing)"); }
        }
        ~Runtime() { MFShutdown(); CoUninitialize(); }
        Runtime(const Runtime&) = delete;
        Runtime& operator=(const Runtime&) = delete;
    };
    inline std::wstring path_from_utf8(const char* path) {
        require(path && *path, "Path must not be empty");
        const int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, nullptr, 0);
        check(n ? S_OK : HRESULT_FROM_WIN32(GetLastError()), "Invalid UTF-8 path");
        std::wstring w(static_cast<size_t>(n), L'\0');
        check(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, w.data(), n)
            ? S_OK : HRESULT_FROM_WIN32(GetLastError()), "UTF-8 conversion");
        w.resize(static_cast<size_t>(n) - 1);
        const bool drive = w.size() >= 3 && w[1] == L':' && (w[2] == L'\\' || w[2] == L'/');
        const bool unc = w.size() >= 3 && w[0] == L'\\' && w[1] == L'\\';
        require(drive || unc, "Use an absolute Windows filesystem path");
        auto a = GetFileAttributesW(w.c_str());
        check(a != INVALID_FILE_ATTRIBUTES ? S_OK : HRESULT_FROM_WIN32(GetLastError()), "File not accessible");
        require(!(a & FILE_ATTRIBUTE_DIRECTORY), "Path names a directory");
        return w;
    }
    inline std::string utf8(std::wstring_view w) {
        if (w.empty()) return {};
        require(w.size() <= INT_MAX, "Metadata string too large");
        int n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
        check(n ? S_OK : HRESULT_FROM_WIN32(GetLastError()), "Metadata UTF-8 conversion");
        std::string s(static_cast<size_t>(n), '\0');
        check(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr)
            ? S_OK : HRESULT_FROM_WIN32(GetLastError()), "Metadata UTF-8 conversion");
        return s;
    }
    inline uint64_t milliseconds(double seconds) {
        if (!std::isfinite(seconds) || seconds <= 0) return 0;
        const double ms = seconds * 1000.0;
        if (ms >= static_cast<double>((std::numeric_limits<uint64_t>::max)()))
            return (std::numeric_limits<uint64_t>::max)();
        return static_cast<uint64_t>(ms);
    }
    inline double fade_volume(double base, double target, uint64_t window, uint64_t duration, uint64_t position) {
        const auto span = (std::min)(window, duration);
        if (!span) return base;
        const auto start = duration - span;
        if (position <= start) return base;
        const auto elapsed = (std::min)(position - start, span);
        return base + (target - base) * (static_cast<double>(elapsed) / static_cast<double>(span));
    }
}*/