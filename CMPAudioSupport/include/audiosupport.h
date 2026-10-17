#ifndef CMP_WINDOWS_AUDIO_H
#define CMP_WINDOWS_AUDIO_H
#include <stdint.h>

#ifdef _WIN32
#ifdef CMPAUDIOSUPPORT_EXPORTS
#define CWA_API __declspec(dllexport)
#else
#define CWA_API __declspec(dllimport)
#endif

#define CWA_CALL __cdecl
#else
#define CWA_API
#define CWA_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif
/* ABI v1: default platform alignment, no packing. Same architecture on both sides.
   All times are milliseconds; volumes are finite values in [0,1].
   Returns 0 on success, negative HRESULT on failure. error may be NULL.
   Error/snapshot storage belongs to caller; initialize with zeroes.
   Paths are NUL-terminated strict UTF-8 absolute filesystem paths (not URLs).
   Calls on one player are serialized internally. Destruction MUST NOT overlap
   any call on that handle. No callbacks into the client and no COM setup needed. */
///
// declare core windows audio player object.
// 
typedef struct cwa_player cwa_player;
//
// declare core windows audio metadata object.
// 
typedef struct cwa_metadata cwa_metadata;
//
// define and declare a core windows audio error object.
//
typedef struct cwa_error { int32_t code; char message[512]; } cwa_error;
//
// define core windows audio status enum.
//
enum { CWA_READY=0, CWA_PLAYING=1, CWA_PAUSED=2, CWA_STOPPED=3, CWA_ENDED=4, CWA_FAILED=5 };
//
// define and declare core windows audio snapshot.
//
typedef struct cwa_snapshot {
    uint64_t position_ms;
    uint64_t duration_ms;
    uint32_t state;
    uint32_t has_played;
    float volume;
    int32_t playback_error;
} cwa_snapshot;
//
// define core windows audio metadata enum.
//
enum { CWA_ARTIST=0, CWA_TITLE=1, CWA_ALBUM=2, CWA_GENRE=3 };
//
// define and declare core windows audio metadata numbers struct.
//
typedef struct cwa_metadata_numbers {
    uint64_t duration_ms;
    uint32_t track;
    uint32_t year;
} cwa_metadata_numbers;
//
// get core windows audio abi version.
//
CWA_API uint32_t CWA_CALL cwa_abi_version(void);
//
// core windows audio create player object. create opens and validates media, 
// but does not play; *out=NULL on failure. Destroy each successfully created 
// handle exactly once; NULL is harmless.
// 
CWA_API int32_t CWA_CALL cwa_create(const char *path, cwa_player **out, cwa_error *error);
//
// core windows audio player destroy.
//
CWA_API void CWA_CALL cwa_destroy(cwa_player *player);
//
// play cwa_player.
//
CWA_API int32_t CWA_CALL cwa_play(cwa_player *player, cwa_error *error);
//
// stop playing cwa_player
//
CWA_API int32_t CWA_CALL cwa_stop(cwa_player *player, cwa_error *error);
//
// pause playing cwa_player
//
CWA_API int32_t CWA_CALL cwa_pause(cwa_player *player, cwa_error *error);
//
// resume playing cwa_player
//
CWA_API int32_t CWA_CALL cwa_resume(cwa_player *player, cwa_error *error);
//
// seek is from START; >duration is rejected. backend seek completes asynchronously.
//
CWA_API int32_t CWA_CALL cwa_seek(cwa_player *player, uint64_t position_ms, cwa_error *error);
//
// set volume [0,1].
//
CWA_API int32_t CWA_CALL cwa_set_volume(cwa_player *player, float volume, cwa_error *error);
//
// duration=0 disables fade; otherwise linear fade from base volume to target
// over min(duration, track duration), recomputed from media position after seek.
//
CWA_API int32_t CWA_CALL cwa_set_end_fade(cwa_player *player, float target, uint64_t duration_ms, cwa_error *error);
//
// get snapshot also reports asynchronous failures via playback_error and state.
// 
CWA_API int32_t CWA_CALL cwa_get_snapshot(cwa_player *player, cwa_snapshot *out, cwa_error *error);
//
//  read cwa_metadata
//
CWA_API int32_t CWA_CALL cwa_read_metadata(const char *path, cwa_metadata **out, cwa_error *error);
//
// destroy cwa_metadata
//
CWA_API void CWA_CALL cwa_metadata_destroy(cwa_metadata *metadata);
//
// immutable borrowed UTF-8 strings, valid until metadata_destroy. Missing tags="".
// invalid metadata/field returns NULL; no client may free a returned string. */
//
CWA_API const char * CWA_CALL cwa_metadata_string(const cwa_metadata *metadata, uint32_t field);
//
// get cwa_metadata_numbers
//
CWA_API int32_t CWA_CALL cwa_metadata_get_numbers(const cwa_metadata *metadata, cwa_metadata_numbers *out, cwa_error *error);
#ifdef __cplusplus
}
#endif
#endif
