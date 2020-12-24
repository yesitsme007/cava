#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <math.h>
#include <unistd.h>
#include "config.h"
#include "artnet.h"
#include "color.h"

static int exceed_counter = 0;
static int packet_counter = 0;
static const int artnet_port = 6454;

static const uint8_t dmx_header[] = {'A', 'r', 't', '-', 'N', 'e', 't', 0x0, 
  0x0, 0x50, // op-code 5000
  0x0, 0x14, // protocol version
  0x0,       // sequence disabled
  0x0,       // physical port
  0x0, 0x0,  // universe, net
  0x2, 0x0   // length always 512 bytes
};
const int dmx_buffer_size = sizeof(dmx_header) + 512;

static inline int min(int x, int y) {
    return y < x ? y : x;
}

#define MAX(X,Y) ((X) > (Y) ? (X) : (Y))

static int connect_udp(const char* host, const char* port_str) {
  struct addrinfo hints;
  struct addrinfo *result, *rp;
  int sfd, s;
  const int y=1;
     
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
  hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
  //hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */

  s = getaddrinfo(host, port_str, &hints, &result);
  if (s != 0) {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
      return -1;
  }

  for (rp = result; rp != NULL; rp = rp->ai_next) {
    printf("create socket\n");
    //struct sockaddr_in s_in;

    sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (sfd == -1) {
      printf("create socket failed\n");
      continue;
    }
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &y, sizeof(int));

    if (connect(sfd, rp->ai_addr, rp->ai_addrlen) >= 0) {
        printf("connect successful\n");
        break;
    }
    // s_in.sin_family = AF_INET;
    // s_in.sin_addr.s_addr = INADDR_ANY;
    // s_in.sin_port = htons(0);
    // if (bind(sfd, (struct sockaddr*) &s_in, sizeof(s_in)) == 0) {
    // //   bind(sfd, rp->ai_addr, rp->ai_addrlen)
    //     printf("bind successful\n");
    //     break;                  /* Success */
    // }
    close(sfd);
  }

  freeaddrinfo(result);           /* No longer needed */

  if (rp == NULL) {               /* No address succeeded */
    fprintf(stderr, "Could not connect to %s, port: %s\n", host, port_str);
    return -1;
  }

  return sfd;
}

void init_artnet_udp(ArtnetT* artnet) {
  for (int i=0; i < artnet->no_universes; i++) {
    char port[12];
    int socket;
    printf("init universe: %s\n", artnet->universes[i].hostname);
    if (artnet->universes[i].port <= 0) {
      snprintf(port, sizeof(port), "%d", artnet_port);
      fprintf(stderr, "Using default Artnet port: %s\n", port);
    } else {
      snprintf(port, sizeof(port), "%d", artnet->universes[i].port);
    }
    printf("connect udp host: %s, port: %s\n", artnet->universes[i].hostname, port);
    socket = connect_udp(artnet->universes[i].hostname, port);
    if (socket >= 0) {
      printf("universe: %d, socket: %d\n", i, socket);
      artnet->sockets[i] = socket;
    }
  }
}

ArtnetT* init_artnet(struct config_params* cfg, int no_bars, bool connect) {
  // struct sockaddr_in addr;
  printf("init_artnet\n");

  ArtnetT* artnet = malloc(sizeof(ArtnetT));
  memset(artnet, 0, sizeof(ArtnetT));

  printf("init_artnet universes\n");
  artnet->no_universes = cfg->no_universes;
  if (artnet->no_universes > 0) {
    printf("alloc universes: %d\n", artnet->no_universes);
    artnet->universes = malloc(artnet->no_universes * sizeof(UniverseT));
    for (int i=0; i< artnet->no_universes; ++i) {
      printf("host: %s, len: %u\n", cfg->universes[i].hostname, strlen(cfg->universes[i].hostname)+1);
      artnet->universes[i].hostname = malloc(strlen(cfg->universes[i].hostname)+1);
      strcpy(artnet->universes[i].hostname, cfg->universes[i].hostname);
      artnet->universes[i].port = cfg->universes[i].port;
      artnet->universes[i].id = cfg->universes[i].id;
    }
  }
  
  printf("init_artnet sockets\n");
  artnet->sockets = malloc(cfg->no_universes * sizeof(int));
  memset(artnet->sockets, 0, cfg->no_universes * sizeof(int));
  artnet->no_sockets = cfg->no_universes;

  if (connect) {
    printf("connect sockets\n");
    init_artnet_udp(artnet);
  }

  // copy devices from config
  printf("init_artnet devices for %d bands\n", no_bars);
  artnet->no_devices = cfg->no_devices;
  artnet->devices = malloc(cfg->no_devices * sizeof(DeviceT));
  printf("no colors-mappings: %d, no devices: %d\n", cfg->no_mappings, cfg->no_devices);
  memcpy(artnet->devices, cfg->devices, cfg->no_devices * sizeof(DeviceT));
  for (int i=0; i < cfg->no_devices; ++i) {
    DeviceT device = artnet->devices[i];
    printf("device %d, universe: %d, group: %d, channel r: %d, g: %d, b: %d\n", 
       i, device.universe, device.group, device.channel_r, device.channel_g, device.channel_b);
  }

  printf("Init color mappings: %d\n", cfg->no_mappings);
  init_artnet_color_mappings(artnet, cfg);
  print_color_mappings(artnet);

  init_artnet_groups(artnet);

  artnet->dmx_buffers = malloc(artnet->no_universes);
  memset(artnet->dmx_buffers, 0,  artnet->no_universes);
  for (int i=0; i< artnet->no_universes; ++i) {
    artnet->dmx_buffers[i] = malloc(dmx_buffer_size);
    memcpy(artnet->dmx_buffers[i], dmx_header, sizeof(dmx_header));
    // set universe id
    int universe_id = artnet->universes[i].id;
    artnet->dmx_buffers[i][14] = (uint8_t)(universe_id & 0xFF);
    artnet->dmx_buffers[i][15] = (uint8_t)((universe_id >> 8) & 0xFF);
  }
  // init_max_colors(artnet);
  return artnet;
}

void init_artnet_groups(ArtnetT* artnet) {
  artnet->num_devices_in_group = malloc(artnet->no_mappings * sizeof(int));
  memset(artnet->num_devices_in_group, 0, artnet->no_mappings * sizeof(int));

  for (int i=0; i < artnet->no_devices; ++i) {
    int current_group = artnet->devices[i].group;
    if (current_group < artnet->no_mappings) {
      ++artnet->num_devices_in_group[current_group];
    }
  }
  artnet->devices_in_group = malloc(artnet->no_mappings * sizeof(DeviceT**));
  for (int i=0; i < artnet->no_mappings; ++i) {
    artnet->devices_in_group[i] = malloc(artnet->num_devices_in_group[i] * sizeof(DeviceT*));
    memset(artnet->devices_in_group[i], 0, artnet->num_devices_in_group[i] * sizeof(DeviceT*));
  }
  for (int i=0; i < artnet->no_devices; ++i) {
    // find index of next free index in array:
    int current_group = artnet->devices[i].group;
    if (current_group < 0 || current_group >= artnet->no_mappings) {
      printf("Warning: you have configured an used group %d in device %d, device will be ignored\n", current_group, i);
    } else {
      int j = 0;
      while(artnet->devices_in_group[current_group][j] != NULL) {
        ++j;
      }
      artnet->devices_in_group[current_group][j] = &artnet->devices[i];
    }
  }
  // diagnostics output:
  for (int i=0; i < artnet->no_mappings; ++i) {
    printf("Group %d has devices:\n", i);
    for (int j=0; j<artnet->num_devices_in_group[i]; ++j) {
      DeviceT* device = artnet->devices_in_group[i][j];
      printf("  group %d, device %d, universe: %d, group: %d, channel r: %d, g: %d, b: %d\n", 
       i, j, device->universe, device->group, device->channel_r, device->channel_g, device->channel_b);
    }
  }
}

void init_artnet_color_mappings(ArtnetT* artnet, struct config_params* cfg) {
  TColorMaps **all_mappings = artnet_alloc_color_map_array(cfg->no_mappings);
  artnet->no_mappings = cfg->no_mappings;
  artnet->mappings = all_mappings;
  for (int i=0; i < cfg->no_mappings; ++i) {
    TColorMaps *mapping = cfg->mappings[i];
    TColorMaps *new_mapping = artnet_alloc_color_map(mapping->no_color_maps);
    new_mapping->no_color_maps = mapping->no_color_maps;
    // printf("  add color map with %d entries\n", mapping->no_color_maps);
    memcpy(new_mapping->maps, mapping->maps, sizeof(struct color_map) * mapping->no_color_maps);
    all_mappings[i] = new_mapping;
  }
}

void print_color_mappings(ArtnetT* artnet) {
  for (int i=0; i < artnet->no_mappings; ++i) {
    TColorMaps *mapping = artnet->mappings[i];
    printf("Color Mapping %d with %d entries:\n", i, mapping->no_color_maps);
    for (int j=0; j < mapping->no_color_maps; ++j) {
      struct color_map *map = &(mapping->maps[j]);
      printf("  map band %d to hue %d\n", map->band, map->hue);
    }
  }
}

/* As we map multiple colors to a single rgb value we need to scale each value
   so that the max possible sum of all red, green, blue values is 255
*/   
// void init_max_colors(ArtnetT* artnet) {
  // // artnet->max_colorvalues = malloc(artnet->no_colors * 3 * sizeof(u_int32_t));
  // float sat = 1.0F;
  // float val =1.0F;
  // float r, g, b;
  // u_int32_t max_colorvalue_blue = 0;
  // u_int32_t max_colorvalue_green = 0;
  // u_int32_t max_colorvalue_red = 0;

  // u_int32_t max_colorvalues[artnet->no_colors * 3];
  // printf("init_max_colors\n");
  // memset(max_colorvalues, 0, artnet->no_colors * 3 * sizeof(u_int32_t));
  // for (int i=0; i < artnet->no_colors; i++) {
  //   float hue = 360.0F / artnet->no_colors * i;
  //   HSVtoRGB(&r, &g, &b, hue, sat, val);
  //   int ir = round(255.0F * r);
  //   int ig = round(255.0F * g);
  //   int ib = round(255.0F * b);
  //   max_colorvalues[i * 3] = ir;
  //   max_colorvalues[i * 3 + 1] = ig;
  //   max_colorvalues[i * 3 + 2] = ib;
  //   max_colorvalue_red += ir;
  //   max_colorvalue_green += ig;
  //   max_colorvalue_blue += ib;
  // }
  // artnet->max_colorvalue = MAX(max_colorvalue_red, max_colorvalue_green);
  // artnet->max_colorvalue = MAX(artnet->max_colorvalue, max_colorvalue_blue);
  // for (int i=0; i < artnet->no_colors; i++) {
  //   printf("i: %d, max r g b: %d %d %d\n", i, max_colorvalues[i * 3], max_colorvalues[i * 3 + 1], max_colorvalues[i * 3 + 2]);
  // }
  // printf("max_colorvalue_red: %d, max_colorvalue_green: %d, max_colorvalue_blue: %d\n", max_colorvalue_red, max_colorvalue_green, max_colorvalue_blue);
// }

void print_artnet_stats() {
  printf("Stats: Packet counter: %d, max exceeded: %d\n", packet_counter, exceed_counter);
}

void free_artnet(ArtnetT* artnet) {
  printf("Free Artnet sockets\n");
  if (artnet->sockets != NULL) {
    for (int i=0; i < artnet->no_sockets; i++) {
      if (artnet->sockets[i] >= 0) {
        close(artnet->sockets[i]);
      }
    }
    free(artnet->sockets);
  }

  printf("Free universes: %d\n", artnet->no_universes);
  if (artnet->universes != NULL) {
    for (int i=0; i< artnet->no_universes; ++i) {
      free(artnet->universes[i].hostname);
    }
    free(artnet->universes);
  }

  printf("Free devices\n");
  if (artnet->devices != NULL) {
    free(artnet->devices);
  }

  printf("Free devices_in_group\n");
  if (artnet->devices_in_group != NULL) {
    for (int i=0; i < artnet->no_mappings; ++i) {
      if (artnet->devices_in_group[i] != NULL) {
        free(artnet->devices_in_group[i]);
      }
    }
    free(artnet->devices_in_group);
  }

  printf("Free num_devices_in_group\n");
  if (artnet->num_devices_in_group != NULL) {
    free(artnet->num_devices_in_group);
  }

  printf("Free color_mappings\n");
  if (artnet->no_mappings > 0) {
    for (int i=0; i<artnet->no_mappings; ++i) {
      artnet_free_color_map(artnet->mappings[i]);
    }
    artnet_free_color_map_array(artnet->mappings);
  }

  printf("Free dmx_buffers\n");
  if (artnet->dmx_buffers != NULL) {
    for (int i=0; i< artnet->no_universes; ++i) {
      free(artnet->dmx_buffers[i]);
    }
    free(artnet->dmx_buffers);
  }
  printf("Free artnet\n");
  free(artnet);
}

void debug_print_buffer_full(int universe, int socket, uint8_t* buffer, int buffer_size) {
  printf("Values for universe %d, socket: %d\n", universe, socket);
  for (int i=0; i < buffer_size / 16; ++i) {
    for (int j=0; j < 16; ++j) {
      int index = i*16+j;
      printf("%3d: %3d ", index, buffer[index]);
    }
    printf("\n");
  }
}
void debug_print_buffer(int universe, int socket, uint8_t* buffer, int buffer_size) {
  socket = buffer_size; // avoid error: parameter ‘socket’ set but not used [-Werror=unused-but-set-parameter]
  buffer_size = socket; // avoid error: parameter ‘socket’ set but not used [-Werror=unused-but-set-parameter]
  printf("Universe %d: %3d, %3d, %3d, %3d, %3d, %3d\n", universe, buffer[19],  buffer[20], buffer[21],  buffer[24],  buffer[25], buffer[26]);
}

void reset_all_buffers(ArtnetT* artnet) {
  for (int i=0; i < artnet->no_universes; ++i) {
    memset(artnet->dmx_buffers[i] + sizeof(dmx_header), 0, 512);
  }
}

int send_dmx_buffers(ArtnetT* artnet, bool universes_to_send[]) {
  // send dmx buffer to all used universes:
  for (int i=0; i < artnet->no_universes; ++i) {
    if (universes_to_send[i]) {
      int socket = artnet->sockets[i];
      if (socket > 0) {
        int n = send(socket, artnet->dmx_buffers[i], dmx_buffer_size, 0);
        if (n == -1) {
          printf("Error: failed to send UDP\n");
        }
        debug_print_buffer(i, socket, artnet->dmx_buffers[i], dmx_buffer_size);
      } 
      else {
        printf("Error: socket for universe %d is zero (no connection)\n", i);
      }
    } 
    // else {
    //   printf("universe %d is false\n", i);
    // }

  }
  return 0;
}

int update_colors(ArtnetT* artnet, int bars_count, int f[200]) {
  const int offset = sizeof(dmx_header) - 1; // -1 one because dmx channels start at 1, but buffer at offset 0
  bool universes_to_send[artnet->no_universes];
  memset(universes_to_send, 0, artnet->no_universes*sizeof(bool));
  reset_all_buffers(artnet);
  ++packet_counter;

   for (int i=0; i < artnet->no_mappings; ++i) {
    //printf("process mapping %d\n", i);
    TColorMaps* mapping = artnet->mappings[i];
    for (int j=0; j < mapping->no_color_maps; ++j) {
      //printf("  band: %d of %d\n", mapping->maps[j].band, mapping->no_color_maps);
      if (mapping->maps[j].band >= bars_count || mapping->maps[j].band < 0) {
        printf("Error: Invalid frequency band %d, check configuration for [general]/bars", mapping->maps[j].band);
        continue;
      }
      int value = f[mapping->maps[j].band];
      if (value > 255) {
        value = 255;
        ++exceed_counter;
      }
      float r, g, b;
      float sat = value / 255.0F;
      float val = value / 255.0F;
      HSVtoRGB(&r, &g, &b, (float)mapping->maps[j].hue, sat, val);
      int ir = round(255.0F * r);
      int ig = round(255.0F * g);
      int ib = round(255.0F * b);
      // scale values so that all colors sum up max to (255, 255, 255)
      // ir = ir * 255 / mapping->max_colorvalue; 
      // ig = ig * 255 / mapping->max_colorvalue;
      // ib = ib * 255 / mapping->max_colorvalue;
      // iterate all devices in this group:
      DeviceT** devices = artnet->devices_in_group[i];
      for (int k=0; k<artnet->num_devices_in_group[i]; ++k) {
        DeviceT* device = devices[k];
        //printf("    device: %d of %d\n", k, artnet->num_devices_in_group[i]);
        int universeNumber = device->universe;
        // printf("channel for r g b: %d %d %d, universe: %d\n", device->channel_r, device->channel_g, device->channel_b, universeNumber);
        uint8_t* buf = artnet->dmx_buffers[universeNumber];
        // printf("    r g b: %d %d %d, universe: %d\n", ir,ig, ib, universeNumber);
        buf[device->channel_r + offset] += (uint8_t) ir;
        buf[device->channel_g + offset] += (uint8_t) ig;
        buf[device->channel_b + offset] += (uint8_t) ib;
        universes_to_send[universeNumber] = true;
      }
    }
  }
  return send_dmx_buffers(artnet, universes_to_send);
}