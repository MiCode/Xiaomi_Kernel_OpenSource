#ifndef _MI_DRM_SYSFS_H_
#define _MI_DRM_SYSFS_H_

#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/slab.h>

#define DATA_NUM (2048 * 5)
struct buf_data{
	char *data;
	int length;
};

struct panel_mi_package_dsi_msg {
	size_t tx_len;
	void *tx_buf;
};

struct panel_package_dsi_cmd_desc {
	struct panel_mi_package_dsi_msg msg;
	u32  post_wait_ms;
};

struct panel_package_cmd_set {
	u32 count;
	struct  panel_package_dsi_cmd_desc *cmds;
};

int mi_read_initcode(void);

#endif /*_MI_DRM_SYSFS_H_*/