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

static int connect_udp(const char* host, const char* port_str) {
  struct addrinfo hints;
  struct addrinfo *result, *rp;
  int sfd, s;
  // char port_str[10];
  // sprintf(port_str, "%d", port);
   
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
  hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
  hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */

  s = getaddrinfo(host, port_str, &hints, &result);
  if (s != 0) {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
      return -1;
  }

  for (rp = result; rp != NULL; rp = rp->ai_next) {
    sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (sfd == -1)
        continue;

    if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0)
        break;                  /* Success */

    close(sfd);
  }

  if (rp == NULL) {               /* No address succeeded */
    fprintf(stderr, "Could not bind to %s, port: %s\n", host, port_str);
    return -1;
  }

  freeaddrinfo(result);           /* No longer needed */
  return sfd;
}

void init_artnet_udp(ArtnetT* artnet) {
  for (int i=0; i < artnet->no_universes; i++) {
    char host[128];
    int socket;
    const char *port;
    printf("init universe: %s\n", artnet->universes[i]);
    const char* sep = strchr(artnet->universes[i], ':');
    if (sep == NULL) {
      fprintf(stderr, "Invalid Syntax: %s, must be 'host:port'\n", artnet->universes[i]);
      return;
    }
    strncpy(host, artnet->universes[i], min(sep - artnet->universes[i], (int)sizeof(host)));
    port = sep+1;
    printf("connect udp host: %s, port: %s\n", host, port);
    socket = connect_udp(host, port);
    artnet->sockets[i] = socket;
  }
}

ArtnetT* init_artnet(struct config_params* cfg, bool connect) {
  // struct sockaddr_in addr;
  printf("init_artnet\n");

  ArtnetT* artnet = malloc(sizeof(ArtnetT));
  memset(artnet, 0, sizeof(ArtnetT));

  printf("init_artnet universes\n");
  artnet->no_universes = cfg->no_universes;
  if (artnet->no_universes > 0) {
    printf("alloc universes: %d\n", artnet->no_universes);
    artnet->universes = malloc(artnet->no_universes * sizeof(char*));
    for (int i=0; i< artnet->no_universes; ++i) {
      printf("  len: %d\n", strlen(cfg->universes[i])+1);
      artnet->universes[i] = malloc(strlen(cfg->universes[i])+1);
      strcpy(artnet->universes[i], cfg->universes[i]);
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
  printf("init_artnet devices\n");
  artnet->no_devices = cfg->no_devices;
  artnet->devices = malloc(cfg->no_devices * sizeof(DeviceT));
  memcpy(artnet->devices, cfg->devices, cfg->no_devices * sizeof(DeviceT));
  artnet->no_colors = cfg->no_colors;
  artnet->no_groups = cfg->no_bars / cfg->no_colors;
  printf("no groups: %d, no devices: %d\n", artnet->no_groups, artnet->no_devices);
  printf("dmx_buffer_size: %d bytes\n", dmx_buffer_size);
  if (cfg->no_bars / artnet->no_colors % 1 != 0) {
    printf("Warning bars should be multiple of device-colors.");
    ++artnet->no_groups;
  }
  init_artnet_groups(artnet);

  artnet->dmx_buffers = malloc(artnet->no_universes);
  memset(artnet->dmx_buffers, 0,  artnet->no_universes);
  for (int i=0; i< artnet->no_universes; ++i) {
    artnet->dmx_buffers[i] = malloc(dmx_buffer_size);
    memcpy(artnet->dmx_buffers[i], dmx_header, sizeof(dmx_header));
    artnet->dmx_buffers[i][14] = (uint8_t)(i & 0xFF);
    artnet->dmx_buffers[i][15] = (uint8_t)((i >> 8) & 0xFF);
  }
  init_max_colors(artnet);
  return artnet;
}

void init_artnet_groups(ArtnetT* artnet) {
  artnet->num_devices_in_group = malloc(artnet->no_groups * sizeof(int));
  memset(artnet->num_devices_in_group, 0, artnet->no_groups * sizeof(int));

  for (int i=0; i < artnet->no_devices; ++i) {
    ++artnet->num_devices_in_group[artnet->devices[i].group];
  }
  artnet->devices_in_group = malloc( artnet->no_groups * sizeof(DeviceT**));
  for (int i=0; i < artnet->no_groups; ++i) {
    printf("alloc bytes: %d\n", artnet->num_devices_in_group[i]* sizeof(DeviceT*));
    artnet->devices_in_group[i] = malloc(artnet->num_devices_in_group[i] * sizeof(DeviceT*));
    memset(artnet->devices_in_group[i], 0, artnet->num_devices_in_group[i] * sizeof(DeviceT*));
  }
  for (int i=0; i < artnet->no_devices; ++i) {
    // find index of next free index in array:
    int current_group = artnet->devices[i].group;
    int j = 0;
    while(artnet->devices_in_group[current_group][j] != NULL) {
      ++j;
    }
    artnet->devices_in_group[current_group][j] = &artnet->devices[i];
  }
  // debug print:
  for (int i=0; i < artnet->no_groups; ++i) {
    printf("Group %d has devices\n", i);
    for (int j=0; j<artnet->num_devices_in_group[i]; ++j) {
      printf("  device %d\n", artnet->devices_in_group[i][j]->channel_r);
    }
  }
}

/* As we map multiple colors to a single rgb value we need to scale each value
   so that the max possible sum of all red, green, blue values is 255
*/   
void init_max_colors(ArtnetT* artnet) {
  artnet->max_colorvalues = malloc(artnet->no_colors * 3 * sizeof(u_int32_t));
  memset(artnet->max_colorvalues, 0, artnet->no_colors * 3 * sizeof(u_int32_t));
  float sat = 1.0F;
  float val =1.0F;
  float r, g, b;
  printf("init_max_colors\n");
  for (int i=0; i < artnet->no_colors; i++) {
    float hue = 360.0F / artnet->no_colors * i;
    HSVtoRGB(&r, &g, &b, hue, sat, val);
    int ir = round(255.0F * r);
    int ig = round(255.0F * g);
    int ib = round(255.0F * b);
    artnet->max_colorvalues[i * 3] = ir;
    artnet->max_colorvalues[i * 3 + 1] = ig;
    artnet->max_colorvalues[i * 3 + 2] = ib;
    artnet->max_colorvalue_red += ir;
    artnet->max_colorvalue_green += ig;
    artnet->max_colorvalue_blue += ib;
  }
}

void free_artnet(ArtnetT* artnet) {
  if (artnet->sockets != NULL) {
    for (int i=0; i < artnet->no_sockets; i++) {
      if (artnet->sockets[i] >= 0) {
        close(artnet->sockets[i]);
      }
    }
    free(artnet->sockets);
  }

  printf("Free universes: %d\n", artnet->no_universes);
  for (int i=0; i< artnet->no_universes; ++i) {
    free(artnet->universes[i]);
  }

  if (artnet->universes != NULL) {
    free(artnet->universes);
  }

  printf("Free devices\n");
  if (artnet->devices != NULL) {
    free(artnet->devices);
  }

  printf("Free devices_in_group\n");
  if (artnet->devices_in_group != NULL) {
    for (int i=0; i < artnet->no_groups; ++i) {
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
  printf("Free dmx_buffers\n");
  if (artnet->dmx_buffers != NULL) {
    for (int i=0; i< artnet->no_universes; ++i) {
      free(artnet->dmx_buffers[i]);
    }
    free(artnet->dmx_buffers);
  }
  printf("Free max_colorvalues\n");
  if (artnet->max_colorvalues != NULL) {
    free(artnet->max_colorvalues);
  }
  printf("Free artnet\n");
  free(artnet);
}

void test_mapping(ArtnetT* artnet, int bars_count) {
  printf("bars: %d, colors: %d, groups: %d\n", bars_count, artnet->no_colors, artnet->no_groups);
  printf("Groups:\n");
  for (int i=0; i < artnet->no_groups; ++i) {
    printf("Groups[%d]: Number of devices: %d\n", i, artnet->num_devices_in_group[i]);
  }
  for (int i=0; i < bars_count; ++i) {
    int group = i / artnet->no_colors;
    // find all devices for this group
    int hue_index = i % artnet->no_colors;
    printf("mapping bar: %d to color step: %d, group: %d\n", i, hue_index, group);
  }
}

void debug_print_buffer(int universe, uint8_t* buffer, int buffer_size) {
  printf("Values for universe %d\n", universe);
  for (int i=0; i < buffer_size; ++i) {
    if (buffer[i] != 0) {
      printf("  %d: %d\n", i, buffer[i]);
    }
  }
}

void reset_all_buffers(ArtnetT* artnet) {
  for (int i=0; i < artnet->no_universes; ++i) {
    memset(artnet->dmx_buffers[i] + sizeof(dmx_header), 0, 512);
  }
}

int update_colors(ArtnetT* artnet, int bars_count, int const f[200]) {
  const int offset = sizeof(dmx_header);
  bool universes_to_send[artnet->no_universes];
  memset(universes_to_send, 0, artnet->no_universes*sizeof(bool));
  reset_all_buffers(artnet);

  for (int i=0; i < bars_count; ++i) {
    float r, g, b;
    int group = i / artnet->no_colors;
    // find all devices for this group
    int hue_index = i % artnet->no_colors;
    float hue = 360.0F / artnet->no_colors * hue_index;
    float sat = f[i] / 255.0F;
    float val = f[i] / 255.0F;
    HSVtoRGB(&r, &g, &b, hue, sat, val);
    printf("bar: %d\n", i);
    printf("color h s v: %f %f %f ### r g b: %f %f %f\n", hue, sat, val, r, g, b);
    int ir = round(255.0F * r);
    int ig = round(255.0F * g);
    int ib = round(255.0F * b);
    // scale values so that all colors sum up max to (255, 255, 255)
    printf(" ir ig ib: %d %d %d ### max r g b: %d %d %d\n", ir, ig, ib,  artnet->max_colorvalue_red,  artnet->max_colorvalue_green,  artnet->max_colorvalue_blue);
    ir = ir * artnet->max_colorvalues[hue_index * 3] / artnet->max_colorvalue_red; 
    ig = ig * artnet->max_colorvalues[hue_index * 3 + 1] / artnet->max_colorvalue_green; 
    ib = ib * artnet->max_colorvalues[hue_index * 3 + 2] / artnet->max_colorvalue_blue; 
    printf(" ### current max r g b: %d %d %d, scaled ir ig ib: %d %d %d\n", artnet->max_colorvalues[i * 3], artnet->max_colorvalues[i * 3 + 1], artnet->max_colorvalues[i * 3 + 2], ir, ig, ib);
    // find channels where to set rgb values
    // iterate all devices in this group:
    DeviceT** devices = artnet->devices_in_group[group];
    for (int j=0; j<artnet->num_devices_in_group[group]; ++j) {
      DeviceT* device = devices[j];
      int universeNumber = device->universe;
      printf("channel r g b: %d %d %d, universe: %d\n", device->channel_r, device->channel_g, device->channel_b, universeNumber);
      uint8_t* buf = artnet->dmx_buffers[universeNumber];
      buf[device->channel_r + offset] += (uint8_t) ir;
      buf[device->channel_g + offset] += (uint8_t) ig;
      buf[device->channel_b + offset] += (uint8_t) ib;
      universes_to_send[universeNumber] = true;
    }
  }
  // send dmx buffer to all used universes:
  for (int i=0; i < artnet->no_universes; ++i) {
    if (universes_to_send[i]) {
      // int socket = artnet->sockets[i];
      // send(artnet->sockets[i], artnet->dmx_buffers[i], dmx_buffer_size, 0);
      debug_print_buffer(i, artnet->dmx_buffers[i]+sizeof(dmx_header), dmx_buffer_size - sizeof(dmx_header));
    }
  }
  return 0;
}