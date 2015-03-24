/*
 * Copyright © 2009,2011 Intel Corporation
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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Eric Anholt <eric@anholt.net>
 *
 */

/** @file gem_tiled_fence_blits.c
 *
 * This is a test of doing many tiled blits, with a working set
 * larger than the aperture size.
 *
 * The goal is to catch a couple types of failure;
 * - Fence management problems on pre-965.
 * - A17 or L-shaped memory tiling workaround problems in acceleration.
 *
 * The model is to fill a collection of 1MB objects in a way that can't trip
 * over A6 swizzling -- upload data to a non-tiled object, blit to the tiled
 * object.  Then, copy the 1MB objects randomly between each other for a while.
 * Finally, download their data through linear objects again and see what
 * resulted.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <drm.h>

#include "ioctl_wrappers.h"
#include "drmtest.h"
#include "intel_bufmgr.h"
#include "intel_batchbuffer.h"
#include "intel_io.h"
#include "intel_chipset.h"
#include "igt_aux.h"

static drm_intel_bufmgr *bufmgr;
struct intel_batchbuffer *batch;
enum {width=512, height=512};
static const int bo_size = width * height * 4;
static uint32_t linear[width * height];

static drm_intel_bo *
create_bo(int fd, uint32_t start_val)
{
	drm_intel_bo *bo;
	uint32_t tiling = I915_TILING_X;
	int ret, i;

	bo = drm_intel_bo_alloc(bufmgr, "tiled bo", bo_size, 4096);
	ret = drm_intel_bo_set_tiling(bo, &tiling, width * 4);
	igt_assert_eq(ret, 0);
	igt_assert(tiling == I915_TILING_X);

	/* Fill the BO with dwords starting at start_val */
	for (i = 0; i < width * height; i++)
		linear[i] = start_val++;

	gem_write(fd, bo->handle, 0, linear, sizeof(linear));

	return bo;
}

static void
check_bo(int fd, drm_intel_bo *bo, uint32_t start_val)
{
	int i;

	gem_read(fd, bo->handle, 0, linear, sizeof(linear));

	for (i = 0; i < width * height; i++) {
		igt_assert_f(linear[i] == start_val,
			     "Expected 0x%08x, found 0x%08x "
			     "at offset 0x%08x\n",
			     start_val, linear[i], i * 4);
		start_val++;
	}
}

static void run_test (int fd, int count)
{
	drm_intel_bo *bo[4096];
	uint32_t bo_start_val[4096];
	uint32_t start = 0;
	int i;

	count |= 1;
	igt_info("Using %d 1MiB buffers\n", count);

	bufmgr = drm_intel_bufmgr_gem_init(fd, 4096);
	drm_intel_bufmgr_gem_enable_reuse(bufmgr);
	batch = intel_batchbuffer_alloc(bufmgr, intel_get_drm_devid(fd));

	for (i = 0; i < count; i++) {
		bo[i] = create_bo(fd, start);
		bo_start_val[i] = start;

		/*
		igt_info("Creating bo %d\n", i);
		check_bo(bo[i], bo_start_val[i]);
		*/

		start += width * height;
	}

	for (i = 0; i < count; i++) {
		int src = count - i - 1;
		intel_copy_bo(batch, bo[i], bo[src], bo_size);
		bo_start_val[i] = bo_start_val[src];
	}

	for (i = 0; i < count * 4; i++) {
		int src = random() % count;
		int dst = random() % count;

		if (src == dst)
			continue;

		intel_copy_bo(batch, bo[dst], bo[src], bo_size);
		bo_start_val[dst] = bo_start_val[src];

		/*
		check_bo(bo[dst], bo_start_val[dst]);
		igt_info("%d: copy bo %d to %d\n", i, src, dst);
		*/
	}

	for (i = 0; i < count; i++) {
		/*
		igt_info("check %d\n", i);
		*/
		check_bo(fd, bo[i], bo_start_val[i]);

		drm_intel_bo_unreference(bo[i]);
		bo[i] = NULL;
	}

	intel_batchbuffer_free(batch);
	drm_intel_bufmgr_destroy(bufmgr);
}

igt_main
{
	int fd, count;

	igt_fixture {
		fd = drm_open_any();
	}

	igt_subtest("basic") {
		run_test (fd, 2);
	}

	/* the rest of the tests are too long for simulation */
	igt_skip_on_simulation();

	igt_subtest("normal") {
		count = 3 * gem_aperture_size(fd) / (bo_size) / 2;
		intel_require_memory(count, bo_size, CHECK_RAM);
		run_test(fd, count);
	}

	close(fd);
}
