//
// #include
//
#include "pch.h"
#include "Support.hpp"
#include <mfreadwrite.h>
#include <propkey.h>
#include <propvarutil.h>
#include <shobjidl.h>
#include <array>
#include <future>
#include <memory>
#include <thread>

//
// struct
//
struct cwa_metadata {
    std::array<std::string, 4> strings;
    cwa_metadata_numbers numbers{};
};

//
// namespace
//
namespace CMPlayer 
{
    /// <summary>
    /// Property variant.
    /// </summary>
    struct Variant {
        PROPVARIANT value;
        Variant() { PropVariantInit(&value); }
        ~Variant() { PropVariantClear(&value); }
        Variant(const Variant &) = delete;
    };
    /// <summary>
    /// Get from IPropertyStore a string of given key.
    /// </summary>
    /// <param name="store"></param>
    /// <param name="key"></param>
    /// <returns>property string value</returns>
    std::string property_string(IPropertyStore *store, REFPROPERTYKEY key) {
        if (!store) return {};
        Variant v;
        check(store->GetValue(key, &v.value), "Read metadata property");
        std::wstring text;
        if (v.value.vt == (VT_VECTOR | VT_LPWSTR)) {
            for (ULONG i = 0; i < v.value.calpwstr.cElems; ++i) {
                if (!v.value.calpwstr.pElems[i]) continue;
                if (!text.empty()) text += L"; ";
                text += v.value.calpwstr.pElems[i];
            }
        } else if (v.value.vt == VT_LPWSTR && v.value.pwszVal) text = v.value.pwszVal;
        else if (v.value.vt == VT_BSTR && v.value.bstrVal)
            text.assign(v.value.bstrVal, SysStringLen(v.value.bstrVal));
        return utf8(text);
    }
    /// <summary>
    /// Get from IPropertyStore a number of given key.
    /// </summary>
    /// <param name="store"></param>
    /// <param name="key"></param>
    /// <returns></returns>
    uint32_t property_number(IPropertyStore *store, REFPROPERTYKEY key) {
        if (!store) return 0;
        Variant v; ULONG n = 0;
        if (SUCCEEDED(store->GetValue(key, &v.value))) PropVariantToUInt32(v.value, &n);
        return n;
    }
    /// <summary>
    /// Reads metadata from given file path.
    /// </summary>
    /// <param name="path"></param>
    /// <returns>cwa_metadata structure.</returns>
    std::unique_ptr<cwa_metadata> read_metadata(const std::wstring &path) {
        // init com
        Runtime runtime;
        
        // Validate that MF sees an audio stream, and get duration independently of
        // shell tags. This avoids requiring metadata to exist on an otherwise valid file.
        ComPtr<IMFSourceReader> reader;
        check(MFCreateSourceReaderFromURL(path.c_str(), nullptr, &reader), "Open audio for metadata");

        // get media type
        ComPtr<IMFMediaType> audio;
        check(reader->GetNativeMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), 0, &audio), "Find audio stream");

        // make cwa_metadata and fill.
        auto result = std::make_unique<cwa_metadata>();
        Variant duration;
        UINT64 ticks = 0;
        if (SUCCEEDED(reader->GetPresentationAttribute(static_cast<DWORD>(MF_SOURCE_READER_MEDIASOURCE), MF_PD_DURATION, &duration.value)))
            PropVariantToUInt64(duration.value, &ticks);
        result->numbers.duration_ms = ticks / 10000;

        // get property store
        ComPtr<IPropertyStore> store;
        // Handler availability differs by file and OS. Missing shell tags are empty.
        auto shellPath = path;
        std::replace(shellPath.begin(), shellPath.end(), L'/', L'\\');
        check(SHGetPropertyStoreFromParsingName(shellPath.c_str(),nullptr,GPS_HANDLERPROPERTIESONLY,IID_PPV_ARGS(&store)),"Open metadata property handler");
        
        if (!result->numbers.duration_ms && store) {
            Variant shell_duration;
            ticks = 0;
            if (SUCCEEDED(store->GetValue(PKEY_Media_Duration, &shell_duration.value)))
                PropVariantToUInt64(shell_duration.value, &ticks);
            result->numbers.duration_ms = ticks / 10000;
        }
        
        // ensure positive duration for media
        require(result->numbers.duration_ms > 0, "Audio duration is unknown or zero");

        //
        // fill cwa_metadata
        //
        result->strings[CWA_ARTIST] = property_string(store.Get(), PKEY_Music_Artist);
        result->strings[CWA_TITLE] = property_string(store.Get(), PKEY_Title);
        result->strings[CWA_ALBUM] = property_string(store.Get(), PKEY_Music_AlbumTitle);
        result->strings[CWA_GENRE] = property_string(store.Get(), PKEY_Music_Genre);
        result->numbers.track = property_number(store.Get(), PKEY_Music_TrackNumber);
        result->numbers.year = property_number(store.Get(), PKEY_Media_Year);
        return result;
    }
}// namespace

//
// extern "C"
//
extern "C" {
    /// <summary>
    /// Read metadata from file of given path.
    /// </summary>
    /// <param name="path"></param>
    /// <param name="out"></param>
    /// <param name="e"></param>
    /// <returns></returns>
    int32_t CWA_CALL cwa_read_metadata(const char *path, cwa_metadata **out, cwa_error *e) {
        if (out) *out = nullptr;
        return CMPlayer::boundary(e, [&] {
            CMPlayer::require(out, "Metadata output is NULL");
            auto w = CMPlayer::path_from_utf8(path);
            std::packaged_task<std::unique_ptr<cwa_metadata>()> task([w=std::move(w)] { return CMPlayer::read_metadata(w); });
            auto result = task.get_future();
            // std::jthread joins even if future.get rethrows; client COM state untouched.
            std::jthread worker(std::move(task));
            *out = result.get().release();
        });
    }
    /// <summary>
    /// Destroy cwa_metdata object.
    /// </summary>
    /// <param name="m"></param>
    /// <returns></returns>
    void CWA_CALL cwa_metadata_destroy(cwa_metadata *m) { delete m; }
    /// <summary>
    /// Get string from cwa_metadata.
    /// </summary>
    /// <param name="m"></param>
    /// <param name="field"></param>
    /// <returns></returns>
    const char * CWA_CALL cwa_metadata_string(const cwa_metadata *m, uint32_t field) {
        return m && field < m->strings.size() ? m->strings[field].c_str() : nullptr;
    }
    /// <summary>
    /// Get number from cwa_metadata.
    /// </summary>
    /// <param name="m"></param>
    /// <param name="out"></param>
    /// <param name="e"></param>
    /// <returns></returns>
    int32_t CWA_CALL cwa_metadata_get_numbers(const cwa_metadata *m, cwa_metadata_numbers *out, cwa_error *e) {
        if (out) *out = {};
        return CMPlayer::boundary(e, [&] { CMPlayer::require(m && out, "Metadata or output is NULL"); *out = m->numbers; });
    }
}// extern "C"
