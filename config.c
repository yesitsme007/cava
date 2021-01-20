#include "config.h"
#include "util.h"

#include <ctype.h>
#include "iniparser.h"
#include <math.h>

#ifdef SNDIO
#include <sndio.h>
#endif

#include <stdarg.h>
#include <stdbool.h>
#include <sys/stat.h>

double smoothDef[5] = {1, 1, 1, 1, 1};
#define PACKAGE "cava"

enum input_method default_methods[] = {
    INPUT_FIFO,
    INPUT_PORTAUDIO,
    INPUT_ALSA,
    INPUT_PULSE,
};

char *outputMethod, *channels, *xaxisScale;

const char *input_method_names[] = {
    "fifo", "portaudio", "alsa", "pulse", "sndio", "shmem",
};

const bool has_input_method[] = {
    true, /** Always have at least FIFO and shmem input. */
    HAS_PORTAUDIO, HAS_ALSA, HAS_PULSE, HAS_SNDIO, true,
};

enum input_method input_method_by_name(const char *str) {
    for (int i = 0; i < INPUT_MAX; i++) {
        if (!strcmp(str, input_method_names[i])) {
            return (enum input_method)i;
        }
    }

    return INPUT_MAX;
}

void write_errorf(void *err, const char *fmt, ...) {
    struct error_s *error = (struct error_s *)err;
    va_list args;
    va_start(args, fmt);
    error->length +=
        vsnprintf((char *)error->message + error->length, MAX_ERROR_LEN - error->length, fmt, args);
    va_end(args);
}

int validate_color(char *checkColor, void *params, void *err) {
    struct config_params *p = (struct config_params *)params;
    struct error_s *error = (struct error_s *)err;
    int validColor = 0;
    if (checkColor[0] == '#' && strlen(checkColor) == 7) {
        // If the output mode is not ncurses, tell the user to use a named colour instead of hex
        // colours.
        if (p->om != OUTPUT_NCURSES) {
#ifdef NCURSES
            write_errorf(error,
                         "hex color configured, but ncurses not set. Forcing ncurses mode.\n");
            p->om = OUTPUT_NCURSES;
#else
            write_errorf(error,
                         "Only 'ncurses' output method supports HTML colors "
                         "(required by gradient). "
                         "Cava was built without ncurses support, install ncurses(w) dev files "
                         "and rebuild.\n");
            return 0;
#endif
        }
        // 0 to 9 and a to f
        for (int i = 1; checkColor[i]; ++i) {
            if (!isdigit(checkColor[i])) {
                if (tolower(checkColor[i]) >= 'a' && tolower(checkColor[i]) <= 'f') {
                    validColor = 1;
                } else {
                    validColor = 0;
                    break;
                }
            } else {
                validColor = 1;
            }
        }
    } else {
        if ((strcmp(checkColor, "black") == 0) || (strcmp(checkColor, "red") == 0) ||
            (strcmp(checkColor, "green") == 0) || (strcmp(checkColor, "yellow") == 0) ||
            (strcmp(checkColor, "blue") == 0) || (strcmp(checkColor, "magenta") == 0) ||
            (strcmp(checkColor, "cyan") == 0) || (strcmp(checkColor, "white") == 0) ||
            (strcmp(checkColor, "default") == 0))
            validColor = 1;
    }
    return validColor;
}

bool validate_colors(void *params, void *err) {
    struct config_params *p = (struct config_params *)params;
    struct error_s *error = (struct error_s *)err;

    // validate: color
    if (!validate_color(p->color, p, error)) {
        write_errorf(error, "The value for 'foreground' is invalid. It can be either one of the 7 "
                            "named colors or a HTML color of the form '#xxxxxx'.\n");
        return false;
    }

    // validate: background color
    if (!validate_color(p->bcolor, p, error)) {
        write_errorf(error, "The value for 'background' is invalid. It can be either one of the 7 "
                            "named colors or a HTML color of the form '#xxxxxx'.\n");
        return false;
    }

    if (p->gradient) {
        for (int i = 0; i < p->gradient_count; i++) {
            if (!validate_color(p->gradient_colors[i], p, error)) {
                write_errorf(
                    error,
                    "Gradient color %d is invalid. It must be HTML color of the form '#xxxxxx'.\n",
                    i + 1);
                return false;
            }
        }
    }

    // In case color is not html format set bgcol and col to predefinedint values
    p->col = -1;
    if (strcmp(p->color, "black") == 0)
        p->col = 0;
    if (strcmp(p->color, "red") == 0)
        p->col = 1;
    if (strcmp(p->color, "green") == 0)
        p->col = 2;
    if (strcmp(p->color, "yellow") == 0)
        p->col = 3;
    if (strcmp(p->color, "blue") == 0)
        p->col = 4;
    if (strcmp(p->color, "magenta") == 0)
        p->col = 5;
    if (strcmp(p->color, "cyan") == 0)
        p->col = 6;
    if (strcmp(p->color, "white") == 0)
        p->col = 7;
    // default if invalid

    // validate: background color
    if (strcmp(p->bcolor, "black") == 0)
        p->bgcol = 0;
    if (strcmp(p->bcolor, "red") == 0)
        p->bgcol = 1;
    if (strcmp(p->bcolor, "green") == 0)
        p->bgcol = 2;
    if (strcmp(p->bcolor, "yellow") == 0)
        p->bgcol = 3;
    if (strcmp(p->bcolor, "blue") == 0)
        p->bgcol = 4;
    if (strcmp(p->bcolor, "magenta") == 0)
        p->bgcol = 5;
    if (strcmp(p->bcolor, "cyan") == 0)
        p->bgcol = 6;
    if (strcmp(p->bcolor, "white") == 0)
        p->bgcol = 7;
    // default if invalid

    return true;
}

#ifdef ARTNET
bool validate_artnet(struct config_params *p, struct error_s *error) {
    bool result = true;
    char section_name[100];

    for (int i=0; i < p->no_universes; ++i) {
        snprintf(section_name, sizeof(section_name), "universe-%d", i+1);
        if (p->universes[i].hostname == NULL) {
             write_errorf(error, "Missing expected entry 'hostname' in section %s\n", section_name);
             result = false;
        }

        if (p->universes[i].id < 0) {
             write_errorf(error, "Missing expected entry 'id' in section %s\n", section_name);
             result = false;
        }
    }

    for (int i=0; i < p->no_devices; ++i) {
        snprintf(section_name, sizeof(section_name), "device-%d", i+1);
        if (p->devices[i].color_mapping < 0) {
             write_errorf(error, "Missing or wrong entry 'color_mapping' in section %s\n", section_name);
             result = false;
        } else if (p->devices[i].color_mapping > p->no_mappings) {
             write_errorf(error, "Invalid entry 'color_mapping' in section %s, group is %d, max. allowed is %d\n", 
                section_name, p->devices[i].color_mapping, p->no_mappings);
             result = false;
        }

        if (p->devices[i].universe < 0) {
             write_errorf(error, "Missing or wrong entry 'universe' in section %s\n", section_name);
             result = false;
        } else if (p->devices[i].universe > p->no_universes) {
             write_errorf(error, "Invalid entry 'universe' in section %s, group is %d, max. allowed is %d\n", 
                section_name, p->devices[i].universe, p->no_universes);
        }

        if (p->devices[i].channel_r < 0) {
             write_errorf(error, "Missing expected entry 'channel_red' in section %s\n", section_name);
             result = false;
        }

        if (p->devices[i].channel_g < 0) {
             write_errorf(error, "Missing expected entry 'channel_g' in section %s\n", section_name);
             result = false;
        }

        if (p->devices[i].channel_b < 0) {
             write_errorf(error, "Missing expected entry 'channel_b' in section %s\n", section_name);
             result = false;
        }
        // TODO: check that there are no uknknown universes and groups

        if (p->min_value < 0 || p->min_value > 255) {
             write_errorf(error, "'min_value' in section 'artnet' must be between 0 and 255, but is %d\n", p->min_value);
             result = false;
        }
    }
    
    return result;
}
#endif

bool validate_config(struct config_params *p, struct error_s *error) {
    // validate: output method
    p->om = OUTPUT_NOT_SUPORTED;
    if (strcmp(outputMethod, "ncurses") == 0) {
        p->om = OUTPUT_NCURSES;
        p->bgcol = -1;
#ifndef NCURSES
        write_errorf(error, "cava was built without ncurses support, install ncursesw dev files "
                            "and run make clean && ./configure && make again\n");
        return false;
#endif
    }
    if (strcmp(outputMethod, "noncurses") == 0) {
        p->om = OUTPUT_NONCURSES;
        p->bgcol = 0;
    }
    if (strcmp(outputMethod, "raw") == 0) { // raw:
        p->om = OUTPUT_RAW;
        p->bar_spacing = 0;
        p->bar_width = 1;

        // checking data format
        p->is_bin = -1;
        if (strcmp(p->data_format, "binary") == 0) {
            p->is_bin = 1;
            // checking bit format:
            if (p->bit_format != 8 && p->bit_format != 16) {
                write_errorf(
                    error,
                    "bit format  %d is not supported, supported data formats are: '8' and '16'\n",
                    p->bit_format);
                return false;
            }
        } else if (strcmp(p->data_format, "ascii") == 0) {
            p->is_bin = 0;
            if (p->ascii_range < 1) {
                write_errorf(error, "ascii max value must be a positive integer\n");
                return false;
            }
        } else {
            write_errorf(error,
                         "data format %s is not supported, supported data formats are: 'binary' "
                         "and 'ascii'\n",
                         p->data_format);
            return false;
        }
    }
#ifdef ARTNET    
    if (strcmp(outputMethod, "artnet") == 0) {
        printf("Using Artnet output\n");
        p->om = OUTPUT_ARTNET;
        p->bar_spacing = 0;
        p->bar_width = 1;
        p->autobars = false;
    }
#endif    
    if (p->om == OUTPUT_NOT_SUPORTED) {
#ifndef NCURSES
        write_errorf(
            error,
            "output method %s is not supported, supported methods are: 'noncurses' and 'raw'\n",
            outputMethod);
        return false;
#endif

#ifdef NCURSES
        write_errorf(error,
                     "output method %s is not supported, supported methods are: 'ncurses', "
                     "'noncurses' and 'raw'\n",
                     outputMethod);
        return false;
#endif
    }

    p->xaxis = NONE;
    if (strcmp(xaxisScale, "none") == 0) {
        p->xaxis = NONE;
    }
    if (strcmp(xaxisScale, "frequency") == 0) {
        p->xaxis = FREQUENCY;
    }
    if (strcmp(xaxisScale, "note") == 0) {
        p->xaxis = NOTE;
    }

    // validate: output channels
    p->stereo = -1;
    if (strcmp(channels, "mono") == 0) {
        p->stereo = 0;
        if (strcmp(p->mono_option, "average") != 0 && strcmp(p->mono_option, "left") != 0 &&
            strcmp(p->mono_option, "right") != 0) {

            write_errorf(error,
                         "mono option %s is not supported, supported options are: 'average', "
                         "'left' or 'right'\n",
                         p->mono_option);
            return false;
        }
    }
    if (strcmp(channels, "stereo") == 0)
        p->stereo = 1;
    if (p->stereo == -1) {
        write_errorf(
            error,
            "output channels %s is not supported, supported channelss are: 'mono' and 'stereo'\n",
            channels);
        return false;
    }

    // validate: bars
    p->autobars = 1;
    if (p->fixedbars > 0)
        p->autobars = 0;
    if (p->fixedbars > 256)
        p->fixedbars = 256;
    if (p->bar_width > 256)
        p->bar_width = 256;
    if (p->bar_width < 1)
        p->bar_width = 1;

    // validate: framerate
    if (p->framerate < 0) {
        write_errorf(error, "framerate can't be negative!\n");
        return false;
    }

    // validate: colors
    if (!validate_colors(p, error)) {
        return false;
    }

    // validate: gravity
    p->gravity = p->gravity / 100;
    if (p->gravity < 0) {
        p->gravity = 0;
    }

    // validate: integral
    p->integral = p->integral / 100;
    if (p->integral < 0) {
        p->integral = 0;
    } else if (p->integral > 1) {
        p->integral = 1;
    }

    // validate: cutoff
    if (p->lower_cut_off == 0)
        p->lower_cut_off++;
    if (p->lower_cut_off > p->upper_cut_off) {
        write_errorf(error,
                     "lower cutoff frequency can't be higher than higher cutoff frequency\n");
        return false;
    }

    // setting sens
    p->sens = p->sens / 100;

#ifdef ARTNET
    // validate: artnet configuration
    if (p->om == OUTPUT_ARTNET && !validate_artnet(p, error)) {
        return false;
    }
#endif

    return true;
}

bool load_colors(struct config_params *p, dictionary *ini, void *err) {
    struct error_s *error = (struct error_s *)err;

    free(p->color);
    free(p->bcolor);

    p->color = strdup(iniparser_getstring(ini, "color:foreground", "default"));
    p->bcolor = strdup(iniparser_getstring(ini, "color:background", "default"));

    p->gradient = iniparser_getint(ini, "color:gradient", 0);
    if (p->gradient) {
        for (int i = 0; i < p->gradient_count; ++i) {
            free(p->gradient_colors[i]);
        }
        p->gradient_count = iniparser_getint(ini, "color:gradient_count", 8);
        if (p->gradient_count < 2) {
            write_errorf(error, "\nAtleast two colors must be given as gradient!\n");
            return false;
        }
        if (p->gradient_count > 8) {
            write_errorf(error, "\nMaximum 8 colors can be specified as gradient!\n");
            return false;
        }
        p->gradient_colors = (char **)malloc(sizeof(char *) * p->gradient_count * 9);
        p->gradient_colors[0] =
            strdup(iniparser_getstring(ini, "color:gradient_color_1", "#59cc33"));
        p->gradient_colors[1] =
            strdup(iniparser_getstring(ini, "color:gradient_color_2", "#80cc33"));
        p->gradient_colors[2] =
            strdup(iniparser_getstring(ini, "color:gradient_color_3", "#a6cc33"));
        p->gradient_colors[3] =
            strdup(iniparser_getstring(ini, "color:gradient_color_4", "#cccc33"));
        p->gradient_colors[4] =
            strdup(iniparser_getstring(ini, "color:gradient_color_5", "#cca633"));
        p->gradient_colors[5] =
            strdup(iniparser_getstring(ini, "color:gradient_color_6", "#cc8033"));
        p->gradient_colors[6] =
            strdup(iniparser_getstring(ini, "color:gradient_color_7", "#cc5933"));
        p->gradient_colors[7] =
            strdup(iniparser_getstring(ini, "color:gradient_color_8", "#cc3333"));
    }
    return true;
}
int get_hue_for_color_string(const char* value) {
    const char* key = strchr(value, ':');
    if (key == NULL) {
        key = value;
    } else {
        key++;
    }
    if (strcmp(key, "red") == 0) {
        return 0;
    } else if (strcmp(key, "green") == 0) {
        return 120;
    } else if (strcmp(key, "blue") == 0) {
        return 240;
    } else if (strcmp(key, "yellow") == 0) {
        return 60;
    } else if (strcmp(key, "cyan") == 0) {
        return 120;
    } else if (strcmp(key, "magenta") == 0) {
        return 300;
    } else if (strspn(key, "0123456789") == strlen(key)){
        return atoi(key);
    } else {
        return -1;
    }
}

bool load_config(char configPath[PATH_MAX], struct config_params *p, bool colorsOnly,
                 struct error_s *error) {
    FILE *fp;

    // config: creating path to default config file
    if (configPath[0] == '\0') {
        char *configFile = "config";
        char *configHome = getenv("XDG_CONFIG_HOME");
        if (configHome != NULL) {
            sprintf(configPath, "%s/%s/", configHome, PACKAGE);
        } else {
            configHome = getenv("HOME");
            if (configHome != NULL) {
                sprintf(configPath, "%s/%s/", configHome, ".config");
                mkdir(configPath, 0777);
                sprintf(configPath, "%s/%s/%s/", configHome, ".config", PACKAGE);
            } else {
                write_errorf(error, "No HOME found (ERR_HOMELESS), exiting...");
                return false;
            }
        }

        // config: create directory
        mkdir(configPath, 0777);

        // config: adding default filename file
        strcat(configPath, configFile);

        fp = fopen(configPath, "ab+");
        if (fp) {
            fclose(fp);
        } else {
            write_errorf(error, "Unable to access config '%s', exiting...\n", configPath);
            return false;
        }

    } else { // opening specified file

        fp = fopen(configPath, "rb+");
        printf("Loading config file %s\n", configPath);
        if (fp) {
            fclose(fp);
        } else {
            write_errorf(error, "Unable to open file '%s', exiting...\n", configPath);
            return false;
        }
    }

    // config: parse ini
    dictionary *ini;
    ini = iniparser_load(configPath);

    if (colorsOnly) {
        if (!load_colors(p, ini, error)) {
            return false;
        }
        return validate_colors(p, error);
    }

#ifdef NCURSES
    outputMethod = (char *)iniparser_getstring(ini, "output:method", "ncurses");
#endif
#ifndef NCURSES
    outputMethod = (char *)iniparser_getstring(ini, "output:method", "noncurses");
#endif

    xaxisScale = (char *)iniparser_getstring(ini, "output:xaxis", "none");
    p->monstercat = 1.5 * iniparser_getdouble(ini, "smoothing:monstercat", 0);
    p->waves = iniparser_getint(ini, "smoothing:waves", 0);
    p->integral = iniparser_getdouble(ini, "smoothing:integral", 77);
    p->gravity = iniparser_getdouble(ini, "smoothing:gravity", 100);
    p->ignore = iniparser_getdouble(ini, "smoothing:ignore", 0);

    if (!load_colors(p, ini, error)) {
        return false;
    }

    p->fixedbars = iniparser_getint(ini, "general:bars", 0);
    p->bar_width = iniparser_getint(ini, "general:bar_width", 2);
    p->bar_spacing = iniparser_getint(ini, "general:bar_spacing", 1);
    p->framerate = iniparser_getint(ini, "general:framerate", 60);
    p->sens = iniparser_getint(ini, "general:sensitivity", 100);
    p->autosens = iniparser_getint(ini, "general:autosens", 1);
    p->overshoot = iniparser_getint(ini, "general:overshoot", 20);
    p->lower_cut_off = iniparser_getint(ini, "general:lower_cutoff_freq", 50);
    p->upper_cut_off = iniparser_getint(ini, "general:higher_cutoff_freq", 10000);
    p->sleep_timer = iniparser_getint(ini, "general:sleep_timer", 0);

    // config: output
    free(channels);
    free(p->mono_option);
    free(p->raw_target);
    free(p->data_format);

    channels = strdup(iniparser_getstring(ini, "output:channels", "stereo"));
    p->mono_option = strdup(iniparser_getstring(ini, "output:mono_option", "average"));
    p->raw_target = strdup(iniparser_getstring(ini, "output:raw_target", "/dev/stdout"));
    p->data_format = strdup(iniparser_getstring(ini, "output:data_format", "binary"));
    p->bar_delim = (char)iniparser_getint(ini, "output:bar_delimiter", 59);
    p->frame_delim = (char)iniparser_getint(ini, "output:frame_delimiter", 10);
    p->ascii_range = iniparser_getint(ini, "output:ascii_max_range", 1000);
    p->bit_format = iniparser_getint(ini, "output:bit_format", 16);

    // read & validate: eq
    p->userEQ_keys = iniparser_getsecnkeys(ini, "eq");
    if (p->userEQ_keys > 0) {
        p->userEQ_enabled = 1;
        p->userEQ = (double *)calloc(p->userEQ_keys + 1, sizeof(double));
#ifndef LEGACYINIPARSER
        const char *keys[p->userEQ_keys];
        iniparser_getseckeys(ini, "eq", keys);
#endif
#ifdef LEGACYINIPARSER
        char **keys = iniparser_getseckeys(ini, "eq");
#endif
        for (int sk = 0; sk < p->userEQ_keys; sk++) {
            p->userEQ[sk] = iniparser_getdouble(ini, keys[sk], 1);
        }
    } else {
        p->userEQ_enabled = 0;
    }

    free(p->audio_source);

    char *input_method_name;
    for (size_t i = 0; i < ARRAY_SIZE(default_methods); i++) {
        enum input_method method = default_methods[i];
        if (has_input_method[method]) {
            input_method_name =
                (char *)iniparser_getstring(ini, "input:method", input_method_names[method]);
        }
    }

    p->im = input_method_by_name(input_method_name);
    switch (p->im) {
#ifdef ALSA
    case INPUT_ALSA:
        p->audio_source = strdup(iniparser_getstring(ini, "input:source", "hw:Loopback,1"));
        break;
#endif
    case INPUT_FIFO:
        p->audio_source = strdup(iniparser_getstring(ini, "input:source", "/tmp/mpd.fifo"));
        p->fifoSample = iniparser_getint(ini, "input:sample_rate", 44100);
        p->fifoSampleBits = iniparser_getint(ini, "input:sample_bits", 16);
        break;
#ifdef PULSE
    case INPUT_PULSE:
        p->audio_source = strdup(iniparser_getstring(ini, "input:source", "auto"));
        break;
#endif
#ifdef SNDIO
    case INPUT_SNDIO:
        p->audio_source = strdup(iniparser_getstring(ini, "input:source", SIO_DEVANY));
        break;
#endif
    case INPUT_SHMEM:
        p->audio_source =
            strdup(iniparser_getstring(ini, "input:source", "/squeezelite-00:00:00:00:00:00"));
        break;
#ifdef PORTAUDIO
    case INPUT_PORTAUDIO:
        p->audio_source = strdup(iniparser_getstring(ini, "input:source", "auto"));
        break;
#endif
    case INPUT_MAX: {
        char supported_methods[255] = "";
        for (int i = 0; i < INPUT_MAX; i++) {
            if (has_input_method[i]) {
                strcat(supported_methods, "'");
                strcat(supported_methods, input_method_names[i]);
                strcat(supported_methods, "' ");
            }
        }
        write_errorf(error, "input method '%s' is not supported, supported methods are: %s\n",
                     input_method_name, supported_methods);
        return false;
    }
    default:
        write_errorf(error, "cava was built without '%s' input support\n",
                     input_method_names[p->im]);
        return false;
    }

#ifdef ARTNET
    if (strcmp(outputMethod, "artnet") == 0) {
        printf("Configurig Artnet\n");
        if (p->fixedbars <= 0) {
            write_errorf(error, "Artnet needs fixed number of bars, please configure artnet/bars to positive number");
            return  false;
        }
        int no_devices = iniparser_getint(ini, "artnet:no_devices", 0);
        if (no_devices == 0) {
            write_errorf(error, "Artnet needs at least one device, please configure artnet/no_devices");
            return  false;
        }
        int no_universes = iniparser_getint(ini, "artnet:no_universes", 0);
        if (no_universes == 0) {
            write_errorf(error, "Artnet needs at least one univers, please configure artnet/no_universes");
            return  false;
        }
        int no_mappings = iniparser_getint(ini, "artnet:no_color_mappings", 0);
        if (no_mappings == 0) {
            write_errorf(error, "Artnet needs number of color-mappings, please configure artnet/no_color_mappings");
            return  false;
        }
        p->no_mappings = no_mappings;
        p->min_value = iniparser_getint(ini, "artnet:min_value", 0);
        
        char section_name[100];
        char key_name[120];
        printf("Allocating space for universes: %d and devices: %d\n", no_universes, no_devices);
        cfg_artnet_alloc(p, no_universes, no_devices, 0);

        // read universes:
        for (int i=0; i < no_universes; ++i) {
            snprintf(section_name, sizeof(section_name), "universe-%d", i+1);
            int no_entries = iniparser_getsecnkeys(ini, section_name);
            if (no_entries == 0) {
                write_errorf(error, "Missing expected section %s", section_name);
                return  false;
            }
            snprintf(key_name, sizeof(key_name), "%s:%s", section_name, "id");
            int id = iniparser_getint(ini, key_name, -1);
            snprintf(key_name, sizeof(key_name), "%s:%s", section_name, "host");
            const char* hostname = iniparser_getstring(ini, key_name, NULL);
            snprintf(key_name, sizeof(key_name), "%s:%s", section_name, "port");
            int port = iniparser_getint(ini, key_name, 0);
            printf("Adding universe: %d, host: %s, id: %d, port: %d\n", i, hostname, id, port);
            cfg_add_universe(&(p->universes[i]), id, hostname, port);
        }
        // read devices:
        for (int i=0; i < no_devices; ++i) {
            snprintf(section_name, sizeof(section_name), "device-%d", i+1);
            int no_entries = iniparser_getsecnkeys(ini, section_name);
            if (no_entries == 0) {
                write_errorf(error, "Missing expected section %s", section_name);
                return  false;
            }
            snprintf(key_name, sizeof(key_name), "%s:%s", section_name, "universe");
            int universe = iniparser_getint(ini, key_name, -1);
            snprintf(key_name, sizeof(key_name), "%s:%s", section_name, "color_mapping");
            int color_mapping = iniparser_getint(ini, key_name, -1);
            snprintf(key_name, sizeof(key_name), "%s:%s", section_name, "channel_red");
            int channel_red = iniparser_getint(ini, key_name, -1);
            snprintf(key_name, sizeof(key_name), "%s:%s", section_name, "channel_green");
            int channel_green = iniparser_getint(ini, key_name, -1);
            snprintf(key_name, sizeof(key_name), "%s:%s", section_name, "channel_blue");
            int channel_blue = iniparser_getint(ini, key_name, -1);
            printf("Set device: %d, universe: %d, color_mapping; %d, channels r: %d, g: %d, b: %d\n", 
                i, universe, color_mapping, channel_red, channel_green, channel_blue);
            DeviceT* device = &p->devices[i];
            device->universe = universe-1;
            device->color_mapping = color_mapping-1;
            device->channel_r = channel_red;
            device->channel_g = channel_green;
            device->channel_b = channel_blue;
        }
        // read color-mappings:
        printf("configure color-mappings %d\n", no_mappings);
        TColorMaps **all_mappings = artnet_alloc_color_map_array(no_mappings);
        p->no_mappings = no_mappings;
        p->mappings = all_mappings;
        for (int i=0; i < no_mappings; ++i) {
            snprintf(section_name, sizeof(section_name), "color_mapping-%d", i+1);
            int no_entries = iniparser_getsecnkeys(ini, section_name);
            if (no_entries == 0) {
                write_errorf(error, "Missing expected section %s", section_name);
                return  false;
            }
            // get all key/value pairs
            printf("  read section %s\n", section_name);
            no_entries = iniparser_getsecnkeys(ini, section_name);
            TColorMaps *color_map =  artnet_alloc_color_map(no_entries);
            color_map->no_color_maps = no_entries;
            const char* keys[no_entries];
            iniparser_getseckeys(ini, section_name, keys);
            for (int j=0; j < no_entries; ++j) {
                int band = iniparser_getint(ini, keys[j], -1);
                int hue = get_hue_for_color_string(keys[j]);
                if (hue == -1) {
                    printf("Illegal hue value %s, must be 0 - 360 or red, green, blue, cyan, magenta, yellow", keys[j]);
                } else {
                    printf("add band: %d, hue: %d, key: %s\n", band, hue, keys[j]);
                    color_map->maps[j].band = band;
                    color_map->maps[j].hue = hue;
                }
            }
            all_mappings[i] = color_map;
        }
    }
#endif

    bool result = validate_config(p, error);
    iniparser_freedict(ini);
    return result;
}

#ifdef ARTNET
void cfg_artnet_alloc (struct config_params* cfg, int no_universes, int no_devices, int no_mappings) {
  cfg->no_universes = no_universes;
  printf("Alloc universes: %d\n",no_universes);
  cfg->universes = malloc(no_universes * sizeof(UniverseT));
  cfg->no_devices = no_devices;
  printf("Alloc devices: %d\n",no_devices);
  cfg->devices = malloc(no_devices * sizeof(DeviceT));
  cfg->no_mappings = no_mappings;
  cfg->mappings = malloc(no_mappings * sizeof(TColorMaps));
}

void cfg_add_universe(UniverseT* universe, int universe_id, const char* hostname, int port) {
  if (hostname != NULL) {
    universe->hostname = malloc(strlen(hostname)+1);
    strcpy(universe->hostname, hostname);
  }
  universe->id = universe_id;
  universe->port = port;
}

TColorMaps* artnet_alloc_color_map(int no_mappings){
  TColorMaps* result = malloc(sizeof(struct color_map) * no_mappings + sizeof(int));
  result->no_color_maps = no_mappings;
  return result;
}

void artnet_free_color_map(TColorMaps* color_map) {
  free(color_map);
}

TColorMaps** artnet_alloc_color_map_array(int no_mappings) {
  TColorMaps** result = malloc(sizeof(TColorMaps*) * no_mappings);
  return result;
}

void artnet_free_color_map_array(TColorMaps** color_map_array) {
    free(color_map_array);
}

void cfg_artnet_free (struct config_params* cfg) {
  for (int i=0; i<cfg->no_universes; ++i) {
    if (cfg->universes[i].hostname != NULL) {
        free(cfg->universes[i].hostname);
    }
  }
  free(cfg->universes);
  free(cfg->devices);
  if (cfg->no_mappings > 0) {
    for (int i=0; i<cfg->no_mappings; ++i) {
        artnet_free_color_map(cfg->mappings[i]);
    }
    artnet_free_color_map_array(cfg->mappings);
  }
}
#endif