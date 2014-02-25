/* Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.
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
#include <linux/mfd/wcd9xxx/core-resource.h>


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

enum wcd9xxx_slim_slave_addr_type {
	WCD9XXX_SLIM_SLAVE_ADDR_TYPE_TABLA,
	WCD9XXX_SLIM_SLAVE_ADDR_TYPE_TAIKO,
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
	WCD9XXX_IRQ_VBAT_MONITOR_ATTACK,
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
	TAPAN_NUM_IRQS = WCD9XXX_NUM_IRQS,
	TOMTOM_NUM_IRQS = WCD9330_NUM_IRQS,
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
};

#define WCD9XXX_CH(xport, xshift) \
	{.port = xport, .shift = xshift}

enum wcd9xxx_chipid_major {
	TABLA_MAJOR = cpu_to_le16(0x100),
	SITAR_MAJOR = cpu_to_le16(0x101),
	TAIKO_MAJOR = cpu_to_le16(0x102),
	TAPAN_MAJOR = cpu_to_le16(0x103),
	TOMTOM_MAJOR = cpu_to_le16(0x105),
};

struct wcd9xxx_codec_type {
	u16 id_major;
	u16 id_minor;
	struct mfd_cell *dev;
	int size;
	int num_irqs;
	int version; /* -1 to retrive version from chip version register */
	enum wcd9xxx_slim_slave_addr_type slim_slave_type;
	u16 i2c_chip_status;
};

struct wcd9xxx {
	struct device *dev;
	struct slim_device *slim;
	struct slim_device *slim_slave;
	struct mutex io_lock;
	struct mutex xfer_lock;
	u8 version;

	int reset_gpio;

	int (*read_dev)(struct wcd9xxx *wcd9xxx, unsigned short reg,
			int bytes, void *dest, bool interface_reg);
	int (*write_dev)(struct wcd9xxx *wcd9xxx, unsigned short reg,
			int bytes, void *src, bool interface_reg);
	int (*dev_down)(struct wcd9xxx *wcd9xxx);
	int (*post_reset)(struct wcd9xxx *wcd9xxx);

	void *ssr_priv;
	bool slim_device_bootup;

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

	const struct wcd9xxx_codec_type *codec_type;
};

int wcd9xxx_interface_reg_read(struct wcd9xxx *wcd9xxx, unsigned short reg);
int wcd9xxx_interface_reg_write(struct wcd9xxx *wcd9xxx, unsigned short reg,
		u8 val);
int wcd9xxx_get_logical_addresses(u8 *pgd_la, u8 *inf_la);
int wcd9xxx_slim_write_repeat(struct wcd9xxx *wcd9xxx, unsigned short reg,
			     int bytes, void *src);

#if defined(CONFIG_WCD9310_CODEC) || \
	defined(CONFIG_WCD9304_CODEC) || \
	defined(CONFIG_WCD9320_CODEC) || \
	defined(CONFIG_WCD9306_CODEC)
int __init wcd9xxx_irq_of_init(struct device_node *node,
			       struct device_node *parent);
#else
static inline int __init wcd9xxx_irq_of_init(struct device_node *node,
			       struct device_node *parent)
{
	return 0;
}
#endif	/* CONFIG_OF */
static inline void wcd9xxx_reg_update(struct wcd9xxx *core,
				      unsigned short reg,
				      u8 mask, u8 val)
{
	u8 reg_val;

	if (core) {
		reg_val = wcd9xxx_reg_read(&core->core_res, reg);
		reg_val = (reg_val & ~mask) | (val & mask);
		wcd9xxx_reg_write(&core->core_res, reg, reg_val);
	}
}
#endif
