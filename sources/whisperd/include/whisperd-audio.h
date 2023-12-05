/**
 * (C)2023 aks
 * https://github.com/akscf/
 **/
#ifndef WHISPERD_AUDIO_H
#define WHISPERD_AUDIO_H
#include <whisperd.h>

#define MP3_BUFFER_INIT_SIZE    131072  // 128Kb
#define MP3_IO_BLOCK_SIZE       8192    // 8Kb

typedef struct {
    float       *data;
    uint32_t    samples;
    uint32_t    samplerate;
    uint32_t    channels;

} wd_audio_buffer_t;

wd_status_t wd_audio_init();
wd_status_t wd_audio_shutdown();

wd_status_t wd_audio_buffer_alloc(wd_audio_buffer_t **waudio_buffer, uint32_t channels, uint32_t samplerate, uint32_t samples);
wd_status_t wd_audio_mp3_decode(wd_audio_buffer_t **waudio_buffer, mbuf_t *mp3_mbuffer);
wd_status_t wd_audio_wav_decode(wd_audio_buffer_t **waudio_buffer, mbuf_t *wav_mbuffer);


#endif

