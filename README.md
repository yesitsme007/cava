C.A.V.A. 
====================

**C**onsole-based **A**udio **V**isualizer for **A**LSA

This is a fork of the [CAVA](https://github.com/karlstav/cava/) project from Karl Stavestrand.
In addition to the original project this fork supports an additional output format for Artnet devices.

This code is currently under development. 

[Art-net](https://en.wikipedia.org/wiki/Art-Net) is a network protocol for transmitting ligthing control
information to DMX devices over a network.

Goal of this project is to use Cava for controlling stage lighting via the Artnet network protocol.
Frequency bands can be assigned to colors and the value calculated from Cava decides about intensity.
Multiple DMX devices can be distributed across multiple universes. Color mappings ran be reused and
shared across devices.

For detailed configuration information see [Artnet documentation](artnet.md).

Thanks to:
* [karlstav](https://github.com/karlstav)

for this project.
