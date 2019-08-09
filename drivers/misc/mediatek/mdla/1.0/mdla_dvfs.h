/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _MDLA_DVFS_H_
#define _MDLA_DVFS_H_

#include "mdla.h"
#include "mdla_ioctl.h"
#include <linux/of_platform.h>
#include <linux/seq_file.h>

/* ++++++++++++++++++++++++++++++++++*/
/* |opp_index  |   mdla frequency  |        power             */
/* ------------------------------------------*/
/* |      0         |   788 MHz          |        336 mA           */
/* ------------------------------------------*/
/* |      1         |   700 MHz          |        250 mA           */
/* ------------------------------------------*/
/* |      2         |   624 MHz          |        221 mA           */
/* ------------------------------------------*/
/* |      3         |   606 MHz          |        208 mA           */
/* ------------------------------------------*/
/* |      4         |   594 MHz          |        140 mA           */
/* ------------------------------------------*/
/* |      5         |   546 MHz          |        120 mA           */
/* ------------------------------------------*/
/* |      6         |   525 MHz          |        114 mA           */
/* ------------------------------------------*/
/* |      7         |   450 MHz          |         84 mA           */
/* ------------------------------------------*/
/* |      8         |   416 MHz          |        336 mA           */
/* ------------------------------------------*/
/* |      9         |   364 MHz          |        250 mA           */
/* ------------------------------------------*/
/* |      10         |   312 MHz          |        221 mA           */
/* ------------------------------------------*/
/* |      11         |   273 MHz          |        208 mA           */
/* ------------------------------------------*/
/* |      12         |   208 MHz          |        140 mA           */
/* ------------------------------------------*/
/* |      13         |   137 MHz          |        120 mA           */
/* ------------------------------------------*/
/* |      14         |   52 MHz          |        114 mA           */
/* ------------------------------------------*/
/* |      15         |   26 MHz          |        114 mA           */
/* ------------------------------------------*/
/* ++++++++++++++++++++++++++++++++++*/

enum MDLA_OPP_INDEX {
	MDLA_OPP_0 = 0,
	MDLA_OPP_1 = 1,
	MDLA_OPP_2 = 2,
	MDLA_OPP_3 = 3,
	MDLA_OPP_4 = 4,
	MDLA_OPP_5 = 5,
	MDLA_OPP_6 = 6,
	MDLA_OPP_7 = 7,
	MDLA_OPP_8 = 8,
	MDLA_OPP_9 = 9,
	MDLA_OPP_10 = 10,
	MDLA_OPP_11 = 11,
	MDLA_OPP_12 = 12,
	MDLA_OPP_NUM
};

struct MDLA_OPP_INFO {
	enum MDLA_OPP_INDEX opp_index;
	int power;	/*mW*/
};

#define MDLA_MAX_NUM_STEPS               (16)
#define MDLA_MAX_NUM_OPPS                (16)
//#define MTK_MDLA_CORE (1)
#define MTK_MDLA_USER (2)
#define MTK_VPU_CORE_NUM (2)
struct mdla_dvfs_steps {
	uint32_t values[MDLA_MAX_NUM_STEPS];
	uint8_t count;
	uint8_t index;
	uint8_t opp_map[MDLA_MAX_NUM_OPPS];
};

struct mdla_dvfs_opps {
	struct mdla_dvfs_steps vcore;
	struct mdla_dvfs_steps vvpu;
	struct mdla_dvfs_steps vmdla;
	struct mdla_dvfs_steps dsp;	/* ipu_conn */
	struct mdla_dvfs_steps dspcore[MTK_VPU_CORE_NUM];	/* ipu_core# */
	struct mdla_dvfs_steps mdlacore;	/* ipu_core# */
	struct mdla_dvfs_steps ipu_if;	/* ipusys_vcore, interface */
	uint8_t index;
	uint8_t count;
};

enum mdlaPowerOnType {
	/* power on previously by setPower */
	MDLA_PRE_ON		= 1,

	/* power on by enque */
	MDLA_ENQUE_ON	= 2,

	/* power on by enque, but want to immediately off(when exception) */
	MDLA_IMT_OFF		= 3,
};

/*3 prioritys of cmd*/
#define MDLA_REQ_MAX_NUM_PRIORITY 3

extern struct MDLA_OPP_INFO mdla_power_table[MDLA_OPP_NUM];
int32_t mdla_thermal_en_throttle_cb(uint8_t vmdla_opp, uint8_t mdla_opp);
int32_t mdla_thermal_dis_throttle_cb(void);
int mdla_quick_suspend(int core);
int mdla_get_opp(void);
int get_mdlacore_opp(void);
int get_mdla_platform_floor_opp(void);
int get_mdla_ceiling_opp(void);
int get_mdla_opp_to_freq(uint8_t step);
void mdla_put_power(int core);
int mdla_get_power(int core);
void mdla_opp_check(int core, uint8_t vmdla_index, uint8_t freq_index);

#ifndef MTK_MDLA_FPGA_PORTING
int mdla_init_hw(int core, struct platform_device *pdev);
int mdla_uninit_hw(void);
int mdla_set_power_parameter(uint8_t param, int argc, int *args);
int mdla_dump_power(struct seq_file *s);
int mdla_dump_opp_table(struct seq_file *s);

long mdla_dvfs_ioctl(struct file *filp, unsigned int command,
		unsigned long arg);
int mdla_dvfs_cmd_start(struct command_entry *ce);
int mdla_dvfs_cmd_end_info(struct command_entry *ce);
int mdla_dvfs_cmd_end_shutdown(void);

#else

static inline
int mdla_init_hw(int core, struct platform_device *pdev)
{
	return 0;
}

static inline
int mdla_uninit_hw(void)
{
	return 0;
}

static inline
int mdla_set_power_parameter(uint8_t param, int argc, int *args)
{
	return 0;
}
static inline
int mdla_dump_power(struct seq_file *s)
{
	return 0;
}
static inline
int mdla_dump_opp_table(struct seq_file *s)
{
	return 0;
}

static inline
long mdla_dvfs_ioctl(struct file *filp, unsigned int command,
		unsigned long arg)
{
	return 0;
}

static inline
int mdla_dvfs_cmd_start(struct command_entry *ce, struct list_head *cmd_list)
{
	return 0;
}

static inline
int mdla_dvfs_cmd_end_info(struct command_entry *ce)
{
	return 0;
}

static inline
int mdla_dvfs_cmd_end_shutdown(void)
{
	return 0;
}

#endif

#endif

