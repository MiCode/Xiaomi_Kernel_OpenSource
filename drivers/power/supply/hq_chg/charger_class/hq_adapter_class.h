// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2023 Huaqin Technology(Shanghai) Co., Ltd.
 */
#ifndef __LINUX_HUAQIN_ADAPTER_CLASS_H__
#define __LINUX_HUAQIN_ADAPTER_CLASS_H__

#include <linux/list.h>

#define ADAPTER_CAP_MAX_NR 7

/**
 * sysfs
 */
enum ADAPTER_ATTR_NUM {
	ADAPTER_ATTR_HANDSHAKE = 0,
	ADAPTER_ATTR_GET_CAP,
	ADAPTER_ATTR_SET_CAP,
	ADAPTER_ATTR_SET_WDT,
	ADAPTER_ATTR_RESET,
};

enum adapter_cap_type {
	ADAPTER_FIX_CAP = 0,
	ADAPTER_APDO_CAP = 3,
};

struct adapter_cap {
	uint8_t cnt;
	uint8_t type[ADAPTER_CAP_MAX_NR];
	uint32_t volt_max[ADAPTER_CAP_MAX_NR];
	uint32_t volt_min[ADAPTER_CAP_MAX_NR];
	uint32_t curr_max[ADAPTER_CAP_MAX_NR];
	uint32_t curr_min[ADAPTER_CAP_MAX_NR];
};

struct adapter_dev;
struct adapter_ops {
	int (*handshake)(struct adapter_dev *);
	int (*get_cap)(struct adapter_dev *, struct adapter_cap *);
	int (*set_cap)(struct adapter_dev *, uint8_t, uint32_t, uint32_t);
	int (*set_wdt)(struct adapter_dev *, uint32_t ms);
	int (*reset)(struct adapter_dev *);
	int (*get_temp)(struct adapter_dev *, uint8_t *);
	int (*get_softreset)(struct adapter_dev *);
	int (*set_softreset)(struct adapter_dev *, bool);
};

struct adapter_dev {
	struct device dev;
	char *name;
	void *private;
	struct adapter_ops *ops;

	bool soft_reset;
};

int adapter_handshake(struct adapter_dev *adapter);
int adapter_get_cap(struct adapter_dev *adapter, struct adapter_cap *cap);
int adapter_set_cap(struct adapter_dev *adapter, uint8_t nr,
											uint32_t mv, uint32_t ma);
int adapter_get_temp(struct adapter_dev *adapter, uint8_t *temp);
int adapter_set_wdt(struct adapter_dev *adapter, uint32_t ms);
int adapter_reset(struct adapter_dev *adapter);
int adapter_get_softreset(struct adapter_dev *adapter);
int adapter_set_softreset(struct adapter_dev *adapter, bool val);

struct adapter_dev *adapter_find_dev_by_name(const char *name);
struct adapter_dev * adapter_register(char *name, struct device *parent,
							struct adapter_ops *ops, void *private);
void * adapter_get_private(struct adapter_dev *adapter);
int adapter_unregister(struct adapter_dev *adapter);

#endif /* __LINUX_HUAQIN_ADAPTER_CLASS_H__ */
