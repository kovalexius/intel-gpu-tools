/*
 * Copyright © 2007 Intel Corporation
 * Copyright © 2011 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Eric Anholt <eric@anholt.net>
 *    Eugeni Dodonov <eugeni.dodonov@intel.com>
 *    Alexandr Kovalev <saneck2ru@netup.ru>
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <err.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>

#include "intel_io.h"
#include "instdone.h"
#include "intel_reg.h"
#include "intel_chipset.h"

#include "netup_get_statistics.h"

#define  FORCEWAKE      0xA18C
#define  FORCEWAKE_ACK      0x130090

#define SAMPLES_PER_SEC             10000
#define SAMPLES_TO_PERCENT_RATIO    (SAMPLES_PER_SEC / 100)

#define MAX_NUM_TOP_BITS            100

#define HAS_STATS_REGS(devid)       IS_965(devid)


int samples_per_sec = SAMPLES_PER_SEC;
unsigned long long last_samples_per_sec;
uint32_t devid;

static uint32_t instdone, instdone1;

struct top_bit top_bits[MAX_NUM_TOP_BITS];

struct top_bit *top_bits_sorted[MAX_NUM_TOP_BITS];

const uint32_t stats_regs[STATS_COUNT] = {
    IA_VERTICES_COUNT_QW,
    IA_PRIMITIVES_COUNT_QW,
    VS_INVOCATION_COUNT_QW,
    GS_INVOCATION_COUNT_QW,
    GS_PRIMITIVES_COUNT_QW,
    CL_INVOCATION_COUNT_QW,
    CL_PRIMITIVES_COUNT_QW,
    PS_INVOCATION_COUNT_QW,
    PS_DEPTH_COUNT_QW,
};

const char *stats_reg_names[STATS_COUNT] = {
    "vert fetch",
    "prim fetch",
    "VS invocations",
    "GS invocations",
    "GS prims",
    "CL invocations",
    "CL prims",
    "PS invocations",
    "PS depth pass",
};

uint64_t stats[STATS_COUNT];
uint64_t last_stats[STATS_COUNT];

struct ring render_ring = {
    .name = "render",
    .mmio = 0x2030,
};
    
struct ring bsd_ring = {
    .name = "bitstream",
    .mmio = 0x4030,
};
    
struct ring bsd6_ring = {
    .name = "bitstream",
    .mmio = 0x12030,
};
    
struct ring blt_ring = {
    .name = "blitter",
    .mmio = 0x22030,
};

char * out_buffer = NULL;
size_t len_buffer = 0;
size_t pos_to_write = 0;

void write_to_buffer(const char* str, size_t n)
{
    if(len_buffer < (pos_to_write + n + 1))
    {
        char *ptr = realloc(out_buffer, pos_to_write + n + 1);
        if(ptr)
        {
            out_buffer = ptr;
            len_buffer = pos_to_write + n + 1;
        }
        else
            n = len_buffer - pos_to_write;
    }
    
    if(out_buffer)
    {
        memcpy(out_buffer + pos_to_write, str, n);
        pos_to_write += n;
        out_buffer[pos_to_write] = '\0';
    }
}

void empty_buffer()
{
    pos_to_write = 0;
}

void _print(const char * format, ...)
{
    const size_t MAX_LEN = 512;
    char buffer[MAX_LEN];
    
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    write_to_buffer(buffer, strlen(buffer));
    va_end(args);
}


void 
ring_print(struct ring *ring, unsigned long samples_per_sec)
{
    int percent_busy;

    if (!ring->size)
        return;
    
    percent_busy = 100 - 100 * ring->idle / samples_per_sec;

    _print("  \"%s busy\": \"%d\",\r\n", ring->name, percent_busy);
    _print("  \"%s space\":\r\n", ring->name);
    _print("  {\r\n");
    _print("    \"speed\": \"%d\",\r\n", (int)(ring->full / samples_per_sec));
    _print("    \"size\": \"%d\"\r\n", ring->size);
    _print("  },\r\n");
}

    
unsigned long
gettime(void)
{
    struct timeval t;
    gettimeofday(&t, NULL);
    return (t.tv_usec + (t.tv_sec * 1000000));
}

static void
update_idle_bit(struct top_bit *top_bit)
{
    uint32_t reg_val;

    if (top_bit->bit->reg == INSTDONE_1)
        reg_val = instdone1;
    else
        reg_val = instdone;

    if ((reg_val & top_bit->bit->bit) == 0)
        top_bit->count++;
}

static uint32_t 
ring_read(struct ring *ring, uint32_t reg)
{
    return INREG(ring->mmio + reg);
}

static void 
ring_init(struct ring *ring)
{
    ring->size = (((ring_read(ring, RING_LEN) & RING_NR_PAGES) >> 12) + 1) * 4096;
}

void ring_reset(struct ring *ring)
{
    ring->idle = ring->full = 0;
}

static void 
ring_sample(struct ring *ring)
{
    int full;

    if (!ring->size)
        return;

    ring->head = ring_read(ring, RING_HEAD) & HEAD_ADDR;
    ring->tail = ring_read(ring, RING_TAIL) & TAIL_ADDR;

    if (ring->tail == ring->head)
        ring->idle++;

    full = ring->tail - ring->head;
    if (full < 0)
        full += ring->size;
    ring->full += full;
}


void init_device()
{
    struct pci_device *pci_dev;
    
    pci_dev = intel_get_pci_device();
    devid = pci_dev->device_id;
    intel_mmio_use_pci_bar(pci_dev);
    init_instdone_definitions(devid);

    for(int i = 0; i < num_instdone_bits; i++)
    {
        top_bits[i].bit = &instdone_bits[i];
        top_bits[i].count = 0;
        top_bits_sorted[i] = &top_bits[i];
    }

    // Grab access to the registers
    intel_register_access_init(pci_dev, 0);
    
    ring_init(&render_ring);
    if (IS_GEN4(devid) || IS_GEN5(devid))
    {
        fprintf(stderr, "GEN4 or GEN5 detected\r\n");
        ring_init(&bsd_ring);
    }
    if (IS_GEN6(devid) || IS_GEN7(devid))
    {
        fprintf(stderr, "GEN6 or GEN7 detected\r\n");
        ring_init(&bsd6_ring);
        ring_init(&blt_ring);
    }
    if (IS_GEN8(devid))
    {
        fprintf(stderr, "GEN8 detected\r\n");
        ring_init(&bsd6_ring);
        ring_init(&blt_ring);
    }
    if (IS_GEN9(devid))
    {
        fprintf(stderr, "GEN9 detected\r\n");
        ring_init(&bsd6_ring);
        ring_init(&blt_ring);
    }

    // Initialize GPU stats
    if (HAS_STATS_REGS(devid))
    {
        for (int i = 0; i < STATS_COUNT; i++) 
        {
            uint32_t stats_high, stats_low, stats_high_2;

            do
            {
                stats_high = INREG(stats_regs[i] + 4);
                stats_low = INREG(stats_regs[i]);
                stats_high_2 = INREG(stats_regs[i] + 4);
            } while (stats_high != stats_high_2);

            last_stats[i] = (uint64_t)stats_high << 32 | stats_low;
        }
    }
}


void deinit_device()
{
    intel_register_access_fini();
    if(out_buffer)
        free(out_buffer);
    len_buffer = 0;
    pos_to_write = 0;
}

void get_device_params()
{
    int j, i;
    unsigned long long t1, ti, tf;
    unsigned long long def_sleep = 1000000 / samples_per_sec;
    last_samples_per_sec = samples_per_sec;

    t1 = gettime();

    ring_reset(&render_ring);
    ring_reset(&bsd_ring);
    ring_reset(&bsd6_ring);
    ring_reset(&blt_ring);

    for (i = 0; i < samples_per_sec; i++)
    {
        long long interval;
        ti = gettime();
        if (IS_965(devid))
        {
            instdone = INREG(INSTDONE_I965);
            instdone1 = INREG(INSTDONE_1);
        }
        else
            instdone = INREG(INSTDONE);

        for (j = 0; j < num_instdone_bits; j++)
            update_idle_bit(&top_bits[j]);

        ring_sample(&render_ring);
        ring_sample(&bsd_ring);
        ring_sample(&bsd6_ring);
        ring_sample(&blt_ring);

        tf = gettime();
        if (tf - t1 >= 1000000)
        {
            // We are out of sync, bail out
            last_samples_per_sec = i+1;
            break;
        }
        interval = def_sleep - (tf - ti);
        if (interval > 0)
            usleep(interval);
    }

    if (HAS_STATS_REGS(devid))
    {
        for (i = 0; i < STATS_COUNT; i++) 
        {
            uint32_t stats_high, stats_low, stats_high_2;
            do
            {
                stats_high = INREG(stats_regs[i] + 4);
                stats_low = INREG(stats_regs[i]);
                stats_high_2 = INREG(stats_regs[i] + 4);
            } while (stats_high != stats_high_2);

            stats[i] = (uint64_t)stats_high << 32 | stats_low;
        }
    }
}

void print_device_params()
{
    int percent;

    _print("Content-type: text/json\r\n\r\n");
    _print("{\r\n");
    ring_print(&render_ring, last_samples_per_sec);
    ring_print(&bsd_ring, last_samples_per_sec);
    ring_print(&bsd6_ring, last_samples_per_sec);
    ring_print(&blt_ring, last_samples_per_sec);
    _print("  \"instdone bits\":\r\n");
    _print("  {\r\n");
    for (int i = 0; i < num_instdone_bits; i++)
    {
        percent = (top_bits_sorted[i]->count * 100) / last_samples_per_sec;
        _print("    \"%s\": \"%d\"",
               top_bits_sorted[i]->bit->name,
               percent);
        if(i < num_instdone_bits - 1)
            _print(",");
        _print("\r\n");
    }
    _print("  },\r\n");
    
    _print("  \"stats\":\r\n");
    _print("  {\r\n");
    for (int i = 0; i < STATS_COUNT; i++)
    {
        _print("    \"%s\":\r\n", stats_reg_names[i]);
        _print("    {\r\n");
        _print("      \"total_count\": \"%llu\",\r\n", (long long)stats[i]);
        _print("      \"speed_per_second\": \"%lld\"\r\n", (long long)(stats[i] - last_stats[i]));
        _print("    }");
        if(i < STATS_COUNT - 1)
            _print(",");
        _print("\r\n");
        last_stats[i] = stats[i];
    }
    _print("  }\r\n");
    
    _print("}\r\n");
}

void reset_params_values()
{
    int i;
    empty_buffer();
    for (i = 0; i < num_instdone_bits; i++)
            top_bits_sorted[i]->count = 0;
}