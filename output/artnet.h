#pragma once

#include "config.h"

struct artnet {
  int no_colors;        // required on input
  int no_universes;     // required on input
  UniverseT* universes; // required on input
  int no_sockets;
  int* sockets;
  int no_groups;
  int* num_devices_in_group;
  DeviceT*** devices_in_group;
  int no_devices;       // required on input
  DeviceT* devices;     // required on input
  uint8_t **dmx_buffers;
  uint32_t max_colorvalue;
  int no_mappings;      // required on input
  TColorMaps *mappings; // required on input
};

typedef struct artnet ArtnetT;


ArtnetT* init_artnet(struct config_params* cfg, int no_bars, bool connect);
void free_artnet(ArtnetT* artnet);

int update_colors(ArtnetT* artnet, int bars_count, int f[200]);
void init_artnet_groups(ArtnetT* artnet);
void init_max_colors(ArtnetT* artnet);
void test_mapping(ArtnetT* artnet, int bars_count);
void print_artnet_stats();