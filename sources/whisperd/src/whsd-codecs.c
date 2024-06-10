/**
 **
 ** (C)2024 aks
 **/
#include <whsd-codecs.h>
#include <whsd-config.h>
#include <mpg123.h>
#include <syn123.h>
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#define MP3_BUFFER_INIT_SIZE    131072  // 128Kb
#define MP3_IO_BLOCK_SIZE       8192    // 8Kb
static bool _init = false;

static void destructor__whsd_audio_buffer_t(void *data) {
    whsd_audio_buffer_t *obj = (whsd_audio_buffer_t *)data;

    wstk_mem_deref(obj->data);
}

static ssize_t mpg123_read_callback(void *handle, void *buf, size_t sz) {
    wstk_mbuf_t *mbuf = (wstk_mbuf_t *)handle;
    size_t leftb = 0;

    leftb = wstk_mbuf_left(mbuf);
    if(sz > leftb) { sz = leftb; }

    if(wstk_mbuf_read_mem(mbuf, buf, sz) != WSTK_STATUS_SUCCESS) {
        return -1;
    }

    return sz;
}

static off_t mpg123_seek_callback(void *handle, off_t offset, int whence) {
    wstk_mbuf_t *mbuf = (wstk_mbuf_t *)handle;
    off_t lofs = 0;

    if(whence == SEEK_SET) {
        lofs = offset;
    } else if(whence == SEEK_CUR) {
        lofs = mbuf->pos + offset;
    } else if(whence == SEEK_END) {
        lofs = mbuf->end + offset;
    }

    if(lofs > mbuf->end) {
        return -1;
    }

    mbuf->pos = lofs;
    return lofs;
}

void mpg123_cleanup_callback(void *handle) {
    wstk_mbuf_t *mbuf = (wstk_mbuf_t *)handle;
    wstk_mbuf_reset(mbuf);
}

static wstk_status_t resample(long inrate, uint32_t outrate, uint32_t channels, float *inbuf, float *outbuf, uint32_t outsamples) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    syn123_handle *sh = NULL;
    size_t wsamples = 0;
    int err = 0;

    sh = syn123_new(outrate, 1, MPG123_ENC_FLOAT_32, 0, &err);
    if(!sh) {
        log_error("syn123_new() : %s", syn123_strerror(err));
        wstk_goto_status(WSTK_STATUS_FALSE, out);
    }

    err = syn123_setup_resample(sh, inrate, outrate, channels, true, false);
    if(err != SYN123_OK) {
        log_error("syn123_setup_resample() : %s", syn123_strerror(err));
        wstk_goto_status(WSTK_STATUS_FALSE, out);
    }

    wsamples = syn123_resample(sh, outbuf, inbuf, outsamples);
    if(wsamples <= 0) {
        log_error("syn123_resample() : err=%i", (int)wsamples);
        wstk_goto_status(WSTK_STATUS_FALSE, out);
    }

out:
    if(sh) {
        syn123_del(sh);
    }
    return status;
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
wstk_status_t whsd_codecs_init(whsd_global_t *whsd_glb) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    int err = 0;

    if((err = mpg123_init()) != MPG123_OK) {
        log_error("mpg123_init() : %s", mpg123_plain_strerror(err));
        status = WSTK_STATUS_FALSE;
    }

    if(status == WSTK_STATUS_SUCCESS) {
        _init = true;
    }

    return status;
}

wstk_status_t whsd_codecs_shutdown() {
    if(_init) {
        mpg123_exit();
        _init = false;
    }
    return WSTK_STATUS_SUCCESS;
}

wstk_status_t whsd_audio_buffer_alloc(whsd_audio_buffer_t **out, uint32_t channels, uint32_t samplerate, uint32_t samples) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    whsd_audio_buffer_t *abuf = NULL;

    status = wstk_mem_zalloc((void *)&abuf, sizeof(whsd_audio_buffer_t), destructor__whsd_audio_buffer_t);
    if(status != WSTK_STATUS_SUCCESS) { goto out; }

    if(samples > 0) {
        status = wstk_mem_zalloc((void *)&abuf->data, (samples * sizeof(float)), NULL);
        if(status != WSTK_STATUS_SUCCESS) { goto out; }
    }

    abuf->samples = samples;
    abuf->channels = channels;
    abuf->samplerate = samplerate;

out:
    if(status == WSTK_STATUS_SUCCESS) {
        *out = abuf;
    } else {
        wstk_mem_deref(abuf);
    }
    return status;
}

wstk_status_t whsd_codecs_decode_mp3(whsd_audio_buffer_t **out, uint32_t samplerate, wstk_mbuf_t *src_mbuf) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    whsd_audio_buffer_t *abuf = NULL;
    wstk_mbuf_t *pcm_mbuffer = NULL;
    mpg123_handle *mh = NULL;
    int err = 0, mp3_channels = 0, mp3_encoding = 0;
    long mp3_samplerate = 0;
    size_t rd_done = 0;
    uint8_t *block_buffer = NULL;
    uint32_t pcm_f32_buffer_size = 0;
    float *pcm_f32_buffer_ptr = NULL;

    if(!src_mbuf || !out || !samplerate) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    src_mbuf->pos = 0;

    if((mh = mpg123_new(NULL, &err)) == NULL) {
        log_error("mpg123_new() : %s", mpg123_plain_strerror(err));
        wstk_goto_status(WSTK_STATUS_FALSE, out);
    }

    mpg123_param(mh, MPG123_ADD_FLAGS, MPG123_FORCE_FLOAT, 0.);
    mpg123_param(mh, MPG123_ADD_FLAGS, MPG123_MONO_MIX, 0);

    if(mpg123_replace_reader_handle(mh, mpg123_read_callback, mpg123_seek_callback, mpg123_cleanup_callback) != MPG123_OK) {
        log_error("mpg123_replace_reader_handle() : %s", mpg123_strerror(mh));
        wstk_goto_status(WSTK_STATUS_FALSE, out);
    }

    if(mpg123_open_handle(mh, src_mbuf) != MPG123_OK) {
        log_error("mpg123_open_handle() : %s", mpg123_strerror(mh));
        wstk_goto_status(WSTK_STATUS_FALSE, out);
    }

    if(mpg123_getformat(mh, &mp3_samplerate, &mp3_channels , &mp3_encoding) != MPG123_OK) {
        log_error("mpg123_getformat() : %s", mpg123_strerror(mh));
        wstk_goto_status(WSTK_STATUS_FALSE, out);
    }

    if(mp3_samplerate <= 0) {
        log_error("Wrong samplerate (%i)", (int) mp3_samplerate);
        wstk_goto_status(WSTK_STATUS_FALSE, out);
    }
    if(mp3_encoding != MPG123_ENC_FLOAT_32) {
        log_error("Wrong encodind (0x%x) (expected: 32)", mp3_encoding);
        wstk_goto_status(WSTK_STATUS_FALSE, out);
    }

    if((status = wstk_mbuf_alloc((void *)&pcm_mbuffer, MP3_BUFFER_INIT_SIZE)) != WSTK_STATUS_SUCCESS) {
        log_error("wstk_mem_alloc() failed");
        wstk_goto_status(WSTK_STATUS_FALSE, out);
    }
    if((status = wstk_mem_alloc((void *)&block_buffer, MP3_IO_BLOCK_SIZE, NULL)) != WSTK_STATUS_SUCCESS) {
        log_error("wstk_mem_alloc() failed");
        wstk_goto_status(WSTK_STATUS_FALSE, out);
    }

    while(true) {
        err = mpg123_read(mh, block_buffer, MP3_IO_BLOCK_SIZE, &rd_done);
        if(err == MPG123_NEW_FORMAT) { continue; }
        if(err != MPG123_OK) { break; }

        wstk_mbuf_write_mem(pcm_mbuffer, block_buffer, rd_done);
    }

    pcm_f32_buffer_size = (pcm_mbuffer->end / sizeof(float));
    pcm_f32_buffer_ptr = (float *)pcm_mbuffer->buf;

    if(mp3_samplerate == samplerate) {
        status = whsd_audio_buffer_alloc(&abuf, mp3_channels, mp3_samplerate, pcm_f32_buffer_size);
        if(status == WSTK_STATUS_SUCCESS) {
            memcpy((uint8_t *)abuf->data, (uint8_t *)pcm_f32_buffer_ptr, (pcm_f32_buffer_size * sizeof(float)));
        } else {
            log_error("whsd_audio_buffer_alloc()");
        }
    } else {
        uint32_t rsmp_buffer_size = ((pcm_f32_buffer_size * samplerate) / mp3_samplerate);

        status = whsd_audio_buffer_alloc(&abuf, mp3_channels, samplerate, rsmp_buffer_size);
        if(status == WSTK_STATUS_SUCCESS) {
            status = resample(mp3_samplerate, samplerate, mp3_channels, pcm_f32_buffer_ptr, abuf->data, pcm_f32_buffer_size);
        } else {
            log_error("whsd_audio_buffer_alloc()");
        }
    }
out:
    wstk_mem_deref(block_buffer);
    wstk_mem_deref(pcm_mbuffer);

    if(mh) {
        mpg123_close(mh);
        mpg123_delete(mh);
    }

    if(status == WSTK_STATUS_SUCCESS) {
        *out = abuf;
    } else {
        wstk_mem_deref(abuf);
    }
    return status;
}

wstk_status_t whsd_codecs_decode_wav(whsd_audio_buffer_t **out, uint32_t samplerate, wstk_mbuf_t *src_mbuf) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    whsd_audio_buffer_t *abuf = NULL;
    drwav wav = {0};
    uint8_t *tmp_buff = NULL;
    uint8_t *pcm_buffer = NULL;
    float   *pcm_f32_buffer_ptr = NULL;
    uint32_t pcm_f32_buffer_size = 0;
    int err = 0;

    if(!src_mbuf || !out || !samplerate) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    src_mbuf->pos = 0;

    if(drwav_init_memory(&wav, src_mbuf->buf, src_mbuf->end, NULL) == DRWAV_FALSE) {
        log_error("drwav_init_memory() failed");
        wstk_goto_status(WSTK_STATUS_FALSE, out);
    }

    if(wav.channels != 1) {
        log_error("Too many channels (%i) (expeted: 1)", wav.channels);
        wstk_goto_status(WSTK_STATUS_FALSE, out);
    }
    if(wav.sampleRate <= 0) {
        log_error("Wrong samplerate (%i)", (int) wav.sampleRate);
        wstk_goto_status(WSTK_STATUS_FALSE, out);
    }
    if(wav.bitsPerSample != 16 && wav.bitsPerSample != 32) {
        log_error("Wrong encodind (0x%x) (expected: 16,32)", wav.bitsPerSample);
        wstk_goto_status(WSTK_STATUS_FALSE, out);
    }

    if(wav.bitsPerSample == 16) {
        uint32_t pcm16_buf_size = (src_mbuf->end / sizeof(int16_t));
        int16_t *pcm16_buf_prt = NULL;
        uint32_t rdsmps = 0, bsz = 0;

        if((status = wstk_mem_alloc((void *)&tmp_buff, src_mbuf->end, NULL)) != WSTK_STATUS_SUCCESS) {
            log_error("wstk_mem_alloc() failed");
            wstk_goto_status(WSTK_STATUS_FALSE, out);
        }

        pcm16_buf_prt = (int16_t *)tmp_buff;

        if((rdsmps = drwav_read_pcm_frames_s16(&wav, pcm16_buf_size, pcm16_buf_prt)) < 1) {
            log_error("Not enough frames (%i)", rdsmps);
            wstk_goto_status(WSTK_STATUS_FALSE, out);
        }

        pcm_f32_buffer_size = rdsmps;
        bsz = (pcm_f32_buffer_size * sizeof(float));

        if((status = wstk_mem_alloc((void *)&pcm_buffer, bsz, NULL)) != WSTK_STATUS_SUCCESS) {
            log_error("wstk_mem_alloc() failed");
            wstk_goto_status(WSTK_STATUS_FALSE, out);
        }

        pcm_f32_buffer_ptr = (float *)pcm_buffer;

        for(uint32_t i = 0; i < pcm_f32_buffer_size; i++) {
            pcm_f32_buffer_ptr[i] = (float) ( (pcm16_buf_prt[i] > 0) ? (pcm16_buf_prt[i] / 32767.0) : (pcm16_buf_prt[i] / 32768.0) );
        }

        tmp_buff = wstk_mem_deref(tmp_buff);
    }

    if(wav.bitsPerSample == 32) {
        pcm_f32_buffer_size = (src_mbuf->end / sizeof(float));
        uint32_t rdsmps = 0;

        if((status = wstk_mem_alloc((void *)&pcm_buffer, src_mbuf->end, NULL)) != WSTK_STATUS_SUCCESS) {
            log_error("wstk_mem_alloc() failed");
            wstk_goto_status(WSTK_STATUS_FALSE, out);
        }

        pcm_f32_buffer_ptr = (float *)pcm_buffer;

        if((rdsmps = drwav_read_pcm_frames_f32(&wav, pcm_f32_buffer_size, pcm_f32_buffer_ptr)) < 1) {
            log_error("Not enough frames (%i)", rdsmps);
            wstk_goto_status(WSTK_STATUS_FALSE, out);
        }
        pcm_f32_buffer_size = rdsmps;
    }

    if(wav.sampleRate == samplerate) {
        status = whsd_audio_buffer_alloc(&abuf, wav.channels, wav.sampleRate, pcm_f32_buffer_size);
        if(status == WSTK_STATUS_SUCCESS) {
            memcpy((uint8_t *)abuf->data, (uint8_t *)pcm_f32_buffer_ptr, (pcm_f32_buffer_size * sizeof(float)));
        } else {
            log_error("whsd_audio_buffer_alloc()");
        }
    } else {
        uint32_t rsmp_buffer_size = ((pcm_f32_buffer_size * samplerate) / wav.sampleRate);

        status = whsd_audio_buffer_alloc(&abuf, wav.channels, samplerate, rsmp_buffer_size);
        if(status == WSTK_STATUS_SUCCESS) {
            status = resample(wav.sampleRate, samplerate, wav.channels, pcm_f32_buffer_ptr, abuf->data, pcm_f32_buffer_size);
        } else {
            log_error("whsd_audio_buffer_alloc()");
        }
    }

out:
    drwav_uninit(&wav);
    wstk_mem_deref(pcm_buffer);
    wstk_mem_deref(tmp_buff);

    if(status == WSTK_STATUS_SUCCESS) {
        *out = abuf;
    } else {
        wstk_mem_deref(abuf);
    }
    return status;
}
