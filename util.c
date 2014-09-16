/*
 * Copyright (c) 2014 Rob Clark <robdclark@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/*
 * Simple test/demo for EGL_EXT_image_dma_buf_import
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "util.h"

#include "freedreno_drmif.h"


/* this part is a bit msm specific for the buffer allocation.. normally
 * the dmabuf fd's would come from somewhere else..
 *
 * yeah, some error handling would be nice..
 */

static struct fd_device *
get_dev(void)
{
	static struct fd_device *dev = NULL;
	if (!dev) {
		/* use render node to avoid needing to authenticate: */
		int fd = open("/dev/dri/renderD128", 02, 0);
		dev = fd_device_new(fd);
	}
	return dev;
}


void *buf_alloc(int size, int *fd)
{
	struct fd_device *dev = get_dev();
	struct fd_bo *bo;

	bo = fd_bo_new(dev, size, 0);
	*fd = fd_bo_dmabuf(bo);

	return fd_bo_map(bo);
}
