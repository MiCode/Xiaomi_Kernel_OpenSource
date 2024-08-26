// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2023 Huaqin Technology(Shanghai) Co., Ltd.
 */

#ifndef __LINUX_HUAQIN_LED_CLASS_H__
#define __LINUX_HUAQIN_LED_CLASS_H__

enum SUBPMIC_LED_ID {
	SUBPMIC_LED1 = 1,
	SUBPMIC_LED2,
	SUBPMIC_LEDMAX,
};

struct subpmic_led_dev;
struct subpmic_led_ops {
	int (*set_led_flash_curr)(struct subpmic_led_dev *subpmic_led,enum SUBPMIC_LED_ID index, int ma);
	int (*set_led_flash_time)(struct subpmic_led_dev *subpmic_led,enum SUBPMIC_LED_ID index, int ma);
	int (*set_led_flash_enable)(struct subpmic_led_dev *subpmic_led,enum SUBPMIC_LED_ID index, bool en);
	int (*set_led_torch_curr)(struct subpmic_led_dev *subpmic_led,enum SUBPMIC_LED_ID index, int ma);
	int (*set_led_torch_enable)(struct subpmic_led_dev *subpmic_led,enum SUBPMIC_LED_ID index, bool en);
};

struct subpmic_led_dev {
	struct device dev;
	char *name;
	void *private;
	struct subpmic_led_ops *ops;
};

struct subpmic_led_dev *subpmic_led_find_dev_by_name(const char *name);
struct subpmic_led_dev *subpmic_led_register(char *name, struct device *parent,
							struct subpmic_led_ops *ops, void *private);
void *subpmic_led_get_private(struct subpmic_led_dev *led);
int subpmic_led_unregister(struct subpmic_led_dev *led);


int subpmic_camera_set_led_flash_curr(struct subpmic_led_dev *subpmic_led,enum SUBPMIC_LED_ID index, int ma);
int subpmic_camera_set_led_flash_time(struct subpmic_led_dev *subpmic_led,enum SUBPMIC_LED_ID index, int ms);
int subpmic_camera_set_led_flash_enable(struct subpmic_led_dev *subpmic_led,enum SUBPMIC_LED_ID index, bool en);
int subpmic_camera_set_led_torch_curr(struct subpmic_led_dev *subpmic_led,enum SUBPMIC_LED_ID index, int ma);
int subpmic_camera_set_led_torch_enable(struct subpmic_led_dev *subpmic_led,enum SUBPMIC_LED_ID index, bool en);

#endif /* __LINUX_HUAQIN_LED__CLASS_H__ */
