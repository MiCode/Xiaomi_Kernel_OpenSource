// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/sysfs.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/string.h>
#include <linux/seq_file.h>
#include <linux/semaphore.h>
#include <linux/alarmtimer.h>
#include <linux/workqueue.h>

#include <mt-plat/rt-regmap.h>
#define RT_REGMAP_VERSION	"1.1.14_G"

struct rt_regmap_ops {
	int (*regmap_block_write)(struct rt_regmap_device *rd, u32 reg,
			 int bytes, const void *data);
	int (*regmap_block_read)(struct rt_regmap_device *rd, u32 reg,
			int bytes, void *dest);
};

enum {
	RT_DBG_REG,
	RT_DBG_DATA,
	RT_DBG_REGS,
	RT_DBG_SYNC,
	RT_DBG_ERROR,
	RT_DBG_NAME,
	RT_DBG_BLOCK,
	RT_DBG_SIZE,
	RT_DBG_SLAVE_ADDR,
	RT_DBG_SUPPORT_MODE,
	RT_DBG_IO_LOG,
	RT_DBG_CACHE_MODE,
	RT_DBG_REG_SIZE,
	RT_DBG_WATCHDOG,
	RT_DBG_MAX,
};

struct reg_index_offset {
	int index;
	int offset;
};

#ifdef CONFIG_DEBUG_FS
struct rt_debug_data {
	struct reg_index_offset rio;
	unsigned int reg_addr;
	unsigned int reg_size;
	unsigned char part_id;
};

struct rt_debug_st {
	void *info;
	int id;
};
#endif /* CONFIG_DEBUG_FS */


/* rt_regmap_device
 *
 * Richtek regmap device. One for each rt_regmap.
 *
 */
struct rt_regmap_device {
	struct rt_regmap_properties props;
	struct rt_regmap_fops *rops;
	struct rt_regmap_ops regmap_ops;
	struct alarm watchdog_alarm;
	struct delayed_work watchdog_work;
	struct device dev;
	void *client;
	struct semaphore semaphore;
	struct semaphore write_mode_lock;
#ifdef CONFIG_DEBUG_FS
	struct dentry *rt_den;
	struct dentry *rt_debug_file[RT_DBG_MAX];
	struct rt_debug_st rtdbg_st[RT_DBG_MAX];
	struct dentry **rt_reg_file;
	struct rt_debug_st **reg_st;
	struct rt_debug_data dbg_data;
#endif /* CONFIG_DEBUG_FS */
	struct delayed_work rt_work;
	unsigned char *cache_flag;
	unsigned char part_size_limit;
	unsigned char *alloc_data;
	unsigned char **cache_data;
	unsigned char *cached;
	char *err_msg;
	int slv_addr;

	int (*rt_block_write[4])(struct rt_regmap_device *rd,
			const struct rt_register *rm, int size,
			const struct reg_index_offset *rio,
			unsigned char *wdata, int *count, int cache_idx);
	unsigned char cache_inited:1;
	unsigned char error_occurred:1;
	unsigned char pending_event:1;
};

#ifdef CONFIG_DEBUG_FS
struct dentry *rt_regmap_dir;

static int get_parameters(char *buf, long *param1, int num_of_par)
{
	char *token;
	int base, cnt;

	token = strsep(&buf, " ");

	for (cnt = 0; cnt < num_of_par; cnt++) {
		if (token != NULL) {
			if ((token[1] == 'x') || (token[1] == 'X'))
				base = 16;
			else
				base = 10;

			if (kstrtoul(token, base, &param1[cnt]) != 0)
				return -EINVAL;

			token = strsep(&buf, " ");
		} else
			return -EINVAL;
	}
	return 0;
}

static int get_datas(const char *buf, const int length,
		     unsigned char *data_buffer, unsigned char data_length)
{
	int i, ptr;
	long value;
	char token[5];

	token[0] = '0';
	token[1] = 'x';
	token[4] = 0;
	if (buf[0] != '0' || buf[1] != 'x')
		return -EINVAL;

	ptr = 2;
	for (i = 0; (i < data_length) && (ptr + 2 <= length); i++) {
		token[2] = buf[ptr++];
		token[3] = buf[ptr++];
		ptr++;
		if (kstrtoul(token, 16, &value) != 0)
			return -EINVAL;
		data_buffer[i] = value;
	}
	return 0;
}
#endif /* CONFIG_DEBUG_FS */

static struct reg_index_offset find_register_index(
		const struct rt_regmap_device *rd, u32 reg)
{
	const rt_register_map_t *rm = rd->props.rm;
	int register_num = rd->props.register_num;
	struct reg_index_offset rio = {0, 0};
	int index = 0, i = 0, unit = RT_1BYTE_MODE;

	for (index = 0; index < register_num; index++) {
		if (reg == rm[index]->addr) {
			rio.index = index;
			rio.offset = 0;
			break;
		}
		if (reg > rm[index]->addr) {
			if ((reg - rm[index]->addr) < rm[index]->size) {
				rio.index = index;
				while (&rd->props.group[i] != NULL) {
					if (reg >= rd->props.group[i].start
					&& reg <= rd->props.group[i].end) {
						unit =
							rd->props.group[i].mode;
						break;
					}
					i++;
					unit = RT_1BYTE_MODE;
				}
				rio.offset =
					(reg-rm[index]->addr)*unit;
			} else
				rio.offset = rio.index = -1;
		}
	}
	return rio;
}

static int rt_chip_block_write(struct rt_regmap_device *rd, u32 reg,
				int bytes, const void *src);

/* rt_regmap_cache_sync - sync all cache data to real chip*/
void rt_regmap_cache_sync(struct rt_regmap_device *rd)
{
	int i, rc, num;
	const rt_register_map_t *rm = rd->props.rm;

	down(&rd->semaphore);
	if (!rd->pending_event)
		goto err_cache_sync;

	num = rd->props.register_num;
	for (i = 0; i < num; i++) {
		if (rd->cache_flag[i] == 1) {
			rc = rt_chip_block_write(rd, rm[i]->addr,
					rm[i]->size, rd->cache_data[i]);
			if (rc < 0) {
				dev_err(&rd->dev, "rt-regmap sync error\n");
				goto err_cache_sync;
			}
			*(rd->cache_flag + i) = 0;
		}
	}
	rd->pending_event = 0;
	dev_info(&rd->dev, "regmap sync successfully\n");
err_cache_sync:
	up(&rd->semaphore);
}
EXPORT_SYMBOL(rt_regmap_cache_sync);

/* rt_regmap_cache_write_back - write current cache data to chip
 * @rd: rt_regmap_device pointer.
 * @reg: register map address
 */
void rt_regmap_cache_write_back(struct rt_regmap_device *rd, u32 reg)
{
	struct reg_index_offset rio;
	const rt_register_map_t *rm = rd->props.rm;
	int rc;

	rio = find_register_index(rd, reg);
	if (rio.index < 0) {
		dev_err(&rd->dev, "reg 0x%02x is out of range\n", reg);
		return;
	}

	down(&rd->semaphore);
	if ((rm[rio.index]->reg_type&RT_REG_TYPE_MASK) != RT_VOLATILE) {
		rc = rt_chip_block_write(rd, rm[rio.index]->addr,
					rm[rio.index]->size,
					rd->cache_data[rio.index]);
		if (rc < 0) {
			dev_err(&rd->dev, "rt-regmap sync error\n");
			goto err_cache_chip_write;
		}
		rd->cache_flag[rio.index] = 0;
	}
	dev_info(&rd->dev, "regmap sync successfully\n");
err_cache_chip_write:
	up(&rd->semaphore);
}
EXPORT_SYMBOL(rt_regmap_cache_write_back);

/* rt_is_reg_volatile - check register map is volatile or not
 * @rd: rt_regmap_device pointer.
 * reg: register map address.
 */
int rt_is_reg_volatile(struct rt_regmap_device *rd, u32 reg)
{
	struct reg_index_offset rio;
	const rt_register_map_t rm;

	rio = find_register_index(rd, reg);
	if (rio.index < 0) {
		dev_err(&rd->dev, "reg 0x%02x is out of range\n", reg);
		return -EINVAL;
	}
	rm = rd->props.rm[rio.index];

	return (rm->reg_type&RT_REG_TYPE_MASK) == RT_VOLATILE ? 1 : 0;
}
EXPORT_SYMBOL(rt_is_reg_volatile);

/* rt_reg_regsize - get register map size for specific register
 * @rd: rt_regmap_device pointer.
 * reg: register map address
 */
int rt_get_regsize(struct rt_regmap_device *rd, u32 reg)
{
	struct reg_index_offset rio;

	rio = find_register_index(rd, reg);
	if (rio.index < 0 || rio.offset != 0) {
		dev_err(&rd->dev, "reg 0x%02x is out of map\n", reg);
		return -EINVAL;
	}
	return rd->props.rm[rio.index]->size;
}
EXPORT_SYMBOL(rt_get_regsize);

static void rt_work_func(struct work_struct *work)
{
	struct rt_regmap_device *rd;

	pr_info(" %s\n", __func__);
	rd = container_of(work, struct rt_regmap_device, rt_work.work);
	rt_regmap_cache_sync(rd);
}

static int rt_chip_block_write(struct rt_regmap_device *rd, u32 reg,
				int bytes, const void *src)
{
	int ret;

	if ((rd->props.rt_regmap_mode & RT_IO_BLK_MODE_MASK) == RT_IO_BLK_ALL ||
	    (rd->props.rt_regmap_mode & RT_IO_BLK_MODE_MASK) == RT_IO_BLK_CHIP)
		return 0;

	ret = rd->rops->write_device(rd->client, reg, bytes, src);

	return ret;
}

static int rt_chip_block_read(struct rt_regmap_device *rd, u32 reg,
				int bytes, void *dst)
{
	int ret;

	ret = rd->rops->read_device(rd->client, reg, bytes, dst);
	return ret;
}

static int rt_cache_block_write(struct rt_regmap_device *rd, u32 reg,
						int bytes, const void *data)
{
	int i, j, reg_base = 0, count = 0, ret = 0, size = 0;
	struct reg_index_offset rio;
	unsigned char wdata[64];
	unsigned char wri_data[128];
	unsigned char blk_index;
	const rt_register_map_t rm;

	if (bytes > 64) {
		dev_err(&rd->dev, "over size > 64 bytes\n");
		return -EINVAL;
	}
	memcpy(wdata, data, bytes);

	rio = find_register_index(rd, reg);
	if (rio.index < 0) {
		dev_err(&rd->dev, "reg 0x%02x is out of range\n", reg);
		return -EINVAL;
	}

	reg_base = 0;
	rm = rd->props.rm[rio.index + reg_base];
	while (bytes > 0) {
		size = ((bytes <= (rm->size-rio.offset)) ?
					bytes : rm->size-rio.offset);
		if ((rm->reg_type&RT_REG_TYPE_MASK) == RT_VOLATILE) {
			ret = rt_chip_block_write(rd,
					rm->addr+rio.offset,
					size,
					&wdata[count]);
			if (ret < 0) {
				dev_notice(&rd->dev,
					   "rd->rt_block_write fail\n");
				goto ERR;
			}
			count += size;
		} else {
			blk_index = (rd->props.rt_regmap_mode &
					RT_IO_BLK_MODE_MASK)>>3;

			ret = rd->rt_block_write[blk_index]
				(rd, rm, size, &rio, wdata,
				&count, rio.index+reg_base);
			if (ret < 0) {
				dev_err(&rd->dev, "rd->rt_block_write fail\n");
				goto ERR;
			}
		}

		if ((rm->reg_type&RT_REG_TYPE_MASK) != RT_VOLATILE)
			rd->cache_flag[rio.index+reg_base] = 1;

		bytes -= size;
		if (bytes <= 0)
			goto finished;
		reg_base++;
		rm = rd->props.rm[rio.index + reg_base];
		if ((rio.index + reg_base) >= rd->props.register_num) {
			dev_err(&rd->dev, "over regmap size\n");
			goto ERR;
		}
	}
finished:
	if (rd->props.io_log_en) {
		j = 0;
		for (i = 0; i < count; i++)
			j += snprintf(wri_data + j, sizeof(wri_data) - j,
			"%02x,", wdata[i]);
		pr_info("RT_REGMAP [WRITE] reg0x%04x  [Data] 0x%s\n",
							reg, wri_data);
	}
	return 0;
ERR:
	return -EIO;
}

static int rt_asyn_cache_block_write(struct rt_regmap_device *rd, u32 reg,
						int bytes, const void *data)
{
	int i, j, reg_base, count = 0, ret = 0, size = 0;
	struct reg_index_offset rio;
	unsigned char wdata[64];
	unsigned char wri_data[128];
	unsigned char blk_index;
	const rt_register_map_t rm;

	memcpy(wdata, data, bytes);

	cancel_delayed_work_sync(&rd->rt_work);

	rio = find_register_index(rd, reg);
	if (rio.index < 0) {
		dev_err(&rd->dev, "reg 0x%02x is out of range\n", reg);
		return -EINVAL;
	}

	reg_base = 0;
	rm = rd->props.rm[rio.index + reg_base];
	while (bytes > 0) {
		size = ((bytes <= (rm->size-rio.offset)) ?
					bytes : rm->size-rio.offset);
		if ((rm->reg_type&RT_REG_TYPE_MASK) == RT_VOLATILE) {
			ret = rt_chip_block_write(rd,
					rm->addr+rio.offset, size,
					&wdata[count]);
			count += size;
		} else {
			blk_index = (rd->props.rt_regmap_mode &
					RT_IO_BLK_MODE_MASK)>>3;
			ret = rd->rt_block_write[blk_index]
				(rd, rm, size, &rio, wdata,
				&count, rio.index+reg_base);
		}
		if (ret < 0) {
			dev_err(&rd->dev, "rd->rt_block_write fail\n");
			goto ERR;
		}

		if ((rm->reg_type&RT_REG_TYPE_MASK) != RT_VOLATILE) {
			rd->cache_flag[rio.index+reg_base] = 1;
			rd->pending_event = 1;
		}

		bytes -= size;
		if (bytes <= 0)
			goto finished;
		reg_base++;
		rm = rd->props.rm[rio.index + reg_base];
		rio.offset = 0;
		if ((rio.index + reg_base) >= rd->props.register_num) {
			dev_err(&rd->dev, "over regmap size\n");
			goto ERR;
		}
	}
finished:
	if (rd->props.io_log_en) {
		j = 0;
		for (i = 0; i < count; i++)
			j += snprintf(wri_data + j, sizeof(wri_data) - j,
			"%02x,", wdata[i]);
		pr_info("RT_REGMAP [WRITE] reg0x%04x  [Data] 0x%s\n",
								reg, wri_data);
	}

	schedule_delayed_work(&rd->rt_work, msecs_to_jiffies(1));
	return 0;
ERR:
	return -EIO;
}

static int rt_block_write_blk_all(struct rt_regmap_device *rd,
				  const struct rt_register *rm, int size,
				  const struct reg_index_offset *rio,
				  unsigned char *wdata, int *count,
				  int cache_idx)
{
	down(&rd->write_mode_lock);
	*count += size;
	up(&rd->write_mode_lock);
	return 0;
}

static int rt_block_write_blk_chip(struct rt_regmap_device *rd,
				   const struct rt_register *rm, int size,
				   const struct reg_index_offset *rio,
				   unsigned char *wdata, int *count,
				   int cache_idx)
{
	int i;

	down(&rd->write_mode_lock);
	for (i = rio->offset; i < rio->offset+size; i++) {
		if ((rm->reg_type&RT_REG_TYPE_MASK) != RT_VOLATILE) {
			rd->cache_data[cache_idx][i] =
				wdata[*count] & rm->wbit_mask[i];
			if (!rd->cached[cache_idx])
				rd->cached[cache_idx] = 1;
		}
		*count = *count + 1;
	}
	up(&rd->write_mode_lock);
	return 0;
}

static int rt_block_write_blk_cache(struct rt_regmap_device *rd,
				    const struct rt_register *rm, int size,
				    const struct reg_index_offset *rio,
				    unsigned char *wdata, int *count,
				    int cache_idx)
{
	int ret, cnt;

	down(&rd->write_mode_lock);
	cnt = *count;

	ret = rt_chip_block_write(rd, rm->addr+rio->offset, size, &wdata[cnt]);
	if (ret < 0) {
		dev_err(&rd->dev,
		"rt block write fail at 0x%02x\n", rm->addr + rio->offset);
		up(&rd->write_mode_lock);
		return -EIO;
	}
	cnt += size;
	*count = cnt;
	up(&rd->write_mode_lock);
	return 0;
}

static int rt_block_write(struct rt_regmap_device *rd,
			  const struct rt_register *rm, int size,
			  const struct reg_index_offset *rio,
			  unsigned char *wdata, int *count, int cache_idx)
{
	int i, ret = 0, cnt, change = 0;

	down(&rd->write_mode_lock);
	cnt = *count;

	if (!rd->cached[cache_idx]) {
		for (i = rio->offset; i < size+rio->offset; i++) {
			if ((rm->reg_type & RT_REG_TYPE_MASK) != RT_VOLATILE) {
				rd->cache_data[cache_idx][i] =
					wdata[cnt] & rm->wbit_mask[i];
			}
			cnt++;
		}
		rd->cached[cache_idx] = 1;
		change++;
	} else {
		for (i = rio->offset; i < size+rio->offset; i++) {
			if ((rm->reg_type & RT_REG_TYPE_MASK) != RT_VOLATILE) {
				if (rm->reg_type&RT_WR_ONCE) {
					if (rd->cache_data[cache_idx][i] !=
						(wdata[cnt]&rm->wbit_mask[i]))
						change++;
				}
				rd->cache_data[cache_idx][i] =
					wdata[cnt] & rm->wbit_mask[i];
			}
			cnt++;
		}
	}

	if (!change && (rm->reg_type&RT_WR_ONCE))
		goto finish;

	ret = rt_chip_block_write(rd,
		rm->addr+rio->offset, size, rd->cache_data[cache_idx]);
	if (ret < 0)
		dev_err(&rd->dev, "rt block write fail at 0x%02x\n",
						rm->addr + rio->offset);
finish:
	*count = cnt;
	up(&rd->write_mode_lock);
	return ret;
}

static int (*rt_block_map[])(struct rt_regmap_device *rd,
			     const struct rt_register *rm, int size,
			     const struct reg_index_offset *rio,
			     unsigned char *wdata, int *count,
			     int cache_idx) = {
	&rt_block_write,
	&rt_block_write_blk_all,
	&rt_block_write_blk_cache,
	&rt_block_write_blk_chip,
};

static int rt_cache_block_read(struct rt_regmap_device *rd, u32 reg,
			int bytes, void *dest)
{
	int i, ret, count = 0, reg_base = 0, total_bytes = 0;
	struct reg_index_offset rio;
	const rt_register_map_t rm;
	unsigned char data[100];
	unsigned char tmp_data[32];

	rio = find_register_index(rd, reg);
	if (rio.index < 0) {
		dev_err(&rd->dev, "reg 0x%02x is out of range\n", reg);
		return -EINVAL;
	}

	rm = rd->props.rm[rio.index];

	total_bytes += (rm->size - rio.offset);

	for (i = rio.index+1; i < rd->props.register_num; i++)
		total_bytes += rd->props.rm[i]->size;

	if (bytes > total_bytes) {
		dev_err(&rd->dev, "out of cache map range\n");
		return -EINVAL;
	}

	memcpy(data, &rd->cache_data[rio.index][rio.offset], bytes);

	if ((rm->reg_type&RT_REG_TYPE_MASK) == RT_VOLATILE
					|| rd->cached[rio.index] == 0) {
		ret = rd->rops->read_device(rd->client,
				rm->addr, rm->size, tmp_data);
		if (ret < 0) {
			dev_err(&rd->dev,
			"rt_regmap Error at 0x%02x\n", rm->addr);
			return -EIO;
		}
		for (i = rio.offset; i < rm->size; i++) {
			data[count] = tmp_data[i];
			count++;
		}
		if (!rd->cached[rio.index]) {
			memcpy(rd->cache_data[rio.index], &tmp_data, rm->size);
			rd->cached[rio.index] = 1;
		}
	} else
		count += (rm->size - rio.offset);

	while (count < bytes) {
		reg_base++;
		rm = rd->props.rm[rio.index + reg_base];
		if ((rm->reg_type&RT_REG_TYPE_MASK) == RT_VOLATILE ||
				rd->cached[rio.index+reg_base] == 0) {
			ret = rd->rops->read_device(rd->client,
					rm->addr, rm->size, &data[count]);
			if (ret < 0) {
				dev_err(&rd->dev,
				"rt_regmap Error at 0x%02x\n", rm->addr);
				return -EIO;
			}
			if (!rd->cached[rio.index+reg_base]) {
				memcpy(rd->cache_data[rio.index+reg_base],
						&data[count], rm->size);
				rd->cached[rio.index+reg_base] = 1;
			}
		}
		count += rm->size;
	}

	if (rd->props.io_log_en)
		pr_info("RT_REGMAP [READ] reg0x%04x\n", reg);

	memcpy(dest, data, bytes);

	return 0;
}

/* rt_regmap_cache_backup - back up all cache register value*/
void rt_regmap_cache_backup(struct rt_regmap_device *rd)
{
	const rt_register_map_t *rm = rd->props.rm;
	int i;

	down(&rd->semaphore);
	for (i = 0; i < rd->props.register_num; i++)
		if ((rm[i]->reg_type&RT_REG_TYPE_MASK) != RT_VOLATILE)
			rd->cache_flag[i] = 1;
	rd->pending_event = 1;
	up(&rd->semaphore);
}
EXPORT_SYMBOL(rt_regmap_cache_backup);

/* _rt_regmap_reg_write - write data to specific register map
 * only support 1, 2, 4 bytes regisetr map
 * @rd: rt_regmap_device pointer.
 * @rrd: rt_reg_data pointer.
 */
static int _rt_regmap_reg_write(struct rt_regmap_device *rd,
				struct rt_reg_data *rrd)
{
	const rt_register_map_t *rm = rd->props.rm;
	struct reg_index_offset rio;
	int ret, tmp_data;

	rio = find_register_index(rd, rrd->reg);
	if (rio.index < 0 || rio.offset != 0) {
		dev_err(&rd->dev, "reg 0x%02x is out of regmap\n", rrd->reg);
		return -EINVAL;
	}

	down(&rd->semaphore);
	switch (rm[rio.index]->size) {
	case 1:
		ret = rd->regmap_ops.regmap_block_write(rd,
				rrd->reg, 1, &rrd->rt_data.data_u8);
		if (ret < 0) {
			dev_err(&rd->dev, "rt regmap block write fail\n");
			up(&rd->semaphore);
			return -EIO;
		}
		break;
	case 2:
		if (rd->props.rt_format == RT_LITTLE_ENDIAN)
			tmp_data = be16_to_cpu(rrd->rt_data.data_u32);
		ret = rd->regmap_ops.regmap_block_write(rd,
				rrd->reg, rm[rio.index]->size, &tmp_data);
		if (ret < 0) {
			dev_err(&rd->dev, "rt regmap block write fail\n");
			up(&rd->semaphore);
			return -EIO;
		}
		break;
	case 3:
		if (rd->props.rt_format == RT_LITTLE_ENDIAN) {
			tmp_data = be32_to_cpu(rrd->rt_data.data_u32);
			tmp_data >>= 8;
		}
		ret = rd->regmap_ops.regmap_block_write(rd,
			rrd->reg, rm[rio.index]->size, &tmp_data);
		if (ret < 0) {
			dev_err(&rd->dev, "rt regmap block write fail\n");
			up(&rd->semaphore);
			return -EIO;
		}
		break;
	case 4:
		if (rd->props.rt_format == RT_LITTLE_ENDIAN)
			tmp_data = be32_to_cpu(rrd->rt_data.data_u32);
		ret = rd->regmap_ops.regmap_block_write(rd,
			rrd->reg, rm[rio.index]->size, &tmp_data);
		if (ret < 0) {
			dev_err(&rd->dev, "rt regmap block write fail\n");
			up(&rd->semaphore);
			return -EIO;
		}
		break;
	default:
		dev_err(&rd->dev,
			"Failed: only support 1~4 bytes regmap write\n");
		break;
	}
	up(&rd->semaphore);
	return 0;
}

int rt_regmap_reg_write(struct rt_regmap_device *rd,
		struct rt_reg_data *rrd, u32 reg, const u32 data)
{
	rrd->reg = reg;
	rrd->rt_data.data_u32 = data;
	return _rt_regmap_reg_write(rd, rrd);
}
EXPORT_SYMBOL(rt_regmap_reg_write);

/* _rt_asyn_regmap_reg_write - asyn write data to specific register map*/
static int _rt_asyn_regmap_reg_write(struct rt_regmap_device *rd,
				struct rt_reg_data *rrd)
{
	const rt_register_map_t *rm = rd->props.rm;
	struct reg_index_offset rio;
	int ret, tmp_data = 0;

	rio = find_register_index(rd, rrd->reg);
	if (rio.index < 0 || rio.offset != 0) {
		dev_err(&rd->dev, "reg 0x%02x is out of regmap\n", rrd->reg);
		return -EINVAL;
	}

	down(&rd->semaphore);
	switch (rm[rio.index]->size) {
	case 1:
		ret = rt_asyn_cache_block_write(rd,
				rrd->reg, 1, &rrd->rt_data.data_u8);
		if (ret < 0) {
			dev_err(&rd->dev, "rt regmap block write fail\n");
			ret = -EIO;
			goto err_regmap_write;
		}
		break;
	case 2:
		if (rd->props.rt_format == RT_LITTLE_ENDIAN)
			tmp_data = be16_to_cpu(rrd->rt_data.data_u32);
		ret = rt_asyn_cache_block_write(rd,
				rrd->reg, rm[rio.index]->size, &tmp_data);
		if (ret < 0) {
			dev_err(&rd->dev, "rt regmap block write fail\n");
			ret = -EIO;
			goto err_regmap_write;
		}
		break;
	case 3:
		if (rd->props.rt_format == RT_LITTLE_ENDIAN) {
			tmp_data = be32_to_cpu(rrd->rt_data.data_u32);
			tmp_data >>= 8;
		}
		ret = rt_asyn_cache_block_write(rd,
				rrd->reg, rm[rio.index]->size, &tmp_data);
		if (ret < 0) {
			dev_err(&rd->dev, "rt regmap block write fail\n");
			ret = -EIO;
			goto err_regmap_write;
		}
		break;
	case 4:
		if (rd->props.rt_format == RT_LITTLE_ENDIAN)
			tmp_data = be32_to_cpu(rrd->rt_data.data_u32);
		ret = rt_asyn_cache_block_write(rd,
				rrd->reg, rm[rio.index]->size, &tmp_data);
		if (ret < 0) {
			dev_err(&rd->dev, "rt regmap block write fail\n");
			ret = -EIO;
			goto err_regmap_write;
		}
		break;
	default:
		dev_err(&rd->dev,
			"Failed: only support 1~4 bytes regmap write\n");
		break;
	}
	up(&rd->semaphore);
	return 0;
err_regmap_write:
	up(&rd->semaphore);
	return ret;
}

int rt_asyn_regmap_reg_write(struct rt_regmap_device *rd,
		struct rt_reg_data *rrd, u32 reg, const u32 data)
{
	rrd->reg = reg;
	rrd->rt_data.data_u32 = data;
	return _rt_asyn_regmap_reg_write(rd, rrd);
}
EXPORT_SYMBOL(rt_asyn_regmap_reg_write);

/* _rt_regmap_update_bits - assign bits specific register map */
static int _rt_regmap_update_bits(struct rt_regmap_device *rd,
				struct rt_reg_data *rrd)
{
	const rt_register_map_t *rm = rd->props.rm;
	struct reg_index_offset rio;
	int ret = 0, new, old;
	bool change = false;

	rio = find_register_index(rd, rrd->reg);
	if (rio.index < 0 || rio.offset != 0) {
		dev_err(&rd->dev, "reg 0x%02x is out of regmap\n", rrd->reg);
		return -EINVAL;
	}

	down(&rd->semaphore);
	switch (rm[rio.index]->size) {
	case 1:
		ret = rd->regmap_ops.regmap_block_read(rd,
					rrd->reg, 1, &old);
		if (ret < 0) {
			dev_err(&rd->dev, "rt regmap block read fail\n");
			goto err_update_bits;
		}
		new = (old & ~(rrd->mask)) | (rrd->rt_data.data_u8 & rrd->mask);
		change = old != new;

		if (((rm[rio.index]->reg_type & RT_WR_ONCE) && change) ||
			!(rm[rio.index]->reg_type & RT_WR_ONCE)) {
			ret = rd->regmap_ops.regmap_block_write(rd,
							rrd->reg, 1, &new);
			if (ret < 0) {
				dev_err(&rd->dev,
					"rt regmap block write fail\n");
				goto err_update_bits;
			}
		}
		break;
	case 2:
		ret = rd->regmap_ops.regmap_block_read(rd,
				rrd->reg, rm[rio.index]->size, &old);
		if (ret < 0) {
			dev_err(&rd->dev, "rt regmap block read fail\n");
			goto err_update_bits;
		}
		if (rd->props.rt_format == RT_LITTLE_ENDIAN)
			old = be16_to_cpu(old);

		new = (old & ~(rrd->mask)) |
				(rrd->rt_data.data_u16 & rrd->mask);

		change = old != new;
		if (((rm[rio.index]->reg_type & RT_WR_ONCE) && change) ||
			!(rm[rio.index]->reg_type & RT_WR_ONCE)) {
			if (rd->props.rt_format == RT_LITTLE_ENDIAN)
				new = be16_to_cpu(new);
			ret = rd->regmap_ops.regmap_block_write(rd,
				rrd->reg, rm[rio.index]->size, &new);
			if (ret < 0) {
				dev_err(&rd->dev,
					"rt regmap block write fail\n");
				goto err_update_bits;
			}
		}
		break;
	case 3:
		ret = rd->regmap_ops.regmap_block_read(rd,
				rrd->reg, rm[rio.index]->size, &old);
		if (ret < 0) {
			dev_err(&rd->dev, "rt regmap block read fail\n");
			goto err_update_bits;
		}
		if (rd->props.rt_format == RT_LITTLE_ENDIAN) {
			old = be32_to_cpu(old);
			old >>= 8;
		}

		new = (old & ~(rrd->mask)) |
				(rrd->rt_data.data_u32 & rrd->mask);
		change = old != new;
		if (((rm[rio.index]->reg_type & RT_WR_ONCE) && change) ||
			!(rm[rio.index]->reg_type & RT_WR_ONCE)) {
			if (rd->props.rt_format == RT_LITTLE_ENDIAN) {
				new <<= 8;
				new = be32_to_cpu(new);
			}
			ret = rd->regmap_ops.regmap_block_write(rd,
				rrd->reg, rm[rio.index]->size, &new);
			if (ret < 0) {
				dev_err(&rd->dev,
					"rt regmap block write fail\n");
				goto err_update_bits;
			}
		}
		break;
	case 4:
		ret = rd->regmap_ops.regmap_block_read(rd,
				rrd->reg, rm[rio.index]->size, &old);
		if (ret < 0) {
			dev_err(&rd->dev, "rt regmap block read fail\n");
			goto err_update_bits;
		}
		if (rd->props.rt_format == RT_LITTLE_ENDIAN)
			old = be32_to_cpu(old);

		new = (old & ~(rrd->mask)) |
				(rrd->rt_data.data_u32 & rrd->mask);
		change = old != new;
		if (((rm[rio.index]->reg_type & RT_WR_ONCE) && change) ||
			!(rm[rio.index]->reg_type & RT_WR_ONCE)) {
			if (rd->props.rt_format == RT_LITTLE_ENDIAN)
				new = be32_to_cpu(new);
			ret = rd->regmap_ops.regmap_block_write(rd,
				rrd->reg, rm[rio.index]->size, &new);
			if (ret < 0) {
				dev_err(&rd->dev,
					"rt regmap block write fail\n");
				goto err_update_bits;
			}
		}
		break;
	default:
		dev_err(&rd->dev,
			"Failed: only support 1~4 bytes regmap write\n");
		break;
	}
	up(&rd->semaphore);
	return ret;
err_update_bits:
	up(&rd->semaphore);
	return ret;
}

int rt_regmap_update_bits(struct rt_regmap_device *rd,
		struct rt_reg_data *rrd, u32 reg, u32 mask, u32 data)
{
	rrd->reg = reg;
	rrd->mask = mask;
	rrd->rt_data.data_u32 = data;
	return _rt_regmap_update_bits(rd, rrd);
}
EXPORT_SYMBOL(rt_regmap_update_bits);

/* rt_regmap_block_write - block write data to register
 * @rd: rt_regmap_device pointer
 * @reg: register address
 * bytes: leng for write
 * src: source data
 */
int rt_regmap_block_write(struct rt_regmap_device *rd, u32 reg,
				int bytes, const void *src)
{
	int ret;

	down(&rd->semaphore);
	ret = rd->regmap_ops.regmap_block_write(rd, reg, bytes, src);
	up(&rd->semaphore);
	return ret;
};
EXPORT_SYMBOL(rt_regmap_block_write);

/* rt_asyn_regmap_block_write - asyn block write*/
int rt_asyn_regmap_block_write(struct rt_regmap_device *rd, u32 reg,
					int bytes, const void *src)
{
	int ret;

	down(&rd->semaphore);
	ret = rt_asyn_cache_block_write(rd, reg, bytes, src);
	up(&rd->semaphore);
	return ret;
};
EXPORT_SYMBOL(rt_asyn_regmap_block_write);

/* rt_regmap_block_read - block read data form register
 * @rd: rt_regmap_device pointer
 * @reg: register address
 * @bytes: read length
 * @dst: destination for read data
 */
int rt_regmap_block_read(struct rt_regmap_device *rd, u32 reg,
				int bytes, void *dst)
{
	int ret;

	down(&rd->semaphore);
	ret = rd->regmap_ops.regmap_block_read(rd, reg, bytes, dst);
	up(&rd->semaphore);
	return ret;
};
EXPORT_SYMBOL(rt_regmap_block_read);

/* _rt_regmap_reg_read - register read for specific register map
 * only support 1, 2, 4 bytes register map.
 * @rd: rt_regmap_device pointer.
 * @rrd: rt_reg_data pointer.
 */
static int _rt_regmap_reg_read(
		struct rt_regmap_device *rd, struct rt_reg_data *rrd)
{
	const rt_register_map_t *rm = rd->props.rm;
	struct reg_index_offset rio;
	int ret, tmp_data = 0;

	rio = find_register_index(rd, rrd->reg);
	if (rio.index < 0 || rio.offset != 0) {
		dev_err(&rd->dev, "reg 0x%02x is out of regmap\n", rrd->reg);
		return -EINVAL;
	}

	down(&rd->semaphore);
	switch (rm[rio.index]->size) {
	case 1:
		ret = rd->regmap_ops.regmap_block_read(rd,
			rrd->reg, 1, &rrd->rt_data.data_u8);
		if (ret < 0) {
			dev_err(&rd->dev, "rt regmap block read fail\n");
			goto err_regmap_reg_read;
		}
		break;
	case 2:
		ret = rd->regmap_ops.regmap_block_read(rd,
			rrd->reg, rm[rio.index]->size, &tmp_data);
		if (ret < 0) {
			dev_err(&rd->dev, "rt regmap block read fail\n");
			goto err_regmap_reg_read;
		}
		if (rd->props.rt_format == RT_LITTLE_ENDIAN)
			tmp_data = be16_to_cpu(tmp_data);
		rrd->rt_data.data_u16 = tmp_data;
		break;
	case 3:
		ret = rd->regmap_ops.regmap_block_read(rd,
			rrd->reg, rm[rio.index]->size, &tmp_data);
		if (ret < 0) {
			dev_err(&rd->dev, "rt regmap block read fail\n");
			goto err_regmap_reg_read;
		}
		if (rd->props.rt_format == RT_LITTLE_ENDIAN)
			tmp_data = be32_to_cpu(tmp_data);
		rrd->rt_data.data_u32 = (tmp_data >> 8);
		break;
	case 4:
		ret = rd->regmap_ops.regmap_block_read(rd,
			rrd->reg, rm[rio.index]->size, &tmp_data);
		if (ret < 0) {
			dev_err(&rd->dev, "rt regmap block read fail\n");
			goto err_regmap_reg_read;
		}
		if (rd->props.rt_format == RT_LITTLE_ENDIAN)
			tmp_data = be32_to_cpu(tmp_data);
		rrd->rt_data.data_u32 = tmp_data;
		break;
	default:
		dev_err(&rd->dev,
			"Failed: only support 1~4 bytes regmap read\n");
		break;
	}
	up(&rd->semaphore);
	return 0;
err_regmap_reg_read:
	up(&rd->semaphore);
	return ret;
}

int rt_regmap_reg_read(struct rt_regmap_device *rd,
			struct rt_reg_data *rrd, u32 reg)
{
	rrd->reg = reg;
	return _rt_regmap_reg_read(rd, rrd);
}
EXPORT_SYMBOL(rt_regmap_reg_read);

void rt_cache_getlasterror(struct rt_regmap_device *rd, char *buf)
{
	int ret = 0;

	down(&rd->semaphore);
	ret = snprintf(buf, PAGE_SIZE, "%s\n", rd->err_msg);
	if (!ret)
		dev_info(&rd->dev, "%s: format error string fail\n", __func__);
	up(&rd->semaphore);
}
EXPORT_SYMBOL(rt_cache_getlasterror);

void rt_cache_clrlasterror(struct rt_regmap_device *rd)
{
	int ret = 0;

	down(&rd->semaphore);
	rd->error_occurred = 0;
	ret = snprintf(rd->err_msg, PAGE_SIZE, "%s", "No Error");
	if (!ret)
		dev_info(&rd->dev, "%s: format error string fail\n", __func__);
	up(&rd->semaphore);
}
EXPORT_SYMBOL(rt_cache_clrlasterror);

/* initialize cache data from rt_register */
int rt_regmap_cache_init(struct rt_regmap_device *rd)
{
	int i, j, bytes_num = 0, count = 0;
	const rt_register_map_t *rm = rd->props.rm;

	dev_info(&rd->dev, "rt register cache data init\n");

	down(&rd->semaphore);
	rd->cache_flag = devm_kzalloc(&rd->dev,
		rd->props.register_num * sizeof(unsigned char), GFP_KERNEL);
	rd->cached = devm_kzalloc(&rd->dev,
		rd->props.register_num * sizeof(unsigned char), GFP_KERNEL);
	rd->cache_data = devm_kzalloc(&rd->dev,
		rd->props.register_num * sizeof(unsigned char *), GFP_KERNEL);

	if (rd->props.group == NULL) {
		rd->props.group = devm_kzalloc(&rd->dev,
				sizeof(*rd->props.group), GFP_KERNEL);
		rd->props.group[0].start = 0x00;
		rd->props.group[0].end = 0xffff;
		rd->props.group[0].mode = RT_1BYTE_MODE;
	}

	for (i = 0; i < rd->props.register_num; i++)
		bytes_num += rm[i]->size;

	rd->alloc_data = devm_kzalloc(&rd->dev,
		bytes_num * sizeof(unsigned char), GFP_KERNEL);

	/* reload cache data from real chip */
	for (i = 0; i < rd->props.register_num; i++) {
		rd->cache_data[i] = rd->alloc_data + count;
		count += rm[i]->size;
		memset(rd->cache_data[i], 0x00, rm[i]->size);
		rd->cache_flag[i] = rd->cached[i] = 0;
	}

	/* set 0xff writeable mask for NORMAL and RESERVE type */
	for (i = 0; i < rd->props.register_num; i++) {
		if ((rm[i]->reg_type & RT_REG_TYPE_MASK) == RT_NORMAL ||
		    (rm[i]->reg_type & RT_REG_TYPE_MASK) == RT_RESERVE) {
			for (j = 0; j < rm[i]->size; j++)
				rm[i]->wbit_mask[j] = 0xff;
		}
	}

	rd->cache_inited = 1;
	dev_info(&rd->dev, "cache cata init successfully\n");
	up(&rd->semaphore);
	return 0;
}
EXPORT_SYMBOL(rt_regmap_cache_init);

/* rt_regmap_cache_reload - reload cache valuew from real chip*/
int rt_regmap_cache_reload(struct rt_regmap_device *rd)
{
	int i;

	down(&rd->semaphore);
	for (i = 0; i < rd->props.register_num; i++)
		rd->cached[i] = rd->cache_flag[i] = 0;
	rd->pending_event = 0;
	up(&rd->semaphore);
	dev_info(&rd->dev, "cache data reload\n");
	return 0;
}
EXPORT_SYMBOL(rt_regmap_cache_reload);

/* rt_regmap_add_debubfs - add user own debugfs node
 * @rd: rt_regmap_devcie pointer.
 * @name: a pointer to a string containing the name of the file to create.
 * @mode: the permission that the file should have.
 * @data: a pointer to something that the caller will want to get to later on.
 *	The inode.i_private pointer will point this value on the open() call.
 * @fops: a pointer to a struct file_operations that should be used for
 *	this file.
 */
int rt_regmap_add_debugfs(struct rt_regmap_device *rd, const char *name,
			  umode_t mode, void *data,
			  const struct file_operations *fops)
{
#ifdef CONFIG_DEBUG_FS
	struct dentry *den;

	den = debugfs_create_file(name, mode, rd->rt_den, data, fops);
	if (!den)
		return -EINVAL;
#endif /*CONFIG_DEBUG_FS*/
	return 0;
}
EXPORT_SYMBOL(rt_regmap_add_debugfs);

/* release cache data*/
static void rt_regmap_cache_release(struct rt_regmap_device *rd)
{
	int i;

	dev_info(&rd->dev, "cache data release\n");
	for (i = 0; i < rd->props.register_num; i++)
		rd->cache_data[i] = NULL;
	devm_kfree(&rd->dev, rd->alloc_data);
	if (rd->cache_flag)
		devm_kfree(&rd->dev, rd->cache_flag);
	if (rd->cached)
		devm_kfree(&rd->dev, rd->cached);
	rd->cache_inited = 0;
}

static void rt_regmap_set_cache_mode(
			struct rt_regmap_device *rd, unsigned char mode)
{
	unsigned char mode_mask;

	mode_mask = mode & RT_CACHE_MODE_MASK;
	dev_info(&rd->dev, "%s mode = %d\n", __func__, mode_mask>>1);

	down(&rd->write_mode_lock);
	if (mode_mask == RT_CACHE_WR_THROUGH) {
		rt_regmap_cache_reload(rd);
		rd->regmap_ops.regmap_block_write =
			rt_cache_block_write;
		rd->regmap_ops.regmap_block_read = &rt_cache_block_read;
	} else if (mode_mask == RT_CACHE_WR_BACK) {
		rt_regmap_cache_reload(rd);
		rd->regmap_ops.regmap_block_write =
			rt_asyn_cache_block_write;
		rd->regmap_ops.regmap_block_read = &rt_cache_block_read;
	} else if (mode_mask == RT_CACHE_DISABLE) {
		rd->regmap_ops.regmap_block_write =
			rt_chip_block_write;
		rd->regmap_ops.regmap_block_read = rt_chip_block_read;
	} else {
		dev_err(&rd->dev, "%s out of cache mode index\n", __func__);
		goto mode_err;
	}

	rd->props.rt_regmap_mode &= ~RT_CACHE_MODE_MASK;
	rd->props.rt_regmap_mode |= mode_mask;
mode_err:
	up(&rd->write_mode_lock);
}

#ifdef CONFIG_DEBUG_FS
static void rt_show_regs(struct rt_regmap_device *rd, struct seq_file *seq_file)
{
	int i = 0, k = 0, ret, count = 0;
	unsigned char *regval;
	const rt_register_map_t *rm = rd->props.rm;

	if (rd->props.map_byte_num == 0)
		regval = devm_kzalloc(&rd->dev, sizeof(char)*512, GFP_KERNEL);
	else
		regval = devm_kzalloc(&rd->dev,
			rd->props.map_byte_num*sizeof(char), GFP_KERNEL);

	if (!regval) {
		dev_err(&rd->dev, "regval is NULL\n");
		return;
	}

	down(&rd->semaphore);
	for (i = 0; i < rd->props.register_num; i++) {
		ret = rd->regmap_ops.regmap_block_read(rd, rm[i]->addr,
						rm[i]->size, &regval[count]);
		count += rm[i]->size;
		if (ret < 0) {
			dev_err(&rd->dev, "regmap block read fail\n");
			if (rd->error_occurred) {
				ret = snprintf(
					rd->err_msg + strlen(rd->err_msg),
					PAGE_SIZE,
					"Error block read fail at 0x%02x\n",
					rm[i]->addr);
			} else {
				ret = snprintf(rd->err_msg, PAGE_SIZE,
					"Error block read fail at 0x%02x\n",
					rm[i]->addr);
					rd->error_occurred = 1;
			}
			goto err_show_regs;
		}

		if ((rm[i]->reg_type & RT_REG_TYPE_MASK) != RT_RESERVE) {
			seq_printf(seq_file, "reg0x%02x:0x", rm[i]->addr);
			for (k = 0; k < rm[i]->size; k++)
				seq_printf(seq_file, "%02x,",
					regval[count - rm[i]->size + k]);
			seq_puts(seq_file, "\n");
		} else
			seq_printf(seq_file,
				"reg0x%02x:reserve\n", rm[i]->addr);
	}
err_show_regs:
	devm_kfree(&rd->dev, regval);
	up(&rd->semaphore);
}

static int general_read(struct seq_file *seq_file, void *_data)
{
	struct rt_debug_st *st = (struct rt_debug_st *)seq_file->private;
	struct rt_regmap_device *rd = st->info;
	const rt_register_map_t rm;
	unsigned char *reg_data;
	unsigned char data;
	int i = 0, rc = 0, size = 0;

	switch (st->id) {
	case RT_DBG_REG:
		seq_printf(seq_file, "0x%04x\n", rd->dbg_data.reg_addr);
		break;
	case RT_DBG_DATA:
		if (rd->dbg_data.reg_size == 0)
			rd->dbg_data.reg_size = 1;

		reg_data = kcalloc(rd->dbg_data.reg_size, sizeof(unsigned char),
			GFP_KERNEL);
		memset(reg_data, 0,
			sizeof(unsigned char)*rd->dbg_data.reg_size);

		size = rd->dbg_data.reg_size;

		if (rd->dbg_data.rio.index == -1) {
			down(&rd->semaphore);
			rc = rt_chip_block_read(rd, rd->dbg_data.reg_addr,
							size, reg_data);
			up(&rd->semaphore);
			if (rc < 0) {
				seq_puts(seq_file, "invalid read\n");
				kfree(reg_data);
				break;
			}
			goto hiden_read;
		}

		rm = rd->props.rm[rd->dbg_data.rio.index];
		down(&rd->semaphore);
		rc = rd->regmap_ops.regmap_block_read(rd,
			rd->dbg_data.reg_addr, size, reg_data);
		up(&rd->semaphore);
		if (rc < 0) {
			seq_puts(seq_file, "invalid read\n");
			kfree(reg_data);
			break;
		}
hiden_read:
		if (&reg_data[i] != NULL) {
			seq_puts(seq_file, "0x");
			for (i = 0; i < size; i++)
				seq_printf(seq_file, "%02x,", reg_data[i]);
			seq_puts(seq_file, "\n");
		}
		kfree(reg_data);
		break;
	case RT_DBG_ERROR:
		seq_puts(seq_file, "======== Error Message ========\n");
		if (!rd->error_occurred)
			seq_puts(seq_file, "No Error\n");
		else
			seq_printf(seq_file, rd->err_msg);
		break;
	case RT_DBG_REGS:
		rt_show_regs(rd, seq_file);
		break;
	case RT_DBG_NAME:
		seq_printf(seq_file, "%s\n", rd->props.aliases);
		break;
	case RT_DBG_SIZE:
		seq_printf(seq_file, "%d\n", rd->dbg_data.reg_size);
		break;
	case RT_DBG_BLOCK:
		data = rd->props.rt_regmap_mode & RT_IO_BLK_MODE_MASK;
		if (data == RT_IO_PASS_THROUGH)
			seq_puts(seq_file, "0 => IO_PASS_THROUGH\n");
		else if (data == RT_IO_BLK_ALL)
			seq_puts(seq_file, "1 => IO_BLK_ALL\n");
		else if (data == RT_IO_BLK_CACHE)
			seq_puts(seq_file, "2 => IO_BLK_CACHE\n");
		else if (data == RT_IO_BLK_CHIP)
			seq_puts(seq_file, "3 => IO_BLK_CHIP\n");
		break;
	case RT_DBG_SLAVE_ADDR:
		seq_printf(seq_file, "0x%02x\n", rd->slv_addr);
		break;
	case RT_DBG_SUPPORT_MODE:
		seq_puts(seq_file, " == BLOCK MODE ==\n");
		seq_puts(seq_file, "0 => IO_PASS_THROUGH\n");
		seq_puts(seq_file, "1 => IO_BLK_ALL\n");
		seq_puts(seq_file, "2 => IO_BLK_CHIP\n");
		seq_puts(seq_file, "3 => IO_BLK_CACHE\n");
		seq_puts(seq_file, " == CACHE MODE ==\n");
		seq_puts(seq_file, "0 => CACHE_WR_THROUGH\n");
		seq_puts(seq_file, "1 => CACHE_WR_BACK\n");
		seq_puts(seq_file, "2 => CACHE_DISABLE\n");

		break;
	case RT_DBG_IO_LOG:
		seq_printf(seq_file, "%d\n", rd->props.io_log_en);
		break;
	case RT_DBG_CACHE_MODE:
		data = rd->props.rt_regmap_mode & RT_CACHE_MODE_MASK;
		if (data == RT_CACHE_WR_THROUGH)
			seq_printf(seq_file, "%s",
					"0 => Cache Write Through\n");
		else if (data == RT_CACHE_WR_BACK)
			seq_printf(seq_file, "%s", "1 => Cache Write Back\n");
		else if (data == RT_CACHE_DISABLE)
			seq_printf(seq_file, "%s", "2 => Cache Disable\n");
		break;
	case RT_DBG_REG_SIZE:
		size = rt_get_regsize(rd, rd->dbg_data.reg_addr);
		if (size < 0)
			seq_printf(seq_file, "%d\n", 0);
		else
			seq_printf(seq_file, "%d\n", size);
		break;
	case RT_DBG_WATCHDOG:
		seq_printf(seq_file, "watchdog = %d\n", rd->props.watchdog);
		break;
	}
	return 0;
}

static int general_open(struct inode *inode, struct file *file)
{
	if (file->f_mode & FMODE_READ)
		return single_open(file, general_read, inode->i_private);
	file->private_data = inode->i_private;
	return 0;
}


#define RT_WATCHDOG_TIMEOUT	(40000000000)

static ssize_t general_write(struct file *file, const char __user *ubuf,
			     size_t count, loff_t *ppos)
{
	struct rt_debug_st *st = file->private_data;
	struct rt_regmap_device *rd = st->info;
	struct reg_index_offset rio;
	long param[5] = {0};
	unsigned char *reg_data;
	int ret, rc, size = 0;
	char lbuf[128];
	ssize_t res;

	pr_info("%s @ %p\n", __func__, ubuf);

	res = simple_write_to_buffer(lbuf, sizeof(lbuf) - 1, ppos, ubuf, count);
	if (res <= 0)
		return -EFAULT;

	lbuf[count] = '\0';

	switch (st->id) {
	case RT_DBG_REG:
		rc = get_parameters(lbuf, param, 1);
		rio = find_register_index(rd, param[0]);
		down(&rd->semaphore);
		if (rio.index < 0) {
			pr_info("this is an invalid or hiden register\n");
			rd->dbg_data.reg_addr = param[0];
			rd->dbg_data.rio.index = -1;
		} else {
			rd->dbg_data.rio = rio;
			rd->dbg_data.reg_addr = param[0];
		}
		up(&rd->semaphore);
		break;
	case RT_DBG_DATA:
		if (rd->dbg_data.reg_size == 0)
			rd->dbg_data.reg_size = 1;
		reg_data = kcalloc(rd->dbg_data.reg_size, sizeof(unsigned char),
			GFP_KERNEL);
		memset(reg_data, 0,
			sizeof(unsigned char)*rd->dbg_data.reg_size);
		if (rd->dbg_data.rio.index == -1) {
			size = rd->dbg_data.reg_size;
			if ((size - 1) * 3 + 5 != count) {
				dev_err(&rd->dev, "wrong input length\n");
				if (rd->error_occurred) {
					ret = snprintf(rd->err_msg +
						strlen(rd->err_msg), PAGE_SIZE,
						"Error, wrong input length\n");
				} else {
					ret = snprintf(rd->err_msg, PAGE_SIZE,
						"Error, wrong input length\n");
						rd->error_occurred = 1;
				}
				kfree(reg_data);
				return -EINVAL;
			}

			rc = get_datas(lbuf, count, reg_data, size);
			if (rc < 0) {
				dev_err(&rd->dev, "get datas fail\n");
				if (rd->error_occurred) {
					ret = snprintf(rd->err_msg +
					strlen(rd->err_msg), PAGE_SIZE,
					"Error, get datas fail\n");
				} else {
					ret = snprintf(rd->err_msg, PAGE_SIZE,
						"Error, get datas fail\n");
					rd->error_occurred = 1;
				}
				kfree(reg_data);
				return -EINVAL;
			}
			down(&rd->semaphore);
			rc = rt_chip_block_write(rd, rd->dbg_data.reg_addr,
							size, reg_data);
			up(&rd->semaphore);
			if (rc < 0) {
				dev_err(&rd->dev, "chip block write fail\n");
				if (rd->error_occurred) {
					ret = snprintf(rd->err_msg +
					strlen(rd->err_msg), PAGE_SIZE,
					"Error chip block write fail at 0x%02x\n",
					rd->dbg_data.reg_addr);
				} else {
					snprintf(rd->err_msg, PAGE_SIZE,
					"Error chip block write fail at 0x%02x\n",
					rd->dbg_data.reg_addr);
					rd->error_occurred = 1;
				}
				kfree(reg_data);
				return -EIO;
			}
			break;
		}

		size = rd->dbg_data.reg_size;

		if ((size - 1)*3 + 5 != count) {
			dev_err(&rd->dev, "wrong input length\n");
			if (rd->error_occurred) {
				ret = snprintf(
					rd->err_msg + strlen(rd->err_msg),
					PAGE_SIZE,
					"Error, wrong input length\n");
			} else {
				ret = snprintf(rd->err_msg, PAGE_SIZE,
					"Error, wrong input length\n");
					rd->error_occurred = 1;
			}
			kfree(reg_data);
			return -EINVAL;
		}

		rc = get_datas(lbuf, count, reg_data, size);
		if (rc < 0) {
			dev_err(&rd->dev, "get datas fail\n");
			if (rd->error_occurred) {
				ret = snprintf(
					rd->err_msg + strlen(rd->err_msg),
					PAGE_SIZE, "Error, get datas fail\n");
			} else {
				ret = snprintf(rd->err_msg, PAGE_SIZE,
					"Error, get datas fail\n");
					rd->error_occurred = 1;
			}
			kfree(reg_data);
			return -EINVAL;
		}

		down(&rd->semaphore);
		rc = rd->regmap_ops.regmap_block_write(rd,
				rd->dbg_data.reg_addr, size, reg_data);
		up(&rd->semaphore);
		if (rc < 0) {
			dev_err(&rd->dev, "regmap block write fail\n");
			if (rd->error_occurred) {
				ret = snprintf(
					rd->err_msg + strlen(rd->err_msg),
					PAGE_SIZE,
					"Error regmap block write fail at 0x%02x\n",
					rd->dbg_data.reg_addr);
			} else {
				ret = snprintf(rd->err_msg, PAGE_SIZE,
					"Error regmap block write fail at 0x%02x\n",
					rd->dbg_data.reg_addr);
					rd->error_occurred = 1;
			}
			kfree(reg_data);
			return -EIO;
		}
		kfree(reg_data);
		break;
	case RT_DBG_SYNC:
		rc = get_parameters(lbuf, param, 1);
		if (param[0])
			rt_regmap_cache_sync(rd);
		break;
	case RT_DBG_ERROR:
		rc = get_parameters(lbuf, param, 1);
		if (param[0])
			rt_cache_clrlasterror(rd);
		break;
	case RT_DBG_SIZE:
		rc = get_parameters(lbuf, param, 1);
		if (param[0] >= 0) {
			down(&rd->semaphore);
			rd->dbg_data.reg_size = param[0];
			up(&rd->semaphore);
		} else {
			if (rd->error_occurred) {
				ret = snprintf(
					rd->err_msg + strlen(rd->err_msg),
					PAGE_SIZE, "Error, size must > 0\n");
			} else {
				ret = snprintf(rd->err_msg, PAGE_SIZE,
					"Error, size must > 0\n");
					rd->error_occurred = 1;
			}
			return -EINVAL;
		}
		break;
	case RT_DBG_BLOCK:
		rc = get_parameters(lbuf, param, 1);
		if (param[0] < 0)
			param[0] = 0;
		else if (param[0] > 3)
			param[0] = 3;

		param[0] <<= 3;

		down(&rd->semaphore);
		rd->props.rt_regmap_mode &= ~RT_IO_BLK_MODE_MASK;
		rd->props.rt_regmap_mode |= param[0];
		up(&rd->semaphore);
		if (param[0] == RT_IO_PASS_THROUGH)
			rt_regmap_cache_sync(rd);
		break;
	case RT_DBG_IO_LOG:
		rc = get_parameters(lbuf, param, 1);
		down(&rd->semaphore);
		if (!param[0])
			rd->props.io_log_en = 0;
		else
			rd->props.io_log_en = 1;
		up(&rd->semaphore);
		break;
	case RT_DBG_CACHE_MODE:
		rc = get_parameters(lbuf, param, 1);
		if (param[0] < 0)
			param[0] = 0;
		else if (param[0] > 2)
			param[0] = 2;
		param[0] <<= 1;
		rt_regmap_set_cache_mode(rd, param[0]);
		break;
	case RT_DBG_WATCHDOG:
		rc = get_parameters(lbuf, param, 1);
		if (param[0]) {
			dev_info(&rd->dev, "enable watchdog\n");
			if (rd->props.watchdog)
				alarm_cancel(&rd->watchdog_alarm);
			else
				rd->props.watchdog = 1;
			alarm_start_relative(&rd->watchdog_alarm,
					ns_to_ktime(RT_WATCHDOG_TIMEOUT));
		} else {
			dev_info(&rd->dev, "disable watchdog\n");
			if (rd->props.watchdog) {
				rd->props.watchdog = 0;
				alarm_cancel(&rd->watchdog_alarm);
				rt_regmap_set_cache_mode(rd,
						rd->props.cache_mode_ori);
			}
		}
		break;
	default:
		return -EINVAL;
	}

	return count;
}

static int general_release(struct inode *inode, struct file *file)
{
	if (file->f_mode & FMODE_READ)
		return single_release(inode, file);
	return 0;
}

static const struct file_operations general_ops = {
	.owner = THIS_MODULE,
	.open = general_open,
	.write = general_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = general_release,
};

#define RT_CREATE_GENERAL_FILE(_id, _name, _mode)			\
{									\
	rd->rtdbg_st[_id].info = rd;					\
	rd->rtdbg_st[_id].id = _id;					\
	rd->rt_debug_file[_id] = debugfs_create_file(_name, _mode, dir,	\
	(void *)&rd->rtdbg_st[_id], &general_ops);			\
}

/* create general debugfs node */
static void rt_create_general_debug(struct rt_regmap_device *rd,
				    struct dentry *dir)
{
	RT_CREATE_GENERAL_FILE(RT_DBG_REG, "reg_addr", 0444);
	RT_CREATE_GENERAL_FILE(RT_DBG_DATA, "data", 0444);
	RT_CREATE_GENERAL_FILE(RT_DBG_REGS, "regs", 0444);
	RT_CREATE_GENERAL_FILE(RT_DBG_SYNC, "sync", 0444);
	RT_CREATE_GENERAL_FILE(RT_DBG_ERROR, "Error", 0444);
	RT_CREATE_GENERAL_FILE(RT_DBG_NAME, "name", 0444);
	RT_CREATE_GENERAL_FILE(RT_DBG_BLOCK, "block", 0444);
	RT_CREATE_GENERAL_FILE(RT_DBG_SIZE, "size", 0444);
	RT_CREATE_GENERAL_FILE(RT_DBG_SLAVE_ADDR,
					"slave_addr", 0444);
	RT_CREATE_GENERAL_FILE(RT_DBG_SUPPORT_MODE,
					"support_mode", 0444);
	RT_CREATE_GENERAL_FILE(RT_DBG_IO_LOG, "io_log", 0444);
	RT_CREATE_GENERAL_FILE(RT_DBG_CACHE_MODE,
					"cache_mode", 0444);
	RT_CREATE_GENERAL_FILE(RT_DBG_REG_SIZE, "reg_size", 0444);
	RT_CREATE_GENERAL_FILE(RT_DBG_WATCHDOG, "watchdog", 0444);
}

static int eachreg_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t eachreg_write(struct file *file, const char __user *ubuf,
			     size_t count, loff_t *ppos)
{
	struct rt_debug_st *st = file->private_data;
	struct rt_regmap_device *rd = st->info;
	const rt_register_map_t rm = rd->props.rm[st->id];
	int rc;
	unsigned char *pars;
	char lbuf[128];
	ssize_t res;

	if ((rm->size - 1) * 3 + 5 != count &&
		(rm->size - 1) * 3 + 4 != count) {
		dev_err(&rd->dev, "wrong input length\n");
		return -EINVAL;
	}

	pr_info("%s @ %p\n", __func__, ubuf);

	res = simple_write_to_buffer(lbuf, sizeof(lbuf) - 1, ppos, ubuf, count);
	if (res <= 0)
		return -EFAULT;

	lbuf[count] = '\0';

	pars = kcalloc(rm->size, sizeof(unsigned char), GFP_KERNEL);
	if (!pars)
		return -ENOMEM;

	rc = get_datas(lbuf, count, pars, rm->size);

	if (rc < 0) {
		kfree(pars);
		dev_err(&rd->dev, "get datas fail\n");
		return -EINVAL;
	}

	down(&rd->semaphore);
	rc = rd->regmap_ops.regmap_block_write(rd, rm->addr,
					rm->size, &pars[0]);
	up(&rd->semaphore);
	if (rc < 0) {
		kfree(pars);
		dev_err(&rd->dev, "regmap block read fail\n");
		return -EIO;
	}

	kfree(pars);
	return count;
}

static ssize_t eachreg_read(struct file *file, char __user *ubuf,
			    size_t count, loff_t *ppos)
{
	struct rt_debug_st *st = file->private_data;
	struct rt_regmap_device *rd = st->info;
	ssize_t retval = 0;
	char *lbuf;
	unsigned char *regval;
	const rt_register_map_t rm = rd->props.rm[st->id];
	int i, j = 0, rc;

	if (rd->props.max_byte_size == 0) {
		regval = devm_kzalloc(&rd->dev,
			sizeof(unsigned char)*32, GFP_KERNEL);
		lbuf = devm_kzalloc(&rd->dev, sizeof(char)*200, GFP_KERNEL);
	} else {
		regval = devm_kzalloc(&rd->dev, rd->props.max_byte_size *
				sizeof(unsigned char), GFP_KERNEL);
		lbuf = devm_kzalloc(&rd->dev,
			rd->props.max_byte_size*3+2, GFP_KERNEL);
	}

	lbuf[0] = '\0';

	down(&rd->semaphore);
	rc = rd->regmap_ops.regmap_block_read(rd, rm->addr, rm->size, regval);
	up(&rd->semaphore);
	if (rc < 0) {
		dev_err(&rd->dev, "regmap block read fail\n");
		devm_kfree(&rd->dev, regval);
		devm_kfree(&rd->dev, lbuf);
		return -EIO;
	}

	j += snprintf(lbuf + j, PAGE_SIZE, "reg0x%02x:0x", rm->addr);
	for (i = 0; i < rm->size; i++)
		j += snprintf(lbuf + j,
			PAGE_SIZE-strlen(lbuf), "%02x,", regval[i]);
	j += snprintf(lbuf + j, PAGE_SIZE-strlen(lbuf), "\n");

	retval = simple_read_from_buffer(ubuf, count, ppos, lbuf, strlen(lbuf));
	devm_kfree(&rd->dev, regval);
	devm_kfree(&rd->dev, lbuf);
	return retval;
}

static const struct file_operations eachreg_ops = {
	.open = eachreg_open,
	.read = eachreg_read,
	.write = eachreg_write,
};

/* create every register node at debugfs */
static void rt_create_every_debug(struct rt_regmap_device *rd,
				  struct dentry *dir)
{
	int i, ret;
	char buf[10];

	rd->rt_reg_file = devm_kzalloc(&rd->dev,
		rd->props.register_num*sizeof(struct dentry *), GFP_KERNEL);
	rd->reg_st = devm_kzalloc(&rd->dev,
		rd->props.register_num*sizeof(struct rt_debug_st *),
								GFP_KERNEL);
	for (i = 0; i < rd->props.register_num; i++) {
		ret = snprintf(buf, sizeof(buf),
			"reg0x%02x", (rd->props.rm[i])->addr);
		rd->rt_reg_file[i] = devm_kzalloc(&rd->dev,
						  sizeof(*rd->rt_reg_file[i]),
						  GFP_KERNEL);
		rd->reg_st[i] =
		    devm_kzalloc(&rd->dev, sizeof(*rd->reg_st[i]), GFP_KERNEL);

		rd->reg_st[i]->info = rd;
		rd->reg_st[i]->id = i;
		rd->rt_reg_file[i] = debugfs_create_file(buf,
							 0444, dir,
							 (void *)rd->reg_st[i],
							 &eachreg_ops);
	}
}

static void rt_release_every_debug(struct rt_regmap_device *rd)
{
	int num = rd->props.register_num;
	int i;

	for (i = 0; i < num; i++) {
		devm_kfree(&rd->dev, rd->rt_reg_file[i]);
		devm_kfree(&rd->dev, rd->reg_st[i]);
	}
	devm_kfree(&rd->dev, rd->rt_reg_file);
	devm_kfree(&rd->dev, rd->reg_st);
}
#endif /* CONFIG_DEBUG_FS */

static void rt_regmap_device_release(struct device *dev)
{
	struct rt_regmap_device *rd = to_rt_regmap_device(dev);

	devm_kfree(dev, rd);
}

/* check the rt_register format is correct */
static int rt_regmap_check(struct rt_regmap_device *rd)
{
	const rt_register_map_t *rm = rd->props.rm;
	int num = rd->props.register_num;
	int i;

	/* check name property */
	if (!rd->props.name) {
		pr_info("there is no node name for rt-regmap\n");
		return -EINVAL;
	}

	if (!(rd->props.rt_regmap_mode & RT_BYTE_MODE_MASK))
		goto single_byte;

	for (i = 0; i < num; i++) {
		/* check byte size, 1 byte ~ 24 bytes is valid */
		if (rm[i]->size < 1 || rm[i]->size > 24) {
			pr_info("rt register size error at reg 0x%02x\n",
				rm[i]->addr);
			return -EINVAL;
		}
	}

	for (i = 0; i < num - 1; i++) {
		/* check register sequence */
		if (rm[i]->addr >= rm[i + 1]->addr) {
			pr_info("sequence format error at reg 0x%02x\n",
				rm[i]->addr);
			return -EINVAL;
		}
	}

single_byte:
	/* no default reg_addr and reister_map first addr is not 0x00 */
#ifdef CONFIG_DEBUG_FS
	if (!rd->dbg_data.reg_addr && rm[0]->addr) {
		rd->dbg_data.reg_addr = rm[0]->addr;
		rd->dbg_data.rio.index = 0;
		rd->dbg_data.rio.offset = 0;
	}
#endif /* CONFIG_DEBUG_FS */
	return 0;
}

static void rt_regmap_watchdog_work(struct work_struct *work)
{
	struct rt_regmap_device *rd = (struct rt_regmap_device *)
		container_of(work,
		struct rt_regmap_device, watchdog_work.work);
	unsigned char current_mode;

	dev_info(&rd->dev, "%s\n", __func__);
	current_mode = rd->props.rt_regmap_mode&RT_CACHE_MODE_MASK;
	if (current_mode != rd->props.cache_mode_ori)
		rt_regmap_set_cache_mode(rd, rd->props.cache_mode_ori);
	else
		dev_info(&rd->dev, "%s same mode, no need change\n", __func__);
	rd->props.watchdog = 0;
}

static enum alarmtimer_restart rt_regmap_watchdog_alarm(
				struct alarm *alarm, ktime_t now)
{
	struct rt_regmap_device *rd = (struct rt_regmap_device *)
		container_of(alarm, struct rt_regmap_device, watchdog_alarm);

	dev_info(&rd->dev, "%s\n", __func__);
	schedule_delayed_work(&rd->watchdog_work, 0);

	return ALARMTIMER_NORESTART;
}

struct rt_regmap_device *rt_regmap_device_register_ex
			(struct rt_regmap_properties *props,
			struct rt_regmap_fops *rops,
			struct device *parent,
			void *client, int slv_addr, void *drvdata)
{
	struct rt_regmap_device *rd;
	int ret = 0, i;
	char device_name[32];
	unsigned char data;

	if (!props) {
		pr_err("%s rt_regmap_properties is NULL\n", __func__);
		return NULL;
	}
	if (!rops) {
		pr_err("%s rt_regmap_fops is NULL\n", __func__);
		return NULL;
	}

	pr_info("regmap_device_register: name = %s\n", props->name);
	rd = devm_kzalloc(parent, sizeof(struct rt_regmap_device), GFP_KERNEL);
	if (!rd) {
		pr_info("rt_regmap_device memory allocate fail\n");
		return NULL;
	}

	/* create a binary semaphore */
	sema_init(&rd->semaphore, 1);
	sema_init(&rd->write_mode_lock, 1);
	rd->dev.parent = parent;
	rd->client = client;
	rd->dev.release = rt_regmap_device_release;
	dev_set_drvdata(&rd->dev, drvdata);
	ret = snprintf(device_name, 32, "rt_regmap_%s", props->name);
	if (!ret)
		return NULL;
	dev_set_name(&rd->dev, device_name);
	memcpy(&rd->props, props, sizeof(struct rt_regmap_properties));
	rd->props.cache_mode_ori = rd->props.rt_regmap_mode&RT_CACHE_MODE_MASK;

	/* check rt_registe_map format */
	ret = rt_regmap_check(rd);
	if (ret) {
		pr_info("rt register map format error\n");
		devm_kfree(parent, rd);
		return NULL;
	}

	ret = device_register(&rd->dev);
	if (ret) {
		pr_info("rt-regmap dev register fail\n");
		devm_kfree(parent, rd);
		return NULL;
	}

	rd->rops = rops;
	rd->slv_addr = slv_addr;
	rd->err_msg = devm_kzalloc(parent, 128*sizeof(char), GFP_KERNEL);

	/* init cache data */
	ret = rt_regmap_cache_init(rd);
	if (ret < 0) {
		pr_info(" rt cache data init fail\n");
		goto err_cacheinit;
	}

	INIT_DELAYED_WORK(&rd->rt_work, rt_work_func);

	for (i = 0; i <= 3; i++)
		rd->rt_block_write[i] = rt_block_map[i];

	data = rd->props.rt_regmap_mode & RT_CACHE_MODE_MASK;
	if (data == RT_CACHE_WR_THROUGH) {
		rd->regmap_ops.regmap_block_write = &rt_cache_block_write;
		rd->regmap_ops.regmap_block_read = &rt_cache_block_read;
	} else if (data == RT_CACHE_WR_BACK) {
		rd->regmap_ops.regmap_block_write = &rt_asyn_cache_block_write;
		rd->regmap_ops.regmap_block_read = &rt_cache_block_read;
	} else if (data == RT_CACHE_DISABLE) {
		rd->regmap_ops.regmap_block_write = &rt_chip_block_write;
		rd->regmap_ops.regmap_block_read = &rt_chip_block_read;
	}

	INIT_DELAYED_WORK(&rd->watchdog_work, rt_regmap_watchdog_work);
	alarm_init(&rd->watchdog_alarm, ALARM_REALTIME,
		rt_regmap_watchdog_alarm);

#ifdef CONFIG_DEBUG_FS
	rd->rt_den = debugfs_create_dir(props->name, rt_regmap_dir);
	if (!IS_ERR(rd->rt_den)) {
		rt_create_general_debug(rd, rd->rt_den);
		if (rd->props.rt_regmap_mode & DBG_MODE_MASK)
			rt_create_every_debug(rd, rd->rt_den);
	} else
		goto err_debug;
#endif /* CONFIG_DEBUG_FS */

	return rd;

#ifdef CONFIG_DEBUG_FS
err_debug:
	rt_regmap_cache_release(rd);
#endif /* CONFIG_DEBUG_FS */
err_cacheinit:
	device_unregister(&rd->dev);
	return NULL;

}
EXPORT_SYMBOL(rt_regmap_device_register_ex);

/* rt_regmap_device_unregister - unregister rt_regmap_device*/
void rt_regmap_device_unregister(struct rt_regmap_device *rd)
{
	if (!rd)
		return;
	down(&rd->semaphore);
	rd->rops = NULL;
	up(&rd->semaphore);
	if (rd->cache_inited)
		rt_regmap_cache_release(rd);
#ifdef CONFIG_DEBUG_FS
	debugfs_remove_recursive(rd->rt_den);
	if (rd->props.rt_regmap_mode & DBG_MODE_MASK)
		rt_release_every_debug(rd);
#endif /* CONFIG_DEBUG_FS */
	device_unregister(&rd->dev);
}
EXPORT_SYMBOL(rt_regmap_device_unregister);

static int __init regmap_plat_init(void)
{
	pr_info("Init Richtek RegMap %s\n", RT_REGMAP_VERSION);
#ifdef CONFIG_DEBUG_FS
	rt_regmap_dir = debugfs_create_dir("rt-regmap", 0);
	if (IS_ERR(rt_regmap_dir)) {
		pr_err("rt-regmap debugfs node create fail\n");
		return -EINVAL;
	}
#endif /* CONFIG_DEBUG_FS */
	return 0;
}

subsys_initcall(regmap_plat_init);

static void __exit regmap_plat_exit(void)
{
#ifdef CONFIG_DEBUG_FS
	debugfs_remove(rt_regmap_dir);
#endif /* CONFIG_DEBUG_FS */
}

module_exit(regmap_plat_exit);

MODULE_DESCRIPTION("Richtek regmap Driver");
MODULE_AUTHOR("Jeff Chang <jeff_chang@richtek.com>");
MODULE_VERSION(RT_REGMAP_VERSION);
MODULE_LICENSE("GPL");
/* Version Note
 * 1.1.14
 *	Fix Coverity by Mandatory's
 */
