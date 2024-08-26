/*
 *  Copyright (C) 2022 Nuvolta Co., Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __LINUX_SUBPMIC_NU6601_H
#define __LINUX_SUBPMIC_NU6601_H
#include "nu6601_reg.h"

struct nu6601_irq_desc {
	const char *name;
	irq_handler_t irq_handler;
	int irq;
};
#define NU6601_IRQDESC(name) { #name, nu6601_##name##_irq_handler, -1}

/* i2c id */
#define ADC_I2C  0
#define CHG_I2C  1
#define DPDM_I2C 2

enum nu6601_bc12_type {
	BC12_TYPE_NONE,
	BC12_TYPE_SDP = 0x1,
	BC12_TYPE_CDP,
	BC12_TYPE_DCP,
	BC12_TYPE_FLOAT = 0x05,
	BC12_TYPE_NONSTANDARD,
	BC12_TYPE_APPLE_5V1A,
	BC12_TYPE_APPLE_5V2A,
	BC12_TYPE_APPLE_5V2_4A,
	BC12_TYPE_SAMSUNG_5V2A,
	BC12_TYPE_OTHERS,
	BC12_TYPE_HVDCP,
	BC12_TYPE_HVDCP_30 ,
	BC12_TYPE_HVDCP_3_PLUS_18,
	BC12_TYPE_HVDCP_3_PLUS_27,
	BC12_TYPE_HVDCP_3_PLUS_40,
};

struct nu6601_charger_cfg {
	int     charge_current;
	int     precharge_current;
	int     input_curr_limit;
	int     iindpm_dis;

	int    batsns_en;
	bool    enable_term;
	int     term_current;
	int     charge_voltage;

	bool    enable_ico;
	int     rechg_dis;
	int     rechg_vol;
	int     otg_vol;
	int     otg_current;

	int cid_en;
};

struct nu6601_fled_config {
	int		torch_max_lvl;
	int		strobe_max_lvl;
	int		strobe_max_timeout;
};

enum CHG_STATE {
    CHG_STATE_NO_CHG = 0,
    CHG_STATE_TRICK,
    CHG_STATE_PRECHG,
    CHG_STATE_CC,
    CHG_STATE_CV,
    CHG_STATE_TERM,
};

enum CHIP_MODE {
    NORMAL_MODE = 0,
    LED_FLASH_MODE,
};

struct chip_state {
    bool online;
    bool boost_good;
    int vbus_type;
    int dcd_to;
    int chg_state;
    int vindpm;
};

struct nu6601_charger {
	struct device *dev;
    struct regmap *rmap;

	struct nu6601_charger_cfg chg_cfg;
    struct chip_state state;

    struct charger_dev *subpmic_dev;
    struct subpmic_led_dev *led_dev;
    struct soft_qc35 *qc;
    struct notifier_block qc35_result_nb;
	int qc3_enable;

	struct delayed_work dump_reg_work;
	struct delayed_work buck_fsw_work;
	/* N19A code for HQ-374473 by p-yanzelin at 2024/03/15 start */
	struct delayed_work first_cid_det_work;
	/* N19A code for HQ-374473 by p-yanzelin at 2024/03/15 end */
	bool vsys_min_work_run;
	bool in_otg;
	bool enable_irq;

	struct nu6601_fled_config fled_cfg;
    struct mutex fled_lock;
	struct delayed_work led_work;
    struct semaphore led_semaphore;
    struct completion led_complete;
    int led_index;

    unsigned long request_otg;

	int	part_no;
	int	revision;

    struct regulator *dpdm_reg;
    struct mutex dpdm_lock;
    int dpdm_enabled;
    int input_suspend_status;
};

extern bool get_nu2115_device_config(void);
extern void Charger_Detect_Init(void);
extern void Charger_Detect_Release(void);

#endif /* __LINUX_SUBPMIC_NU6601_H */
