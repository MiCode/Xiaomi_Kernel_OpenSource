/* drivers/misc/mediatek/rt-regmap/rt-regmap.c
 * Richtek regmap with debugfs Driver
 *
 * Copyright (C) 2014 Richtek Technology Corp.
 * Author: Jeff Chang <jeff_chang@richtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
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

#include <mt-plat/rt-regmap.h>

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
	RT_SUPPORT_MODE,
	RT_DBG_IO_LOG,
	RT_DBG_CACHE_MODE,
	RT_DBG_REG_SIZE,
};

struct reg_index_offset {
	int index;
	int offset;
};

struct rt_debug_data {
	struct reg_index_offset rio;
	unsigned int reg_addr;
	unsigned int reg_size;
	unsigned char part_id;
};

/* rt_regmap_device
 *
 * Richtek regmap device. One for each rt_regmap.
 *
 */
struct rt_regmap_device {
	struct rt_regmap_properties props;
	struct rt_regmap_fops *rops;
	struct rt_regmap_ops regmap_ops;
	struct device dev;
	void *client;
	struct semaphore semaphore;
	struct dentry *rt_den;
	struct dentry *rt_debug_file[13];
	struct rt_debug_st rtdbg_st[13];
	struct dentry **rt_reg_file;
	struct rt_debug_st **reg_st;
	struct rt_debug_data dbg_data;
	struct delayed_work rt_work;
	unsigned char *cache_flag;
	unsigned char part_size_limit;
	unsigned char *alloc_data;
	char *err_msg;

	int (*rt_block_write[4])(struct rt_regmap_device *rd,
			struct rt_register *rm, int size,
			const struct reg_index_offset *rio,
			unsigned char *wdata, int *count);
	unsigned char cache_inited:1;
	unsigned char error_occurred:1;
	unsigned char pending_event:1;
};

struct dentry *rt_regmap_dir;

static int get_parameters(char *buf, long int *param1, int num_of_par)
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
	long int value;
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
		} else if (reg > rm[index]->addr) {
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
		if (*(rd->cache_flag + i) == 1) {
			rc = rt_chip_block_write(rd, rm[i]->addr,
					rm[i]->size, rm[i]->cache_data);
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
					rm[rio.index]->cache_data);
		if (rc < 0) {
			dev_err(&rd->dev, "rt-regmap sync error\n");
			goto err_cache_chip_write;
		}
		*(rd->cache_flag + rio.index) = 0;
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
	rt_register_map_t rm;

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
	rt_register_map_t rm;

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
			count += size;
		} else {
			blk_index = (rd->props.rt_regmap_mode &
					RT_IO_BLK_MODE_MASK)>>3;

			ret = rd->rt_block_write[blk_index]
					(rd, rm, size, &rio, wdata, &count);
			if (ret < 0) {
				dev_err(&rd->dev, "rd->rt_block_write fail\n");
				goto ERR;
			}
		}

		if ((rm->reg_type&RT_REG_TYPE_MASK) != RT_VOLATILE)
			*(rd->cache_flag + rio.index + reg_base) = 1;

		bytes -= size;
		if (bytes <= 0)
			goto finished;
		reg_base++;
		rio.offset = 0;
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
			j += sprintf(wri_data + j, "%02x,", wdata[i]);
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
	rt_register_map_t rm;

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
				(rd, rm, size, &rio, wdata, &count);
		}
		if (ret < 0) {
			dev_err(&rd->dev, "rd->rt_block_write fail\n");
			goto ERR;
		}

		if ((rm->reg_type&RT_REG_TYPE_MASK) != RT_VOLATILE) {
			*(rd->cache_flag + rio.index + reg_base) = 1;
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
			j += sprintf(wri_data + j, "%02x,", wdata[i]);
		pr_info("RT_REGMAP [WRITE] reg0x%04x  [Data] 0x%s\n",
								reg, wri_data);
	}

	schedule_delayed_work(&rd->rt_work, msecs_to_jiffies(1));
	return 0;
ERR:
	return -EIO;
}

static int rt_block_write_blk_all(struct rt_regmap_device *rd,
				  struct rt_register *rm, int size,
				  const struct reg_index_offset *rio,
				  unsigned char *wdata, int *count)
{
	int cnt;

	cnt = *count;
	cnt += size;
	*count = cnt;
	return 0;
}

static int rt_block_write_blk_chip(struct rt_regmap_device *rd,
				   struct rt_register *rm, int size,
				   const struct reg_index_offset *rio,
				   unsigned char *wdata, int *count)
{
	int i, cnt;

	cnt = *count;
	for (i = rio->offset; i < rio->offset+size; i++) {
		if ((rm->reg_type&RT_REG_TYPE_MASK) != RT_VOLATILE)
			rm->cache_data[i] =
				wdata[cnt] & rm->wbit_mask[i];
		cnt++;
	}
	*count = cnt;
	return 0;
}

static int rt_block_write_blk_cache(struct rt_regmap_device *rd,
				    struct rt_register *rm, int size,
				    const struct reg_index_offset *rio,
				    unsigned char *wdata, int *count)
{
	int ret, cnt;

	cnt = *count;

	ret = rt_chip_block_write(rd, rm->addr+rio->offset, size, &wdata[cnt]);
	if (ret < 0) {
		dev_err(&rd->dev,
		"rt block write fail at 0x%02x\n", rm->addr + rio->offset);
		return -EIO;
	}
	cnt += size;
	*count = cnt;
	return 0;
}

static int rt_block_write(struct rt_regmap_device *rd,
			  struct rt_register *rm, int size,
			  const struct reg_index_offset *rio,
			  unsigned char *wdata, int *count)
{
	int i, ret, cnt, change = 0;

	cnt = *count;

	for (i = rio->offset; i < size+rio->offset; i++) {
		if ((rm->reg_type & RT_REG_TYPE_MASK) != RT_VOLATILE) {
			if (rm->reg_type&RT_WR_ONCE) {
				if (rm->cache_data[i] !=
					(wdata[cnt]&rm->wbit_mask[i]))
					change++;
			}
			rm->cache_data[i] = wdata[cnt] & rm->wbit_mask[i];
		}
		cnt++;
	}

	if (!change && (rm->reg_type&RT_WR_ONCE))
		goto finish;

	if ((rd->props.rt_regmap_mode&RT_CACHE_MODE_MASK) ==
						RT_CACHE_WR_THROUGH) {
		ret = rt_chip_block_write(rd,
				rm->addr+rio->offset, size, rm->cache_data);
		if (ret < 0) {
			dev_err(&rd->dev,
			"rt block write fail at 0x%02x\n",
			rm->addr + rio->offset);
			return -EIO;
		}
	}

finish:
	*count = cnt;
	return 0;
}

static int (*rt_block_map[])(struct rt_regmap_device *rd,
			     struct rt_register *rm, int size,
			     const struct reg_index_offset *rio,
			     unsigned char *wdata, int *count) = {
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
	rt_register_map_t rm;
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

	memcpy(data, &rm->cache_data[rio.offset], bytes);

	if ((rm->reg_type&RT_REG_TYPE_MASK) == RT_VOLATILE) {
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
	} else
		count += (rm->size - rio.offset);

	while (count < bytes) {
		reg_base++;
		rm = rd->props.rm[rio.index + reg_base];
		if ((rm->reg_type&RT_REG_TYPE_MASK) == RT_VOLATILE) {
			ret = rd->rops->read_device(rd->client,
					rm->addr, rm->size, &data[count]);
			if (ret < 0) {
				dev_err(&rd->dev,
				"rt_regmap Error at 0x%02x\n", rm->addr);
				return -EIO;
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
			*(rd->cache_flag + i) = 1;
	rd->pending_event = 1;
	up(&rd->semaphore);
}
EXPORT_SYMBOL(rt_regmap_cache_backup);

/* _rt_regmap_reg_write - write data to specific register map
 * only support 1, 2, 4 bytes regisetr map
 * @rd: rt_regmap_device pointer.
 * @rrd: rt_reg_data pointer.
 */
int _rt_regmap_reg_write(struct rt_regmap_device *rd,
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
EXPORT_SYMBOL(_rt_regmap_reg_write);

/* _rt_asyn_regmap_reg_write - asyn write data to specific register map*/
int _rt_asyn_regmap_reg_write(struct rt_regmap_device *rd,
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
EXPORT_SYMBOL(_rt_asyn_regmap_reg_write);

/* _rt_regmap_update_bits - assign bits specific register map */
int _rt_regmap_update_bits(struct rt_regmap_device *rd,
				struct rt_reg_data *rrd)
{
	const rt_register_map_t *rm = rd->props.rm;
	struct reg_index_offset rio;
	int ret, new, old;
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
				dev_err(&rd->dev, "rt regmap block write fail\n");
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
				dev_err(&rd->dev, "rt regmap block write fail\n");
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
				dev_err(&rd->dev, "rt regmap block write fail\n");
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
				dev_err(&rd->dev, "rt regmap block write fail\n");
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
	return change;
err_update_bits:
	up(&rd->semaphore);
	return ret;
}
EXPORT_SYMBOL(_rt_regmap_update_bits);

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
int _rt_regmap_reg_read(struct rt_regmap_device *rd, struct rt_reg_data *rrd)
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
EXPORT_SYMBOL(_rt_regmap_reg_read);

void rt_cache_getlasterror(struct rt_regmap_device *rd, char *buf)
{
	down(&rd->semaphore);
	snprintf(buf, 512, "%s\n", rd->err_msg);
	up(&rd->semaphore);
}
EXPORT_SYMBOL(rt_cache_getlasterror);

void rt_cache_clrlasterror(struct rt_regmap_device *rd)
{
	down(&rd->semaphore);
	rd->error_occurred = 0;
	snprintf(rd->err_msg, 512, "%s", "No Error");
	up(&rd->semaphore);
}
EXPORT_SYMBOL(rt_cache_clrlasterror);

/* initialize cache data from rt_register */
int rt_regmap_cache_init(struct rt_regmap_device *rd)
{
	int i, j, ret, bytes_num = 0, count = 0;
	const rt_register_map_t *rm = rd->props.rm;

	dev_info(&rd->dev, "rt register cache data init\n");

	down(&rd->semaphore);
	rd->cache_flag = devm_kzalloc(&rd->dev,
		rd->props.register_num * sizeof(int), GFP_KERNEL);

	if (rd->props.group == NULL) {
		rd->props.group = devm_kzalloc(&rd->dev,
				sizeof(*rd->props.group), GFP_KERNEL);
		rd->props.group[0].start = 0x00;
		rd->props.group[0].end = 0xffff;
		rd->props.group[0].mode = RT_1BYTE_MODE;
	}

	/* calculate maxima size for showing on regs debugfs node*/
	rd->part_size_limit = 0;
	for (i = 0; i < rd->props.register_num; i++) {
		if (!rm[i]->cache_data)
			bytes_num += rm[i]->size;
		if (rm[i]->size > rd->part_size_limit &&
		    (rm[i]->reg_type & RT_REG_TYPE_MASK) != RT_RESERVE)
			rd->part_size_limit = rm[i]->size;
	}
	rd->part_size_limit = 400 / ((rd->part_size_limit-1)*3 + 5);

	rd->alloc_data =
	    devm_kzalloc(&rd->dev,
			bytes_num * sizeof(unsigned char), GFP_KERNEL);
	if (!rd->alloc_data) {
		pr_info("tmp data memory allocate fail\n");
		goto mem_err;
	}

	/* reload cache data from real chip */
	for (i = 0; i < rd->props.register_num; i++) {
		if (!rm[i]->cache_data) {
			rm[i]->cache_data = rd->alloc_data + count;
			count += rm[i]->size;
			if ((rm[i]->reg_type&RT_REG_TYPE_MASK) != RT_VOLATILE) {
				ret = rd->rops->read_device(rd->client,
						rm[i]->addr, rm[i]->size,
							rm[i]->cache_data);
				if (ret < 0) {
					dev_err(&rd->dev, "chip read fail\n");
					goto io_err;
				}
			} else
				memset(rm[i]->cache_data, 0x00, rm[i]->size);
		}
		*(rd->cache_flag + i) = 0;
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
mem_err:
	up(&rd->semaphore);
	return -ENOMEM;
io_err:
	up(&rd->semaphore);
	return -EIO;
}
EXPORT_SYMBOL(rt_regmap_cache_init);

/* rt_regmap_cache_reload - reload cache valuew from real chip*/
int rt_regmap_cache_reload(struct rt_regmap_device *rd)
{
	int i, ret;
	const rt_register_map_t *rm = rd->props.rm;

	down(&rd->semaphore);
	for (i = 0; i < rd->props.register_num; i++) {
		if ((rm[i]->reg_type&RT_REG_TYPE_MASK) != RT_VOLATILE) {
			ret = rd->rops->read_device(rd->client, rm[i]->addr,
						rm[i]->size, rm[i]->cache_data);
			if (ret < 0) {
				dev_err(&rd->dev, "i2c read fail\n");
				goto io_err;
			}
			*(rd->cache_flag + i) = 0;
		}
	}
	rd->pending_event = 0;
	up(&rd->semaphore);
	dev_info(&rd->dev, "cache data reload\n");
	return 0;

io_err:
	up(&rd->semaphore);
	return -EIO;
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
	const rt_register_map_t *rm = rd->props.rm;

	dev_info(&rd->dev, "cache data release\n");
	for (i = 0; i < rd->props.register_num; i++)
		rm[i]->cache_data = NULL;
	devm_kfree(&rd->dev, rd->alloc_data);
	if (rd->cache_flag)
		devm_kfree(&rd->dev, rd->cache_flag);
	rd->cache_inited = 0;
}

#ifdef CONFIG_DEBUG_FS
static void rt_check_dump_config_file(struct rt_regmap_device *rd,
				long int *reg_dump, int *cnt, char *type)
{
	char *token, *buf, *tmp_type;
	char PATH[64];
	mm_segment_t fs;
	struct file *fp;
	int ret, tmp_cnt = 0;

	buf = devm_kzalloc(&rd->dev, 64*sizeof(char), GFP_KERNEL);
	snprintf(PATH, 64, "/sdcard/%s_dump_config.txt", rd->props.name);
	fp = filp_open(PATH, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		pr_info("There is no Dump config file in sdcard\n");
		devm_kfree(&rd->dev, buf);
	} else {
		fs = get_fs();
		set_fs(get_ds());
		fp->f_op->read(fp, buf, 64, &fp->f_pos);
		set_fs(fs);

		tmp_type = token = strsep(&buf, " ");
		token = strsep(&buf, " ");
		while (token != NULL) {
			ret = kstrtoul(token, 16, &reg_dump[tmp_cnt]);
			if (ret == 0)
				tmp_cnt++;
			token = strsep(&buf, " ");
		}
		filp_close(fp, NULL);
		*cnt = tmp_cnt;
		memcpy(type, tmp_type, 16);
		devm_kfree(&rd->dev, buf);
	}
}

static void rt_show_regs(struct rt_regmap_device *rd, struct seq_file *seq_file)
{
	int i = 0, k = 0, ret, count = 0, cnt = 0;
	unsigned char regval[512];
	long int reg_dump[64] = {0};
	const rt_register_map_t *rm = rd->props.rm;
	char type[16];

	rt_check_dump_config_file(rd, reg_dump, &cnt, type);
	down(&rd->semaphore);
	for (i = 0; i < rd->props.register_num; i++) {
		ret = rd->regmap_ops.regmap_block_read(rd, rm[i]->addr,
						rm[i]->size, &regval[count]);
		count += rm[i]->size;
		if (ret < 0) {
			dev_err(&rd->dev, "regmap block read fail\n");
			if (rd->error_occurred) {
				sprintf(rd->err_msg + strlen(rd->err_msg),
				"Error block read fail at 0x%02x\n",
				rm[i]->addr);
			} else {
				sprintf(rd->err_msg,
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
	up(&rd->semaphore);
}

static int general_read(struct seq_file *seq_file, void *_data)
{
	struct rt_debug_st *st = (struct rt_debug_st *)seq_file->private;
	struct rt_regmap_device *rd = st->info;
	rt_register_map_t rm;
	char lbuf[900];
	unsigned char reg_data[24] = { 0 };
	unsigned char data;
	int i = 0, rc = 0, size = 0;

	lbuf[0] = '\0';
	switch (st->id) {
	case RT_DBG_REG:
		seq_printf(seq_file, "0x%04x\n", rd->dbg_data.reg_addr);
		break;
	case RT_DBG_DATA:
		if (rd->dbg_data.reg_size == 0)
			rd->dbg_data.reg_size = 1;

		size = rd->dbg_data.reg_size;

		if (rd->dbg_data.rio.index == -1) {
			down(&rd->semaphore);
			rc = rt_chip_block_read(rd, rd->dbg_data.reg_addr,
							size, reg_data);
			up(&rd->semaphore);
			if (rc < 0) {
				seq_puts(seq_file, "invalid read\n");
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
			break;
		}

hiden_read:
		if (&reg_data[i] != NULL) {
			seq_puts(seq_file, "0x");
			for (i = 0; i < size; i++)
				seq_printf(seq_file, "%02x,", reg_data[i]);
			seq_puts(seq_file, "\n");
		}
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
		{
			struct i2c_client *i2c = rd->client;

			seq_printf(seq_file, "0x%02x\n", i2c->addr);
		}
		break;
	case RT_SUPPORT_MODE:
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

static ssize_t general_write(struct file *file, const char __user *ubuf,
			     size_t count, loff_t *ppos)
{
	struct rt_debug_st *st = file->private_data;
	struct rt_regmap_device *rd = st->info;
	struct reg_index_offset rio;
	long int param[5];
	unsigned char reg_data[24] = { 0 };
	int rc, size = 0;
	char lbuf[128];

	if (count > sizeof(lbuf) - 1)
		return -EFAULT;

	rc = copy_from_user(lbuf, ubuf, count);
	if (rc)
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

		if (rd->dbg_data.rio.index == -1) {
			size = rd->dbg_data.reg_size;
			if ((size - 1)*3 + 5 != count) {
				dev_err(&rd->dev, "wrong input length\n");
				if (rd->error_occurred) {
					sprintf(rd->err_msg +
						strlen(rd->err_msg),
						"Error, wrong input length\n");
				} else {
					sprintf(rd->err_msg,
						"Error, wrong input length\n");
					rd->error_occurred = 1;
				}
				return -EINVAL;
			}

			rc = get_datas((char *)ubuf, count, reg_data, size);
			if (rc < 0) {
				dev_err(&rd->dev, "get datas fail\n");
				if (rd->error_occurred) {
					sprintf(rd->err_msg +
					strlen(rd->err_msg),
					"Error, get datas fail\n");
				} else {
					sprintf(rd->err_msg,
						"Error, get datas fail\n");
					rd->error_occurred = 1;
				}
				return -EINVAL;
			}
			down(&rd->semaphore);
			rc = rt_chip_block_write(rd, rd->dbg_data.reg_addr,
							size, reg_data);
			up(&rd->semaphore);
			if (rc < 0) {
				dev_err(&rd->dev, "chip block write fail\n");
				if (rd->error_occurred) {
					sprintf(rd->err_msg +
					strlen(rd->err_msg),
				"Error chip block write fail at 0x%02x\n",
					rd->dbg_data.reg_addr);
				} else {
					sprintf(rd->err_msg,
				"Error chip block write fail at 0x%02x\n",
					rd->dbg_data.reg_addr);
					rd->error_occurred = 1;
				}
				return -EIO;
			}
			break;
		}

		size = rd->dbg_data.reg_size;

		if ((size - 1)*3 + 5 != count) {
			dev_err(&rd->dev, "wrong input length\n");
			if (rd->error_occurred) {
				sprintf(rd->err_msg + strlen(rd->err_msg),
					"Error, wrong input length\n");
			} else {
				sprintf(rd->err_msg,
					"Error, wrong input length\n");
				rd->error_occurred = 1;
			}
			return -EINVAL;
		}

		rc = get_datas((char *)ubuf, count, reg_data, size);
		if (rc < 0) {
			dev_err(&rd->dev, "get datas fail\n");
			if (rd->error_occurred) {
				sprintf(rd->err_msg + strlen(rd->err_msg),
				"Error, get datas fail\n");
			} else {
				sprintf(rd->err_msg,
				"Error, get datas fail\n");
				rd->error_occurred = 1;
			}
			return -EINVAL;
		}

		down(&rd->semaphore);
		rc = rd->regmap_ops.regmap_block_write(rd,
				rd->dbg_data.reg_addr, size, reg_data);
		up(&rd->semaphore);
		if (rc < 0) {
			dev_err(&rd->dev, "regmap block write fail\n");
			if (rd->error_occurred) {
				sprintf(rd->err_msg + strlen(rd->err_msg),
				"Error regmap block write fail at 0x%02x\n",
				rd->dbg_data.reg_addr);
			} else {
				sprintf(rd->err_msg,
				"Error regmap block write fail at 0x%02x\n",
				rd->dbg_data.reg_addr);
				rd->error_occurred = 1;
			}
			return -EIO;
		}

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
				sprintf(rd->err_msg + strlen(rd->err_msg),
				"Error, size must > 0\n");
			} else {
				sprintf(rd->err_msg,
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

		if (param[0] == RT_CACHE_WR_THROUGH) {
			rt_regmap_cache_reload(rd);
			rd->regmap_ops.regmap_block_write =
						rt_cache_block_write;
			rd->regmap_ops.regmap_block_read = &rt_cache_block_read;
		} else if (param[0] == RT_CACHE_WR_BACK) {
			rt_regmap_cache_reload(rd);
			rd->regmap_ops.regmap_block_write =
						rt_asyn_cache_block_write;
			rd->regmap_ops.regmap_block_read = &rt_cache_block_read;
		} else if (param[0] == RT_CACHE_DISABLE) {
			rd->regmap_ops.regmap_block_write =
							rt_chip_block_write;
			rd->regmap_ops.regmap_block_read = rt_chip_block_read;
		}

		rd->props.rt_regmap_mode &= ~RT_CACHE_MODE_MASK;
		rd->props.rt_regmap_mode |= param[0];

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

/* create general debugfs node */
static void rt_create_general_debug(struct rt_regmap_device *rd,
				    struct dentry *dir)
{
	rd->rtdbg_st[0].info = rd;
	rd->rtdbg_st[0].id = RT_DBG_REG;
	rd->rt_debug_file[0] = debugfs_create_file("reg_addr",
						   S_IFREG | S_IRUGO, dir,
						   (void *)&rd->rtdbg_st[0],
						   &general_ops);
	rd->rtdbg_st[1].info = rd;
	rd->rtdbg_st[1].id = RT_DBG_DATA;
	rd->rt_debug_file[1] = debugfs_create_file("data",
						   S_IFREG | S_IRUGO, dir,
						   (void *)&rd->rtdbg_st[1],
						   &general_ops);

	rd->rtdbg_st[2].info = rd;
	rd->rtdbg_st[2].id = RT_DBG_REGS;
	rd->rt_debug_file[2] = debugfs_create_file("regs",
						   S_IFREG | S_IRUGO, dir,
						   (void *)&rd->rtdbg_st[2],
						   &general_ops);

	rd->rtdbg_st[3].info = rd;
	rd->rtdbg_st[3].id = RT_DBG_SYNC;
	rd->rt_debug_file[3] = debugfs_create_file("sync",
						   S_IFREG | S_IRUGO, dir,
						   (void *)&rd->rtdbg_st[3],
						   &general_ops);

	rd->rtdbg_st[4].info = rd;
	rd->rtdbg_st[4].id = RT_DBG_ERROR;
	rd->rt_debug_file[4] = debugfs_create_file("Error",
						   S_IFREG | S_IRUGO, dir,
						   (void *)&rd->rtdbg_st[4],
						   &general_ops);

	rd->rtdbg_st[5].info = rd;
	rd->rtdbg_st[5].id = RT_DBG_NAME;
	rd->rt_debug_file[5] = debugfs_create_file("name",
						   S_IFREG | S_IRUGO, dir,
						   (void *)&rd->rtdbg_st[5],
						   &general_ops);

	rd->rtdbg_st[6].info = rd;
	rd->rtdbg_st[6].id = RT_DBG_BLOCK;
	rd->rt_debug_file[6] = debugfs_create_file("block",
						   S_IFREG | S_IRUGO, dir,
						   (void *)&rd->rtdbg_st[6],
						   &general_ops);

	rd->rtdbg_st[7].info = rd;
	rd->rtdbg_st[7].id = RT_DBG_SIZE;
	rd->rt_debug_file[7] = debugfs_create_file("size",
						   S_IFREG | S_IRUGO, dir,
						   (void *)&rd->rtdbg_st[7],
						   &general_ops);

	rd->rtdbg_st[8].info = rd;
	rd->rtdbg_st[8].id = RT_DBG_SLAVE_ADDR;
	rd->rt_debug_file[8] = debugfs_create_file("slave_addr",
						   S_IFREG | S_IRUGO, dir,
						   (void *)
						   &rd->rtdbg_st[8],
						   &general_ops);

	rd->rtdbg_st[9].info = rd;
	rd->rtdbg_st[9].id = RT_SUPPORT_MODE;
	rd->rt_debug_file[9] = debugfs_create_file("support_mode",
						   S_IFREG | S_IRUGO, dir,
						   (void *)&rd->rtdbg_st[9],
						   &general_ops);

	rd->rtdbg_st[10].info = rd;
	rd->rtdbg_st[10].id = RT_DBG_IO_LOG;
	rd->rt_debug_file[10] = debugfs_create_file("io_log",
						    S_IFREG | S_IRUGO, dir,
						    (void *)&rd->rtdbg_st[10],
						    &general_ops);

	rd->rtdbg_st[11].info = rd;
	rd->rtdbg_st[11].id = RT_DBG_CACHE_MODE;
	rd->rt_debug_file[11] = debugfs_create_file("cache_mode",
						    S_IFREG | S_IRUGO, dir,
						    (void *)&rd->rtdbg_st[11],
						    &general_ops);
	rd->rtdbg_st[12].info = rd;
	rd->rtdbg_st[12].id = RT_DBG_REG_SIZE;
	rd->rt_debug_file[12] = debugfs_create_file("reg_size",
						   S_IFREG | S_IRUGO, dir,
						   (void *)&rd->rtdbg_st[12],
						   &general_ops);
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
	rt_register_map_t rm = rd->props.rm[st->id];
	int rc;
	unsigned char pars[20];

	if ((rm->size - 1)*3 + 5 != count) {
		dev_err(&rd->dev, "wrong input length\n");
		return -EINVAL;
	}
	rc = get_datas((char *)ubuf, count, pars, rm->size);
	if (rc < 0) {
		dev_err(&rd->dev, "get datas fail\n");
		return -EINVAL;
	}

	down(&rd->semaphore);
	rc = rd->regmap_ops.regmap_block_write(rd, rm->addr,
					rm->size, &pars[0]);
	up(&rd->semaphore);
	if (rc < 0) {
		dev_err(&rd->dev, "regmap block read fail\n");
		return -EIO;
	}

	return count;
}

static ssize_t eachreg_read(struct file *file, char __user *ubuf,
			    size_t count, loff_t *ppos)
{
	struct rt_debug_st *st = file->private_data;
	struct rt_regmap_device *rd = st->info;
	char lbuf[200];
	unsigned char regval[32];
	rt_register_map_t rm = rd->props.rm[st->id];
	int i, j = 0, rc;

	lbuf[0] = '\0';

	down(&rd->semaphore);
	rc = rd->regmap_ops.regmap_block_read(rd, rm->addr, rm->size, regval);
	up(&rd->semaphore);
	if (rc < 0) {
		dev_err(&rd->dev, "regmap block read fail\n");
		return -EIO;
	}

	j += sprintf(lbuf + j, "reg0x%02x:0x", rm->addr);
	for (i = 0; i < rm->size; i++)
		j += sprintf(lbuf + j, "%02x,", regval[i]);
	j += sprintf(lbuf + j, "\n");

	return simple_read_from_buffer(ubuf, count, ppos, lbuf, strlen(lbuf));
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
	int i;
	char buf[10];

	rd->rt_reg_file = devm_kzalloc(&rd->dev,
		rd->props.register_num*sizeof(struct dentry *), GFP_KERNEL);
	rd->reg_st = devm_kzalloc(&rd->dev,
		rd->props.register_num*sizeof(struct rt_debug_st *),
								GFP_KERNEL);
	for (i = 0; i < rd->props.register_num; i++) {
		sprintf(buf, "reg0x%02x", (rd->props.rm[i])->addr);
		rd->rt_reg_file[i] = devm_kzalloc(&rd->dev,
						  sizeof(*rd->rt_reg_file[i]),
						  GFP_KERNEL);
		rd->reg_st[i] =
		    devm_kzalloc(&rd->dev, sizeof(*rd->reg_st[i]), GFP_KERNEL);

		rd->reg_st[i]->info = rd;
		rd->reg_st[i]->id = i;
		rd->rt_reg_file[i] = debugfs_create_file(buf,
							 S_IFREG | S_IRUGO, dir,
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
	if (!rd->dbg_data.reg_addr && rm[0]->addr) {
		rd->dbg_data.reg_addr = rm[0]->addr;
		rd->dbg_data.rio.index = 0;
		rd->dbg_data.rio.offset = 0;
	}
	return 0;
}

static int rt_create_simple_map(struct rt_regmap_device *rd)
{
	int i, j, count = 0, num = 0;
	rt_register_map_t *rm;

	pr_info("%s\n", __func__);
	for (i = 0; i < rd->props.register_num; i++)
		num += rd->props.rm[i]->size;

	rm = devm_kzalloc(&rd->dev, num * sizeof(*rm), GFP_KERNEL);

	for (i = 0; i < rd->props.register_num; i++) {
		for (j = 0; j < rd->props.rm[i]->size; j++) {
			rm[count] = devm_kzalloc(&rd->dev,
						 sizeof(struct rt_register),
						 GFP_KERNEL);
			rm[count]->wbit_mask = devm_kzalloc(&rd->dev,
				sizeof(unsigned char), GFP_KERNEL);

			rm[count]->addr = rd->props.rm[i]->addr + j;
			rm[count]->size = 1;
			rm[count]->reg_type = rd->props.rm[i]->reg_type;
			if ((rd->props.rm[i]->reg_type&RT_REG_TYPE_MASK) !=
								RT_WBITS)
				rm[count]->wbit_mask[0] = 0xff;
			else
				rm[count]->wbit_mask[0] =
					rd->props.rm[i]->wbit_mask[0];
			count++;
		}
		if (count > num)
			break;
	}

	rd->props.register_num = num;
	rd->props.rm = rm;

	return 0;
}

/* rt_regmap_device_register
 * @props: a pointer to rt_regmap_properties for rt_regmap_device
 * @rops: a pointer to rt_regmap_fops for rt_regmap_device
 * @parent: a pinter to parent device
 * @client: a pointer to the slave client of this device
 * @drvdata: a pointer to the driver data
 */
struct rt_regmap_device *rt_regmap_device_register
			(struct rt_regmap_properties *props,
			struct rt_regmap_fops *rops,
			struct device *parent,
			void *client, void *drvdata)
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
	rd = devm_kzalloc(parent, sizeof(*rd), GFP_KERNEL);
	if (!rd) {
		pr_info("rt_regmap_device memory allocate fail\n");
		return NULL;
	}

	/* create a binary semaphore */
	sema_init(&rd->semaphore, 1);
	rd->dev.parent = parent;
	rd->client = client;
	rd->dev.release = rt_regmap_device_release;
	dev_set_drvdata(&rd->dev, drvdata);
	snprintf(device_name, 32, "rt_regmap_%s", props->name);
	dev_set_name(&rd->dev, device_name);
	memcpy(&rd->props, props, sizeof(struct rt_regmap_properties));

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
	rd->err_msg = devm_kzalloc(parent, 128*sizeof(char), GFP_KERNEL);

	if (!(rd->props.rt_regmap_mode &  RT_BYTE_MODE_MASK)) {
		ret = rt_create_simple_map(rd);
		if (ret < 0) {
			pr_info(" rt create simple register map fail\n");
			goto err_cacheinit;
		}
	}

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
EXPORT_SYMBOL(rt_regmap_device_register);

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
	rt_regmap_dir = debugfs_create_dir("rt-regmap", 0);
	pr_info("Init Richtek RegMap\n");
	if (IS_ERR(rt_regmap_dir)) {
		pr_err("rt-regmap debugfs node create fail\n");
		return -EINVAL;
	}
	return 0;
}

subsys_initcall(regmap_plat_init);

static void __exit regmap_plat_exit(void)
{
	debugfs_remove(rt_regmap_dir);
}

module_exit(regmap_plat_exit);

MODULE_DESCRIPTION("Richtek regmap Driver");
MODULE_AUTHOR("Jeff Chang <jeff_chang@richtek.com>");
MODULE_VERSION(RT_REGMAP_VERSION);
MODULE_LICENSE("GPL");
