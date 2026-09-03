// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *
 * Copyright (C) 2019 Spreadtrum Co., Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 of
 * the License as published by the Free Software Foundation.
 *
 * This program is used to control external modem in AP side for
 * Spreadtrum SoCs.
 */

#include <linux/cdev.h>
#include <linux/compat.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/of_device.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/sipc.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/debugfs.h>
#include <linux/gpio/consumer.h>

#include "sprd_modem_loader.h"

/* modem io cmd */
#define MODEM_MAGIC 'M'

#define MODEM_READ_LOCK_CMD _IO(MODEM_MAGIC, 0x1)
#define MODEM_READ_UNLOCK_CMD _IO(MODEM_MAGIC, 0x2)

#define MODEM_WRITE_LOCK_CMD _IO(MODEM_MAGIC, 0x3)
#define MODEM_WRITE_UNLOCK_CMD _IO(MODEM_MAGIC, 0x4)

#define MODEM_GET_LOAD_INFO_CMD _IOR(MODEM_MAGIC, 0x5, struct modem_load_info)
#define MODEM_SET_LOAD_INFO_CMD _IOW(MODEM_MAGIC, 0x6, struct modem_load_info)

#define MODEM_SET_READ_REGION_CMD _IOR(MODEM_MAGIC, 0x7, int)
#define MODEM_SET_WRITE_GEGION_CMD _IOW(MODEM_MAGIC, 0x8, int)

#ifdef CONFIG_SPRD_EXT_MODEM
#define MODEM_GET_REMOTE_FLAG_CMD _IOR(MODEM_MAGIC, 0x9, int)
#define MODEM_SET_REMOTE_FLAG_CMD _IOW(MODEM_MAGIC, 0xa, int)
#define MODEM_CLR_REMOTE_FLAG_CMD _IOW(MODEM_MAGIC, 0xb, int)
#endif

#define MODEM_STOP_CMD _IO(MODEM_MAGIC, 0xc)
#define MODEM_START_CMD _IO(MODEM_MAGIC, 0xd)
#define MODEM_ASSERT_CMD _IO(MODEM_MAGIC, 0xe)
#define MODEM_UNCONDITIONAL_STOP_CMD _IO(MODEM_MAGIC, 0x12)

#ifdef CONFIG_SPRD_EXT_MODEM_POWER_CTRL
#define MODEM_REBOOT_EXT_MODEM_CMD _IO(MODEM_MAGIC, 0xf)
#define MODEM_POWERON_EXT_MODEM_CMD _IO(MODEM_MAGIC, 0x10)
#define MODEM_POWEROFF_EXT_MODEM_CMD _IO(MODEM_MAGIC, 0x11)
#endif
#define MODEM_ENTER_SLEEP_CMD _IO(MODEM_MAGIC, 0x13)

#define	MODEM_READ_ALL_MEM 0xff
#define	MODEM_READ_MODEM_MEM 0xfe
#define	MODEM_READ_MINI_MEM_PS 0xfd
#define MODEM_READ_MINI_MEM_PHY 0xfc

#define RUN_STATE_INVALID 0xff
#define MINI_DUMP_NAME_MAX_LEN  20

#define MODEM_VMALLOC_SIZE_LIMIT 4096

enum {
	VERSION_V2 = 0x2,
	VERSION_V3,
	VERSION_V4
};

enum {
	SPRD_V4_MODEM_PM = 0,
	SPRD_V4_MODEM_CH,
	SPRD_V4_MODEM_PS,
	SPRD_V4_MODEM_PHY,
	SPRD_V4_MODEM_CNT
};

enum {
	SPRD_V3_MODEM_DP = 0,
	SPRD_V3_MODEM_PS,
	SPRD_V3_MODEM_NR_PHY,
	SPRD_V3_MODEM_V3_PHY,
	SPRD_V3_MODEM_CNT
};

enum {
	SPRD_V2_MODEM_PM = 0,
	SPRD_V2_MODEM_PUBCP,
	SPRD_V2_MODEM_CNT,
};

struct modem_data {
	const char *name;
	u32	dst;
};

#ifdef CONFIG_SPRD_EXT_MODEM
const struct ext_modem_operations *ext_modem_ops;
#endif

#ifdef CONFIG_ARM64
#define modem_memory_unmap(type, vmem)		modem_ram_unmap((type), (vmem))
#define ALIGN_NUM				8
#define ALIGN_MASK				0xFFFFFFFFFFFFFFF8
#else
#define modem_memory_unmap(type, vmem)		memunmap((vmem))
#define ALIGN_NUM				4
#define ALIGN_MASK				0xFFFFFFFC
#endif

const char *modem_ctrl_args[MODEM_CTRL_NR] = {
	"shutdown",
	"deepsleep",
	"corereset",
	"sysreset",
	"dspreset",
	"getstatus",
	"fshutdown",
	"autoshutdown"
};

static const struct modem_data modem_v4[SPRD_V4_MODEM_CNT] = {
	{"pmsys", SIPC_ID_PM_SYS},
	{"chsys", SIPC_ID_CH},
	{"modem", SIPC_ID_PSCP},
	{"phycp", SIPC_ID_NR_PHY}
};

static const struct modem_data modem_v3[SPRD_V3_MODEM_CNT] = {
	{"dpsys", SIPC_ID_PM_SYS},
	{"modem", SIPC_ID_PSCP},
	{"nrphy", SIPC_ID_NR_PHY},
	{"v3phy", SIPC_ID_V3_PHY}
};

static const struct modem_data modem_v2[SPRD_V2_MODEM_CNT] = {
	{"pmsys", SIPC_ID_PM_SYS},
	{"modem", SIPC_ID_PSCP}
};

typedef int (*MODEM_PARSE_FUN)(struct modem_device *modem,
			       struct device_node *np);

static struct class *modem_class;

static int sprd_modem_pms_request_resource(struct sprd_pms *pms, int timeout)
{
	int ret = sprd_pms_request_resource(pms, timeout);

	if (!ret)
		sprd_pms_request_wakelock(pms);

	return ret;
}

static void sprd_modem_pms_release_resource(struct sprd_pms *pms)
{
	sprd_pms_release_wakelock(pms);
	sprd_pms_release_resource(pms);
}

static int modem_enter_sleep(struct modem_device *modem)
{
	int ret = sprd_mpm_disable_later_idle_for_sleep(modem->modem_dst);

	dev_info(modem->p_dev, "modem enter sleep, ret=%d!\n", ret);
	return ret;
}

static int modem_open(struct inode *inode, struct file *filp)
{
	struct modem_device *modem;

	modem = container_of(inode->i_cdev, struct modem_device, cdev);
	filp->private_data = modem;

	return 0;
}

static int modem_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static void modem_get_base_range(struct modem_device *modem,
				 phys_addr_t *p_base,
				 size_t *p_size, int b_read_region)
{
	phys_addr_t base = 0;
	size_t size = 0;
	u8 index;
	struct modem_region_info *region;

	index = b_read_region ? modem->read_region : modem->write_region;
	if (b_read_region) {
		switch (modem->read_region) {
		case MODEM_READ_ALL_MEM:
			base = modem->all_base;
			size = modem->all_size;
			break;

		case MODEM_READ_MODEM_MEM:
			base = modem->modem_base;
			size = modem->modem_size;
			break;

		case MODEM_READ_MINI_MEM_PS:
			base = modem->mini_base[MINI_DUMP_PS];
			size = modem->mini_size[MINI_DUMP_PS];
			break;

		case MODEM_READ_MINI_MEM_PHY:
			base = modem->mini_base[MINI_DUMP_PHY];
			size = modem->mini_size[MINI_DUMP_PHY];
			break;

		default:
			if ((index < modem->load->region_cnt) && (index < MAX_REGION_CNT)) {
				region = &modem->load->regions[index];
				base = region->address;
				size = region->size;
			}
			break;
		}
	} else if (index < MAX_REGION_CNT) {
		region = &modem->load->regions[index];
		base = region->address;
		size = region->size;
	}

	dev_dbg(modem->p_dev, "get base 0x%llx, size = 0x%lx!\n", base, size);

	*p_base = base;
	*p_size = size;
}

static void *modem_map_memory(struct modem_device *modem, phys_addr_t start,
			      size_t size, size_t *map_size_ptr)
{
	size_t map_size = size;
	void *map;

	do {
		map_size = PAGE_ALIGN(map_size);
#ifdef CONFIG_ARM64
		map = modem_ram_vmap_ex(modem->modem_type, start, map_size, MMAP_NONCACHE);
#else
#ifdef CONFIG_UNISOC_MODEM_LOADER_RAM_VMAP
		map = modem_ram_vmap_ex(modem->modem_type, start, map_size, MMAP_NONCACHE);
#else
		map = memremap(start, map_size, MEMREMAP_WB);
#endif
#endif
		if (map) {
			if (map_size_ptr)
				*map_size_ptr = map_size;
			return map;
		}
		map_size /= 2;
	} while (map_size >= PAGE_SIZE);
	return NULL;
}

static int list_each_dump_info(struct modem_dump_info *base,
			       struct modem_dump_info **info, int *mini_number)
{
	struct modem_dump_info *next;
	int ret = 1;

	if (!info)
		return 0;

	next = *info;
	if (!next)
		next = base;
	else
		next++;

	if (next->parent_name[0] != '\0') {
		*info = next;
	} else {
		*info = NULL;
		ret = 0;
		*mini_number = 0;
	}

	return ret;
}

static ssize_t sprd_modem_seg_dump(struct modem_device *modem, u32 base, u32 maxsz,
					char __user *buf, size_t count, loff_t offset)
{
	void *vmem;
	phys_addr_t loop = 0;
	u32 start_addr;
	u32 total;
	ssize_t map_size;

	if (offset >= maxsz)
		return 0;

	if ((offset + count) > maxsz)
		count = maxsz - offset;

	start_addr = base + offset;
	total = count;

	do {
		u32 copy_size = MODEM_VMALLOC_SIZE_LIMIT;
		vmem = modem_map_memory(modem, start_addr + MODEM_VMALLOC_SIZE_LIMIT * loop,
				MODEM_VMALLOC_SIZE_LIMIT, &map_size);
		if (!vmem) {
			pr_err("unable to map base: 0x%08llx\n",
			       start_addr + MODEM_VMALLOC_SIZE_LIMIT * loop);
			if (loop > 0)
				return MODEM_VMALLOC_SIZE_LIMIT * loop;
			else
				return -ENOMEM;
		}

		if (count < MODEM_VMALLOC_SIZE_LIMIT)
			copy_size = count;

		if (_unalign_copy_to_user(buf, vmem, copy_size)) {
			pr_err("copy data to user error !\n");
			modem_memory_unmap(modem->modem_type, vmem);
			return -EFAULT;
		}

		modem_memory_unmap(modem->modem_type, vmem);

		count -= copy_size;
		loop++;
		buf += copy_size;
	} while (count);

	return total;
}

static bool sprd_mini_dump_check_parent_name(struct modem_dump_info **info, size_t count)
{
	char mini_parent_name[MINI_DUMP_NAME_MAX_LEN] = {0};

	if (!count)
		return false;

	strncpy(mini_parent_name, (*info)->parent_name, sizeof(mini_parent_name));
	mini_parent_name[MINI_DUMP_NAME_MAX_LEN - 1] = '\0';
	if (strstr(mini_parent_name, "sharemem")
		|| strstr(mini_parent_name, "pscp")
		|| strstr(mini_parent_name, "phy")
		|| strstr(mini_parent_name, "ldsp")
		|| strstr(mini_parent_name, "tgdsp")
		|| strstr(mini_parent_name, "warm")
		|| strstr(mini_parent_name, "modem")
		|| strstr(mini_parent_name, "dsp"))
		return true;

	return false;
}

static ssize_t modem_read_mini_dump(struct file *filp,
			  char __user *buf, size_t count, loff_t *ppos,
			  phys_addr_t base, size_t size)

{
	static struct modem_dump_info *s_cur_info;
	static int mini_number;
	struct modem_device *modem = filp->private_data;
	u8 head[sizeof(struct modem_dump_info) + 32];
	int len, total = 0, offset = 0;
	int struct_size = sizeof(struct modem_dump_info);
	ssize_t written = 0, map_size;
	void *vmem;

	dev_info(modem->p_dev, "read, %s mini_dump!\n", modem->modem_name);

	if (!s_cur_info && *ppos)
		return 0;

	if (mini_number) {
		vmem = modem_map_memory(modem, base +
					struct_size * mini_number,
					struct_size, &map_size);
		s_cur_info = (struct modem_dump_info *)vmem;
	} else {
		vmem = modem_map_memory(modem, base, size, &map_size);
		s_cur_info = NULL;
	}

	if (!vmem) {
		dev_err(modem->p_dev,
			"read, Unable to map  base: 0x%llx\n", base);
		return -ENOMEM;
	}

	if (!s_cur_info)
		list_each_dump_info(vmem, &s_cur_info, &mini_number);

	while (s_cur_info) {
		if (!sprd_mini_dump_check_parent_name(&s_cur_info, count)) {
			dev_info(modem->p_dev, "minidump parent_name no init,disable mini_dump!\n");
			break;
		}
		len = snprintf(head, sizeof(head), "%s_%s_0x%8x_0x%x.bin",
			s_cur_info->parent_name,
			s_cur_info->name,
			s_cur_info->start_addr,
			s_cur_info->size);

		if (*ppos > len) {
			offset = *ppos - len;
		} else {
			if (*ppos + count > len)
				written = len - *ppos;
			else
				written = count;

			if (_unalign_copy_to_user(buf + total,
					head + *ppos, written)) {
				dev_err(modem->p_dev, "copy mini-dump data to user error !\n");
				modem_memory_unmap(modem->modem_type, vmem);
				return -EFAULT;
			}
			*ppos += written;
		}
		total += written;
		count -= written;
		if (count) {
			written = sprd_modem_seg_dump(
					modem,
					s_cur_info->start_addr,
					s_cur_info->size,
					buf + total,
					count,
					offset);
			if (written > 0) {
				total += written;
				count -= written;
				*ppos += written;
			} else if (written == 0) {
				if (list_each_dump_info(vmem, &s_cur_info,
							&mini_number)) {
					*ppos = 0;
					mini_number++;
				}
			} else {
				modem_memory_unmap(modem->modem_type, vmem);
				return written;
			}

		} else {
			break;
		}

		written = 0;
		offset = 0;
	}

	modem_memory_unmap(modem->modem_type, vmem);
	return total;
}

static ssize_t modem_read(struct file *filp,
			  char __user *buf, size_t count, loff_t *ppos)
{
	phys_addr_t base;
	size_t size, offset, copy_size, map_size, r;
	void *vmem;
	struct modem_device *modem = filp->private_data;
	struct pm_reg_ctrl *ctrl = modem->pm_reg_ctrl;
	phys_addr_t addr;
	ssize_t ret;

	dev_info(modem->p_dev, "read, %s!\n", modem->modem_name);

	/* only get read lock task can be read */
	if (strcmp(current->comm, modem->rd_lock_name) != 0) {
		dev_err(modem->p_dev,  "read, task %s need get rd lock!\n",
			current->comm);
		return -EACCES;
	}

	modem_get_base_range(modem, &base, &size, 1);

	if (modem->read_region == MODEM_READ_MINI_MEM_PS ||
	    modem->read_region == MODEM_READ_MINI_MEM_PHY) {
		ret = sprd_modem_pms_request_resource(modem->rd_pms, -1);
		if (ret)
			return ret;
		ret = modem_read_mini_dump(filp, buf, count, ppos,
					   base, size);
		sprd_modem_pms_release_resource(modem->rd_pms);
		return ret;
	}

	offset = *ppos;
	dev_info(modem->p_dev, "read, offset = 0x%lx, count = 0x%lx!\n",
		offset, count);

	if (size <= offset)
		return -EINVAL;

	/* get sp | ch sys bus control */
	if (ctrl && ctrl->reg_offset) {
		regmap_read(ctrl->ctrl_map, ctrl->reg_offset, &ctrl->reg_save);
		regmap_update_bits(ctrl->ctrl_map, ctrl->reg_offset, ctrl->reg_mask, 0);
	}

	count = min_t(size_t, size - offset, count);
	r = count;
	do {
		addr = base + offset + (count - r);
		ret = sprd_modem_pms_request_resource(modem->rd_pms, -1);
		if (ret)
			return ret;

		vmem = modem_map_memory(modem, addr, r, &map_size);
		if (!vmem) {
			dev_err(modem->p_dev,
				"read, Unable to map  base: 0x%llx\n", addr);
			sprd_modem_pms_release_resource(modem->rd_pms);
			ret = -ENOMEM;
			goto FAIL_READ;
		}

		copy_size = min_t(size_t, r, map_size);
		if (copy_size > ALIGN_NUM) {
			copy_size &= ALIGN_MASK;
		}
		if (_copy_to_user(buf, vmem, copy_size)) {
			dev_err(modem->p_dev,
				"read, copy data from user err!\n");
			modem_memory_unmap(modem->modem_type, vmem);
			sprd_modem_pms_release_resource(modem->rd_pms);
			ret = -EFAULT;
			goto FAIL_READ;
		}
		modem_memory_unmap(modem->modem_type, vmem);
		sprd_modem_pms_release_resource(modem->rd_pms);
		r -= copy_size;
		buf += copy_size;
	} while (r > 0);

	*ppos += (count - r);
	ret = count - r;

FAIL_READ:
	/* put sp | ch sys bus control */
	if (ctrl && ctrl->reg_offset)
		regmap_update_bits(ctrl->ctrl_map,
				   ctrl->reg_offset, ctrl->reg_mask, ctrl->reg_save);

	return ret;
}

static ssize_t modem_write(struct file *filp,
			   const char __user *buf,
			   size_t count, loff_t *ppos)
{
	phys_addr_t base;
	size_t size, offset, copy_size, map_size, r;
	void *vmem;
	struct modem_device *modem = filp->private_data;
	struct pm_reg_ctrl *ctrl = modem->pm_reg_ctrl;
	phys_addr_t addr;
	ssize_t ret;

	dev_dbg(modem->p_dev, "write, %s!\n", modem->modem_name);

	/* only get write lock task can be write */
	if (strcmp(current->comm, modem->wt_lock_name) != 0) {
		dev_err(modem->p_dev, "write, task %s need get wt lock!\n",
			current->comm);
		return -EACCES;
	}

	modem_get_base_range(modem, &base, &size, 0);
	offset = *ppos;
	dev_dbg(modem->p_dev, "write, offset 0x%lx, count = 0x%lx!\n",
		offset, count);

	if (size <= offset)
		return -EINVAL;

	/* get sp | ch sys bus control */
	if (ctrl && ctrl->reg_offset) {
		regmap_read(ctrl->ctrl_map, ctrl->reg_offset, &ctrl->reg_save);
		regmap_update_bits(ctrl->ctrl_map, ctrl->reg_offset, ctrl->reg_mask, 0);
	}

	count = min_t(size_t, size - offset, count);
	r = count;
	do {
		addr = base + offset + (count - r);
		ret = sprd_modem_pms_request_resource(modem->wt_pms, -1);
		if (ret)
			return ret;

		vmem = modem_map_memory(modem, addr, r, &map_size);
		if (!vmem) {
			dev_err(modem->p_dev,
				"write, Unable to map  base: 0x%llx\n",
				addr);
			sprd_modem_pms_release_resource(modem->rd_pms);
			ret = -ENOMEM;
			goto FAIL_WRITE;
		}
		copy_size = min_t(size_t, r, map_size);
		if (copy_size > ALIGN_NUM) {
			copy_size &= ALIGN_MASK;
		}

		if (_copy_from_user(vmem, buf, copy_size)) {
			dev_err(modem->p_dev,
				"write, copy data from user err!\n");
			modem_memory_unmap(modem->modem_type, vmem);
			sprd_modem_pms_release_resource(modem->rd_pms);
			ret = -EFAULT;
			goto FAIL_WRITE;
		}
		modem_memory_unmap(modem->modem_type, vmem);
		sprd_modem_pms_release_resource(modem->wt_pms);
		r -= copy_size;
		buf += copy_size;
	} while (r > 0);

	*ppos += (count - r);
	ret = count - r;

FAIL_WRITE:
	/* put sp | ch sys bus control */
	if (ctrl && ctrl->reg_offset)
		regmap_update_bits(ctrl->ctrl_map,
				   ctrl->reg_offset, ctrl->reg_mask, ctrl->reg_save);

	return ret;
}

static loff_t modem_lseek(struct file *filp, loff_t off, int whence)
{
	switch (whence) {
	case SEEK_SET:
		filp->f_pos = off;
		break;

	default:
		return -EINVAL;
	}
	return off;
}

static int modem_cmd_lock(struct file *filp,
			  struct modem_device *modem, int b_rx)
{
	struct mutex *mut; /* mutex point to rd_mutex or wt_mutex*/
	struct sprd_pms *pms;
	char *name;
	int ret;

	mut = b_rx ? &modem->rd_mutex : &modem->wt_mutex;
	pms = b_rx ? modem->rd_pms : modem->wt_pms;
	name = b_rx ? modem->rd_lock_name : modem->wt_lock_name;

	if (filp->f_flags & O_NONBLOCK) {
		if (!mutex_trylock(mut)) {
			dev_err(modem->p_dev, "lock, %s get lock %d busy!\n",
				current->comm, b_rx);
			return -EBUSY;
		}
	} else {
		dev_dbg(modem->p_dev, "lock, %s has get lock %d !\n",
			current->comm, b_rx);
		mutex_lock(mut);
	}

	ret = sprd_modem_pms_request_resource(pms, -1);

	if (ret < 0) {
		mutex_unlock(mut);
		return ret;
	}

	strcpy(name, current->comm);
	return 0;
}

static int modem_cmd_unlock(struct modem_device *modem, int b_rx)
{
	struct mutex *mut; /* mutex point to rd_mutex or wt_mutex*/
	struct sprd_pms *pms;
	char *name;

	mut = b_rx ? &modem->rd_mutex : &modem->wt_mutex;
	pms = b_rx ? modem->rd_pms : modem->wt_pms;
	name = b_rx ? modem->rd_lock_name : modem->wt_lock_name;

	if (strlen(name) == 0)
		/* means no lock, so don't unlock */
		return 0;

	/* release resource */
	sprd_modem_pms_release_resource(pms);

	/* unlock, set name[0] to 0 */
	name[0] = 0;
	mutex_unlock(mut);

	dev_dbg(modem->p_dev,
		"unlock, %s has unlock %d!\n",
		current->comm, b_rx);

	return 0;
}

static int modem_get_something(struct modem_device *modem,
			       void *from,
			       unsigned int cmd,
			       unsigned long arg)
{
	if (strcmp(current->comm, modem->rd_lock_name) != 0) {
		dev_err(modem->p_dev, "get, task %s need get rd lock!\n",
			current->comm);
		return -EBUSY;
	}

	if (copy_to_user((void __user *)arg, from, _IOC_SIZE(cmd)))
		return -EFAULT;

	dev_dbg(modem->p_dev, "get, %s arg 0x%lx!\n", current->comm, arg);

	return 0;
}

static int modem_set_something(struct modem_device *modem,
			       void *to, unsigned int cmd, unsigned long arg)
{
	dev_dbg(modem->p_dev, "set, %s cmd 0x%x!\n", current->comm, cmd);

	if (strcmp(current->comm, modem->wt_lock_name) != 0) {
		dev_err(modem->p_dev, "set, task %s need get wt lock!\n",
		       current->comm);
		return -EBUSY;
	}

	if (copy_from_user(to, (void __user *)arg, _IOC_SIZE(cmd)))
		return -EFAULT;

	return 0;
}

static void modem_reg_ctrl(struct modem_device *modem, u32 index, int b_clear)
{
	struct regmap *map;
	u32 reg, mask, val;
	struct modem_ctrl *ctrl = modem->modem_ctrl;

	reg = ctrl->ctrl_reg[index];
	if ((reg == MODEM_INVALID_REG) || (reg == MODEM_NULL_REG))
		return;

	map = ctrl->ctrl_map[index];
	mask = ctrl->ctrl_mask[index];
	val = b_clear ? ~mask : mask;
	dev_dbg(modem->p_dev, "ctrl reg = 0x%x, mask =0x%x, val =0x%x\n",
		reg, mask, val);

	regmap_update_bits(map, reg, mask, val);
}

static void soc_modem_start(struct modem_device *modem)
{
	/* clear cp force shutdown */
	modem_reg_ctrl(modem, MODEM_CTRL_SHUT_DOWN, 1);

	if (!strcmp(modem->modem_name, "v3phy"))
		modem_reg_ctrl(modem, MODEM_CTRL_FSHUT_DOWN, 1);

	/* clear cp force deep sleep */
	modem_reg_ctrl(modem, MODEM_CTRL_DEEP_SLEEP, 1);

	/* waiting for power on stably */
	msleep(50);

	/* clear sys reset */
	modem_reg_ctrl(modem, MODEM_CTRL_SYS_RESET, 1);

	/* clear core reset */
	modem_reg_ctrl(modem, MODEM_CTRL_CORE_RESET, 1);

	/* waiting for core reset release stably */
	msleep(50);

	dev_info(modem->p_dev, "start over\n");
}

static void soc_modem_stop(struct modem_device *modem)
{
	/* set core reset */
	modem_reg_ctrl(modem, MODEM_CTRL_CORE_RESET, 0);

	/* set sys reset */
	modem_reg_ctrl(modem, MODEM_CTRL_SYS_RESET, 0);

	/* waiting for core reset hold stably */
	msleep(50);

	/* set cp force deep sleep */
	modem_reg_ctrl(modem, MODEM_CTRL_DEEP_SLEEP, 0);

	if (!strcmp(modem->modem_name, "v3phy"))
		modem_reg_ctrl(modem, MODEM_CTRL_FSHUT_DOWN, 0);

	/* set cp force shutdown */
	modem_reg_ctrl(modem, MODEM_CTRL_SHUT_DOWN, 0);

	/* clear cp auto shutdown */
	modem_reg_ctrl(modem, MODEM_CTRL_AUTO_SHUT_DOWN, 1);

	/* waiting for power off stably */
	msleep(50);

	dev_info(modem->p_dev, "stop over\n");
}

static int modem_run(struct modem_device *modem, u8 b_run, u8 b_restrict)
{
	dev_info(modem->p_dev, "%s modem run = %d!\n", current->comm, b_run);

	if (b_restrict && modem->run_state == b_run)
		return -EINVAL;

	modem->run_state = b_run;
	if (modem->modem_type == SOC_MODEM) {
		if (b_run)
			soc_modem_start(modem);
		else
			soc_modem_stop(modem);
	}

	return 0;
}

#ifdef CONFIG_SPRD_EXT_MODEM
static void modem_get_remote_flag(struct modem_device *modem)
{
	ext_modem_ops->get_remote_flag(modem);
	dev_info(modem->p_dev, "get remote flag = 0x%x!\n", modem->remote_flag);
}

static void modem_set_remote_flag(struct modem_device *modem, u8 b_clear)
{
	ext_modem_ops->set_remote_flag(modem, b_clear);
	dev_info(modem->p_dev, "set remote flag = 0x%x, b_clear = %d!\n",
		 modem->remote_flag, b_clear);
}
#endif

#ifdef CONFIG_SPRD_EXT_MODEM_POWER_CTRL
static int modem_reboot_ext_modem(struct modem_device *modem, u8 b_reset)
{
	return ext_modem_ops->reboot(modem, b_reset);
}

static int modem_poweroff_ext_modem(struct modem_device *modem)
{
	return ext_modem_ops->poweroff(modem);
}
#endif

static int modem_assert(struct modem_device *modem)
{
	return smsg_senddie(modem->modem_dst);
}

static long modem_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = -EINVAL;
	int access = 0;
	int param = 0;
#ifdef CONFIG_SPRD_EXT_MODEM
	u8 b_clear;
#endif

	struct modem_device *modem = (struct modem_device *)filp->private_data;

	dev_dbg(modem->p_dev, "ioctl, cmd=0x%x (%c nr=%d len=%d dir=%d)\n", cmd,
		_IOC_TYPE(cmd), _IOC_NR(cmd), _IOC_SIZE(cmd), _IOC_DIR(cmd));

	if (_IOC_DIR(cmd) & _IOC_READ)
		access = !access_ok((void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		access = !access_ok((void __user *)arg, _IOC_SIZE(cmd));

	if (access) {
		dev_err(modem->p_dev, "ioctl, access isn't ok! ret=%d\n", ret);
		return -EFAULT;
	}

	dev_dbg(modem->p_dev, "ioctl, arg = 0x%lx!", arg);

	switch (cmd) {
	case MODEM_READ_LOCK_CMD:
		ret = modem_cmd_lock(filp, modem, 1);
		break;

	case MODEM_READ_UNLOCK_CMD:
		ret = modem_cmd_unlock(modem, 1);
		break;

	case MODEM_WRITE_LOCK_CMD:
		ret = modem_cmd_lock(filp, modem, 0);
		break;

	case MODEM_WRITE_UNLOCK_CMD:
		ret = modem_cmd_unlock(modem, 0);
		break;

	case MODEM_GET_LOAD_INFO_CMD:
		ret = modem_get_something(modem, modem->load,
			      cmd, arg);
		break;

	case MODEM_SET_LOAD_INFO_CMD:
		if (strcmp(current->comm, "modem_control"))
			return -EPERM;

		ret = modem_set_something(modem, modem->load,
					  cmd, arg);
		if (!ret) {
			modem->all_base = (phys_addr_t)modem->load->all_base;
			modem->all_size = (size_t)modem->load->all_size;
			modem->modem_base = (phys_addr_t)
					    modem->load->modem_base;
			modem->modem_size = (size_t)modem->load->modem_size;
			modem->mini_base[MINI_DUMP_PS] =
				(phys_addr_t)modem->load->mini_base[MINI_DUMP_PS];
			modem->mini_size[MINI_DUMP_PS] =
				(size_t)modem->load->mini_size[MINI_DUMP_PS];
			modem->mini_base[MINI_DUMP_PHY] =
				(phys_addr_t)modem->load->mini_base[MINI_DUMP_PHY];
			modem->mini_size[MINI_DUMP_PHY] =
				(size_t)modem->load->mini_size[MINI_DUMP_PHY];
		}
		break;

	case MODEM_SET_READ_REGION_CMD:
		ret = modem_set_something(modem,
					  &param,
					  cmd, arg);
		modem->read_region = (u8)param;
		break;

	case MODEM_SET_WRITE_GEGION_CMD:
		ret = modem_set_something(modem,
					  &param,
					  cmd, arg);
		modem->write_region = (u8)param;
		break;

#ifdef CONFIG_SPRD_EXT_MODEM
	case MODEM_GET_REMOTE_FLAG_CMD:
		modem_get_remote_flag(modem);
		param = (int)modem->remote_flag;
		ret = modem_get_something(modem,
					  &param,
					  cmd, arg);
		break;

	case MODEM_SET_REMOTE_FLAG_CMD:
	case MODEM_CLR_REMOTE_FLAG_CMD:
		b_clear = cmd == MODEM_CLR_REMOTE_FLAG_CMD ? 1 : 0;
		ret = modem_set_something(modem,
					  &param,
					  cmd, arg);
		if (ret == 0) {
			modem->remote_flag = param;
			modem_set_remote_flag(modem, b_clear);
		}
		break;
#endif

	case MODEM_ENTER_SLEEP_CMD:
		ret = modem_enter_sleep(modem);
		break;

	case MODEM_STOP_CMD:
		ret = modem_run(modem, 0, 1);
		break;

	case MODEM_START_CMD:
		ret = modem_run(modem, 1, 1);
		break;

	case MODEM_ASSERT_CMD:
		if (strcmp(current->comm, "modem_control"))
			return -EPERM;
		ret = modem_assert(modem);
		break;

#ifdef CONFIG_SPRD_EXT_MODEM_POWER_CTRL
	case MODEM_REBOOT_EXT_MODEM_CMD:
		ret = modem_reboot_ext_modem(modem, 1);
		break;

	case MODEM_POWERON_EXT_MODEM_CMD:
		ret = modem_reboot_ext_modem(modem, 0);
		break;

	case MODEM_POWEROFF_EXT_MODEM_CMD:
		ret = modem_poweroff_ext_modem(modem);
		break;
#endif
	case MODEM_UNCONDITIONAL_STOP_CMD:
		ret = modem_run(modem, 0, 0);
		break;

	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long modem_compat_ioctl(struct file *filp, unsigned int cmd,
				unsigned long arg)
{
	return modem_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#endif


static int modem_get_device_name(struct modem_device *modem,
			       struct device_node *np)
{
	int modem_id;
	int ret = 0;
	u32 version;

	/* get version */
	ret = of_property_read_u32(np, "sprd,version", &version);
	if (ret) {
		dev_dbg(modem->p_dev, "not found version!\n");
		version = VERSION_V2;
	}

	switch (version) {
	case VERSION_V4:
		modem_id = of_alias_get_id(np, "v4-modem");
		if (modem_id == -ENODEV || modem_id >= SPRD_V4_MODEM_CNT) {
			dev_err(modem->p_dev, "fail to get id\n");
			return -ENODEV;
		}
		modem->modem_name = modem_v4[modem_id].name;
		modem->modem_dst = modem_v4[modem_id].dst;
		break;
	case VERSION_V3:
		modem_id = of_alias_get_id(np, "v3-modem");
		if (modem_id == -ENODEV || modem_id >= SPRD_V3_MODEM_CNT) {
			dev_err(modem->p_dev, "fail to get id\n");
			return -ENODEV;
		}
		modem->modem_name = modem_v3[modem_id].name;
		modem->modem_dst = modem_v3[modem_id].dst;
		break;
	case VERSION_V2:
		modem_id = of_alias_get_id(np, "v2-modem");
		if (modem_id == -ENODEV || modem_id >= SPRD_V2_MODEM_CNT) {
			dev_err(modem->p_dev, "fail to get id\n");
			return -ENODEV;
		}
		modem->modem_name = modem_v2[modem_id].name;
		modem->modem_dst = modem_v2[modem_id].dst;
		break;
	default:
		return -ENODEV;
	}

	return 0;
}

static int pcie_modem_parse_dt(struct modem_device *modem,
			       struct device_node *np)
{
	int ret;

	modem->modem_type = PCIE_MODEM;

	ret = modem_get_device_name(modem, np);
	if (ret)
		return ret;

#ifdef CONFIG_SPRD_EXT_MODEM_POWER_CTRL
	modem->modem_power = devm_gpiod_get(modem->p_dev,
					    "poweron",
					    GPIOD_OUT_HIGH);
	if (IS_ERR(modem->modem_power)) {
		dev_err(modem->p_dev, "get poweron gpio failed!\n");
		return PTR_ERR(modem->modem_power);
	}


	modem->modem_reset = devm_gpiod_get(modem->p_dev,
					    "reset",
					    GPIOD_OUT_HIGH);
	if (IS_ERR(modem->modem_reset)) {
		dev_err(modem->p_dev, "get reset gpio failed!\n");
		return PTR_ERR(modem->modem_reset);
	}
#endif

	return 0;
}

static int soc_modem_parse_dt(struct modem_device *modem,
			      struct device_node *np)
{
	int ret, cr_num;
	struct modem_ctrl *modem_ctl;
	struct pm_reg_ctrl *pm_reg_ctl;
	u32 syscon_args[2];
	char sysconn[8];
	struct of_phandle_args out_args;

	modem->modem_type = SOC_MODEM;

	ret = modem_get_device_name(modem, np);
	if (ret)
		return ret;

	modem_ctl = devm_kzalloc(modem->p_dev,
				 sizeof(struct modem_ctrl),
				 GFP_KERNEL);
	if (!modem_ctl)
		return -ENOMEM;

	cr_num = 0;

	do {
		/* get apb & pmu reg handle */
		sprintf(sysconn, "%s%d", "syscon", cr_num);
		dev_dbg(modem->p_dev, "sysconn = %s\n", sysconn);

		modem_ctl->ctrl_map[cr_num] = syscon_regmap_lookup_by_phandle_args(np,
						sysconn, 2, (u32 *)syscon_args);

		if (IS_ERR(modem_ctl->ctrl_map[cr_num])) {
			if ((-ENOENT) == PTR_ERR(modem_ctl->ctrl_map[cr_num]))
				/* some dts no need config all args, just break */
				break;
			dev_err(modem->p_dev, "failed to find %s\n",
				modem_ctrl_args[cr_num]);
			return -EINVAL;
		}

		/**
		 * 1.get ctrl_reg offset, the ctrl-reg variable number, so need
		 * to start reading from the largest until success.
		 * 2.get ctrl_mask
		 */
		modem_ctl->ctrl_reg[cr_num] = syscon_args[0];
		modem_ctl->ctrl_mask[cr_num] = syscon_args[1];
		dev_dbg(modem->p_dev, "%s: ctrl_reg[%d] = 0x%x, ctrl_mask[%d] = 0x%08x\n",
					__func__, cr_num, modem_ctl->ctrl_reg[cr_num], cr_num,
					modem_ctl->ctrl_mask[cr_num]);

		cr_num++;
	} while (cr_num < MODEM_CTRL_NR && modem_ctrl_args[cr_num] != NULL);
	ret = of_parse_phandle_with_args(np, "sprd,sys-bus-ctrl", "#syscon-cells", 0, &out_args);
	if (!ret) {
		of_node_put(out_args.np);
		pm_reg_ctl = devm_kzalloc(modem->p_dev,
					sizeof(struct pm_reg_ctrl),
					GFP_KERNEL);
		if (!pm_reg_ctl)
			return -ENOMEM;

		pm_reg_ctl->reg_offset = out_args.args[0];
		pm_reg_ctl->reg_mask = out_args.args[1];
		modem->pm_reg_ctrl = pm_reg_ctl;
		pm_reg_ctl->ctrl_map = syscon_regmap_lookup_by_phandle(np, "sprd,sys-bus-ctrl");
		if (IS_ERR(pm_reg_ctl->ctrl_map)) {
			dev_err(modem->p_dev, "failed to find pm_aon_apb reg.\n");
			return -EINVAL;
		}

		dev_info(modem->p_dev, "offset = 0x%x, mask = 0x%x.\n",
			 modem->pm_reg_ctrl->reg_offset, modem->pm_reg_ctrl->reg_mask);
	}
	modem->modem_ctrl = modem_ctl;
	return 0;
}

#ifdef CONFIG_DEBUG_FS
static const struct file_operations modem_debug_fops;
static struct dentry *modem_root;

static void modem_debug_putline(struct seq_file *m, char c, int n)
{
	char buf[300];
	int i, max, len;

	/* buf will end with '\n' and 0 */
	max = ARRAY_SIZE(buf) - 2;
	len = n > max ? max : n;

	for (i = 0; i < len; i++)
		buf[i] = c;

	buf[i] = '\n';
	buf[i + 1] = 0;

	seq_puts(m, buf);
}

static int modem_debug_show(struct seq_file *m, void *private)
{
	u32 region_cnt, i;
	struct modem_region_info *regions;
	struct modem_device *modem = (struct modem_device *)m->private;

	region_cnt = modem->load->region_cnt;
	regions = modem->load->regions;

	modem_debug_putline(m, '*', 100);

	seq_printf(m, "%s info:\n", modem->modem_name);
	modem_debug_putline(m, '-', 80);
	seq_printf(m, "read_region: %d, write_region: %d\n",
		   modem->read_region, modem->write_region);
	seq_printf(m, "run_state: %d, remote_flag: %d\n",
		   modem->run_state, modem->remote_flag);
	seq_printf(m, "modem_base: 0x%llx, size: 0x%lx\n",
		   modem->modem_base, modem->modem_size);
	seq_printf(m, "all_base: 0x%llx, size: 0x%lx\n",
		   modem->all_base, modem->all_size);

	modem_debug_putline(m, '-', 80);
	seq_puts(m, "region list:\n");

	for (i = 0; i < region_cnt; i++)
		seq_printf(m, "region[%2d]:address=0x%llx, size=0x%lx, name=%s\n",
			   i,
			   (phys_addr_t)regions[i].address,
			   (size_t)regions[i].size,
			   regions[i].name);

	if (modem->modem_ctrl) {
		struct modem_ctrl *ctrl = modem->modem_ctrl;

		modem_debug_putline(m, '-', 80);
		seq_puts(m, "modem ctl info:\n");

		for (i = 0; i < MODEM_CTRL_NR; i++)
			seq_printf(m, "region[%2d]:reg=0x%x, mask=0x%x\n",
				   i,
				   ctrl->ctrl_reg[i],
				   ctrl->ctrl_mask[i]);
	}
	modem_debug_putline(m, '*', 100);

	return 0;
}

static int modem_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, modem_debug_show, inode->i_private);
}

static const struct file_operations modem_debug_fops = {
	.open = modem_debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static void modem_init_debugfs(struct modem_device *modem)
{
	modem->debug_file = debugfs_create_file(modem->modem_name, 0444,
				modem_root,
				modem,
				&modem_debug_fops);
}

static void modem_remove_debugfs(struct modem_device *modem)
{
	debugfs_remove(modem->debug_file);
}
#endif

static const struct file_operations modem_fops = {
	.open = modem_open,
	.release = modem_release,
	.llseek = modem_lseek,
	.read = modem_read,
	.write = modem_write,
	.unlocked_ioctl = modem_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = modem_compat_ioctl,
#endif
	.owner = THIS_MODULE
};

static const struct of_device_id modem_match_table[] = {
	{.compatible = "unisoc,modem", .data = soc_modem_parse_dt},
	{.compatible = "unisoc,pcie-modem", .data = pcie_modem_parse_dt},
	{ },
};

static int modem_probe(struct platform_device *pdev)
{
	struct modem_device *modem;
	int ret = 0;
	MODEM_PARSE_FUN dt_parse;
	struct device_node *np = pdev->dev.of_node;
	struct device *dev;

	modem = devm_kzalloc(&pdev->dev,
			     sizeof(struct modem_device),
			     GFP_KERNEL);
	if (!modem)
		return -ENOMEM;

	modem->p_dev = &pdev->dev;
	modem->run_state = RUN_STATE_INVALID;
	modem->load = devm_kzalloc(modem->p_dev,
				   sizeof(struct modem_load_info),
				   GFP_KERNEL);
	if (!modem->load)
		return -ENOMEM;

	dt_parse = of_device_get_match_data(modem->p_dev);
	if (!dt_parse) {
		dev_err(modem->p_dev, "cat't get parse fun!\n");
		return -EINVAL;
	}

	ret = dt_parse(modem, np);
	if (ret)
		return ret;

	ret = alloc_chrdev_region(&modem->devid, 0, 1, modem->modem_name);
	if (ret != 0) {
		dev_err(modem->p_dev, "get name fail, ret = %d!\n", ret);
		return ret;
	}

	cdev_init(&modem->cdev, &modem_fops);
	ret = cdev_add(&modem->cdev, modem->devid, 1);
	if (ret != 0) {
		unregister_chrdev_region(modem->devid, 1);
		dev_err(modem->p_dev, "add dev fail, ret = %d!\n", ret);
		return ret;
	}

	dev = device_create(modem_class, NULL,
		      modem->devid,
		      NULL, "%s", modem->modem_name);
	if (IS_ERR(dev))
		dev_err(modem->p_dev, "device_create fail,ERRNO = %ld!\n",
			PTR_ERR(dev));

	mutex_init(&modem->rd_mutex);
	mutex_init(&modem->wt_mutex);
	snprintf(modem->rd_pms_name,
		 sizeof(modem->rd_pms_name), "%s-rd", modem->modem_name);
	snprintf(modem->wt_pms_name,
		 sizeof(modem->wt_pms_name), "%s-wt", modem->modem_name);
	modem->rd_pms = sprd_pms_create(modem->modem_dst,
					modem->rd_pms_name, false);
	if (!modem->rd_pms)
		pr_warn("create pms %s failed!\n", modem->rd_pms_name);

	modem->wt_pms = sprd_pms_create(modem->modem_dst,
					modem->wt_pms_name, false);
	if (!modem->rd_pms)
		pr_warn("create pms %s failed!\n", modem->wt_pms_name);

	platform_set_drvdata(pdev, modem);

#if defined(CONFIG_DEBUG_FS)
	modem_init_debugfs(modem);
#endif

	return 0;
}

static int  modem_remove(struct platform_device *pdev)
{
	struct modem_device *modem = platform_get_drvdata(pdev);

	if (modem) {
		sprd_pms_destroy(modem->rd_pms);
		sprd_pms_destroy(modem->wt_pms);
		mutex_destroy(&modem->rd_mutex);
		mutex_destroy(&modem->wt_mutex);
		device_destroy(modem_class, modem->devid);
		cdev_del(&modem->cdev);
		unregister_chrdev_region(modem->devid, 1);
#if defined(CONFIG_DEBUG_FS)
		modem_remove_debugfs(modem);
#endif
		platform_set_drvdata(pdev, NULL);
	}

	return 0;
}

static struct platform_driver modem_driver = {
	.driver = {
		.name = "modem",
		.of_match_table = modem_match_table,
	},
	.probe = modem_probe,
	.remove = modem_remove,
};

static int __init modem_init(void)
{
	modem_class = class_create(THIS_MODULE, "ext_modem");
	if (IS_ERR(modem_class))
		return PTR_ERR(modem_class);

#if defined(CONFIG_DEBUG_FS)
	modem_root = debugfs_create_dir("modem", NULL);
	if (IS_ERR(modem_root))
		return PTR_ERR(modem_root);
#endif

#ifdef CONFIG_SPRD_EXT_MODEM
	 modem_get_ext_modem_ops(&ext_modem_ops);
#endif

	return platform_driver_register(&modem_driver);
}

static void __exit modem_exit(void)
{
	class_destroy(modem_class);

#if defined(CONFIG_DEBUG_FS)
	debugfs_remove(modem_root);
#endif

	platform_driver_unregister(&modem_driver);
}

module_init(modem_init);
module_exit(modem_exit);

MODULE_AUTHOR("Wenping zhou");
MODULE_DESCRIPTION("External modem driver");
MODULE_LICENSE("GPL v2");
