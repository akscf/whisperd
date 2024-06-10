/**
 **
 ** (C)2024 aks
 **/
#ifndef WHSD_AUDIO_CODECS_H
#define WHSD_AUDIO_CODECS_H
#include <whsd-core.h>
#include <whsd-config.h>

typedef struct {
    float       *data;
    uint32_t    samples;
    uint32_t    channels;
    uint32_t    samplerate;
} whsd_audio_buffer_t;

wstk_status_t whsd_codecs_decode_mp3(whsd_audio_buffer_t **out, uint32_t samplerate, wstk_mbuf_t *src_mbuf);
wstk_status_t whsd_codecs_decode_wav(whsd_audio_buffer_t **out, uint32_t samplerate, wstk_mbuf_t *src_mbuf);
wstk_status_t whsd_audio_buffer_alloc(whsd_audio_buffer_t **out, uint32_t channels, uint32_t samplerate, uint32_t samples);

wstk_status_t whsd_codecs_init(whsd_global_t *whsd_glb);
wstk_status_t whsd_codecs_shutdown();

#endif
