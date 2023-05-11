/*
 * max77729-private.h
 *
 * Copyrights (C) 2021 Maxim Integrated Products, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __LINUX_MFD_MAX77729_PRIV_H
#define __LINUX_MFD_MAX77729_PRIV_H

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/usb/typec/maxim/max77729.h>
#include <linux/mfd/max77729_common.h>
#define MAX77729_I2C_ADDR		(0x92)
#define MAX77729_REG_INVALID		(0xff)

#define MAX77729_IRQSRC_CHG		(1 << 0)
#define MAX77729_IRQSRC_TOP		(1 << 1)
#define MAX77729_IRQSRC_FG              (1 << 2)
#define MAX77729_IRQSRC_USBC		(1 << 3)

enum max77729_hw_rev {
	MAX77729_PASS1 = 0x1,
	MAX77729_PASS2 = 0x2,
	MAX77729_PASS3 = 0x3,
	MAX77729_PASS4 = 0x4,
	MAX77729_PASS5 = 0x5,
};

enum max77729_reg {
	/* Slave addr = 0xCC */
	/* PMIC Top-Level Registers */
	MAX77729_PMIC_REG_PMICID1		= 0x00,
	MAX77729_PMIC_REG_PMICREV		= 0x01,
	MAX77729_PMIC_REG_MAINCTRL1		= 0x02,
	MAX77729_PMIC_REG_INTSRC		= 0x22,
	MAX77729_PMIC_REG_INTSRC_MASK		= 0x23,
	MAX77729_PMIC_REG_SYSTEM_INT		= 0x24,
	MAX77729_PMIC_REG_RESERVED_25		= 0x25,
	MAX77729_PMIC_REG_SYSTEM_INT_MASK	= 0x26,
	MAX77729_PMIC_REG_RESERVED_27		= 0x27,
	MAX77729_PMIC_REG_RESERVED_28		= 0x28,
	MAX77729_PMIC_REG_RESERVED_29		= 0x29,
	MAX77729_PMIC_REG_BOOSTCONTROL1		= 0x4C,
	MAX77729_PMIC_REG_BSTOUT_MASK		= 0x03,
	MAX77729_PMIC_REG_BOOSTCONTROL2		= 0x4F,
	MAX77729_PMIC_REG_FORCE_EN_MASK		= 0x08,
	MAX77729_PMIC_REG_SW_RESET		= 0x50,
	MAX77729_PMIC_REG_USBC_RESET		= 0x51,

#if 0
	MAX77729_PMIC_REG_LSCNFG		= 0x2B,
	MAX77729_PMIC_REG_RESERVED_2C		= 0x2C,
	MAX77729_PMIC_REG_RESERVED_2D		= 0x2D,
#endif

	/* Haptic motor driver Registers */
	MAX77729_PMIC_REG_MCONFIG		= 0x10,
	MAX77729_PMIC_REG_MCONFIG2		= 0x11,

	MAX77729_CHG_REG_INT			= 0xB0,
	MAX77729_CHG_REG_INT_MASK		= 0xB1,
	MAX77729_CHG_REG_INT_OK			= 0xB2,
	MAX77729_CHG_REG_DETAILS_00		= 0xB3,
	MAX77729_CHG_REG_DETAILS_01		= 0xB4,
	MAX77729_CHG_REG_DETAILS_02		= 0xB5,
	MAX77729_CHG_REG_DTLS_03		= 0xB6,
	MAX77729_CHG_REG_CNFG_00		= 0xB7,
	MAX77729_CHG_REG_CNFG_01		= 0xB8,
	MAX77729_CHG_REG_CNFG_02		= 0xB9,
	MAX77729_CHG_REG_CNFG_03		= 0xBA,
	MAX77729_CHG_REG_CNFG_04		= 0xBB,
	MAX77729_CHG_REG_CNFG_05		= 0xBC,
	MAX77729_CHG_REG_CNFG_06		= 0xBD,
	MAX77729_CHG_REG_CNFG_07		= 0xBE,
	MAX77729_CHG_REG_CNFG_08		= 0xBF,
	MAX77729_CHG_REG_CNFG_09		= 0xC0,
	MAX77729_CHG_REG_CNFG_10		= 0xC1,
	MAX77729_CHG_REG_CNFG_11		= 0xC2,
	MAX77729_CHG_REG_CNFG_12		= 0xC3,
	MAX77729_CHG_REG_CNFG_13		= 0xC4,
	MAX77729_CHG_REG_CNFG_14		= 0xC5,
	MAX77729_CHG_REG_SAFEOUT_CTRL		= 0xC6,

	MAX77729_PMIC_REG_END,
};

/* Slave addr = 0x6C : Fuelgauge */
enum max77729_fuelgauge_reg {
	STATUS_REG				= 0x00,
	VALRT_THRESHOLD_REG			= 0x01,
	TALRT_THRESHOLD_REG			= 0x02,
	SALRT_THRESHOLD_REG			= 0x03,
	REMCAP_REP_REG				= 0x05,
	SOCREP_REG				= 0x06,
	AGES_REG				= 0x07,	
	TEMPERATURE_REG				= 0x08,
	VCELL_REG				= 0x09,
	TIME_TO_EMPTY_REG			= 0x11,
	FULLSOCTHR_REG				= 0x13,
	CURRENT_REG				= 0x0A,
	AVG_CURRENT_REG				= 0x0B,
	SOCMIX_REG				= 0x0D,
	SOCAV_REG				= 0x0E,
	REMCAP_MIX_REG				= 0x0F,
	FULLCAP_REG				= 0x10,
	RFAST_REG				= 0x15,
	AVR_TEMPERATURE_REG			= 0x16,
	CYCLES_REG				= 0x17,
	DESIGNCAP_REG				= 0x18,
	AVR_VCELL_REG				= 0x19,
	TIME_TO_FULL_REG			= 0x20,
	CONFIG_REG				= 0x1D,
	ICHGTERM_REG				= 0x1E,
	REMCAP_AV_REG				= 0x1F,
	FULLCAP_NOM_REG				= 0x23,
	LEARN_CFG_REG				= 0x28,
	FILTER_CFG_REG				= 0x29,
	MISCCFG_REG				= 0x2B,
	QRTABLE20_REG				= 0x32,
	FULLCAP_REP_REG				= 0x35,
	RCOMP_REG				= 0x38,
	TEMPCO_REG				= 0x39,
	VEMPTY_REG				= 0x3A,
	FSTAT_REG				= 0x3D,
	DISCHARGE_THRESHOLD_REG			= 0x40,
	QRTABLE30_REG				= 0x42,
	ISYS_REG				= 0x43,
	DQACC_REG				= 0x45,
	DPACC_REG				= 0x46,
	AVGISYS_REG				= 0x4B,
	QH_REG					= 0x4D,
	VSYS_REG				= 0xB1,
	TALRTTH2_REG				= 0xB2,
	/* "not used REG(0xB2)" is for checking fuelgague init result. */
	FG_INIT_RESULT_REG			= TALRTTH2_REG,
	VBYP_REG				= 0xB3,
	CONFIG2_REG				= 0xBB,
	IIN_REG					= 0xD0,
	OCV_REG					= 0xEE,
	VFOCV_REG				= 0xFB,
	VFSOC_REG				= 0xFF,

	MAX77729_FG_END,
};

#define MAX77729_REG_MAINCTRL1_BIASEN		(1 << 7)

/* Slave addr = 0x4A: USBC */
enum max77729_usbc_reg {
	MAX77729_USBC_REG_UIC_HW_REV		= 0x00,
	MAX77729_USBC_REG_UIC_FW_REV		= 0x01,
	MAX77729_USBC_REG_UIC_INT		= 0x02,
	MAX77729_USBC_REG_CC_INT		= 0x03,
	MAX77729_USBC_REG_PD_INT		= 0x04,
	MAX77729_USBC_REG_VDM_INT		= 0x05,
	MAX77729_USBC_REG_USBC_STATUS1		= 0x06,
	MAX77729_USBC_REG_USBC_STATUS2		= 0x07,
	MAX77729_USBC_REG_BC_STATUS		= 0x08,
	MAX77729_USBC_REG_CC_STATUS0		= 0x0a,
	MAX77729_USBC_REG_CC_STATUS1		= 0x0b,
	MAX77729_USBC_REG_PD_STATUS0		= 0x0c,
	MAX77729_USBC_REG_PD_STATUS1		= 0x0d,
	MAX77729_USBC_REG_UIC_INT_M		= 0x0e,
	MAX77729_USBC_REG_CC_INT_M		= 0x0f,
	MAX77729_USBC_REG_PD_INT_M		= 0x10,
	MAX77729_USBC_REG_VDM_INT_M		= 0x11,

	MAX77729_USBC_REG_AP_DATAOUT0		= 0x21,
	MAX77729_USBC_REG_AP_DATAOUT1		= 0x22,
	MAX77729_USBC_REG_AP_DATAOUT2		= 0x23,
	MAX77729_USBC_REG_AP_DATAOUT3		= 0x24,
	MAX77729_USBC_REG_AP_DATAOUT4		= 0x25,
	MAX77729_USBC_REG_AP_DATAOUT5		= 0x26,
	MAX77729_USBC_REG_AP_DATAOUT6		= 0x27,
	MAX77729_USBC_REG_AP_DATAOUT7		= 0x28,
	MAX77729_USBC_REG_AP_DATAOUT8		= 0x29,
	MAX77729_USBC_REG_AP_DATAOUT9		= 0x2a,
	MAX77729_USBC_REG_AP_DATAOUT10		= 0x2b,
	MAX77729_USBC_REG_AP_DATAOUT11		= 0x2c,
	MAX77729_USBC_REG_AP_DATAOUT12		= 0x2d,
	MAX77729_USBC_REG_AP_DATAOUT13		= 0x2e,
	MAX77729_USBC_REG_AP_DATAOUT14		= 0x2f,
	MAX77729_USBC_REG_AP_DATAOUT15		= 0x30,
	MAX77729_USBC_REG_AP_DATAOUT16		= 0x31,
	MAX77729_USBC_REG_AP_DATAOUT17		= 0x32,
	MAX77729_USBC_REG_AP_DATAOUT18		= 0x33,
	MAX77729_USBC_REG_AP_DATAOUT19		= 0x34,
	MAX77729_USBC_REG_AP_DATAOUT20		= 0x35,
	MAX77729_USBC_REG_AP_DATAOUT21		= 0x36,
	MAX77729_USBC_REG_AP_DATAOUT22		= 0x37,
	MAX77729_USBC_REG_AP_DATAOUT23		= 0x38,
	MAX77729_USBC_REG_AP_DATAOUT24		= 0x39,
	MAX77729_USBC_REG_AP_DATAOUT25		= 0x3a,
	MAX77729_USBC_REG_AP_DATAOUT26		= 0x3b,
	MAX77729_USBC_REG_AP_DATAOUT27		= 0x3c,
	MAX77729_USBC_REG_AP_DATAOUT28		= 0x3d,
	MAX77729_USBC_REG_AP_DATAOUT29		= 0x3e,
	MAX77729_USBC_REG_AP_DATAOUT30		= 0x3f,
	MAX77729_USBC_REG_AP_DATAOUT31		= 0x40,
	MAX77729_USBC_REG_AP_DATAOUT32		= 0x41,

	MAX77729_USBC_REG_AP_DATAIN0		= 0x51,
	MAX77729_USBC_REG_AP_DATAIN1		= 0x52,
	MAX77729_USBC_REG_AP_DATAIN2		= 0x53,
	MAX77729_USBC_REG_AP_DATAIN3		= 0x54,
	MAX77729_USBC_REG_AP_DATAIN4		= 0x55,
	MAX77729_USBC_REG_AP_DATAIN5		= 0x56,
	MAX77729_USBC_REG_AP_DATAIN6		= 0x57,
	MAX77729_USBC_REG_AP_DATAIN7		= 0x58,
	MAX77729_USBC_REG_AP_DATAIN8		= 0x59,
	MAX77729_USBC_REG_AP_DATAIN9		= 0x5a,
	MAX77729_USBC_REG_AP_DATAIN10		= 0x5b,
	MAX77729_USBC_REG_AP_DATAIN11		= 0x5c,
	MAX77729_USBC_REG_AP_DATAIN12		= 0x5d,
	MAX77729_USBC_REG_AP_DATAIN13		= 0x5e,
	MAX77729_USBC_REG_AP_DATAIN14		= 0x5f,
	MAX77729_USBC_REG_AP_DATAIN15		= 0x60,
	MAX77729_USBC_REG_AP_DATAIN16		= 0x61,
	MAX77729_USBC_REG_AP_DATAIN17		= 0x62,
	MAX77729_USBC_REG_AP_DATAIN18		= 0x63,
	MAX77729_USBC_REG_AP_DATAIN19		= 0x64,
	MAX77729_USBC_REG_AP_DATAIN20		= 0x65,
	MAX77729_USBC_REG_AP_DATAIN21		= 0x66,
	MAX77729_USBC_REG_AP_DATAIN22		= 0x67,
	MAX77729_USBC_REG_AP_DATAIN23		= 0x68,
	MAX77729_USBC_REG_AP_DATAIN24		= 0x69,
	MAX77729_USBC_REG_AP_DATAIN25		= 0x6a,
	MAX77729_USBC_REG_AP_DATAIN26		= 0x6b,
	MAX77729_USBC_REG_AP_DATAIN27		= 0x6c,
	MAX77729_USBC_REG_AP_DATAIN28		= 0x6d,
	MAX77729_USBC_REG_AP_DATAIN29		= 0x6e,
	MAX77729_USBC_REG_AP_DATAIN30		= 0x6f,
	MAX77729_USBC_REG_AP_DATAIN31		= 0x70,
	MAX77729_USBC_REG_AP_DATAIN32		= 0x71,

	MAX77729_USBC_REG_END,
};

/* Slave addr = 0x94: RGB LED */
enum max77729_led_reg {
	MAX77729_RGBLED_REG_LEDEN			= 0x30,
	MAX77729_RGBLED_REG_LED0BRT			= 0x31,
	MAX77729_RGBLED_REG_LED1BRT			= 0x32,
	MAX77729_RGBLED_REG_LED2BRT			= 0x33,
	MAX77729_RGBLED_REG_LED3BRT			= 0x34,
	MAX77729_RGBLED_REG_LEDRMP			= 0x36,
	MAX77729_RGBLED_REG_LEDBLNK			= 0x38,
	MAX77729_LED_REG_END,
};

enum max77729_irq_source {
	SYS_INT = 0,
	CHG_INT,
	FUEL_INT,
	USBC_INT,
	CC_INT,
	PD_INT,
	VDM_INT,
	VIR_INT,
	MAX77729_IRQ_GROUP_NR,
};

enum max77729_irq {
	/* PMIC; TOPSYS */
	MAX77729_SYSTEM_IRQ_BSTEN_INT,
	MAX77729_SYSTEM_IRQ_SYSUVLO_INT,
	MAX77729_SYSTEM_IRQ_SYSOVLO_INT,
	MAX77729_SYSTEM_IRQ_TSHDN_INT,
	MAX77729_SYSTEM_IRQ_TM_INT,

	/* PMIC; Charger */
	MAX77729_CHG_IRQ_BYP_I,
	MAX77729_CHG_IRQ_BATP_I,
	MAX77729_CHG_IRQ_BAT_I,
	MAX77729_CHG_IRQ_CHG_I,
	MAX77729_CHG_IRQ_WCIN_I,
	MAX77729_CHG_IRQ_CHGIN_I,
	MAX77729_CHG_IRQ_AICL_I,

	/* Fuelgauge */
	MAX77729_FG_IRQ_ALERT,

	/* USBC */
	MAX77729_USBC_IRQ_APC_INT,

	/* CC */
	MAX77729_CC_IRQ_VCONNCOP_INT,
	MAX77729_CC_IRQ_VSAFE0V_INT,
	MAX77729_CC_IRQ_DETABRT_INT,
	MAX77729_CC_IRQ_CCPINSTAT_INT,
	MAX77729_CC_IRQ_CCISTAT_INT,
	MAX77729_CC_IRQ_CCVCNSTAT_INT,
	MAX77729_CC_IRQ_CCSTAT_INT,

	MAX77729_USBC_IRQ_VBUS_INT,
	MAX77729_USBC_IRQ_VBADC_INT,
	MAX77729_USBC_IRQ_DCD_INT,
	MAX77729_USBC_IRQ_FAKVB_INT,
	MAX77729_USBC_IRQ_CHGT_INT,

	/*
	 * USBC: SYSMSG INT should be after CC INT
	 * because of 2 times of CC Sync INT at WDT reset
	 */
	MAX77729_USBC_IRQ_SYSM_INT,

	/* PD */
	MAX77729_PD_IRQ_PDMSG_INT,
	MAX77729_PD_IRQ_PS_RDY_INT,
	MAX77729_PD_IRQ_DATAROLE_INT,


	/* VDM */
	MAX77729_IRQ_VDM_DISCOVER_ID_INT,
	MAX77729_IRQ_VDM_DISCOVER_SVIDS_INT,
	MAX77729_IRQ_VDM_DISCOVER_MODES_INT,
	MAX77729_IRQ_VDM_ENTER_MODE_INT,
	MAX77729_IRQ_VDM_DP_STATUS_UPDATE_INT,
	MAX77729_IRQ_VDM_DP_CONFIGURE_INT,
	MAX77729_IRQ_VDM_ATTENTION_INT,

	/* VIRTUAL */
	MAX77729_VIR_IRQ_ALTERROR_INT,

	MAX77729_IRQ_NR,
};

struct max77729_dev {
	struct device *dev;
	struct i2c_client *i2c; /* 0xCC; Haptic, PMIC */
	struct i2c_client *charger; /* 0xD2; Charger */
	struct i2c_client *fuelgauge; /* 0x6C; Fuelgauge */
	struct i2c_client *muic; /* 0x4A; MUIC */
	struct i2c_client *debug; /* 0xC4; Debug */
	struct mutex i2c_lock;

	int type;

	int irq;
	int irq_base;
	int irq_gpio;
	bool wakeup;
	bool blocking_waterevent;
	int device_product_id;
	struct mutex irqlock;
	int irq_masks_cur[MAX77729_IRQ_GROUP_NR];
	int irq_masks_cache[MAX77729_IRQ_GROUP_NR];

	u8 HW_Revision;
	u8 FW_Revision;
	u8 FW_Minor_Revision;
	u8 FW_Product_ID;
	struct work_struct fw_work;
	struct workqueue_struct *fw_workqueue;
	struct completion fw_completion;
	int fw_update_state;
	int fw_size;

#ifdef CONFIG_HIBERNATION
	/* For hibernation */
	u8 reg_pmic_dump[MAX77729_PMIC_REG_END];
	u8 reg_muic_dump[MAX77729_USBC_REG_END];
	u8 reg_led_dump[MAX77729_LED_REG_END];
#endif

	/* pmic VER/REV register */
	u8 pmic_rev;	/* pmic Rev */
	u8 pmic_ver;	/* pmic version */

	u8 cc_booting_complete;

	int set_altmode_en;
	int enable_nested_irq;    
	u8 usbc_irq;

	bool suspended;
	wait_queue_head_t suspend_wait;

	/* QCOM_IFPMIC_SUSPEND */
	wait_queue_head_t queue_empty_wait_q;
	int doing_irq;
	int is_usbc_queue;
	bool (*check_usbc_opcode_queue) (void);

	void (*check_pdmsg)(void *data, u8 pdmsg);
	void *usbc_data;

	struct max77729_platform_data *pdata;
};

enum max77729_types {
	TYPE_MAX77729,
};

extern int max77729_irq_init(struct max77729_dev *max77729);
extern void max77729_irq_exit(struct max77729_dev *max77729);

/* MAX77729 shared i2c API function */
extern int max77729_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest);
extern int max77729_bulk_read(struct i2c_client *i2c, u8 reg, int count,
				u8 *buf);
extern int max77729_write_reg(struct i2c_client *i2c, u8 reg, u8 value);
extern int max77729_write_reg_nolock(struct i2c_client *i2c, u8 reg, u8 value);
extern int max77729_bulk_write(struct i2c_client *i2c, u8 reg, int count,
				u8 *buf);
extern int max77729_write_word(struct i2c_client *i2c, u8 reg, u16 value);
extern int max77729_read_word(struct i2c_client *i2c, u8 reg);

extern int max77729_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask);

/* MAX77729 check muic path function */
extern bool is_muic_usb_path_ap_usb(void);
extern bool is_muic_usb_path_cp_usb(void);

/* for charger api */
extern void max77729_hv_muic_charger_init(void);
extern int max77729_usbc_fw_update(struct max77729_dev *max77729, const u8 *fw_bin, int fw_bin_len, int enforce_do);
extern void max77729_usbc_fw_setting(struct max77729_dev *max77729, int enforce_do);
extern void max77729_register_pdmsg_func(struct max77729_dev *max77729,
	void (*check_pdmsg)(void *, u8), void *data);
#endif /* __LINUX_MFD_MAX77729_PRIV_H */

