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

struct cwa_metadata {
    std::array<std::string, 4> strings;
    cwa_metadata_numbers numbers{};
};
namespace cwa {
struct Variant {
    PROPVARIANT value;
    Variant() { PropVariantInit(&value); }
    ~Variant() { PropVariantClear(&value); }
    Variant(const Variant &) = delete;
};
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
uint32_t property_number(IPropertyStore *store, REFPROPERTYKEY key) {
    if (!store) return 0;
    Variant v; ULONG n = 0;
    if (SUCCEEDED(store->GetValue(key, &v.value))) PropVariantToUInt32(v.value, &n);
    return n;
}
std::unique_ptr<cwa_metadata> read_metadata(const std::wstring &path) {
    Runtime runtime;
    // Validate that MF sees an audio stream, and get duration independently of
    // shell tags. This avoids requiring metadata to exist on an otherwise valid file.
    ComPtr<IMFSourceReader> reader;
    check(MFCreateSourceReaderFromURL(path.c_str(), nullptr, &reader), "Open audio for metadata");
    ComPtr<IMFMediaType> audio;
    check(reader->GetNativeMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), 0, &audio), "Find audio stream");
    auto result = std::make_unique<cwa_metadata>();
    Variant duration;
    UINT64 ticks = 0;
    if (SUCCEEDED(reader->GetPresentationAttribute(static_cast<DWORD>(MF_SOURCE_READER_MEDIASOURCE), MF_PD_DURATION, &duration.value)))
        PropVariantToUInt64(duration.value, &ticks);
    result->numbers.duration_ms = ticks / 10000;
    ComPtr<IPropertyStore> store;
    // Handler availability differs by file and OS. Missing shell tags are empty.
    auto shellPath = path;
    std::replace(shellPath.begin(), shellPath.end(), L'/', L'\\');

    check(
        SHGetPropertyStoreFromParsingName(
            shellPath.c_str(),
            nullptr,
            GPS_HANDLERPROPERTIESONLY,
            IID_PPV_ARGS(&store)),
        "Open metadata property handler"
    );
    if (!result->numbers.duration_ms && store) {
        Variant shell_duration;
        ticks = 0;
        if (SUCCEEDED(store->GetValue(PKEY_Media_Duration, &shell_duration.value)))
            PropVariantToUInt64(shell_duration.value, &ticks);
        result->numbers.duration_ms = ticks / 10000;
    }
    require(result->numbers.duration_ms > 0, "Audio duration is unknown or zero");
    result->strings[CWA_ARTIST] = property_string(store.Get(), PKEY_Music_Artist);
    result->strings[CWA_TITLE] = property_string(store.Get(), PKEY_Title);
    result->strings[CWA_ALBUM] = property_string(store.Get(), PKEY_Music_AlbumTitle);
    result->strings[CWA_GENRE] = property_string(store.Get(), PKEY_Music_Genre);
    result->numbers.track = property_number(store.Get(), PKEY_Music_TrackNumber);
    result->numbers.year = property_number(store.Get(), PKEY_Media_Year);
    return result;
}
}
extern "C" {
int32_t CWA_CALL cwa_read_metadata(const char *path, cwa_metadata **out, cwa_error *e) {
    if (out) *out = nullptr;
    return cwa::boundary(e, [&] {
        cwa::require(out, "Metadata output is NULL");
        auto w = cwa::path_from_utf8(path);
        std::packaged_task<std::unique_ptr<cwa_metadata>()> task([w=std::move(w)] { return cwa::read_metadata(w); });
        auto result = task.get_future();
        // std::jthread joins even if future.get rethrows; client COM state untouched.
        std::jthread worker(std::move(task));
        *out = result.get().release();
    });
}
void CWA_CALL cwa_metadata_destroy(cwa_metadata *m) { delete m; }
const char * CWA_CALL cwa_metadata_string(const cwa_metadata *m, uint32_t field) {
    return m && field < m->strings.size() ? m->strings[field].c_str() : nullptr;
}
int32_t CWA_CALL cwa_metadata_get_numbers(const cwa_metadata *m, cwa_metadata_numbers *out, cwa_error *e) {
    if (out) *out = {};
    return cwa::boundary(e, [&] { cwa::require(m && out, "Metadata or output is NULL"); *out = m->numbers; });
}
}
