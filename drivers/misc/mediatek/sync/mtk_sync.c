/*
* Copyright (C) 2011-2014 MediaTek Inc.
*
* This program is free software: you can redistribute it and/or modify it under the terms of the
* GNU General Public License version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

#include <linux/debugfs.h>
#include <linux/export.h>
#include <linux/seq_file.h>
#include <linux/file.h>
#include <linux/kthread.h>
/* #include <linux/xlog.h> */
#include <linux/delay.h>

#include "mtk_sync.h"

/* -------------------------------------------------------------------------- */

struct sw_sync_timeline *timeline_create(const char *name)
{
	return sw_sync_timeline_create(name);
}
EXPORT_SYMBOL(timeline_create);

void timeline_destroy(struct sw_sync_timeline *obj)
{
	sync_timeline_destroy(&obj->obj);
}
EXPORT_SYMBOL(timeline_destroy);

void timeline_inc(struct sw_sync_timeline *obj, u32 value)
{
	sw_sync_timeline_inc(obj, value);
}
EXPORT_SYMBOL(timeline_inc);

int fence_create(struct sw_sync_timeline *obj, struct fence_data *data)
{
	int fd = get_unused_fd();
	int err;
	struct sync_pt *pt;
	struct sync_fence *fence;

	if (fd < 0)
		return fd;

	pt = sw_sync_pt_create(obj, data->value);
	if (pt == NULL) {
		err = -ENOMEM;
		goto err;
	}

	data->name[sizeof(data->name) - 1] = '\0';
	fence = sync_fence_create(data->name, pt);
	if (fence == NULL) {
		sync_pt_free(pt);
		err = -ENOMEM;
		goto err;
	}

	data->fence = fd;

	sync_fence_install(fence, fd);

	return 0;

 err:
	put_unused_fd(fd);
	return err;
}
EXPORT_SYMBOL(fence_create);

int fence_merge(char *const name, int fd1, int fd2)
{
	int fd = get_unused_fd();
	int err;
	struct sync_fence *fence1, *fence2, *fence3;

	if (fd < 0)
		return fd;

	fence1 = sync_fence_fdget(fd1);
	if (NULL == fence1) {
		err = -ENOENT;
		goto err_put_fd;
	}

	fence2 = sync_fence_fdget(fd2);
	if (NULL == fence2) {
		err = -ENOENT;
		goto err_put_fence1;
	}

	name[sizeof(name) - 1] = '\0';
	fence3 = sync_fence_merge(name, fence1, fence2);
	if (fence3 == NULL) {
		err = -ENOMEM;
		goto err_put_fence2;
	}

	sync_fence_install(fence3, fd);
	sync_fence_put(fence2);
	sync_fence_put(fence1);

	return fd;

 err_put_fence2:
	sync_fence_put(fence2);

 err_put_fence1:
	sync_fence_put(fence1);

 err_put_fd:
	put_unused_fd(fd);
	return err;
}
EXPORT_SYMBOL(fence_merge);
