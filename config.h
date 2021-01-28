#pragma once

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define MAX_ERROR_LEN 1024

#ifdef PORTAUDIO
#define HAS_PORTAUDIO true
#else
#define HAS_PORTAUDIO false
#endif

#ifdef ALSA
#define HAS_ALSA true
#else
#define HAS_ALSA false
#endif

#ifdef PULSE
#define HAS_PULSE true
#else
#define HAS_PULSE false
#endif

#ifdef SNDIO
#define HAS_SNDIO true
#else
#define HAS_SNDIO false
#endif

// These are in order of least-favourable to most-favourable choices, in case
// multiple are supported and configured.
enum input_method {
    INPUT_FIFO,
    INPUT_PORTAUDIO,
    INPUT_ALSA,
    INPUT_PULSE,
    INPUT_SNDIO,
    INPUT_SHMEM,
    INPUT_MAX
};

enum output_method { OUTPUT_NCURSES, OUTPUT_NONCURSES, OUTPUT_RAW, OUTPUT_ARTNET, OUTPUT_NOT_SUPORTED };

enum xaxis_scale { NONE, FREQUENCY, NOTE };

struct device {
  int universe;
  int channel_r;
  int channel_g;
  int channel_b;
  int group;
};

typedef struct device DeviceT;

struct config_params {
    char *color, *bcolor, *raw_target, *audio_source,
        /**gradient_color_1, *gradient_color_2,*/ **gradient_colors, *data_format, *mono_option;
    char bar_delim, frame_delim;
    double monstercat, integral, gravity, ignore, sens;
    unsigned int lower_cut_off, upper_cut_off;
    double *userEQ;
    enum input_method im;
    enum output_method om;
    enum xaxis_scale xaxis;
    int userEQ_keys, userEQ_enabled, col, bgcol, autobars, stereo, is_bin, ascii_range, bit_format,
        gradient, gradient_count, fixedbars, framerate, bar_width, bar_spacing, autosens, overshoot,
        waves, fifoSample, fifoSampleBits, sleep_timer;
    
    int no_bars; // number of frequency bands
    int no_universes;
    int no_devices;
    DeviceT* devices;

    const char** universes;
    int no_colors;  // number of colors each device will display (distributed evenly across hue 0-360°)
};

struct error_s {
    char message[MAX_ERROR_LEN];
    int length;
};

bool load_config(char configPath[PATH_MAX], struct config_params *p, bool colorsOnly,
                 struct error_s *error);
                 
void cfg_artnet_alloc (struct config_params* cfg, int no_devices);
void cfg_artnet_free (struct config_params* cfg);