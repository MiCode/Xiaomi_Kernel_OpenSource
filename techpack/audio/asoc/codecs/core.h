/* Copyright (c) 2011-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MFD_TABLA_CORE_H__
#define __MFD_TABLA_CORE_H__

#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/pm_qos.h>

#define WCD9XXX_MAX_IRQ_REGS 4
#define WCD9XXX_MAX_NUM_IRQS (WCD9XXX_MAX_IRQ_REGS * 8)
#define WCD9XXX_SLIM_NUM_PORT_REG 3
#define TABLA_VERSION_1_0	0
#define TABLA_VERSION_1_1	1
#define TABLA_VERSION_2_0	2
#define TABLA_IS_1_X(ver) \
	(((ver == TABLA_VERSION_1_0) || (ver == TABLA_VERSION_1_1)) ? 1 : 0)
#define TABLA_IS_2_0(ver) ((ver == TABLA_VERSION_2_0) ? 1 : 0)

#define WCD9XXX_SUPPLY_BUCK_NAME "cdc-vdd-buck"

#define SITAR_VERSION_1P0 0
#define SITAR_VERSION_1P1 1
#define SITAR_IS_1P0(ver) \
	((ver == SITAR_VERSION_1P0) ? 1 : 0)
#define SITAR_IS_1P1(ver) \
	((ver == SITAR_VERSION_1P1) ? 1 : 0)

#define TAIKO_VERSION_1_0	1
#define TAIKO_IS_1_0(ver) \
	((ver == TAIKO_VERSION_1_0) ? 1 : 0)

#define TAPAN_VERSION_1_0	0
#define TAPAN_IS_1_0(ver) \
	((ver == TAPAN_VERSION_1_0) ? 1 : 0)

#define TOMTOM_VERSION_1_0	1
#define TOMTOM_IS_1_0(ver) \
	((ver == TOMTOM_VERSION_1_0) ? 1 : 0)

#define TASHA_VERSION_1_0     0
#define TASHA_VERSION_1_1     1
#define TASHA_VERSION_2_0     2

#define TASHA_IS_1_0(wcd) \
	((wcd->type == WCD9335 || wcd->type == WCD9326) ? \
	((wcd->version == TASHA_VERSION_1_0) ? 1 : 0) : 0)

#define TASHA_IS_1_1(wcd) \
	((wcd->type == WCD9335 || wcd->type == WCD9326) ? \
	((wcd->version == TASHA_VERSION_1_1) ? 1 : 0) : 0)

#define TASHA_IS_2_0(wcd) \
	((wcd->type == WCD9335 || wcd->type == WCD9326) ? \
	((wcd->version == TASHA_VERSION_2_0) ? 1 : 0) : 0)

/*
 * As fine version info cannot be retrieved before tavil probe.
 * Define three coarse versions for possible future use before tavil probe.
 */
#define TAVIL_VERSION_1_0             0
#define TAVIL_VERSION_1_1             1
#define TAVIL_VERSION_WCD9340_1_0     2
#define TAVIL_VERSION_WCD9341_1_0     3
#define TAVIL_VERSION_WCD9340_1_1     4
#define TAVIL_VERSION_WCD9341_1_1     5

#define TAVIL_IS_1_0(wcd) \
	((wcd->type == WCD934X) ? \
	 ((wcd->version == TAVIL_VERSION_1_0 || \
	   wcd->version == TAVIL_VERSION_WCD9340_1_0 || \
	   wcd->version == TAVIL_VERSION_WCD9341_1_0) ? 1 : 0) : 0)
#define TAVIL_IS_1_1(wcd) \
	((wcd->type == WCD934X) ? \
	 ((wcd->version == TAVIL_VERSION_1_1 || \
	   wcd->version == TAVIL_VERSION_WCD9340_1_1 || \
	   wcd->version == TAVIL_VERSION_WCD9341_1_1) ? 1 : 0) : 0)
#define TAVIL_IS_WCD9340_1_0(wcd) \
	((wcd->type == WCD934X) ? \
	 ((wcd->version == TAVIL_VERSION_WCD9340_1_0) ? 1 : 0) : 0)
#define TAVIL_IS_WCD9341_1_0(wcd) \
	((wcd->type == WCD934X) ? \
	 ((wcd->version == TAVIL_VERSION_WCD9341_1_0) ? 1 : 0) : 0)
#define TAVIL_IS_WCD9340_1_1(wcd) \
	((wcd->type == WCD934X) ? \
	 ((wcd->version == TAVIL_VERSION_WCD9340_1_1) ? 1 : 0) : 0)
#define TAVIL_IS_WCD9341_1_1(wcd) \
	((wcd->type == WCD934X) ? \
	 ((wcd->version == TAVIL_VERSION_WCD9341_1_1) ? 1 : 0) : 0)

#define IS_CODEC_TYPE(wcd, wcdtype) \
	((wcd->type == wcdtype) ? true : false)
#define IS_CODEC_VERSION(wcd, wcdversion) \
	((wcd->version == wcdversion) ? true : false)

enum {
	CDC_V_1_0,
	CDC_V_1_1,
	CDC_V_2_0,
};

enum codec_variant {
	WCD9XXX,
	WCD9330,
	WCD9335,
	WCD9326,
	WCD934X,
};

enum wcd9xxx_slim_slave_addr_type {
	WCD9XXX_SLIM_SLAVE_ADDR_TYPE_0,
	WCD9XXX_SLIM_SLAVE_ADDR_TYPE_1,
};

enum wcd9xxx_pm_state {
	WCD9XXX_PM_SLEEPABLE,
	WCD9XXX_PM_AWAKE,
	WCD9XXX_PM_ASLEEP,
};

enum {
	WCD9XXX_INTR_STATUS_BASE = 0,
	WCD9XXX_INTR_CLEAR_BASE,
	WCD9XXX_INTR_MASK_BASE,
	WCD9XXX_INTR_LEVEL_BASE,
	WCD9XXX_INTR_CLR_COMMIT,
	WCD9XXX_INTR_REG_MAX,
};

enum wcd9xxx_intf_status {
	WCD9XXX_INTERFACE_TYPE_PROBING,
	WCD9XXX_INTERFACE_TYPE_SLIMBUS,
	WCD9XXX_INTERFACE_TYPE_I2C,
};

enum {
	/* INTR_REG 0 */
	WCD9XXX_IRQ_SLIMBUS = 0,
	WCD9XXX_IRQ_MBHC_REMOVAL,
	WCD9XXX_IRQ_MBHC_SHORT_TERM,
	WCD9XXX_IRQ_MBHC_PRESS,
	WCD9XXX_IRQ_MBHC_RELEASE,
	WCD9XXX_IRQ_MBHC_POTENTIAL,
	WCD9XXX_IRQ_MBHC_INSERTION,
	WCD9XXX_IRQ_BG_PRECHARGE,
	/* INTR_REG 1 */
	WCD9XXX_IRQ_PA1_STARTUP,
	WCD9XXX_IRQ_PA2_STARTUP,
	WCD9XXX_IRQ_PA3_STARTUP,
	WCD9XXX_IRQ_PA4_STARTUP,
	WCD9306_IRQ_HPH_PA_OCPR_FAULT = WCD9XXX_IRQ_PA4_STARTUP,
	WCD9XXX_IRQ_PA5_STARTUP,
	WCD9XXX_IRQ_MICBIAS1_PRECHARGE,
	WCD9306_IRQ_HPH_PA_OCPL_FAULT = WCD9XXX_IRQ_MICBIAS1_PRECHARGE,
	WCD9XXX_IRQ_MICBIAS2_PRECHARGE,
	WCD9XXX_IRQ_MICBIAS3_PRECHARGE,
	/* INTR_REG 2 */
	WCD9XXX_IRQ_HPH_PA_OCPL_FAULT,
	WCD9XXX_IRQ_HPH_PA_OCPR_FAULT,
	WCD9XXX_IRQ_EAR_PA_OCPL_FAULT,
	WCD9XXX_IRQ_HPH_L_PA_STARTUP,
	WCD9XXX_IRQ_HPH_R_PA_STARTUP,
	WCD9320_IRQ_EAR_PA_STARTUP,
	WCD9306_IRQ_MBHC_JACK_SWITCH = WCD9320_IRQ_EAR_PA_STARTUP,
	WCD9310_NUM_IRQS,
	WCD9XXX_IRQ_RESERVED_0 = WCD9310_NUM_IRQS,
	WCD9XXX_IRQ_RESERVED_1,
	WCD9330_IRQ_SVASS_ERR_EXCEPTION = WCD9310_NUM_IRQS,
	WCD9330_IRQ_MBHC_JACK_SWITCH,
	/* INTR_REG 3 */
	WCD9XXX_IRQ_MAD_AUDIO,
	WCD9XXX_IRQ_MAD_ULTRASOUND,
	WCD9XXX_IRQ_MAD_BEACON,
	WCD9XXX_IRQ_SPEAKER_CLIPPING,
	WCD9320_IRQ_MBHC_JACK_SWITCH,
	WCD9306_NUM_IRQS,
	WCD9XXX_IRQ_VBAT_MONITOR_ATTACK = WCD9306_NUM_IRQS,
	WCD9XXX_IRQ_VBAT_MONITOR_RELEASE,
	WCD9XXX_NUM_IRQS,
	/* WCD9330 INTR1_REG 3*/
	WCD9330_IRQ_SVASS_ENGINE = WCD9XXX_IRQ_MAD_AUDIO,
	WCD9330_IRQ_MAD_AUDIO,
	WCD9330_IRQ_MAD_ULTRASOUND,
	WCD9330_IRQ_MAD_BEACON,
	WCD9330_IRQ_SPEAKER1_CLIPPING,
	WCD9330_IRQ_SPEAKER2_CLIPPING,
	WCD9330_IRQ_VBAT_MONITOR_ATTACK,
	WCD9330_IRQ_VBAT_MONITOR_RELEASE,
	WCD9330_NUM_IRQS,
	WCD9XXX_IRQ_RESERVED_2 = WCD9330_NUM_IRQS,
};

enum {
	TABLA_NUM_IRQS = WCD9310_NUM_IRQS,
	SITAR_NUM_IRQS = WCD9310_NUM_IRQS,
	TAIKO_NUM_IRQS = WCD9XXX_NUM_IRQS,
	TAPAN_NUM_IRQS = WCD9306_NUM_IRQS,
	TOMTOM_NUM_IRQS = WCD9330_NUM_IRQS,
};

struct intr_data {
	int intr_num;
	bool clear_first;
};

struct wcd9xxx_core_resource {
	struct mutex irq_lock;
	struct mutex nested_irq_lock;

	enum wcd9xxx_pm_state pm_state;
	struct mutex pm_lock;
	/* pm_wq notifies change of pm_state */
	wait_queue_head_t pm_wq;
	struct pm_qos_request pm_qos_req;
	int wlock_holders;


	/* holds the table of interrupts per codec */
	const struct intr_data *intr_table;
	int intr_table_size;
	unsigned int irq_base;
	unsigned int irq;
	u8 irq_masks_cur[WCD9XXX_MAX_IRQ_REGS];
	u8 irq_masks_cache[WCD9XXX_MAX_IRQ_REGS];
	bool irq_level_high[WCD9XXX_MAX_NUM_IRQS];
	int num_irqs;
	int num_irq_regs;
	u16 intr_reg[WCD9XXX_INTR_REG_MAX];
	struct regmap *wcd_core_regmap;

	/* Pointer to parent container data structure */
	void *parent;

	struct device *dev;
	struct irq_domain *domain;
};

/*
 * data structure for Slimbus and I2S channel.
 * Some of fields are only used in smilbus mode
 */
struct wcd9xxx_ch {
	u32 sph;		/* share channel handle - slimbus only	*/
	u32 ch_num;		/*
				 * vitrual channel number, such as 128 -144.
				 * apply for slimbus only
				 */
	u16 ch_h;		/* chanel handle - slimbus only */
	u16 port;		/*
				 * tabla port for RX and TX
				 * such as 0-9 for TX and 10 -16 for RX
				 * apply for both i2s and slimbus
				 */
	u16 shift;		/*
				 * shift bit for RX and TX
				 * apply for both i2s and slimbus
				 */
	struct list_head list;	/*
				 * channel link list
				 * apply for both i2s and slimbus
				 */
};

struct wcd9xxx_codec_dai_data {
	u32 rate;				/* sample rate          */
	u32 bit_width;				/* sit width 16,24,32   */
	struct list_head wcd9xxx_ch_list;	/* channel list         */
	u16 grph;				/* slimbus group handle */
	unsigned long ch_mask;
	wait_queue_head_t dai_wait;
	bool bus_down_in_recovery;
};

#define WCD9XXX_CH(xport, xshift) \
	{.port = xport, .shift = xshift}

enum wcd9xxx_chipid_major {
	TABLA_MAJOR = cpu_to_le16(0x100),
	SITAR_MAJOR = cpu_to_le16(0x101),
	TAIKO_MAJOR = cpu_to_le16(0x102),
	TAPAN_MAJOR = cpu_to_le16(0x103),
	TOMTOM_MAJOR = cpu_to_le16(0x105),
	TASHA_MAJOR = cpu_to_le16(0x0),
	TASHA2P0_MAJOR = cpu_to_le16(0x107),
	TAVIL_MAJOR = cpu_to_le16(0x108),
};

enum codec_power_states {
	WCD_REGION_POWER_COLLAPSE_REMOVE,
	WCD_REGION_POWER_COLLAPSE_BEGIN,
	WCD_REGION_POWER_DOWN,
};

enum wcd_power_regions {
	WCD9XXX_DIG_CORE_REGION_1,
	WCD9XXX_MAX_PWR_REGIONS,
};

struct wcd9xxx_codec_type {
	u16 id_major;
	u16 id_minor;
	struct mfd_cell *dev;
	int size;
	int num_irqs;
	int version; /* -1 to retrieve version from chip version register */
	enum wcd9xxx_slim_slave_addr_type slim_slave_type;
	u16 i2c_chip_status;
	const struct intr_data *intr_tbl;
	int intr_tbl_size;
	u16 intr_reg[WCD9XXX_INTR_REG_MAX];
};

struct wcd9xxx_power_region {
	enum codec_power_states power_state;
	u16 pwr_collapse_reg_min;
	u16 pwr_collapse_reg_max;
};

struct wcd9xxx {
	struct device *dev;
	struct slim_device *slim;
	struct slim_device *slim_slave;
	struct mutex io_lock;
	struct mutex xfer_lock;
	struct mutex reset_lock;
	u8 version;

	int reset_gpio;
	struct device_node *wcd_rst_np;

	int (*read_dev)(struct wcd9xxx *wcd9xxx, unsigned short reg,
			int bytes, void *dest, bool interface_reg);
	int (*write_dev)(struct wcd9xxx *wcd9xxx, unsigned short reg,
			int bytes, void *src, bool interface_reg);
	int (*multi_reg_write)(struct wcd9xxx *wcd9xxx, const void *data,
			       size_t count);
	int (*dev_down)(struct wcd9xxx *wcd9xxx);
	int (*post_reset)(struct wcd9xxx *wcd9xxx);

	void *ssr_priv;
	bool dev_up;

	u32 num_of_supplies;
	struct regulator_bulk_data *supplies;

	struct wcd9xxx_core_resource core_res;

	u16 id_minor;
	u16 id_major;

	/* Slimbus or I2S port */
	u32 num_rx_port;
	u32 num_tx_port;
	struct wcd9xxx_ch *rx_chs;
	struct wcd9xxx_ch *tx_chs;
	u32 mclk_rate;
	enum codec_variant type;
	struct regmap *regmap;

	struct wcd9xxx_codec_type *codec_type;
	bool prev_pg_valid;
	u8 prev_pg;
	u8 avoid_cdc_rstlow;
	struct wcd9xxx_power_region *wcd9xxx_pwr[WCD9XXX_MAX_PWR_REGIONS];
};

struct wcd9xxx_reg_val {
	unsigned short reg; /* register address */
	u8 *buf;            /* buffer to be written to reg. addr */
	int bytes;          /* number of bytes to be written */
};

int wcd9xxx_interface_reg_read(struct wcd9xxx *wcd9xxx, unsigned short reg);
int wcd9xxx_interface_reg_write(struct wcd9xxx *wcd9xxx, unsigned short reg,
		u8 val);
int wcd9xxx_get_logical_addresses(u8 *pgd_la, u8 *inf_la);
int wcd9xxx_slim_write_repeat(struct wcd9xxx *wcd9xxx, unsigned short reg,
			     int bytes, void *src);
int wcd9xxx_slim_reserve_bw(struct wcd9xxx *wcd9xxx,
			    u32 bw_ops, bool commit);
int wcd9xxx_set_power_state(struct wcd9xxx *wcd9xxx, enum codec_power_states,
			    enum wcd_power_regions);
int wcd9xxx_get_current_power_state(struct wcd9xxx *wcd9xxx,
				    enum wcd_power_regions);

int wcd9xxx_page_write(struct wcd9xxx *wcd9xxx, unsigned short *reg);

int wcd9xxx_slim_bulk_write(struct wcd9xxx *wcd9xxx,
			    struct wcd9xxx_reg_val *bulk_reg,
			    unsigned int size, bool interface);

extern int wcd9xxx_core_res_init(
	struct wcd9xxx_core_resource *wcd9xxx_core_res,
	int num_irqs, int num_irq_regs, struct regmap *wcd_regmap);

extern void wcd9xxx_core_res_deinit(
	struct wcd9xxx_core_resource *wcd9xxx_core_res);

extern int wcd9xxx_core_res_suspend(
	struct wcd9xxx_core_resource *wcd9xxx_core_res,
	pm_message_t pmesg);

extern int wcd9xxx_core_res_resume(
	struct wcd9xxx_core_resource *wcd9xxx_core_res);

extern int wcd9xxx_core_irq_init(
	struct wcd9xxx_core_resource *wcd9xxx_core_res);

extern int wcd9xxx_assign_irq(struct wcd9xxx_core_resource *wcd9xxx_core_res,
			      unsigned int irq,
			      unsigned int irq_base);

extern enum wcd9xxx_intf_status wcd9xxx_get_intf_type(void);
extern void wcd9xxx_set_intf_type(enum wcd9xxx_intf_status);

extern enum wcd9xxx_pm_state wcd9xxx_pm_cmpxchg(
			struct wcd9xxx_core_resource *wcd9xxx_core_res,
			enum wcd9xxx_pm_state o,
			enum wcd9xxx_pm_state n);
static inline int __init wcd9xxx_irq_of_init(struct device_node *node,
			       struct device_node *parent)
{
	return 0;
}

int wcd9xxx_init(void);
void wcd9xxx_exit(void);
#endif
