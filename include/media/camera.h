/*
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.

 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.

 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __CAMERA_H__
#define __CAMERA_H__

#ifdef __KERNEL__
#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/edp.h>
#include <media/nvc.h>
#endif

#define CAMERA_INT_MASK			0xf0000000
#define CAMERA_TABLE_WAIT_US		(CAMERA_INT_MASK | 1)
#define CAMERA_TABLE_WAIT_MS		(CAMERA_INT_MASK | 2)
#define CAMERA_TABLE_END		(CAMERA_INT_MASK | 9)
#define CAMERA_TABLE_PWR		(CAMERA_INT_MASK | 20)
#define CAMERA_TABLE_PINMUX		(CAMERA_INT_MASK | 25)
#define CAMERA_TABLE_INX_PINMUX		(CAMERA_INT_MASK | 26)
#define CAMERA_TABLE_GPIO_ACT		(CAMERA_INT_MASK | 30)
#define CAMERA_TABLE_GPIO_DEACT		(CAMERA_INT_MASK | 31)
#define CAMERA_TABLE_GPIO_INX_ACT	(CAMERA_INT_MASK | 32)
#define CAMERA_TABLE_GPIO_INX_DEACT	(CAMERA_INT_MASK | 33)
#define CAMERA_TABLE_REG_NEW_POWER	(CAMERA_INT_MASK | 40)
#define CAMERA_TABLE_INX_POWER		(CAMERA_INT_MASK | 41)
#define CAMERA_TABLE_INX_CLOCK		(CAMERA_INT_MASK | 50)
#define CAMERA_TABLE_INX_CGATE		(CAMERA_INT_MASK | 51)
#define CAMERA_TABLE_EDP_STATE		(CAMERA_INT_MASK | 60)

#define CAMERA_TABLE_PWR_FLAG_MASK	0xf0000000
#define CAMERA_TABLE_PWR_FLAG_ON	0x80000000
#define CAMERA_TABLE_PINMUX_FLAG_MASK	0xf0000000
#define CAMERA_TABLE_PINMUX_FLAG_ON	0x80000000
#define CAMERA_TABLE_CLOCK_VALUE_BITS	24
#define CAMERA_TABLE_CLOCK_VALUE_MASK	\
			((u32)(-1) >> (32 - CAMERA_TABLE_CLOCK_VALUE_BITS))
#define CAMERA_TABLE_CLOCK_INDEX_BITS	(32 - CAMERA_TABLE_CLOCK_VALUE_BITS)
#define CAMERA_TABLE_CLOCK_INDEX_MASK	\
			((u32)(-1) << (32 - CAMERA_TABLE_CLOCK_INDEX_BITS))

#define PCLLK_IOCTL_CHIP_REG	_IOW('o', 100, struct virtual_device)
#define PCLLK_IOCTL_DEV_REG	_IOW('o', 104, struct camera_device_info)
#define PCLLK_IOCTL_DEV_DEL	_IOW('o', 105, int)
#define PCLLK_IOCTL_DEV_FREE	_IOW('o', 106, int)
#define PCLLK_IOCTL_PWR_WR	_IOW('o', 108, int)
#define PCLLK_IOCTL_PWR_RD	_IOR('o', 109, int)
#define PCLLK_IOCTL_SEQ_WR	_IOWR('o', 112, struct nvc_param)
#define PCLLK_IOCTL_SEQ_RD	_IOWR('o', 113, struct nvc_param)
#define PCLLK_IOCTL_UPDATE	_IOW('o', 116, struct nvc_param)
#define PCLLK_IOCTL_LAYOUT_WR	_IOW('o', 120, struct nvc_param)
#define PCLLK_IOCTL_LAYOUT_RD	_IOWR('o', 121, struct nvc_param)
#define PCLLK_IOCTL_PARAM_WR	_IOWR('o', 140, struct nvc_param)
#define PCLLK_IOCTL_PARAM_RD	_IOWR('o', 141, struct nvc_param)
#define PCLLK_IOCTL_DRV_ADD	_IOW('o', 150, struct nvc_param)

#define CAMERA_MAX_EDP_ENTRIES  16
#define CAMERA_MAX_NAME_LENGTH	32
#define CAMDEV_INVALID		0xffffffff

#define	CAMERA_SEQ_STATUS_MASK	0xf0000000
#define	CAMERA_SEQ_INDEX_MASK	0x0000ffff
#define	CAMERA_SEQ_FLAG_MASK	(~CAMERA_SEQ_INDEX_MASK)
#define	CAMERA_SEQ_FLAG_EDP	0x80000000
enum {
	CAMERA_SEQ_EXEC,
	CAMERA_SEQ_REGISTER_EXEC,
	CAMERA_SEQ_REGISTER_ONLY,
	CAMERA_SEQ_EXIST,
	CAMERA_SEQ_MAX_NUM,
};

enum {
	CAMERA_DEVICE_TYPE_I2C,
	CAMERA_DEVICE_TYPE_MAX_NUM,
};

struct camera_device_info {
	u8 name[CAMERA_MAX_NAME_LENGTH];
	u32 type;
	u8 bus;
	u8 addr;
};

struct camera_reg {
	u32 addr;
	u32 val;
};

struct regmap_cfg {
	int addr_bits;
	int val_bits;
	u32 cache_type;
};

struct gpio_cfg {
	int gpio;
	u8 own;
	u8 active_high;
	u8 flag;
	u8 reserved;
};

struct edp_cfg {
	uint estates[CAMERA_MAX_EDP_ENTRIES];
	uint num;
	uint e0_index;
	int priority;
};

#define VIRTUAL_DEV_MAX_REGULATORS	8
#define VIRTUAL_DEV_MAX_GPIOS		8
#define VIRTUAL_REGNAME_SIZE		(VIRTUAL_DEV_MAX_REGULATORS * \
						CAMERA_MAX_NAME_LENGTH)

struct virtual_device {
	__u32 power_on;
	__u32 power_off;
	struct regmap_cfg regmap_cfg;
	__u32 bus_type;
	__u32 gpio_num;
	__u32 reg_num;
	__u32 pwr_on_size;
	__u32 pwr_off_size;
	__u32 clk_num;
	__u8 name[32];
	__u8 reg_names[VIRTUAL_REGNAME_SIZE];
};

enum {
	UPDATE_PINMUX,
	UPDATE_GPIO,
	UPDATE_POWER,
	UPDATE_CLOCK,
	UPDATE_EDP,
	UPDATE_MAX_NUM,
};

struct cam_update {
	u32 type;
	u32 index;
	u32 size;
	u32 arg;
};

enum {
	DEVICE_SENSOR,
	DEVICE_FOCUSER,
	DEVICE_FLASH,
	DEVICE_ROM,
	DEVICE_OTHER,
	DEVICE_OTHER2,
	DEVICE_OTHER3,
	DEVICE_OTHER4,
	DEVICE_MAX_NUM,
};

struct cam_device_layout {
	__u64 guid;
	__u8 name[CAMERA_MAX_NAME_LENGTH];
	__u8 type;
	__u8 alt_name[CAMERA_MAX_NAME_LENGTH];
	__u8 pos;
	__u8 bus;
	__u8 addr;
	__u8 addr_byte;
	__u32 dev_id;
	__u32 index;
	__u32 reserved1;
	__u32 reserved2;
};

#ifdef __KERNEL__

#define NUM_OF_SEQSTACK		16
#define SIZEOF_I2C_BUF		32

struct camera_device;

struct camera_module {
	struct i2c_board_info *sensor;
	struct i2c_board_info *focuser;
	struct i2c_board_info *flash;
};

struct camera_platform_data {
	unsigned cfg;
	int pinmux_num;
	struct tegra_pingroup_config **pinmux;
	struct camera_module *modules;
};

struct camera_edp_cfg {
	struct edp_client edp_client;
	unsigned edp_state;
	u8 edpc_en;
	struct camera_reg *s_throttle;
	int (*shutdown)(struct camera_device *cdev);
};

struct camera_seq_status {
	u32 idx;
	u32 status;
};

struct camera_device {
	struct list_head list;
	u8 name[CAMERA_MAX_NAME_LENGTH];
	struct device *dev;
	struct i2c_client *client;
	struct camera_chip *chip;
	struct regmap *regmap;
	struct camera_info *cam;
	atomic_t in_use;
	struct mutex mutex;
	uint estates[CAMERA_MAX_EDP_ENTRIES];
	struct camera_edp_cfg edpc;
	struct clk **clks;
	u32 num_clk;
	struct nvc_regulator *regs;
	u32 num_reg;
	struct nvc_gpio *gpios;
	u32 num_gpio;
	struct tegra_pingroup_config **pinmux_tbl;
	u32 pinmux_num;
	u32 mclk_enable_idx;
	u32 mclk_disable_idx;
	struct regulator *ext_regs;
	struct camera_reg *seq_stack[NUM_OF_SEQSTACK];
	int pwr_state;
	u8 is_power_on;
	u8 i2c_buf[SIZEOF_I2C_BUF];
};

struct camera_chip {
	const u8			name[CAMERA_MAX_NAME_LENGTH];
	u32				type;
	const struct regmap_config	regmap_cfg;
	struct list_head		list;
	atomic_t			ref_cnt;
	void				*private;
	/* function pointers */
	int	(*init)(struct camera_device *cdev, void *);
	int	(*release)(struct camera_device *cdev);
	int	(*power_on)(struct camera_device *cdev);
	int	(*power_off)(struct camera_device *cdev);
	int	(*shutdown)(struct camera_device *cdev);
	int	(*update)(struct camera_device *cdev,
			struct cam_update *upd, int num);
};

extern int camera_chip_add(struct camera_chip *chip);

#ifdef CAMERA_DEVICE_INTERNAL

struct camera_info {
	struct list_head list;
	atomic_t in_use;
	struct device *dev;
	struct mutex k_mutex;
	struct camera_device *cdev;
};

struct camera_platform_info {
	char dname[CAMERA_MAX_NAME_LENGTH];
	struct miscdevice miscdev;
	atomic_t in_use;
	struct device *dev;
	struct camera_platform_data *pdata;
	struct mutex *u_mutex;
	struct list_head *app_list;
	struct mutex *d_mutex;
	struct list_head *dev_list;
	struct mutex *c_mutex;
	struct list_head *chip_list;
	struct dentry *d_entry;
	void *layout;
	size_t size_layout;
};

/* common functions */
extern int virtual_device_add(
	struct device *, unsigned long
);
extern int camera_regulator_get(
	struct device *, struct nvc_regulator *, char *
);

/* device access functions */
extern int camera_dev_parser(
	struct camera_device *, u32, u32, struct camera_seq_status *
);
extern int camera_dev_wr_table(
	struct camera_device *, struct camera_reg *, struct camera_seq_status *
);
extern int camera_dev_rd_table(
	struct camera_device *, struct camera_reg *
);

/* edp functions */
void camera_edp_register(
	struct camera_device *
);
int camera_edp_req(
	struct camera_device *, unsigned
);
void camera_edp_lowest(
	struct camera_device *
);

/* debugfs functions */
extern int camera_debugfs_init(
	struct camera_platform_info *
);
extern int camera_debugfs_remove(void);

#endif

#endif
#endif
/* __CAMERA_H__ */
