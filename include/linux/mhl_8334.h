/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MHL_MSM_H__
#define __MHL_MSM_H__

#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/mhl_devcap.h>
#include <linux/power_supply.h>
#include <linux/mhl_defs.h>

#define MHL_DEVICE_NAME "sii8334"
#define MHL_DRIVER_NAME "sii8334"

#define HPD_UP               1
#define HPD_DOWN             0

enum discovery_result_enum {
	MHL_DISCOVERY_RESULT_USB = 0,
	MHL_DISCOVERY_RESULT_MHL,
};

struct msc_command_struct {
	u8 command;
	u8 offset;
	u8 length;
	u8 retry;
	union {
		u8 data[16];
		u8 *burst_data;
	} payload;
	u8 retval;
};

struct scrpd_struct {
	u8 offset;
	u8 length;
	u8 data[MHL_SCRATCHPAD_SIZE];
};

/* MHL 8334 supports a max HD pixel clk of 75 MHz */
#define MAX_MHL_PCLK 75000

/* USB driver interface  */

#if defined(CONFIG_FB_MSM_HDMI_MHL_8334)
 /*  mhl_device_discovery */
extern int mhl_device_discovery(const char *name, int *result);

/* - register|unregister MHL cable plug callback. */
extern int mhl_register_callback
	(const char *name, void (*callback)(int online));
extern int mhl_unregister_callback(const char *name);
#else
static inline int mhl_device_discovery(const char *name, int *result)
{
	return -ENODEV;
}

static inline int
	mhl_register_callback(const char *name, void (*callback)(int online))
{
	return -ENODEV;
}

static inline int mhl_unregister_callback(const char *name)
{
	return -ENODEV;
}
#endif

struct msc_cmd_envelope {
	/*
	 * this list head is for list APIs
	 */
	struct list_head msc_queue_envelope;
	struct msc_command_struct msc_cmd_msg;
};

struct mhl_msm_state_t {
	struct i2c_client *i2c_client;
	struct i2c_driver *i2c_driver;
	uint8_t      cur_state;
	uint8_t chip_rev_id;
	struct msm_mhl_platform_data *mhl_data;
	/* Device Discovery stuff */
	int mhl_mode;
	struct completion rgnd_done;
	struct completion msc_cmd_done;
	uint16_t devcap_state;
	uint8_t path_en_state;
	struct work_struct mhl_msc_send_work;
	struct list_head list_cmd;
	void (*msc_command_put_work) (struct msc_command_struct *);
	struct msc_command_struct* (*msc_command_get_work) (void);
};

#ifdef CONFIG_FB_MSM_MDSS_HDMI_MHL_SII8334
enum mhl_gpio_type {
	MHL_TX_RESET_GPIO,
	MHL_TX_INTR_GPIO,
	MHL_TX_PMIC_PWR_GPIO,
	MHL_TX_MAX_GPIO,
};

enum mhl_vreg_type {
	MHL_TX_3V_VREG,
	MHL_TX_MAX_VREG,
};


struct mhl_tx_platform_data {
	/* Data filled from device tree nodes */
	struct dss_gpio *gpios[MHL_TX_MAX_GPIO];
	struct dss_vreg *vregs[MHL_TX_MAX_VREG];
	int irq;
	struct platform_device *hdmi_pdev;
};

struct mhl_tx_ctrl {
	struct platform_device *pdev;
	struct mhl_tx_platform_data *pdata;
	struct i2c_client *i2c_handle;
	uint8_t cur_state;
	uint8_t chip_rev_id;
	int mhl_mode;
	struct completion rgnd_done;
	void (*notify_usb_online)(void *ctx, int online);
	void *notify_ctx;
	struct usb_ext_notification *mhl_info;
	bool disc_enabled;
	struct power_supply mhl_psy;
	bool vbus_active;
	int current_val;
	struct completion msc_cmd_done;
	uint8_t devcap[16];
	uint16_t devcap_state;
	uint8_t status[2];
	uint8_t path_en_state;
	uint8_t tmds_en_state;
	void *hdmi_mhl_ops;
	struct work_struct mhl_msc_send_work;
	struct list_head list_cmd;
	struct input_dev *input;
	struct workqueue_struct *msc_send_workqueue;
	struct workqueue_struct *mhl_workq;
	struct work_struct mhl_intr_work;
	u16 *rcp_key_code_tbl;
	size_t rcp_key_code_tbl_len;
	struct scrpd_struct scrpd;
	int scrpd_busy;
	int wr_burst_pending;
	struct completion req_write_done;
	spinlock_t lock;
	bool tx_powered_off;
	uint8_t dwnstream_hpd;
	bool mhl_det_discon;
	bool irq_req_done;
};

int mhl_i2c_reg_read(struct i2c_client *client,
		     uint8_t slave_addr_index, uint8_t reg_offset);
int mhl_i2c_reg_write(struct i2c_client *client,
		      uint8_t slave_addr_index, uint8_t reg_offset,
		      uint8_t value);
void mhl_i2c_reg_modify(struct i2c_client *client,
			uint8_t slave_addr_index, uint8_t reg_offset,
			uint8_t mask, uint8_t val);

#endif /* CONFIG_FB_MSM_MDSS_HDMI_MHL_SII8334 */

enum {
	TX_PAGE_TPI          = 0x00,
	TX_PAGE_L0           = 0x01,
	TX_PAGE_L1           = 0x02,
	TX_PAGE_2            = 0x03,
	TX_PAGE_3            = 0x04,
	TX_PAGE_CBUS         = 0x05,
	TX_PAGE_DDC_EDID     = 0x06,
	TX_PAGE_DDC_SEGM     = 0x07,
};

enum mhl_st_type {
	POWER_STATE_D0_NO_MHL = 0,
	POWER_STATE_D0_MHL    = 2,
	POWER_STATE_D3        = 3,
};

enum {
	DEV_PAGE_TPI_0      = (0x72),
	DEV_PAGE_TX_L0_0    = (0x72),
	DEV_PAGE_TPI_1      = (0x76),
	DEV_PAGE_TX_L0_1    = (0x76),
	DEV_PAGE_TX_L1_0    = (0x7A),
	DEV_PAGE_TX_L1_1    = (0x7E),
	DEV_PAGE_TX_2_0     = (0x92),
	DEV_PAGE_TX_2_1     = (0x96),
	DEV_PAGE_TX_3_0	    = (0x9A),
	DEV_PAGE_TX_3_1	    = (0x9E),
	DEV_PAGE_CBUS       = (0xC8),
	DEV_PAGE_DDC_EDID   = (0xA0),
	DEV_PAGE_DDC_SEGM   = (0x60),
};

#define MHL_SII_PAGE0_RD(off) \
	mhl_i2c_reg_read(client, TX_PAGE_L0, off)
#define MHL_SII_PAGE0_WR(off, val) \
	mhl_i2c_reg_write(client, TX_PAGE_L0, off, val)
#define MHL_SII_PAGE0_MOD(off, mask, val)		\
	mhl_i2c_reg_modify(client, TX_PAGE_L0, off, mask, val)


#define MHL_SII_PAGE1_RD(off) \
	mhl_i2c_reg_read(client, TX_PAGE_L1, off)
#define MHL_SII_PAGE1_WR(off, val) \
	mhl_i2c_reg_write(client, TX_PAGE_L1, off, val)
#define MHL_SII_PAGE1_MOD(off, mask, val) \
	mhl_i2c_reg_modify(client, TX_PAGE_L1, off, mask, val)


#define MHL_SII_PAGE2_RD(off) \
	mhl_i2c_reg_read(client, TX_PAGE_2, off)
#define MHL_SII_PAGE2_WR(off, val) \
	mhl_i2c_reg_write(client, TX_PAGE_2, off, val)
#define MHL_SII_PAGE2_MOD(off, mask, val) \
	mhl_i2c_reg_modify(client, TX_PAGE_2, off, mask, val)


#define MHL_SII_PAGE3_RD(off) \
	mhl_i2c_reg_read(client, TX_PAGE_3, off)
#define MHL_SII_PAGE3_WR(off, val) \
	mhl_i2c_reg_write(client, TX_PAGE_3, off, val)
#define MHL_SII_PAGE3_MOD(off, mask, val)		\
	mhl_i2c_reg_modify(client, TX_PAGE_3, off, mask, val)

#define MHL_SII_CBUS_RD(off) \
	mhl_i2c_reg_read(client, TX_PAGE_CBUS, off)
#define MHL_SII_CBUS_WR(off, val) \
	mhl_i2c_reg_write(client, TX_PAGE_CBUS, off, val)
#define MHL_SII_CBUS_MOD(off, mask, val) \
	mhl_i2c_reg_modify(client, TX_PAGE_CBUS, off, mask, val)

#define REG_SRST        ((TX_PAGE_3 << 16) | 0x0000)
#define REG_INTR1       ((TX_PAGE_L0 << 16) | 0x0071)
#define REG_INTR1_MASK  ((TX_PAGE_L0 << 16) | 0x0075)
#define REG_INTR2       ((TX_PAGE_L0 << 16) | 0x0072)
#define REG_TMDS_CCTRL  ((TX_PAGE_L0 << 16) | 0x0080)

#define REG_DISC_CTRL1	((TX_PAGE_3 << 16) | 0x0010)
#define REG_DISC_CTRL2	((TX_PAGE_3 << 16) | 0x0011)
#define REG_DISC_CTRL3	((TX_PAGE_3 << 16) | 0x0012)
#define REG_DISC_CTRL4	((TX_PAGE_3 << 16) | 0x0013)
#define REG_DISC_CTRL5	((TX_PAGE_3 << 16) | 0x0014)
#define REG_DISC_CTRL6	((TX_PAGE_3 << 16) | 0x0015)
#define REG_DISC_CTRL7	((TX_PAGE_3 << 16) | 0x0016)
#define REG_DISC_CTRL8	((TX_PAGE_3 << 16) | 0x0017)
#define REG_DISC_CTRL9	((TX_PAGE_3 << 16) | 0x0018)
#define REG_DISC_CTRL10	((TX_PAGE_3 << 16) | 0x0019)
#define REG_DISC_CTRL11	((TX_PAGE_3 << 16) | 0x001A)
#define REG_DISC_STAT	((TX_PAGE_3 << 16) | 0x001B)
#define REG_DISC_STAT2	((TX_PAGE_3 << 16) | 0x001C)

#define REG_INT_CTRL	((TX_PAGE_3 << 16) | 0x0020)
#define REG_INTR4		((TX_PAGE_3 << 16) | 0x0021)
#define REG_INTR4_MASK	((TX_PAGE_3 << 16) | 0x0022)
#define REG_INTR5		((TX_PAGE_3 << 16) | 0x0023)
#define REG_INTR5_MASK	((TX_PAGE_3 << 16) | 0x0024)

#define REG_MHLTX_CTL1	((TX_PAGE_3 << 16) | 0x0030)
#define REG_MHLTX_CTL2	((TX_PAGE_3 << 16) | 0x0031)
#define REG_MHLTX_CTL3	((TX_PAGE_3 << 16) | 0x0032)
#define REG_MHLTX_CTL4	((TX_PAGE_3 << 16) | 0x0033)
#define REG_MHLTX_CTL5	((TX_PAGE_3 << 16) | 0x0034)
#define REG_MHLTX_CTL6	((TX_PAGE_3 << 16) | 0x0035)
#define REG_MHLTX_CTL7	((TX_PAGE_3 << 16) | 0x0036)
#define REG_MHLTX_CTL8	((TX_PAGE_3 << 16) | 0x0037)

#define REG_TMDS_CSTAT	((TX_PAGE_3 << 16) | 0x0040)

#define REG_CBUS_INTR_STATUS            ((TX_PAGE_CBUS << 16) | 0x0008)
#define REG_CBUS_INTR_ENABLE            ((TX_PAGE_CBUS << 16) | 0x0009)

#define REG_DDC_ABORT_REASON            ((TX_PAGE_CBUS << 16) | 0x000B)
#define REG_CBUS_BUS_STATUS             ((TX_PAGE_CBUS << 16) | 0x000A)
#define REG_PRI_XFR_ABORT_REASON        ((TX_PAGE_CBUS << 16) | 0x000D)
#define REG_CBUS_PRI_FWR_ABORT_REASON   ((TX_PAGE_CBUS << 16) | 0x000E)
#define REG_CBUS_PRI_START              ((TX_PAGE_CBUS << 16) | 0x0012)
#define REG_CBUS_PRI_ADDR_CMD           ((TX_PAGE_CBUS << 16) | 0x0013)
#define REG_CBUS_PRI_WR_DATA_1ST        ((TX_PAGE_CBUS << 16) | 0x0014)
#define REG_CBUS_PRI_WR_DATA_2ND        ((TX_PAGE_CBUS << 16) | 0x0015)
#define REG_CBUS_PRI_RD_DATA_1ST        ((TX_PAGE_CBUS << 16) | 0x0016)
#define REG_CBUS_PRI_RD_DATA_2ND        ((TX_PAGE_CBUS << 16) | 0x0017)
#define REG_CBUS_PRI_VS_CMD             ((TX_PAGE_CBUS << 16) | 0x0018)
#define REG_CBUS_PRI_VS_DATA            ((TX_PAGE_CBUS << 16) | 0x0019)
#define	REG_CBUS_MSC_RETRY_INTERVAL		((TX_PAGE_CBUS << 16) | 0x001A)
#define	REG_CBUS_DDC_FAIL_LIMIT			((TX_PAGE_CBUS << 16) | 0x001C)
#define	REG_CBUS_MSC_FAIL_LIMIT			((TX_PAGE_CBUS << 16) | 0x001D)
#define	REG_CBUS_MSC_INT2_STATUS        ((TX_PAGE_CBUS << 16) | 0x001E)
#define REG_CBUS_MSC_INT2_ENABLE        ((TX_PAGE_CBUS << 16) | 0x001F)
#define	REG_MSC_WRITE_BURST_LEN         ((TX_PAGE_CBUS << 16) | 0x0020)
#define	REG_MSC_HEARTBEAT_CONTROL       ((TX_PAGE_CBUS << 16) | 0x0021)
#define REG_MSC_TIMEOUT_LIMIT           ((TX_PAGE_CBUS << 16) | 0x0022)
#define	REG_CBUS_LINK_CONTROL_1			((TX_PAGE_CBUS << 16) | 0x0030)
#define	REG_CBUS_LINK_CONTROL_2			((TX_PAGE_CBUS << 16) | 0x0031)
#define	REG_CBUS_LINK_CONTROL_3			((TX_PAGE_CBUS << 16) | 0x0032)
#define	REG_CBUS_LINK_CONTROL_4			((TX_PAGE_CBUS << 16) | 0x0033)
#define	REG_CBUS_LINK_CONTROL_5			((TX_PAGE_CBUS << 16) | 0x0034)
#define	REG_CBUS_LINK_CONTROL_6			((TX_PAGE_CBUS << 16) | 0x0035)
#define	REG_CBUS_LINK_CONTROL_7			((TX_PAGE_CBUS << 16) | 0x0036)
#define REG_CBUS_LINK_STATUS_1          ((TX_PAGE_CBUS << 16) | 0x0037)
#define REG_CBUS_LINK_STATUS_2          ((TX_PAGE_CBUS << 16) | 0x0038)
#define	REG_CBUS_LINK_CONTROL_8			((TX_PAGE_CBUS << 16) | 0x0039)
#define	REG_CBUS_LINK_CONTROL_9			((TX_PAGE_CBUS << 16) | 0x003A)
#define	REG_CBUS_LINK_CONTROL_10		((TX_PAGE_CBUS << 16) | 0x003B)
#define	REG_CBUS_LINK_CONTROL_11		((TX_PAGE_CBUS << 16) | 0x003C)
#define	REG_CBUS_LINK_CONTROL_12		((TX_PAGE_CBUS << 16) | 0x003D)


#define REG_CBUS_LINK_CTRL9_0			((TX_PAGE_CBUS << 16) | 0x003A)
#define REG_CBUS_LINK_CTRL9_1           ((TX_PAGE_CBUS << 16) | 0x00BA)

#define	REG_CBUS_DRV_STRENGTH_0			((TX_PAGE_CBUS << 16) | 0x0040)
#define	REG_CBUS_DRV_STRENGTH_1			((TX_PAGE_CBUS << 16) | 0x0041)
#define	REG_CBUS_ACK_CONTROL			((TX_PAGE_CBUS << 16) | 0x0042)
#define	REG_CBUS_CAL_CONTROL			((TX_PAGE_CBUS << 16) | 0x0043)

#define REG_CBUS_SCRATCHPAD_0           ((TX_PAGE_CBUS << 16) | 0x00C0)
#define REG_CBUS_DEVICE_CAP_0           ((TX_PAGE_CBUS << 16) | 0x0080)
#define REG_CBUS_DEVICE_CAP_1           ((TX_PAGE_CBUS << 16) | 0x0081)
#define REG_CBUS_DEVICE_CAP_2           ((TX_PAGE_CBUS << 16) | 0x0082)
#define REG_CBUS_DEVICE_CAP_3           ((TX_PAGE_CBUS << 16) | 0x0083)
#define REG_CBUS_DEVICE_CAP_4           ((TX_PAGE_CBUS << 16) | 0x0084)
#define REG_CBUS_DEVICE_CAP_5           ((TX_PAGE_CBUS << 16) | 0x0085)
#define REG_CBUS_DEVICE_CAP_6           ((TX_PAGE_CBUS << 16) | 0x0086)
#define REG_CBUS_DEVICE_CAP_7           ((TX_PAGE_CBUS << 16) | 0x0087)
#define REG_CBUS_DEVICE_CAP_8           ((TX_PAGE_CBUS << 16) | 0x0088)
#define REG_CBUS_DEVICE_CAP_9           ((TX_PAGE_CBUS << 16) | 0x0089)
#define REG_CBUS_DEVICE_CAP_A           ((TX_PAGE_CBUS << 16) | 0x008A)
#define REG_CBUS_DEVICE_CAP_B           ((TX_PAGE_CBUS << 16) | 0x008B)
#define REG_CBUS_DEVICE_CAP_C           ((TX_PAGE_CBUS << 16) | 0x008C)
#define REG_CBUS_DEVICE_CAP_D           ((TX_PAGE_CBUS << 16) | 0x008D)
#define REG_CBUS_DEVICE_CAP_E           ((TX_PAGE_CBUS << 16) | 0x008E)
#define REG_CBUS_DEVICE_CAP_F           ((TX_PAGE_CBUS << 16) | 0x008F)
#define REG_CBUS_SET_INT_0              ((TX_PAGE_CBUS << 16) | 0x00A0)
#define REG_CBUS_SET_INT_1		((TX_PAGE_CBUS << 16) | 0x00A1)
#define REG_CBUS_SET_INT_2		((TX_PAGE_CBUS << 16) | 0x00A2)
#define REG_CBUS_SET_INT_3		((TX_PAGE_CBUS << 16) | 0x00A3)
#define REG_CBUS_WRITE_STAT_0           ((TX_PAGE_CBUS << 16) | 0x00B0)
#define REG_CBUS_WRITE_STAT_1           ((TX_PAGE_CBUS << 16) | 0x00B1)
#define REG_CBUS_WRITE_STAT_2           ((TX_PAGE_CBUS << 16) | 0x00B2)
#define REG_CBUS_WRITE_STAT_3           ((TX_PAGE_CBUS << 16) | 0x00B3)

#define GET_PAGE(x) ((x) >> 16)
#define GET_OFF(x) ((x) & 0xffff)


#define MHL_SII_REG_NAME_RD(arg)\
	mhl_i2c_reg_read(client, GET_PAGE(arg), GET_OFF(arg))
#define MHL_SII_REG_NAME_WR(arg, val)\
	mhl_i2c_reg_write(client, GET_PAGE(arg), GET_OFF(arg), val)
#define MHL_SII_REG_NAME_MOD(arg, mask, val)\
	mhl_i2c_reg_modify(client, GET_PAGE(arg), GET_OFF(arg), mask, val)

#endif /* __MHL_MSM_H__ */
