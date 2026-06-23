#include "miniaudio.h"

typedef struct mote_miniaudio_decoder {
    ma_decoder decoder;
} mote_miniaudio_decoder;

typedef struct mote_miniaudio_format_info {
    int format;
    unsigned int channels;
    unsigned int sample_rate;
} mote_miniaudio_format_info;

typedef struct mote_miniaudio_decoded_audio {
    void* frames;
    ma_uint64 frame_count;
    int format;
    unsigned int channels;
    unsigned int sample_rate;
} mote_miniaudio_decoded_audio;

typedef struct mote_miniaudio_engine_config {
    unsigned int channels;
    unsigned int sample_rate;
    int no_device;
    int no_auto_start;
} mote_miniaudio_engine_config;

typedef struct mote_miniaudio_engine {
    ma_engine engine;
} mote_miniaudio_engine;

typedef struct mote_miniaudio_sound {
    ma_sound sound;
} mote_miniaudio_sound;

static ma_decoder_config mote_miniaudio_make_decoder_config(int format, unsigned int channels, unsigned int sample_rate)
{
    if (format == 0 && channels == 0 && sample_rate == 0) {
        return ma_decoder_config_init_default();
    }

    return ma_decoder_config_init((ma_format)format, channels, sample_rate);
}

int mote_miniaudio_decoder_size(void)
{
    return (int)sizeof(mote_miniaudio_decoder);
}

int mote_miniaudio_decoder_init_file(const char* path, int format, unsigned int channels, unsigned int sample_rate, mote_miniaudio_decoder* out_decoder)
{
    ma_decoder_config config;
    if (out_decoder == NULL) {
        return MA_INVALID_ARGS;
    }

    config = mote_miniaudio_make_decoder_config(format, channels, sample_rate);
    return (int)ma_decoder_init_file(path, &config, &out_decoder->decoder);
}

int mote_miniaudio_decoder_init_memory(const void* data, size_t data_size, int format, unsigned int channels, unsigned int sample_rate, mote_miniaudio_decoder* out_decoder)
{
    ma_decoder_config config;
    if (out_decoder == NULL) {
        return MA_INVALID_ARGS;
    }

    config = mote_miniaudio_make_decoder_config(format, channels, sample_rate);
    return (int)ma_decoder_init_memory(data, data_size, &config, &out_decoder->decoder);
}

int mote_miniaudio_decoder_uninit(mote_miniaudio_decoder* decoder)
{
    if (decoder == NULL) {
        return MA_INVALID_ARGS;
    }

    return (int)ma_decoder_uninit(&decoder->decoder);
}

int mote_miniaudio_decoder_get_data_format(mote_miniaudio_decoder* decoder, mote_miniaudio_format_info* out_info)
{
    ma_result result;
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sample_rate;
    if (decoder == NULL || out_info == NULL) {
        return MA_INVALID_ARGS;
    }

    result = ma_decoder_get_data_format(&decoder->decoder, &format, &channels, &sample_rate, NULL, 0);
    if (result != MA_SUCCESS) {
        return (int)result;
    }

    out_info->format = (int)format;
    out_info->channels = channels;
    out_info->sample_rate = sample_rate;
    return MA_SUCCESS;
}

int mote_miniaudio_decoder_get_length_in_pcm_frames(mote_miniaudio_decoder* decoder, ma_uint64* out_length)
{
    if (decoder == NULL || out_length == NULL) {
        return MA_INVALID_ARGS;
    }

    return (int)ma_decoder_get_length_in_pcm_frames(&decoder->decoder, out_length);
}

int mote_miniaudio_decoder_get_cursor_in_pcm_frames(mote_miniaudio_decoder* decoder, ma_uint64* out_cursor)
{
    if (decoder == NULL || out_cursor == NULL) {
        return MA_INVALID_ARGS;
    }

    return (int)ma_decoder_get_cursor_in_pcm_frames(&decoder->decoder, out_cursor);
}

int mote_miniaudio_decoder_seek_to_pcm_frame(mote_miniaudio_decoder* decoder, ma_uint64 frame_index)
{
    if (decoder == NULL) {
        return MA_INVALID_ARGS;
    }

    return (int)ma_decoder_seek_to_pcm_frame(&decoder->decoder, frame_index);
}

int mote_miniaudio_decoder_read_pcm_frames(mote_miniaudio_decoder* decoder, void* frames_out, ma_uint64 frame_count, ma_uint64* out_frames_read)
{
    if (decoder == NULL) {
        return MA_INVALID_ARGS;
    }

    return (int)ma_decoder_read_pcm_frames(&decoder->decoder, frames_out, frame_count, out_frames_read);
}

int mote_miniaudio_decode_file(const char* path, int format, unsigned int channels, unsigned int sample_rate, mote_miniaudio_decoded_audio* out_audio)
{
    ma_decoder_config config;
    mote_miniaudio_decoder decoder;
    ma_result result;
    void* frames;
    ma_uint64 frame_count;
    if (out_audio == NULL) {
        return MA_INVALID_ARGS;
    }

    config = mote_miniaudio_make_decoder_config(format, channels, sample_rate);
    result = ma_decoder_init_file(path, &config, &decoder.decoder);
    if (result != MA_SUCCESS) {
        return (int)result;
    }

    frames = NULL;
    frame_count = 0;
    result = ma_decode_file(path, &config, &frame_count, &frames);
    if (result != MA_SUCCESS) {
        ma_decoder_uninit(&decoder.decoder);
        return (int)result;
    }

    out_audio->frames = frames;
    out_audio->frame_count = frame_count;
    out_audio->format = (int)decoder.decoder.outputFormat;
    out_audio->channels = decoder.decoder.outputChannels;
    out_audio->sample_rate = decoder.decoder.outputSampleRate;
    ma_decoder_uninit(&decoder.decoder);
    return MA_SUCCESS;
}

int mote_miniaudio_decode_memory(const void* data, size_t data_size, int format, unsigned int channels, unsigned int sample_rate, mote_miniaudio_decoded_audio* out_audio)
{
    ma_decoder_config config;
    mote_miniaudio_decoder decoder;
    ma_result result;
    void* frames;
    ma_uint64 frame_count;
    if (out_audio == NULL) {
        return MA_INVALID_ARGS;
    }

    config = mote_miniaudio_make_decoder_config(format, channels, sample_rate);
    result = ma_decoder_init_memory(data, data_size, &config, &decoder.decoder);
    if (result != MA_SUCCESS) {
        return (int)result;
    }

    frames = NULL;
    frame_count = 0;
    result = ma_decode_memory(data, data_size, &config, &frame_count, &frames);
    if (result != MA_SUCCESS) {
        ma_decoder_uninit(&decoder.decoder);
        return (int)result;
    }

    out_audio->frames = frames;
    out_audio->frame_count = frame_count;
    out_audio->format = (int)decoder.decoder.outputFormat;
    out_audio->channels = decoder.decoder.outputChannels;
    out_audio->sample_rate = decoder.decoder.outputSampleRate;
    ma_decoder_uninit(&decoder.decoder);
    return MA_SUCCESS;
}

void mote_miniaudio_decoded_audio_uninit(mote_miniaudio_decoded_audio* audio)
{
    if (audio == NULL) {
        return;
    }

    if (audio->frames != NULL) {
        ma_free(audio->frames, NULL);
    }

    audio->frames = NULL;
    audio->frame_count = 0;
    audio->format = 0;
    audio->channels = 0;
    audio->sample_rate = 0;
}

int mote_miniaudio_engine_size(void)
{
    return (int)sizeof(mote_miniaudio_engine);
}

int mote_miniaudio_sound_size(void)
{
    return (int)sizeof(mote_miniaudio_sound);
}

int mote_miniaudio_engine_init(const mote_miniaudio_engine_config* config, mote_miniaudio_engine* out_engine)
{
    ma_engine_config engine_config;
    if (out_engine == NULL) {
        return MA_INVALID_ARGS;
    }

    engine_config = ma_engine_config_init();
    if (config != NULL) {
        engine_config.channels = config->channels;
        engine_config.sampleRate = config->sample_rate;
        engine_config.noDevice = config->no_device ? MA_TRUE : MA_FALSE;
        engine_config.noAutoStart = config->no_auto_start ? MA_TRUE : MA_FALSE;
    }

    return (int)ma_engine_init(&engine_config, &out_engine->engine);
}

void mote_miniaudio_engine_uninit(mote_miniaudio_engine* engine)
{
    if (engine == NULL) {
        return;
    }

    ma_engine_uninit(&engine->engine);
}

int mote_miniaudio_engine_start(mote_miniaudio_engine* engine)
{
    if (engine == NULL) {
        return MA_INVALID_ARGS;
    }

    return (int)ma_engine_start(&engine->engine);
}

int mote_miniaudio_engine_stop(mote_miniaudio_engine* engine)
{
    if (engine == NULL) {
        return MA_INVALID_ARGS;
    }

    return (int)ma_engine_stop(&engine->engine);
}

int mote_miniaudio_engine_get_channels(mote_miniaudio_engine* engine)
{
    if (engine == NULL) {
        return 0;
    }

    return (int)ma_engine_get_channels(&engine->engine);
}

int mote_miniaudio_engine_get_sample_rate(mote_miniaudio_engine* engine)
{
    if (engine == NULL) {
        return 0;
    }

    return (int)ma_engine_get_sample_rate(&engine->engine);
}

float mote_miniaudio_engine_get_volume(mote_miniaudio_engine* engine)
{
    if (engine == NULL) {
        return 0.0f;
    }

    return ma_engine_get_volume(&engine->engine);
}

void mote_miniaudio_engine_set_volume(mote_miniaudio_engine* engine, float volume)
{
    if (engine == NULL) {
        return;
    }

    ma_engine_set_volume(&engine->engine, volume);
}

int mote_miniaudio_engine_read_pcm_frames(mote_miniaudio_engine* engine, void* frames_out, ma_uint64 frame_count, ma_uint64* out_frames_read)
{
    if (engine == NULL) {
        return MA_INVALID_ARGS;
    }

    return (int)ma_engine_read_pcm_frames(&engine->engine, frames_out, frame_count, out_frames_read);
}

int mote_miniaudio_sound_init_from_file(mote_miniaudio_engine* engine, const char* path, ma_uint32 flags, mote_miniaudio_sound* out_sound)
{
    if (engine == NULL || out_sound == NULL) {
        return MA_INVALID_ARGS;
    }

    return (int)ma_sound_init_from_file(&engine->engine, path, flags, NULL, NULL, &out_sound->sound);
}

void mote_miniaudio_sound_uninit(mote_miniaudio_sound* sound)
{
    if (sound == NULL) {
        return;
    }

    ma_sound_uninit(&sound->sound);
}

int mote_miniaudio_sound_start(mote_miniaudio_sound* sound)
{
    if (sound == NULL) {
        return MA_INVALID_ARGS;
    }

    return (int)ma_sound_start(&sound->sound);
}

int mote_miniaudio_sound_stop(mote_miniaudio_sound* sound)
{
    if (sound == NULL) {
        return MA_INVALID_ARGS;
    }

    return (int)ma_sound_stop(&sound->sound);
}

int mote_miniaudio_sound_is_playing(const mote_miniaudio_sound* sound)
{
    if (sound == NULL) {
        return 0;
    }

    return ma_sound_is_playing(&sound->sound) ? 1 : 0;
}

void mote_miniaudio_sound_set_volume(mote_miniaudio_sound* sound, float volume)
{
    if (sound == NULL) {
        return;
    }

    ma_sound_set_volume(&sound->sound, volume);
}

float mote_miniaudio_sound_get_volume(const mote_miniaudio_sound* sound)
{
    if (sound == NULL) {
        return 0.0f;
    }

    return ma_sound_get_volume(&sound->sound);
}

int mote_miniaudio_sound_set_looping(mote_miniaudio_sound* sound, int looping)
{
    if (sound == NULL) {
        return MA_INVALID_ARGS;
    }

    ma_sound_set_looping(&sound->sound, looping ? MA_TRUE : MA_FALSE);
    return MA_SUCCESS;
}

int mote_miniaudio_sound_is_looping(const mote_miniaudio_sound* sound)
{
    if (sound == NULL) {
        return 0;
    }

    return ma_sound_is_looping(&sound->sound) ? 1 : 0;
}

int mote_miniaudio_sound_seek_to_pcm_frame(mote_miniaudio_sound* sound, ma_uint64 frame_index)
{
    if (sound == NULL) {
        return MA_INVALID_ARGS;
    }

    return (int)ma_sound_seek_to_pcm_frame(&sound->sound, frame_index);
}

int mote_miniaudio_sound_get_cursor_in_pcm_frames(const mote_miniaudio_sound* sound, ma_uint64* out_cursor)
{
    if (sound == NULL || out_cursor == NULL) {
        return MA_INVALID_ARGS;
    }

    return (int)ma_sound_get_cursor_in_pcm_frames(&sound->sound, out_cursor);
}

int mote_miniaudio_sound_get_length_in_pcm_frames(const mote_miniaudio_sound* sound, ma_uint64* out_length)
{
    if (sound == NULL || out_length == NULL) {
        return MA_INVALID_ARGS;
    }

    return (int)ma_sound_get_length_in_pcm_frames(&sound->sound, out_length);
}

int mote_miniaudio_sound_get_cursor_in_seconds(const mote_miniaudio_sound* sound, float* out_cursor)
{
    if (sound == NULL || out_cursor == NULL) {
        return MA_INVALID_ARGS;
    }

    return (int)ma_sound_get_cursor_in_seconds(&sound->sound, out_cursor);
}

int mote_miniaudio_sound_get_length_in_seconds(const mote_miniaudio_sound* sound, float* out_length)
{
    if (sound == NULL || out_length == NULL) {
        return MA_INVALID_ARGS;
    }

    return (int)ma_sound_get_length_in_seconds(&sound->sound, out_length);
}
