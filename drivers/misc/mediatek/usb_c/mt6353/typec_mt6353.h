/*
 * Copyright (C) 2016 MediaTek Inc.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef _TYPEC_H
#define _TYPEC_H

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/bitops.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/random.h>
#include <linux/jiffies.h>
#include <linux/kthread.h>

#include <asm/irq.h>
#include <asm/byteorder.h>
#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_tcq.h>
#include <scsi/scsi_dbg.h>
#include <scsi/scsi_eh.h>

#include <linux/cdev.h>

/*pmic*/
#include <mt-plat/upmu_common.h>
#include <mt-plat/mt_pmic_wrap.h>
/*pmic*/

/**************************************************************************/

#define MAX_SIZE 80
#define TYPEC "typec"

/**************************************************************************/

/* configurations */

/* platform dependent */
#define FPGA_PLATFORM 0 /* set to 1 if V7 FPGA is used */
#define USE_AUXADC 0 /* set to 1 if AUXADC is used */
#define CC_STANDALONE_COMPLIANCE 0 /* set to 1 use CC only. No related to USB function */

/* enable SNK<->ACC, SRC<->ACC transitions */
#define ENABLE_ACC 1

/* debug */
#define DBG_PROBE 0 /* set to 1 if SW debug probe is used */
#define DBG_STATIC 0
#define TYPEC_CLI 0

/**************************************************************************/
#include "typec-test.h"
#include "typec_reg.h"
#include "pd.h"

/**************************************************************************/

/* macros */
#define ZERO_INDEXED_VAL(val) ((val) - 1)
#define DIV_AND_RND_UP(dividend, divider) (((dividend) + (divider) - 1) / (divider))
#define ZERO_INDEXED_DIV_AND_RND_UP(dividdend, divider) (ZERO_INDEXED_VAL(DIV_AND_RND_UP((dividdend), (divider))))

/**************************************************************************/

/* TYPEC configurations */

/* when set, signals vbus off when VSAFE_0V is 1 */
/* when not set, signals vbus off when VSAFE_5V is 0 */
/* on FPGA, VSAFE5V is 1 when voltage is greater than 4.8v; VSAFE0V is 1 when voltage is smaller than 0.8v */
#define DETECT_VSAFE_0V 1

/* reference clock */
#if FPGA_PLATFORM
#define REF_CLK_DIVIDEND 234 /* 23.4k */
#define REF_CLK_DIVIDER 10
#else
#define REF_CLK_DIVIDEND 750 /* 75k */
#define REF_CLK_DIVIDER 10
#endif

/* DAC reference voltage */
#if FPGA_PLATFORM
#define SRC_VRD_DEFAULT_DAC_VAL 3
#define SRC_VOPEN_DEFAULT_DAC_VAL 30
#define SRC_VRD_15_DAC_VAL 5
#define SRC_VOPEN_15_DAC_VAL 30
#define SRC_VRD_30_DAC_VAL 15
#define SRC_VOPEN_30_DAC_VAL 50

#define SNK_VRPUSB_DAC_VAL 3
#define SNK_VRP15_DAC_VAL 13
#define SNK_VRP30_DAC_VAL 24
#else
#define SRC_VRD_DEFAULT_DAC_VAL 2
#define SRC_VOPEN_DEFAULT_DAC_VAL 37
#define SRC_VRD_15_DAC_VAL 7
#define SRC_VOPEN_15_DAC_VAL 37
#define SRC_VRD_30_DAC_VAL 17
#define SRC_VOPEN_30_DAC_VAL 62

#define SNK_VRPUSB_DAC_VAL 2
#define SNK_VRP15_DAC_VAL 14
#define SNK_VRP30_DAC_VAL 29
#endif

#if USE_AUXADC
/* AUXADC reference voltage */
#define AUXADC_INTERVAL_MS 10
#define AUXADC_DEBOUNCE_TIME 2

#define AUXADC_EVENT_INTERVAL_MS 10

#define AUXADC_VOLTAGE_MV 1800
#define AUXADC_VOLTAGE_SCALE 4096

#define SNK_VRPUSB_VTH_MV 340
#define SNK_VRP15_VTH_MV 640
#define SNK_VRPUSB_AUXADC_MIN_VAL 0
#define SNK_VRPUSB_AUXADC_MAX_VAL DIV_AND_RND_UP((SNK_VRPUSB_VTH_MV * AUXADC_VOLTAGE_MV), AUXADC_VOLTAGE_SCALE)
#define SNK_VRP15_AUXADC_MIN_VAL SNK_VRPUSB_AUXADC_MAX_VAL
#define SNK_VRP15_AUXADC_MAX_VAL DIV_AND_RND_UP((SNK_VRP15_VTH_MV * AUXADC_VOLTAGE_MV), AUXADC_VOLTAGE_SCALE)
#define SNK_VRP30_AUXADC_MIN_VAL SNK_VRP15_AUXADC_MAX_VAL
#define SNK_VRP30_AUXADC_MAX_VAL (AUXADC_VOLTAGE_SCALE - 1)
#endif

/* timing */
#define CC_VOL_PERIODIC_MEAS_VAL 10 /* 10ms */
#define DRP_SRC_CNT_VAL 37.5 /* 37.5ms */
#define DRP_SNK_CNT_VAL 37.5 /* 37.5ms */
#define DRP_TRY_CNT_VAL 100 /* 100ms */
#define DRP_TRY_WAIT_CNT_VAL 400 /* 400ms */

#define POLLING_INTERVAL_MS 5

/**************************************************************************/

/* debug related */
enum enum_typec_dbg_lvl {
	TYPEC_DBG_LVL_0 = 0, /* nothing */
	TYPEC_DBG_LVL_1 = 1, /* important interrupt dump, critical events */
	TYPEC_DBG_LVL_2 = 2, /* nice to have dump */
	TYPEC_DBG_LVL_3 = 3, /* full interrupt dump, verbose information dump */
};

/**************************************************************************/

/* definitions */

#define TYPE_C_INTR_EN_0_ATTACH_MSK (REG_TYPE_C_CC_ENT_ATTACH_SRC_INTR_EN | REG_TYPE_C_CC_ENT_ATTACH_SNK_INTR_EN)
#define TYPE_C_INTR_EN_0_MSK (TYPE_C_INTR_EN_0_ATTACH_MSK |\
	REG_TYPE_C_CC_ENT_AUDIO_ACC_INTR_EN | REG_TYPE_C_CC_ENT_DBG_ACC_INTR_EN | REG_TYPE_C_CC_ENT_DISABLE_INTR_EN |\
	REG_TYPE_C_CC_ENT_ATTACH_WAIT_SRC_INTR_EN |\
	REG_TYPE_C_CC_ENT_ATTACH_WAIT_SNK_INTR_EN | REG_TYPE_C_CC_ENT_TRY_SRC_INTR_EN |\
	REG_TYPE_C_CC_ENT_TRY_WAIT_SNK_INTR_EN | REG_TYPE_C_CC_ENT_ATTACH_WAIT_ACC_INTR_EN)
#define TYPE_C_INTR_EN_2_MSK (REG_TYPE_C_CC_ENT_SNK_PWR_IDLE_INTR_EN | REG_TYPE_C_CC_ENT_SNK_PWR_DEFAULT_INTR_EN |\
	REG_TYPE_C_CC_ENT_SNK_PWR_15_INTR_EN | REG_TYPE_C_CC_ENT_SNK_PWR_30_INTR_EN |\
	REG_TYPE_C_CC_ENT_SNK_PWR_REDETECT_INTR_EN)
#define TYPE_C_INTR_DRP_TOGGLE (REG_TYPE_C_CC_ENT_UNATTACH_SNK_INTR_EN | REG_TYPE_C_CC_ENT_UNATTACH_SRC_INTR_EN)
#define TYPE_C_INTR_ACC_TOGGLE (REG_TYPE_C_CC_ENT_UNATTACH_SNK_INTR_EN | REG_TYPE_C_CC_ENT_UNATTACH_ACC_INTR_EN)

#define TYPEC_TERM_CC (REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_RCC1 | REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_RCC2)
#define TYPEC_TERM_CC1 (REG_TYPE_C_SW_FORCE_MODE_DA_CC_RPCC1_EN | REG_TYPE_C_SW_FORCE_MODE_DA_CC_RDCC1_EN |\
	REG_TYPE_C_SW_FORCE_MODE_DA_CC_RACC1_EN)
#define TYPEC_TERM_CC2 (REG_TYPE_C_SW_FORCE_MODE_DA_CC_RPCC2_EN | REG_TYPE_C_SW_FORCE_MODE_DA_CC_RDCC2_EN |\
	REG_TYPE_C_SW_FORCE_MODE_DA_CC_RACC2_EN)
#define TYPEC_ACC_EN (REG_TYPE_C_ACC_EN | REG_TYPE_C_AUDIO_ACC_EN | REG_TYPE_C_DEBUG_ACC_EN)
#define TYPEC_TRY (REG_TYPE_C_ATTACH_SRC_2_TRY_WAIT_SNK_ST_EN | REG_TYPE_C_TRY_SRC_ST_EN)

/* enumerations */

enum enum_typec_role {
	TYPEC_ROLE_SINK = 0,
	TYPEC_ROLE_SOURCE = 1,
	TYPEC_ROLE_DRP = 2,
	TYPEC_ROLE_RESERVED = 3,
	TYPEC_ROLE_SINK_W_ACTIVE_CABLE = 4,
	TYPEC_ROLE_ACCESSORY_AUDIO = 5,
	TYPEC_ROLE_ACCESSORY_DEBUG = 6,
	TYPEC_ROLE_OPEN = 7
};

enum enum_typec_rp {
	TYPEC_RP_DFT = 0, /* 36K ohm */
	TYPEC_RP_15A = 1, /* 12K ohm */
	TYPEC_RP_30A = 2, /* 4.7K ohm */
	TYPEC_RP_RESERVED = 3
};

enum enum_typec_term {
	TYPEC_TERM_RP = 0,
	TYPEC_TERM_RD = 1,
	TYPEC_TERM_RA = 2,
	TYPEC_TERM_NA = 3
};

enum enum_try_mode {
	TYPEC_TRY_DISABLE = 0,
	TYPEC_TRY_ENABLE = 1,
};

enum enum_typec_state {
	TYPEC_STATE_DISABLED = 0,
	TYPEC_STATE_UNATTACHED_SRC = 1,
	TYPEC_STATE_ATTACH_WAIT_SRC = 2,
	TYPEC_STATE_ATTACHED_SRC = 3,
	TYPEC_STATE_UNATTACHED_SNK = 4,
	TYPEC_STATE_ATTACH_WAIT_SNK = 5,
	TYPEC_STATE_ATTACHED_SNK = 6,
	TYPEC_STATE_TRY_SRC = 7,
	TYPEC_STATE_TRY_WAIT_SNK = 8,
	TYPEC_STATE_UNATTACHED_ACCESSORY = 9,
	TYPEC_STATE_ATTACH_WAIT_ACCESSORY = 10,
	TYPEC_STATE_AUDIO_ACCESSORY = 11,
	TYPEC_STATE_DEBUG_ACCESSORY = 12,
};

enum enum_vbus_lvl {
	TYPEC_VSAFE_5V = 0,
	TYPEC_VSAFE_0V = 1,
};

/**************************************************************************/

/* structures */

/**
 * struct typec_hba - per adapter private structure
 * @mmio_base: base register address
 * @dev: device handle
 * @irq: Irq number of the controller
 * @ee_ctrl_mask: Exception event control mask
 * @feh_workq: Work queue for fatal controller error handling
 * @eeh_work: Worker to handle exception events
 * @id: device id
 * @support_role: controller role
 * @power_role: power role
 */
struct typec_hba {
	void __iomem *mmio_base;
	struct device *dev;
	struct cdev cdev;
	unsigned int irq;
	int id;

	struct mutex ioctl_lock;
	spinlock_t typec_lock;

	struct work_struct wait_vbus_on_attach_wait_snk;
	struct work_struct wait_vbus_on_try_wait_snk;
	struct work_struct wait_vbus_off_attached_snk;
	struct work_struct wait_vbus_off_then_drive_attached_src;
	struct work_struct drive_vbus_work;
	struct work_struct platform_hanlder_work;
	#if USE_AUXADC
	struct work_struct auxadc_voltage_mon_attached_snk;
	uint8_t auxadc_event;
	#endif

	enum enum_typec_role support_role;
	enum enum_typec_rp rp_val;
	enum enum_typec_dbg_lvl dbg_lvl;

	uint8_t vbus_en;
	uint8_t vbus_det_en;
	uint8_t vbus_present;
	uint8_t vconn_en;

	#if SUPPORT_PD
	uint16_t cc_is0;
	uint16_t pd_is0;
	uint16_t pd_is1;

	uint8_t pd_comm_enabled;
	uint32_t bist_mode;

	/* current port power role (SOURCE or SINK) */
	uint8_t power_role;
	/* current port data role (DFP or UFP) */
	uint8_t data_role;
	/* port flags, see PD_FLAGS_* */
	uint16_t flags;

	/* PD state for port */
	enum pd_states task_state;
	/* PD state when we run state handler the last time */
	enum pd_states last_state;
	/* The state to go to after timeout */
	enum pd_states timeout_state;
	/* Timeout for the current state. Set to 0 for no timeout. */
	unsigned long timeout_jiffies;
	unsigned long timeout_ms;
	unsigned long timeout_user;
	/* Time for source recovery after hard reset */
	unsigned long src_recover;

	struct completion tx_event;
	struct completion event;
	struct completion ready;

	/* status of last transmit */
	/* uint8_t tx_status;*/
	/* last requested voltage PDO index */
	int requested_idx;
	#ifdef CONFIG_USB_PD_DUAL_ROLE
	/* Current limit / voltage based on the last request message */
	uint32_t curr_limit;
	uint32_t supply_voltage;
	/* Signal charging update that affects the port */
	int new_power_request;
	/* Store previously requested voltage request */
	int prev_request_mv;
	#endif

	/* PD state for Vendor Defined Messages */
	enum vdm_states vdm_state;
	/* Timeout for the current vdm state.  Set to 0 for no timeout. */
	/*timestamp_t vdm_timeout;*/
	/* next Vendor Defined Message to send */
	uint32_t vdo_data[VDO_MAX_SIZE];
	uint8_t vdo_count;
	/* VDO to retry if UFP responder replied busy. */
	uint32_t vdo_retry;

	#if PD_SW_WORKAROUND1_1
	uint16_t header;
	uint32_t payload[7];
	#endif
	#endif
};

struct bit_mapping {
	uint16_t mask;
	char name[MAX_SIZE];
};

struct reg_mapping {
	uint16_t addr;
	uint16_t mask;
	uint16_t ofst;
	char name[MAX_SIZE];
};

/**************************************************************************/

/* read/write function calls */

#if FPGA_PLATFORM
static inline void typec_writew(struct typec_hba *hba, uint16_t val, unsigned int reg)
{
	writew(val, hba->mmio_base + reg);
}

static inline uint16_t typec_readw(struct typec_hba *hba, unsigned int reg)
{
	return readw(hba->mmio_base + reg);
}
#else
static inline void typec_writew(struct typec_hba *hba, uint16_t val, unsigned int reg)
{
	unsigned int ret = 0;
	unsigned int mask = 0xFFFF;
	unsigned int shift = 0;

	ret = pmic_config_interface_nolock((unsigned int)(MT6353_PMIC_REG_BASE + reg),
			     (unsigned int)(val),
			     mask,
			     shift);

	pmic_read_interface_nolock((unsigned int)(MT6353_PMIC_REG_BASE + reg),
			   (&ret),
			   mask,
			   shift
			       );
}

static inline uint16_t typec_readw(struct typec_hba *hba, unsigned int reg)
{
	unsigned int ret = 0;
	unsigned int val = 0;
	unsigned int mask = 0xFFFF;
	unsigned int shift = 0;

	ret = pmic_read_interface_nolock((unsigned int)(MT6353_PMIC_REG_BASE + reg),
			   (&val),
			   mask,
			   shift);

	return val;
}
#endif

static inline void typec_writew_msk(struct typec_hba *hba, uint16_t msk, uint16_t val, unsigned int reg)
{
	uint16_t tmp;

	tmp = typec_readw(hba, reg);
	tmp = (tmp & ~msk) | (val & msk);
	typec_writew(hba, tmp, reg);
}
static inline void typec_set(struct typec_hba *hba, uint16_t val, unsigned int reg)
{
	uint16_t tmp;

	tmp = typec_readw(hba, reg);
	typec_writew(hba, tmp | val, reg);
}

static inline void typec_clear(struct typec_hba *hba, uint16_t val, unsigned int reg)
{
	uint16_t tmp;

	tmp = typec_readw(hba, reg);
	typec_writew(hba, tmp & ~val, reg);
}

/**************************************************************************/

/* SW probe */

#define DBG_TIMER0_VAL_OFST 20
#define DBG_VBUS_DET_EN_OFST 19
#define DBG_VCONN_EN_OFST 18
#define DBG_VBUS_EN_OFST 17
#define DBG_VBUS_PRESENT_OFST 16
#define DBG_INTR_TIMER_EVENT_OFST 15
#define DBG_INTR_CC_EVENT_OFST 14
#define DBG_INTR_RX_EVENT_OFST 13
#define DBG_INTR_TX_EVENT_OFST 12
#define DBG_LOOP_STATE_OFST 8
#define DBG_INTR_STATE_OFST 6
#define DBG_PD_STATE_OFST 0

#define DBG_TIMER0_VAL (0xfff<<DBG_TIMER0_VAL_OFST)
#define DBG_VBUS_DET_EN (0x1<<DBG_VBUS_DET_EN_OFST)
#define DBG_VCONN_EN (0x1<<DBG_VCONN_EN_OFST)
#define DBG_VBUS_EN (0x1<<DBG_VBUS_EN_OFST)
#define DBG_VBUS_PRESENT (0x1<<DBG_VBUS_PRESENT_OFST)
#define DBG_INTR_TIMER_EVENT (0x1<<DBG_INTR_TIMER_EVENT_OFST)
#define DBG_INTR_CC_EVENT (0x1<<DBG_INTR_CC_EVENT_OFST)
#define DBG_INTR_RX_EVENT (0x1<<DBG_INTR_RX_EVENT_OFST)
#define DBG_INTR_TX_EVENT (0x1<<DBG_INTR_TX_EVENT_OFST)
#define DBG_LOOP_STATE (0xf<<DBG_LOOP_STATE_OFST)
#define DBG_INTR_STATE (0x3<<DBG_INTR_STATE_OFST)
#define DBG_PD_STATE (0x3f<<DBG_PD_STATE_OFST)

enum enum_intr_state {
	DBG_INTR_NONE = 0,
	DBG_INTR_CC = 1,
	DBG_INTR_PD = 2,
};

enum enum_loop_state {
	DBG_LOOP_NONE = 0,
	DBG_LOOP_WAIT = 1,
	DBG_LOOP_CHK_EVENT = 2,
	DBG_LOOP_RX = 3,
	DBG_LOOP_HARD_RESET = 4,
	DBG_LOOP_CHK_CC_EVENT = 5,
	DBG_LOOP_STATE_MACHINE = 6,
	DBG_LOOP_CHK_TIMEOUT = 7,
};

#if DBG_PROBE
static inline void typec_sw_probe(struct typec_hba *hba, uint32_t msk, uint32_t val)
{
	typec_writew_msk(hba, msk, val, TYPE_C_SW_DEBUG_PORT_0);
	typec_writew_msk(hba, (msk>>16), (val>>16), TYPE_C_SW_DEBUG_PORT_1);
}
#else
static inline void typec_sw_probe(struct typec_hba *hba, uint32_t msk, uint32_t val)
{
}
#endif


/**************************************************************************/
extern u32 upmu_get_rgs_chrdet(void);
extern bool upmu_is_chr_det(void);
#if defined(CONFIG_MTK_BQ25896_SUPPORT)
extern void bq25890_set_boost_ilim(unsigned int val);
extern void bq25890_otg_en(unsigned int val);
#endif


/**************************************************************************/

/* TYPEC function calls */

int typec_runtime_suspend(struct typec_hba *hba);
int typec_runtime_resume(struct typec_hba *hba);
int typec_runtime_idle(struct typec_hba *hba);

int typec_init(struct device *, struct typec_hba ** , void __iomem * , unsigned int , int);
void typec_remove(struct typec_hba *);

int typec_pltfrm_init(void);
void typec_pltfrm_exit(void);

/**************************************************************************/

#if SUPPORT_PD
/* PD function calls */
extern void pd_init(struct typec_hba *hba);
extern void pd_intr(struct typec_hba *hba, uint16_t pd_is0, uint16_t pd_is1, uint16_t cc_is0, uint16_t cc_is2);
#endif

/**************************************************************************/

#endif /* End of Header */
