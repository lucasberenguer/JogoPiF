#include "menu_video.h"
#include <stddef.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <stdlib.h>

struct MenuVideo {
    IMFSourceReader *reader;
    unsigned char *pixels;
    UINT32 width, height;
    LONG stride;
    BOOL comStarted, mfStarted;
};

void CloseMenuVideo(MenuVideo *video) {
    if (!video) return;
    if (video->reader) IMFSourceReader_Release(video->reader);
    if (video->mfStarted) MFShutdown();
    if (video->comStarted) CoUninitialize();
    free(video->pixels);
    free(video);
}

MenuVideo *OpenMenuVideo(const char *path, int *width, int *height, double *frameInterval) {
    const DWORD stream = MF_SOURCE_READER_FIRST_VIDEO_STREAM;
    WCHAR filename[MAX_PATH];
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, filename, MAX_PATH)) return NULL;

    MenuVideo *video = calloc(1, sizeof(*video));
    if (!video) return NULL;
    IMFAttributes *attributes = NULL;
    IMFMediaType *requested = NULL;
    IMFMediaType *actual = NULL;
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    video->comStarted = SUCCEEDED(hr);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) goto done;
    if (FAILED(MFStartup(MF_VERSION, MFSTARTUP_LITE))) goto done;
    video->mfStarted = TRUE;

    if (FAILED(MFCreateAttributes(&attributes, 1)) ||
        FAILED(IMFAttributes_SetUINT32(attributes, &MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING, TRUE)) ||
        FAILED(MFCreateSourceReaderFromURL(filename, attributes, &video->reader)) ||
        FAILED(IMFSourceReader_SetStreamSelection(video->reader, MF_SOURCE_READER_ALL_STREAMS, FALSE)) ||
        FAILED(IMFSourceReader_SetStreamSelection(video->reader, stream, TRUE)) ||
        FAILED(MFCreateMediaType(&requested)) ||
        FAILED(IMFMediaType_SetGUID(requested, &MF_MT_MAJOR_TYPE, &MFMediaType_Video)) ||
        FAILED(IMFMediaType_SetGUID(requested, &MF_MT_SUBTYPE, &MFVideoFormat_RGB32)) ||
        FAILED(IMFSourceReader_SetCurrentMediaType(video->reader, stream, NULL, requested)) ||
        FAILED(IMFSourceReader_GetCurrentMediaType(video->reader, stream, &actual))) goto done;

    UINT64 dimensions;
    if (FAILED(IMFMediaType_GetUINT64(actual, &MF_MT_FRAME_SIZE, &dimensions))) goto done;
    video->width = (UINT32)(dimensions >> 32);
    video->height = (UINT32)dimensions;
    if (!video->width || !video->height || video->width > 8192 || video->height > 8192) goto done;
    UINT32 stride;
    if (SUCCEEDED(IMFMediaType_GetUINT32(actual, &MF_MT_DEFAULT_STRIDE, &stride)))
        video->stride = (LONG)stride;
    else if (FAILED(MFGetStrideForBitmapInfoHeader(MFVideoFormat_RGB32.Data1, video->width, &video->stride)))
        goto done;

    video->pixels = malloc((size_t)video->width * video->height * 4);
    if (!video->pixels) goto done;
    UINT64 rate;
    *frameInterval = 1.0 / 30;
    if (SUCCEEDED(IMFMediaType_GetUINT64(actual, &MF_MT_FRAME_RATE, &rate)) &&
        (rate >> 32) && (UINT32)rate) *frameInterval = (double)(UINT32)rate / (rate >> 32);
    *width = (int)video->width;
    *height = (int)video->height;

done:
    if (actual) IMFMediaType_Release(actual);
    if (requested) IMFMediaType_Release(requested);
    if (attributes) IMFAttributes_Release(attributes);
    if (!video->pixels) {
        CloseMenuVideo(video);
        return NULL;
    }
    return video;
}

static BOOL CopyFrame(MenuVideo *video, IMFMediaBuffer *buffer) {
    IMF2DBuffer *surface = NULL;
    BYTE *row = NULL;
    LONG stride = video->stride;
    DWORD length = 0;
    if (SUCCEEDED(IMFMediaBuffer_QueryInterface(buffer, &IID_IMF2DBuffer, (void **)&surface))) {
        HRESULT hr = IMF2DBuffer_Lock2D(surface, &row, &stride);
        if (FAILED(hr)) {
            IMF2DBuffer_Release(surface);
            return FALSE;
        }
    } else if (FAILED(IMFMediaBuffer_Lock(buffer, &row, NULL, &length))) return FALSE;

    size_t rowBytes = (size_t)video->width * 4;
    size_t pitch = stride < 0 ? (size_t)-(long long)stride : (size_t)stride;
    BOOL valid = pitch >= rowBytes && (surface || length >= pitch * (video->height - 1) + rowBytes);
    if (valid) {
        if (!surface && stride < 0) row += pitch * (video->height - 1);
        for (UINT32 y = 0; y < video->height; y++) {
            const BYTE *src = row + (ptrdiff_t)y * stride;
            BYTE *dest = video->pixels + y * rowBytes;
            for (size_t x = 0; x < rowBytes; x += 4) {
                dest[x] = src[x + 2];
                dest[x + 1] = src[x + 1];
                dest[x + 2] = src[x];
                dest[x + 3] = 255;
            }
        }
    }
    if (surface) {
        IMF2DBuffer_Unlock2D(surface);
        IMF2DBuffer_Release(surface);
    } else IMFMediaBuffer_Unlock(buffer);
    return valid;
}

const unsigned char *ReadMenuVideo(MenuVideo *video) {
    BOOL rewound = FALSE;
    for (int i = 0; i < 8; i++) {
        DWORD flags = 0;
        IMFSample *sample = NULL;
        HRESULT hr = IMFSourceReader_ReadSample(video->reader, MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                                                0, NULL, &flags, NULL, &sample);
        if (FAILED(hr) || (flags & MF_SOURCE_READERF_ERROR)) {
            if (sample) IMFSample_Release(sample);
            return NULL;
        }
        if (sample) {
            IMFMediaBuffer *buffer = NULL;
            hr = IMFSample_ConvertToContiguousBuffer(sample, &buffer);
            IMFSample_Release(sample);
            if (FAILED(hr)) return NULL;
            BOOL copied = CopyFrame(video, buffer);
            IMFMediaBuffer_Release(buffer);
            return copied ? video->pixels : NULL;
        }
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
            PROPVARIANT start = {0};
            start.vt = VT_I8;
            if (rewound || FAILED(IMFSourceReader_SetCurrentPosition(video->reader, &GUID_NULL, &start)))
                return NULL;
            rewound = TRUE;
        }
    }
    return NULL;
}

#else

MenuVideo *OpenMenuVideo(const char *path, int *width, int *height, double *frameInterval) {
    (void)path; (void)width; (void)height; (void)frameInterval;
    return NULL;
}

const unsigned char *ReadMenuVideo(MenuVideo *video) { (void)video; return NULL; }
void CloseMenuVideo(MenuVideo *video) { (void)video; }

#endif
