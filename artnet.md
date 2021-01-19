# Artnet for Cava
All configuration is done in the Cava configuration file (see Cava documentation). There are new sections and entries for Artnet.

First we need to configure universes. Universes in Artnet have an IP address and a number of DMX channels. Then we have devices. Each device has a number of DMX channels and belongs to a universe. Relevant here are the red, green and blue channels of each device. The last entity are color mappings. Color mappings define how frequency bands are mapped to colors. Each device uses a color mapping. If multiple devices use the same color mapping they will show the same lighting.

Also relevant are other settings in the Cava configuration. `Framerate` defines how often data are sent out. High rates give better accuracy but cause more CPU load and network traffic. `Bars` configures the number of frequency bands. The output `method` entry must be set to `artnet`. All other settings (except those that are terminal specific) will be considered as well. Artnet output is similar to Cava's raw output.

## Color Model
Artnet for Cava uses the [HSV color model](https://en.wikipedia.org/wiki/HSL_and_HSV) for configuration. Hue is mapped to a frequency band (bar number in cava), saturation is fixed and value is mapped to the value of the frequency band. Only the value will change over time.

## Example
How it works is best explained with an example. Here we assume a setup with four PAR64 spots. Each PAR64 has 5 DMX channels, where three are for RGB and the other two are not important here. We will use four different color mappings one fore each device. Thus each device will show different colors.

```
[Artnet]
no_universes=2
no_devices=4
no_color_mappings=4
min_value=10

[universe-1]
id=1
host=192.168.10.1
;port should be rarely set, defaults to 6454
;port=6454

[universe-2]
id=2
host=192.168.10.2

[device-1]
universe = 1
group = 1
channel_red = 2
channel_green = 3
channel_blue = 4

[device-2]
universe = 1
group = 2
channel_red = 7
channel_green = 8
channel_blue = 9

[device-3]
universe = 2
group = 3
channel_red = 2
channel_green = 3
channel_blue = 4

[device-4]
universe = 2
group = 4
channel_red = 7
channel_green = 8
channel_blue = 9

[color_mapping-1]
red=1
;green=10

[color_mapping-2]
blue=2

[color_mapping-3]
blue=1

[color_mapping-4]
green = 10

[output]
method = artnet
channels = mono
# bit_format must be 8bit for Artnet
bit_format = 8bit

[general]
framereate = 50
bars = 12
```

In this configuration we have two universes. Each universe has two devices (e.g. PAR64 spots). The first uses DMX channels 2, 3 and 4 (for reg, green, blue) in universe 1. The second uses channels 7, 8 and 9 in universe 1. The third uses channels 2, 3 and 4 in universe 2 and the fourth uses channels 7, 8 and 9 in universe 2. The host entry can either be a DNS name or an IP address.

We define 4 color mappings, so that each devices shows something different. The first color mapping maps band 1 (lowest, bass) to color red. The second uses blue color for band 2. The third mapping also uses band 1 but maps it to blue. And the fourth mapping uses green for band 10 (higher frequencies, treble).

We configure device 1 to use color mapping 1, device 2 to use color mapping 2, device 3 uses mapping 3 and device 4 uses mapping 4. In this configuration device 1 and 3 will show the same pattern (because they use the same band) but one shows it in red color, and the other in blue.

This configuration sends 50 packets per second to each universe and uses 12 frequency bands in total.

If you want to map colors to stereo channels calculate the bar numbers according to how Cava does this. Check with terminal output for comparison.

You can also configure devices to react to multiple bands, e.g.:

```
[color_mapping-3]
red=3
yellow = 9
blue=1
```

Devices using this mapping will use the red color for band 3, blue for band 1 and yellow for band 9.

The supported color names are: red, green, blue, cyan, magenta and yellow. For other colors use the hue angle (0-359) as decimal (see color model).

The entries `no_universes, no_devices, no_color_mappings` define the number of expected sections. So `no_devices=3` implies that sections `[device-1], [device-1]` and `[device-1]` will follow below.

The entry `min_value` defines a threshold value. If all values from the retrieved bands in the color mapping are below this value (range 0-255) then this value will be set. This can be used to avoid complete darkness for example if there is short pause within a song.

### Miscellanous
You can use the number of total bars in Cava to set the bandwidth of each band (high number, many narrow bands or low number few broad bands).
It works best if you map colors so that they do not overlap within a device. E.g. mapping band 0 to red and band 5 to green works good. If you map band 5 to yellow instead then yellow is mixed by red and green internally and red will overlap with band 0. This is possible, but you will notice then that red is on most of the time. Avoid using too many bands and colors in one device. This will quickly result in bright colors most of the time often close to white. Less is more here and shows nicer colors.

You also can configure multiple bands to the same color in a color mapping. Then it will react to both bands (not recommended).