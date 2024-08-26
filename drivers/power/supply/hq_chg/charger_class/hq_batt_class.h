// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2023 Huaqin Technology(Shanghai) Co., Ltd.
 */

#ifndef __LINUX_HUAQIN_HQ_BATT_H__
#define __LINUX_HUAQIN_HQ_BATT_H__


#define BATTERY_ID_UNKNOWN 0xff
#define BATTERY_NAME_UNKNOWN "UNKNOWN"
struct batt_info_dev;
struct batt_info_ops {
	int (*get_batt_id)(struct batt_info_dev *batt_info);
	char* (*get_batt_name)(struct batt_info_dev *batt_info);
	int (*get_chip_ok)(struct batt_info_dev *batt_info);
};

struct batt_info_dev {
	struct device dev;
	char *name;
	void *private;
	struct batt_info_ops *ops;
};

struct batt_info_dev *batt_info_find_dev_by_name(const char *name);
struct batt_info_dev *batt_info_register(char *name, struct device *parent,
							struct batt_info_ops *ops, void *private);
void *batt_info_get_private(struct batt_info_dev *info);
int batt_info_unregister(struct batt_info_dev *info);


int batt_info_get_batt_id(struct batt_info_dev *batt_info);
char * batt_info_get_batt_name(struct batt_info_dev *batt_info);
int  batt_info_get_chip_ok(struct batt_info_dev *batt_info);

#endif /* __LINUX_HUAQIN_BATT_CLASS_H__ */
