#ifndef __NETUP_GET_STATISTICS_H
#define __NETUP_GET_STATISTICS_H

#include <stdint.h>

struct ring 
{
    const char *name;
    uint32_t mmio;
    int head, tail, size;
    uint64_t full;
    int idle;
};

struct top_bit 
{
    struct instdone_bit *bit;
    int count;
};

enum stats_counts 
{
    IA_VERTICES,
    IA_PRIMITIVES,
    VS_INVOCATION,
    GS_INVOCATION,
    GS_PRIMITIVES,
    CL_INVOCATION,
    CL_PRIMITIVES,
    PS_INVOCATION,
    PS_DEPTH,
    STATS_COUNT
};

void init_device();
void deinit_device();
void get_device_params();
void print_device_params();
void reset_params_values();

extern int samples_per_sec;
extern char * out_buffer;

#endif