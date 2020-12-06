C.A.V.A. 
====================

**C**onsole-based **A**udio **V**isualizer for **A**LSA

This is a fork of the [CAVA](https://github.com/karlstav/cava/) project from Karl Stavestrand.
In addition to the original project this fork support an additional output format for Artnet devices.

This code is currently under development. 

[Art-net](https://en.wikipedia.org/wiki/Art-Net) is a network protocol for transmitting ligthing control
information to DMX devices over a network.

Goal of this project is to map a number of frequency bands to colors. Each band is assigned a hue value.
Saturation and value is calculated from the output of CAVA. Multiple DMX devices can be in multiple 
universes and assigned to groups. All devices in the same group show the same color.

Detailed configuration information still needs to be added.

Thanks to:
* [karlstav](https://github.com/karlstav)

for this project.
