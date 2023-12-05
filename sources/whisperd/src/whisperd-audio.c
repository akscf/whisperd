/**
 * (C)2023 aks
 * https://github.com/akscf/
 **/
#include <whisperd-audio.h>
#include <whisperd-whisper.h>
#include <mpg123.h>
#include <syn123.h>
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

static ssize_t io_cb_read(void *handle, void *buf, size_t sz) {
    mbuf_t *mp3_mbuffer = (mbuf_t *)handle;
    size_t leftb = 0;

    leftb = mbuf_get_left(mp3_mbuffer);
	if(sz > leftb) { sz = leftb; }

	if(mbuf_read_mem(mp3_mbuffer, buf, sz) != 0) {
        return -1;
	}

	return sz;
}

static off_t io_cb_lseek(void *handle, off_t offset, int whence) {
    mbuf_t *mp3_mbuffer = (mbuf_t *)handle;
    off_t lofs = 0;

    if(whence == SEEK_SET) {
        lofs = offset;
    } else if(whence == SEEK_CUR) {
        lofs = mp3_mbuffer->pos + offset;
    } else if(whence == SEEK_END) {
        lofs = mp3_mbuffer->end + offset;
    }

    if(lofs > mp3_mbuffer->end) {
        return -1;
    }

    mp3_mbuffer->pos = lofs;
	return lofs;
}

void io_cb_cleanup(void *handle) {
	mbuf_t *mp3_mbuffer = (mbuf_t *)handle;
    mbuf_reset(mp3_mbuffer);
}

static wd_status_t resample(long inrate, uint32_t channels, float *in, float *out, uint32_t samples) {
    wd_status_t status = WD_STATUS_SUCCESS;
    syn123_handle *sh = NULL;
    size_t wsamples = 0;
    int err = 0;

    if((sh = syn123_new(WD_WHISPER_SAMPLERATE, 1, MPG123_ENC_FLOAT_32, 0, &err)) == NULL) {
        log_error("Resampler fail (%s)", syn123_strerror(err));
        wd_goto_status(WD_STATUS_FALSE, out);
    }
    if((err = syn123_setup_resample(sh, inrate, WD_WHISPER_SAMPLERATE, channels, true, false)) != SYN123_OK) {
        log_error("Resampler fail (%s)", syn123_strerror(err));
        wd_goto_status(WD_STATUS_FALSE, out);
    }

    wsamples = syn123_resample(sh, out, in, samples);
    if(wsamples <= 0 ) {
        log_warn("Resampler fail (wsamples=%i)", (int)wsamples);
        wd_goto_status(WD_STATUS_FALSE, out);
    }

out:
    if(sh) {
        syn123_del(sh);
    }
    return status;
}

static void destructor__wd_audio_buffer_t(void *data) {
    wd_audio_buffer_t *ptr = (wd_audio_buffer_t *)data;

    mem_deref(ptr->data);
}

// ----------------------------------------------------------------------------------------------------------------------------------
// public
// ----------------------------------------------------------------------------------------------------------------------------------
wd_status_t wd_audio_init() {
    int err = 0;

    if((err = mpg123_init()) != MPG123_OK) {
        log_error("Couldn't init libmpg123: %s", mpg123_plain_strerror(err));
        return WD_STATUS_FALSE;
    }

    return WD_STATUS_SUCCESS;
}

wd_status_t wd_audio_shutdown() {
    mpg123_exit();
    return WD_STATUS_SUCCESS;
}

wd_status_t wd_audio_buffer_alloc(wd_audio_buffer_t **waudio_buffer, uint32_t channels, uint32_t samplerate, uint32_t samples) {
    wd_status_t status = WD_STATUS_SUCCESS;
    wd_audio_buffer_t *wab_local = NULL;

    wab_local = mem_zalloc(sizeof(wd_audio_buffer_t), destructor__wd_audio_buffer_t);
    if(wab_local == NULL) {
        log_mem_fail_goto_status(WD_STATUS_FALSE, out);
    }

    if(samples > 0) {
        wab_local->data = mem_alloc((samples * sizeof(float)), NULL);
        if(wab_local->data == NULL) {
            log_mem_fail_goto_status(WD_STATUS_FALSE, out);
        }
    }

    wab_local->samples = samples;
    wab_local->channels = channels;
    wab_local->samplerate = samplerate;

    *waudio_buffer = wab_local;
out:
    if(status != WD_STATUS_SUCCESS) {
        mem_deref(wab_local);
    }
    return status;
}

/**
 ** MP3
 **/
wd_status_t wd_audio_mp3_decode(wd_audio_buffer_t **waudio_buffer, mbuf_t *mp3_mbuffer) {
    wd_status_t status = WD_STATUS_SUCCESS;
    wd_audio_buffer_t *wab_local = NULL;
    mpg123_handle *mh = NULL;
    int err = 0, channels = 0, encoding = 0;
	long samplerate = 0;
    size_t rd_done = 0;
	uint8_t *block_buffer = NULL;
	uint32_t pcm_f32_buffer_size = 0;
	float *pcm_f32_buffer_ptr = NULL;
	mbuf_t *pcm_mbuffer = NULL;

	mp3_mbuffer->pos = 0;

    if((mh = mpg123_new(NULL, &err)) == NULL) {
		log_error("libmpg123 fail (%s)", mpg123_plain_strerror(err));
		wd_goto_status(WD_STATUS_FALSE, out);
	}

    mpg123_param(mh, MPG123_ADD_FLAGS, MPG123_FORCE_FLOAT, 0.);
    mpg123_param(mh, MPG123_ADD_FLAGS, MPG123_MONO_MIX, 0);

    if(mpg123_replace_reader_handle(mh, io_cb_read, io_cb_lseek, io_cb_cleanup) != MPG123_OK) {
        log_error("libmpg123 fail (%s)", mpg123_strerror(mh));
        wd_goto_status(WD_STATUS_FALSE, out);
    }

    if(mpg123_open_handle(mh, mp3_mbuffer) != MPG123_OK) {
        log_error("libmpg123 fail (%s)", mpg123_strerror(mh));
        wd_goto_status(WD_STATUS_FALSE, out);
    }

    if(mpg123_getformat(mh, &samplerate, &channels , &encoding) != MPG123_OK) {
        log_error("libmpg123 fail (%s)", mpg123_strerror(mh));
        wd_goto_status(WD_STATUS_FALSE, out);
    }

    if(samplerate <= 0) {
        log_error("libmpg123 bad samplerate (%i)", (int) samplerate);
        wd_goto_status(WD_STATUS_FALSE, out);
    }
    if(encoding != MPG123_ENC_FLOAT_32) {
        log_error("libmpg123 bad encoding (0x%x)", encoding);
        wd_goto_status(WD_STATUS_FALSE, out);
    }

    if((pcm_mbuffer = mbuf_alloc(MP3_BUFFER_INIT_SIZE)) == NULL) {
        log_mem_fail_goto_status(WD_STATUS_FALSE, out);
    }
    if((block_buffer = mem_alloc(MP3_IO_BLOCK_SIZE, NULL)) == NULL) {
        log_mem_fail_goto_status(WD_STATUS_FALSE, out);
    }

    while(true) {
        err = mpg123_read(mh, block_buffer, MP3_IO_BLOCK_SIZE, &rd_done);
        if(err == MPG123_NEW_FORMAT) { continue; }
        if(err != MPG123_OK) { break; }

        mbuf_write_mem(pcm_mbuffer, block_buffer, rd_done);
    }

    pcm_f32_buffer_size = (pcm_mbuffer->end / sizeof(float));
    pcm_f32_buffer_ptr = (float *)pcm_mbuffer->buf;

    if(samplerate == WD_WHISPER_SAMPLERATE) {
        if(wd_audio_buffer_alloc(&wab_local, channels, samplerate, pcm_f32_buffer_size) != WD_STATUS_SUCCESS) {
            log_mem_fail_goto_status(WD_STATUS_FALSE, out);
        }
        memcpy((uint8_t *)wab_local->data, (uint8_t *)pcm_f32_buffer_ptr, (pcm_f32_buffer_size * sizeof(float)));

    } else {
        uint32_t rsmp_buffer_size = ((pcm_f32_buffer_size * WD_WHISPER_SAMPLERATE) / samplerate);

        if(wd_audio_buffer_alloc(&wab_local, channels, WD_WHISPER_SAMPLERATE, rsmp_buffer_size) != WD_STATUS_SUCCESS) {
            log_mem_fail_goto_status(WD_STATUS_FALSE, out);
        }

        if(resample(samplerate, channels, pcm_f32_buffer_ptr, wab_local->data, pcm_f32_buffer_size) != WD_STATUS_SUCCESS) {
            wd_goto_status(WD_STATUS_FALSE, out);
        }
    }

    *waudio_buffer = wab_local;
out:
    mem_deref(block_buffer);
    mem_deref(pcm_mbuffer);

    if(mh) {
        mpg123_close(mh);
        mpg123_delete(mh);
    }

    if(status != WD_STATUS_SUCCESS) {
        mem_deref(wab_local);
        *waudio_buffer = NULL;
    }

    return status;
}


/**
 ** WAV
 **/
wd_status_t wd_audio_wav_decode(wd_audio_buffer_t **waudio_buffer, mbuf_t *wav_mbuffer) {
    wd_status_t status = WD_STATUS_SUCCESS;
    wd_audio_buffer_t *wab_local = NULL;
    uint8_t *tmp_buff = NULL;
    uint8_t *pcm_buffer = NULL;
    float   *pcm_f32_buffer_ptr = NULL;
    uint32_t pcm_f32_buffer_size = 0;
    int err = 0;
    drwav wav;

	wav_mbuffer->pos = 0;

    if(drwav_init_memory(&wav, wav_mbuffer->buf, wav_mbuffer->end, NULL) == DRWAV_FALSE) {
        log_error("drWav fail (drwav_init_memory)");
        wd_goto_status(WD_STATUS_FALSE, out);
    }

    if(wav.channels != 1) {
        log_warn("drWav too many channels (%i)", wav.channels);
        wd_goto_status(WD_STATUS_FALSE, out);
    }
    if(wav.sampleRate <= 0) {
        log_error("drWav bad samplerate (%i)", (int) wav.sampleRate);
        wd_goto_status(WD_STATUS_FALSE, out);
    }
    if(wav.bitsPerSample != 16 && wav.bitsPerSample != 32) {
        log_warn("drWav bad encoding (0x%x)", wav.bitsPerSample);
        wd_goto_status(WD_STATUS_FALSE, out);
    }

    if(wav.bitsPerSample == 16) {
        uint32_t pcm16_buf_size = (wav_mbuffer->end / sizeof(int16_t));
        int16_t *pcm16_buf_prt = NULL;
        uint32_t rdsmps = 0, bsz = 0;

        if((tmp_buff = mem_alloc(wav_mbuffer->end , NULL)) == NULL) {
            log_mem_fail_goto_status(WD_STATUS_FALSE, out);
        }
        pcm16_buf_prt = (int16_t *)tmp_buff;

        if((rdsmps = drwav_read_pcm_frames_s16(&wav, pcm16_buf_size, pcm16_buf_prt)) < 1) {
            log_error("drWav not enough frames (%i)", rdsmps);
            wd_goto_status(WD_STATUS_FALSE, out);
        }

        pcm_f32_buffer_size = rdsmps;
        bsz = (pcm_f32_buffer_size * sizeof(float));

        if((pcm_buffer = mem_alloc(bsz, NULL)) == NULL) {
            log_mem_fail_goto_status(WD_STATUS_FALSE, out);
        }
        pcm_f32_buffer_ptr = (float *)pcm_buffer;

        for(uint32_t i = 0; i < pcm_f32_buffer_size; i++) {
            pcm_f32_buffer_ptr[i] = (float) ( (pcm16_buf_prt[i] > 0) ? (pcm16_buf_prt[i] / 32767.0) : (pcm16_buf_prt[i] / 32768.0) );
        }

        mem_deref(tmp_buff);
        tmp_buff = NULL;
    }

    if(wav.bitsPerSample == 32) {
        pcm_f32_buffer_size = (wav_mbuffer->end / sizeof(float));
        uint32_t rdsmps = 0;

        if((pcm_buffer = mem_alloc(wav_mbuffer->end, NULL)) == NULL) {
            log_mem_fail_goto_status(WD_STATUS_FALSE, out);
        }
        pcm_f32_buffer_ptr = (float *)pcm_buffer;

        if((rdsmps = drwav_read_pcm_frames_f32(&wav, pcm_f32_buffer_size, pcm_f32_buffer_ptr)) < 1) {
            log_error("drWav not enough frames (%i)", rdsmps);
            wd_goto_status(WD_STATUS_FALSE, out);
        }
        pcm_f32_buffer_size = rdsmps;
    }

    if(wav.sampleRate == WD_WHISPER_SAMPLERATE) {
        if(wd_audio_buffer_alloc(&wab_local, wav.channels, wav.sampleRate, pcm_f32_buffer_size) != WD_STATUS_SUCCESS) {
            log_mem_fail_goto_status(WD_STATUS_FALSE, out);
        }
        memcpy((uint8_t *)wab_local->data, (uint8_t *)pcm_f32_buffer_ptr, (pcm_f32_buffer_size * sizeof(float)));

    } else {
        uint32_t rsmp_buffer_size = ((pcm_f32_buffer_size * WD_WHISPER_SAMPLERATE) /  wav.sampleRate);

        if(wd_audio_buffer_alloc(&wab_local, wav.channels, WD_WHISPER_SAMPLERATE, rsmp_buffer_size) != WD_STATUS_SUCCESS) {
            log_mem_fail_goto_status(WD_STATUS_FALSE, out);
        }

        if(resample(wav.sampleRate, wav.channels, pcm_f32_buffer_ptr, wab_local->data, pcm_f32_buffer_size) != WD_STATUS_SUCCESS) {
            wd_goto_status(WD_STATUS_FALSE, out);
        }
    }

    *waudio_buffer = wab_local;
out:
    drwav_uninit(&wav);

    mem_deref(pcm_buffer);
    mem_deref(tmp_buff);

    if(status == WD_STATUS_SUCCESS) {
        mbuf_reset(wav_mbuffer);
    } else {
        mem_deref(wab_local);
        *waudio_buffer = NULL;
    }

    return status;
}
