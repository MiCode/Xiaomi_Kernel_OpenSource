// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <soc/soundwire.h>
#include <soc/swr-common.h>
#include <linux/regmap.h>
#include <dsp/msm-audio-event-notify.h>
#include "swr-mstr-registers.h"
#include "swr-slave-registers.h"
#include <dsp/digital-cdc-rsc-mgr.h>
#include "swr-mstr-ctrl.h"
#include "swr-slave-port-config.h"

#define SWR_NUM_PORTS    4 /* TODO - Get this info from DT */

#define SWRM_FRAME_SYNC_SEL    4000 /* 4KHz */
#define SWRM_FRAME_SYNC_SEL_NATIVE 3675 /* 3.675KHz */

#define SWRM_PCM_OUT    0
#define SWRM_PCM_IN     1

#define SWRM_SYSTEM_RESUME_TIMEOUT_MS 700
#define SWRM_SYS_SUSPEND_WAIT 1

#define SWRM_DSD_PARAMS_PORT 4

#define SWR_BROADCAST_CMD_ID            0x0F
#define SWR_DEV_ID_MASK			0xFFFFFFFFFFFF
#define SWR_REG_VAL_PACK(data, dev, id, reg)	\
			((reg) | ((id) << 16) | ((dev) << 20) | ((data) << 24))

#define SWR_INVALID_PARAM 0xFF
#define SWR_HSTOP_MAX_VAL 0xF
#define SWR_HSTART_MIN_VAL 0x0

#define ERR_AUTO_SUSPEND_TIMER_VAL 0x1

#define SWRM_INTERRUPT_STATUS_MASK 0x1FDFD
#define SWRM_LINK_STATUS_RETRY_CNT 100

#define SWRM_ROW_48    48
#define SWRM_ROW_50    50
#define SWRM_ROW_64    64
#define SWRM_COL_02    02
#define SWRM_COL_16    16

#define SWRS_SCP_INT_STATUS_CLEAR_1 0x40
#define SWRS_SCP_INT_STATUS_MASK_1 0x41
#define SWRM_NUM_AUTO_ENUM_SLAVES    6

#define SWRM_MCP_SLV_STATUS_MASK    0x03
#define SWRM_ROW_CTRL_MASK    0xF8
#define SWRM_COL_CTRL_MASK    0x07
#define SWRM_CLK_DIV_MASK     0x700
#define SWRM_SSP_PERIOD_MASK  0xff0000
#define SWRM_NUM_PINGS_MASK   0x3E0000
#define SWRM_MCP_FRAME_CTRL_BANK_ROW_CTRL_SHFT    3
#define SWRM_MCP_FRAME_CTRL_BANK_COL_CTRL_SHFT    0
#define SWRM_MCP_FRAME_CTRL_BANK_CLK_DIV_VALUE_SHFT 8
#define SWRM_MCP_FRAME_CTRL_BANK_SSP_PERIOD_SHFT  16
#define SWRM_NUM_PINGS_POS    0x11

#define SWRM_DP_PORT_CTRL_EN_CHAN_SHFT    0x18
#define SWRM_DP_PORT_CTRL_OFFSET2_SHFT    0x10
#define SWRM_DP_PORT_CTRL_OFFSET1_SHFT    0x08

#define SWR_OVERFLOW_RETRY_COUNT 30

/* pm runtime auto suspend timer in msecs */
static int auto_suspend_timer = 500;
module_param(auto_suspend_timer, int, 0664);
MODULE_PARM_DESC(auto_suspend_timer, "timer for auto suspend");

enum {
	SWR_NOT_PRESENT, /* Device is detached/not present on the bus */
	SWR_ATTACHED_OK, /* Device is attached */
	SWR_ALERT,       /* Device alters master for any interrupts */
	SWR_RESERVED,    /* Reserved */
};

enum {
	MASTER_ID_WSA = 1,
	MASTER_ID_RX,
	MASTER_ID_TX
};

enum {
	ENABLE_PENDING,
	DISABLE_PENDING
};

enum {
	LPASS_HW_CORE,
	LPASS_AUDIO_CORE,
};

enum {
	SWRM_WR_CHECK_AVAIL,
	SWRM_RD_CHECK_AVAIL,
};

#define TRUE 1
#define FALSE 0

#define SWRM_MAX_PORT_REG    120
#define SWRM_MAX_INIT_REG    11

#define MAX_FIFO_RD_FAIL_RETRY 3

static bool swrm_lock_sleep(struct swr_mstr_ctrl *swrm);
static void swrm_unlock_sleep(struct swr_mstr_ctrl *swrm);
static u32 swr_master_read(struct swr_mstr_ctrl *swrm, unsigned int reg_addr);
static void swr_master_write(struct swr_mstr_ctrl *swrm, u16 reg_addr, u32 val);
static int swrm_runtime_resume(struct device *dev);

static u64 swrm_phy_dev[] = {
	0,
	0xd01170223,
	0x858350223,
	0x858350222,
	0x858350221,
	0x858350220,
};

static u8 swrm_get_device_id(struct swr_mstr_ctrl *swrm, u8 devnum)
{
	int i;

	for (i = 1; i < (swrm->num_dev + 1); i++) {
		if (swrm->logical_dev[devnum] == swrm_phy_dev[i])
			break;
	}

	if (i == (swrm->num_dev + 1)) {
		pr_info("%s: could not find the slave\n", __func__);
		i = devnum;
	}

	return i;
}

static u8 swrm_get_clk_div(int mclk_freq, int bus_clk_freq)
{
	int clk_div = 0;
	u8 div_val = 0;

	if (!mclk_freq || !bus_clk_freq)
		return 0;

	clk_div = (mclk_freq / bus_clk_freq);

	switch (clk_div) {
	case 32:
		div_val = 5;
		break;
	case 16:
		div_val = 4;
		break;
	case 8:
		div_val = 3;
		break;
	case 4:
		div_val = 2;
		break;
	case 2:
		div_val = 1;
		break;
	case 1:
	default:
		div_val = 0;
		break;
	}

	return div_val;
}

static bool swrm_is_msm_variant(int val)
{
	return (val == SWRM_VERSION_1_3);
}

#ifdef CONFIG_DEBUG_FS
static int swrm_debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static int get_parameters(char *buf, u32 *param1, int num_of_par)
{
	char *token;
	int base, cnt;

	token = strsep(&buf, " ");
	for (cnt = 0; cnt < num_of_par; cnt++) {
		if (token) {
			if ((token[1] == 'x') || (token[1] == 'X'))
				base = 16;
			else
				base = 10;

			if (kstrtou32(token, base, &param1[cnt]) != 0)
				return -EINVAL;

			token = strsep(&buf, " ");
		} else
			return -EINVAL;
	}
	return 0;
}

static ssize_t swrm_reg_show(struct swr_mstr_ctrl *swrm, char __user *ubuf,
			     size_t count, loff_t *ppos)
{
	int i, reg_val, len;
	ssize_t total = 0;
	char tmp_buf[SWR_MSTR_MAX_BUF_LEN];
	int rem = 0;

	if (!ubuf || !ppos)
		return 0;

	i = ((int) *ppos + SWRM_BASE);
	rem = i%4;

	if (rem)
		i = (i - rem);

	for (; i <= SWRM_MAX_REGISTER; i += 4) {
		usleep_range(100, 150);
		reg_val = swr_master_read(swrm, i);
		len = snprintf(tmp_buf, 25, "0x%.3x: 0x%.2x\n", i, reg_val);
		if (len < 0) {
			pr_err("%s: fail to fill the buffer\n", __func__);
			total = -EFAULT;
			goto copy_err;
		}
		if ((total + len) >= count - 1)
			break;
		if (copy_to_user((ubuf + total), tmp_buf, len)) {
			pr_err("%s: fail to copy reg dump\n", __func__);
			total = -EFAULT;
			goto copy_err;
		}
		*ppos += len;
		total += len;
	}

copy_err:
	return total;
}

static ssize_t swrm_debug_reg_dump(struct file *file, char __user *ubuf,
				size_t count, loff_t *ppos)
{
	struct swr_mstr_ctrl *swrm;

	if (!count || !file || !ppos || !ubuf)
		return -EINVAL;

	swrm = file->private_data;
	if (!swrm)
		return -EINVAL;

	if (*ppos < 0)
		return -EINVAL;

	return swrm_reg_show(swrm, ubuf, count, ppos);
}

static ssize_t swrm_debug_read(struct file *file, char __user *ubuf,
				size_t count, loff_t *ppos)
{
	char lbuf[SWR_MSTR_RD_BUF_LEN];
	struct swr_mstr_ctrl *swrm = NULL;

	if (!count || !file || !ppos || !ubuf)
		return -EINVAL;

	swrm = file->private_data;
	if (!swrm)
		return -EINVAL;

	if (*ppos < 0)
		return -EINVAL;

	snprintf(lbuf, sizeof(lbuf), "0x%x\n", swrm->read_data);

	return simple_read_from_buffer(ubuf, count, ppos, lbuf,
					       strnlen(lbuf, 7));
}

static ssize_t swrm_debug_peek_write(struct file *file, const char __user *ubuf,
				     size_t count, loff_t *ppos)
{
	char lbuf[SWR_MSTR_RD_BUF_LEN];
	int rc;
	u32 param[5];
	struct swr_mstr_ctrl *swrm = NULL;

	if (!count || !file || !ppos || !ubuf)
		return -EINVAL;

	swrm = file->private_data;
	if (!swrm)
		return -EINVAL;

	if (*ppos < 0)
		return -EINVAL;

	if (count > sizeof(lbuf) - 1)
		return -EINVAL;

	rc = copy_from_user(lbuf, ubuf, count);
	if (rc)
		return -EFAULT;

	lbuf[count] = '\0';
	rc = get_parameters(lbuf, param, 1);
	if ((param[0] <= SWRM_MAX_REGISTER) && (rc == 0))
		swrm->read_data = swr_master_read(swrm, param[0]);
	else
		rc = -EINVAL;

	if (rc == 0)
		rc = count;
	else
		dev_err(swrm->dev, "%s: rc = %d\n", __func__, rc);

	return rc;
}

static ssize_t swrm_debug_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	char lbuf[SWR_MSTR_WR_BUF_LEN];
	int rc;
	u32 param[5];
	struct swr_mstr_ctrl *swrm;

	if (!file || !ppos || !ubuf)
		return -EINVAL;

	swrm = file->private_data;
	if (!swrm)
		return -EINVAL;

	if (count > sizeof(lbuf) - 1)
		return -EINVAL;

	rc = copy_from_user(lbuf, ubuf, count);
	if (rc)
		return -EFAULT;

	lbuf[count] = '\0';
	rc = get_parameters(lbuf, param, 2);
	if ((param[0] <= SWRM_MAX_REGISTER) &&
		(param[1] <= 0xFFFFFFFF) &&
		(rc == 0))
		swr_master_write(swrm, param[0], param[1]);
	else
		rc = -EINVAL;

	if (rc == 0)
		rc = count;
	else
		pr_err("%s: rc = %d\n", __func__, rc);

	return rc;
}

static const struct file_operations swrm_debug_read_ops = {
	.open = swrm_debug_open,
	.write = swrm_debug_peek_write,
	.read = swrm_debug_read,
};

static const struct file_operations swrm_debug_write_ops = {
	.open = swrm_debug_open,
	.write = swrm_debug_write,
};

static const struct file_operations swrm_debug_dump_ops = {
	.open = swrm_debug_open,
	.read = swrm_debug_reg_dump,
};
#endif

static void swrm_reg_dump(struct swr_mstr_ctrl *swrm,
			  u32 *reg, u32 *val, int len, const char* func)
{
	int i = 0;

	for (i = 0; i < len; i++)
		dev_dbg(swrm->dev, "%s: reg = 0x%x val = 0x%x\n",
			func, reg[i], val[i]);
}

static bool is_swr_clk_needed(struct swr_mstr_ctrl *swrm)
{
	return ((swrm->version <= SWRM_VERSION_1_5_1) ? true : false);
}

static int swrm_request_hw_vote(struct swr_mstr_ctrl *swrm,
				int core_type, bool enable)
{
	int ret = 0;

	mutex_lock(&swrm->devlock);
	if (core_type == LPASS_HW_CORE) {
		if (swrm->lpass_core_hw_vote) {
			if (enable) {
				if (!swrm->dev_up) {
					dev_dbg(swrm->dev, "%s: device is down or SSR state\n",
							__func__);
					trace_printk("%s: device is down or SSR state\n",
							__func__);
					mutex_unlock(&swrm->devlock);
					return -ENODEV;
				}
				if (++swrm->hw_core_clk_en == 1) {
					ret =
					   digital_cdc_rsc_mgr_hw_vote_enable(
							swrm->lpass_core_hw_vote);
					if (ret < 0) {
						dev_err(swrm->dev,
							"%s:lpass core hw enable failed\n",
							__func__);
						--swrm->hw_core_clk_en;
					}
				}
			} else {
				--swrm->hw_core_clk_en;
				if (swrm->hw_core_clk_en < 0)
					swrm->hw_core_clk_en = 0;
				else if (swrm->hw_core_clk_en == 0)
					digital_cdc_rsc_mgr_hw_vote_disable(
							swrm->lpass_core_hw_vote);
			}
		}
	}
	if (core_type == LPASS_AUDIO_CORE) {
		if (swrm->lpass_core_audio) {
			if (enable) {
				if (!swrm->dev_up) {
					dev_dbg(swrm->dev, "%s: device is down or SSR state\n",
							__func__);
					trace_printk("%s: device is down or SSR state\n",
							__func__);
					mutex_unlock(&swrm->devlock);
					return -ENODEV;
				}
				if (++swrm->aud_core_clk_en == 1) {
					ret =
					   digital_cdc_rsc_mgr_hw_vote_enable(
							swrm->lpass_core_audio);
					if (ret < 0) {
						dev_err(swrm->dev,
							"%s:lpass audio hw enable failed\n",
							__func__);
						--swrm->aud_core_clk_en;
					}
				}
			} else {
				--swrm->aud_core_clk_en;
				if (swrm->aud_core_clk_en < 0)
					swrm->aud_core_clk_en = 0;
				else if (swrm->aud_core_clk_en == 0)
					digital_cdc_rsc_mgr_hw_vote_disable(
							swrm->lpass_core_audio);
			}
		}
	}

	mutex_unlock(&swrm->devlock);
	dev_dbg(swrm->dev, "%s: hw_clk_en: %d audio_core_clk_en: %d\n",
		__func__, swrm->hw_core_clk_en, swrm->aud_core_clk_en);
	trace_printk("%s: hw_clk_en: %d audio_core_clk_en: %d\n",
		__func__, swrm->hw_core_clk_en, swrm->aud_core_clk_en);
	return ret;
}

static int swrm_get_ssp_period(struct swr_mstr_ctrl *swrm,
				int row, int col,
				int frame_sync)
{
	if (!swrm || !row || !col || !frame_sync)
		return 1;

	return ((swrm->bus_clk * 2) / ((row * col) * frame_sync));
}

static int swrm_core_vote_request(struct swr_mstr_ctrl *swrm)
{
	int ret = 0;

	if (!swrm->handle)
		return -EINVAL;

	mutex_lock(&swrm->clklock);
	if (!swrm->dev_up) {
		ret = -ENODEV;
		goto exit;
	}
	if (swrm->core_vote) {
		ret = swrm->core_vote(swrm->handle, true);
		if (ret)
			dev_err_ratelimited(swrm->dev,
				"%s: core vote request failed\n", __func__);
	}
exit:
	mutex_unlock(&swrm->clklock);

	return ret;
}

static int swrm_clk_request(struct swr_mstr_ctrl *swrm, bool enable)
{
	int ret = 0;

	if (!swrm->clk || !swrm->handle)
		return -EINVAL;

	mutex_lock(&swrm->clklock);
	if (enable) {
		if (!swrm->dev_up) {
			ret = -ENODEV;
			goto exit;
		}
		if (is_swr_clk_needed(swrm)) {
			if (swrm->core_vote) {
				ret = swrm->core_vote(swrm->handle, true);
				if (ret) {
					dev_err_ratelimited(swrm->dev,
						"%s: core vote request failed\n",
						__func__);
					goto exit;
				}
			}
		}
		swrm->clk_ref_count++;
		if (swrm->clk_ref_count == 1) {
			trace_printk("%s: clock enable count %d",
				__func__, swrm->clk_ref_count);
			ret = swrm->clk(swrm->handle, true);
			if (ret) {
				dev_err_ratelimited(swrm->dev,
					"%s: clock enable req failed",
					__func__);
				--swrm->clk_ref_count;
			}
		}
	} else if (--swrm->clk_ref_count == 0) {
		trace_printk("%s: clock disable count %d",
			__func__, swrm->clk_ref_count);
		swrm->clk(swrm->handle, false);
		complete(&swrm->clk_off_complete);
	}
	if (swrm->clk_ref_count < 0) {
		dev_err(swrm->dev, "%s: swrm clk count mismatch\n", __func__);
		swrm->clk_ref_count = 0;
	}

exit:
	mutex_unlock(&swrm->clklock);
	return ret;
}

static int swrm_ahb_write(struct swr_mstr_ctrl *swrm,
					u16 reg, u32 *value)
{
	u32 temp = (u32)(*value);
	int ret = 0;

	mutex_lock(&swrm->devlock);
	if (!swrm->dev_up)
		goto err;

	if (is_swr_clk_needed(swrm)) {
		ret = swrm_clk_request(swrm, TRUE);
		if (ret) {
			dev_err_ratelimited(swrm->dev,
					    "%s: clock request failed\n",
					    __func__);
			goto err;
		}
	} else if (swrm_core_vote_request(swrm)) {
		goto err;
	}

	iowrite32(temp, swrm->swrm_dig_base + reg);
	if (is_swr_clk_needed(swrm))
		swrm_clk_request(swrm, FALSE);
err:
	mutex_unlock(&swrm->devlock);
	return ret;
}

static int swrm_ahb_read(struct swr_mstr_ctrl *swrm,
					u16 reg, u32 *value)
{
	u32 temp = 0;
	int ret = 0;

	mutex_lock(&swrm->devlock);
	if (!swrm->dev_up)
		goto err;

	if (is_swr_clk_needed(swrm)) {
		ret = swrm_clk_request(swrm, TRUE);
		if (ret) {
			dev_err_ratelimited(swrm->dev, "%s: clock request failed\n",
					    __func__);
			goto err;
		}
	} else if (swrm_core_vote_request(swrm)) {
		goto err;
	}

	temp = ioread32(swrm->swrm_dig_base + reg);
	*value = temp;
	if (is_swr_clk_needed(swrm))
		swrm_clk_request(swrm, FALSE);
err:
	mutex_unlock(&swrm->devlock);
	return ret;
}

static u32 swr_master_read(struct swr_mstr_ctrl *swrm, unsigned int reg_addr)
{
	u32 val = 0;

	if (swrm->read)
		val = swrm->read(swrm->handle, reg_addr);
	else
		swrm_ahb_read(swrm, reg_addr, &val);
	return val;
}

static void swr_master_write(struct swr_mstr_ctrl *swrm, u16 reg_addr, u32 val)
{
	if (swrm->write)
		swrm->write(swrm->handle, reg_addr, val);
	else
		swrm_ahb_write(swrm, reg_addr, &val);
}

static int swr_master_bulk_write(struct swr_mstr_ctrl *swrm, u32 *reg_addr,
				u32 *val, unsigned int length)
{
	int i = 0;

	if (swrm->bulk_write)
		swrm->bulk_write(swrm->handle, reg_addr, val, length);
	else {
		mutex_lock(&swrm->iolock);
		for (i = 0; i < length; i++) {
		/* wait for FIFO WR command to complete to avoid overflow */
		/*
		 * Reduce sleep from 100us to 50us to meet KPIs
		 * This still meets the hardware spec
		 */
			usleep_range(50, 55);
			swr_master_write(swrm, reg_addr[i], val[i]);
		}
		usleep_range(100, 110);
		mutex_unlock(&swrm->iolock);
	}
	return 0;
}

static bool swrm_check_link_status(struct swr_mstr_ctrl *swrm, bool active)
{
	int retry = SWRM_LINK_STATUS_RETRY_CNT;
	int ret = false;
	int status = active ? 0x1 : 0x0;
	int comp_sts = 0x0;

	if ((swrm->version <= SWRM_VERSION_1_5_1))
		return true;

	do {
		comp_sts = swr_master_read(swrm, SWRM_COMP_STATUS) & 0x01;
		/* check comp status and status requested met */
		if ((comp_sts && status) || (!comp_sts && !status)) {
			ret = true;
			break;
		}
		retry--;
		usleep_range(500, 510);
	} while (retry);

	if (retry == 0)
		dev_err(swrm->dev, "%s: link status not %s\n", __func__,
			active ? "connected" : "disconnected");

	return ret;
}

static bool swrm_is_port_en(struct swr_master *mstr)
{
	return !!(mstr->num_port);
}

static void copy_port_tables(struct swr_mstr_ctrl *swrm,
				struct port_params *params)
{
	u8 i;
	struct port_params *config = params;

	for (i = 0; i < SWR_MSTR_PORT_LEN; i++) {
		/* wsa uses single frame structure for all configurations */
		if (!swrm->mport_cfg[i].port_en)
			continue;
		swrm->mport_cfg[i].sinterval = config[i].si;
		swrm->mport_cfg[i].offset1 = config[i].off1;
		swrm->mport_cfg[i].offset2 = config[i].off2;
		swrm->mport_cfg[i].hstart = config[i].hstart;
		swrm->mport_cfg[i].hstop = config[i].hstop;
		swrm->mport_cfg[i].blk_pack_mode = config[i].bp_mode;
		swrm->mport_cfg[i].blk_grp_count = config[i].bgp_ctrl;
		swrm->mport_cfg[i].word_length = config[i].wd_len;
		swrm->mport_cfg[i].lane_ctrl = config[i].lane_ctrl;
		swrm->mport_cfg[i].dir = config[i].dir;
		swrm->mport_cfg[i].stream_type = config[i].stream_type;
	}
}

static int swrm_get_port_config(struct swr_mstr_ctrl *swrm)
{
	struct port_params *params;
	u32 usecase = 0;

	/* TODO - Send usecase information to avoid checking for master_id */
	if (swrm->mport_cfg[SWRM_DSD_PARAMS_PORT].port_en &&
				(swrm->master_id == MASTER_ID_RX))
		usecase = 1;
	else if ((swrm->master_id == MASTER_ID_RX) &&
		(swrm->bus_clk == SWR_CLK_RATE_11P2896MHZ))
		usecase = 2;

	if (swrm->bus_clk == SWR_CLK_RATE_4P8MHZ)
		usecase = 1;
	else if (swrm->bus_clk == SWR_CLK_RATE_0P6MHZ)
		usecase = 2;

	params = swrm->port_param[usecase];
	copy_port_tables(swrm, params);

	return 0;
}

static int swrm_pcm_port_config(struct swr_mstr_ctrl *swrm, u8 port_num,
				bool dir, bool enable)
{
	u16 reg_addr = 0;

	if (!port_num || port_num > 6) {
		dev_err(swrm->dev, "%s: invalid port: %d\n",
			__func__, port_num);
		return -EINVAL;
	}
	reg_addr = ((dir) ? SWRM_DIN_DP_PCM_PORT_CTRL(port_num) : \
				SWRM_DOUT_DP_PCM_PORT_CTRL(port_num));
	swr_master_write(swrm, reg_addr, enable);

	if (enable)
		swr_master_write(swrm, SWRM_COMP_FEATURE_CFG, 0x1E);
	else
		swr_master_write(swrm, SWRM_COMP_FEATURE_CFG, 0x6);
	return 0;
}

static int swrm_get_master_port(struct swr_mstr_ctrl *swrm, u8 *mstr_port_id,
					u8 *mstr_ch_mask, u8 mstr_prt_type,
					u8 slv_port_id)
{
	int i, j;
	*mstr_port_id = 0;

	for (i = 1; i <= swrm->num_ports; i++) {
		for (j = 0; j < SWR_MAX_CH_PER_PORT; j++) {
			if (swrm->port_mapping[i][j].port_type == mstr_prt_type)
				goto found;
		}
	}
found:
	if (i > swrm->num_ports || j == SWR_MAX_CH_PER_PORT)  {
		dev_err(swrm->dev, "%s: port type not supported by master\n",
					__func__);
		return -EINVAL;
	}
	/* id 0 corresponds to master port 1 */
	*mstr_port_id = i - 1;
	*mstr_ch_mask = swrm->port_mapping[i][j].ch_mask;

	return 0;
}

static u32 swrm_get_packed_reg_val(u8 *cmd_id, u8 cmd_data,
				 u8 dev_addr, u16 reg_addr)
{
	u32 val;
	u8 id = *cmd_id;

	if (id != SWR_BROADCAST_CMD_ID) {
		if (id < 14)
			id += 1;
		else
			id = 0;
		*cmd_id = id;
	}
	val = SWR_REG_VAL_PACK(cmd_data, dev_addr, id, reg_addr);

	return val;
}

static void swrm_wait_for_fifo_avail(struct swr_mstr_ctrl *swrm, int swrm_rd_wr)
{
	u32 fifo_outstanding_cmd;
	u32 fifo_retry_count = SWR_OVERFLOW_RETRY_COUNT;

	if (swrm_rd_wr) {
		/* Check for fifo underflow during read */
		/* Check no of outstanding commands in fifo before read */
		fifo_outstanding_cmd = ((swr_master_read(swrm,
				SWRM_CMD_FIFO_STATUS) & 0x001F0000) >> 16);
		if (fifo_outstanding_cmd == 0) {
			while (fifo_retry_count) {
				usleep_range(500, 510);
				fifo_outstanding_cmd =
					((swr_master_read (swrm,
					  SWRM_CMD_FIFO_STATUS) & 0x001F0000)
					  >> 16);
				fifo_retry_count--;
				if (fifo_outstanding_cmd > 0)
					break;
			}
		}
		if (fifo_outstanding_cmd == 0)
			dev_err_ratelimited(swrm->dev,
					"%s err read underflow\n", __func__);
	} else {
		/* Check for fifo overflow during write */
		/* Check no of outstanding commands in fifo before write */
		fifo_outstanding_cmd = ((swr_master_read(swrm,
					 SWRM_CMD_FIFO_STATUS) & 0x00001F00)
					 >> 8);
		if (fifo_outstanding_cmd == swrm->wr_fifo_depth) {
			while (fifo_retry_count) {
				usleep_range(500, 510);
				fifo_outstanding_cmd =
				((swr_master_read(swrm, SWRM_CMD_FIFO_STATUS)
				  & 0x00001F00) >> 8);
				fifo_retry_count--;
				if (fifo_outstanding_cmd < swrm->wr_fifo_depth)
					break;
			}
		}
		if (fifo_outstanding_cmd == swrm->wr_fifo_depth)
			dev_err_ratelimited(swrm->dev,
					"%s err write overflow\n", __func__);
	}
}

static int swrm_cmd_fifo_rd_cmd(struct swr_mstr_ctrl *swrm, int *cmd_data,
				 u8 dev_addr, u8 cmd_id, u16 reg_addr,
				 u32 len)
{
	u32 val;
	u32 retry_attempt = 0;

	mutex_lock(&swrm->iolock);
	val = swrm_get_packed_reg_val(&swrm->rcmd_id, len, dev_addr, reg_addr);
	if (swrm->read) {
		/* skip delay if read is handled in platform driver */
		swr_master_write(swrm, SWRM_CMD_FIFO_RD_CMD, val);
	} else {
		/*
		 * Check for outstanding cmd wrt. write fifo depth to avoid
		 * overflow as read will also increase write fifo cnt.
		 */
		swrm_wait_for_fifo_avail(swrm, SWRM_WR_CHECK_AVAIL);
		/* wait for FIFO RD to complete to avoid overflow */
		usleep_range(100, 105);
		swr_master_write(swrm, SWRM_CMD_FIFO_RD_CMD, val);
		/* wait for FIFO RD CMD complete to avoid overflow */
		usleep_range(250, 255);
	}
	/* Check if slave responds properly after FIFO RD is complete */
	swrm_wait_for_fifo_avail(swrm, SWRM_RD_CHECK_AVAIL);
retry_read:
	*cmd_data = swr_master_read(swrm, SWRM_CMD_FIFO_RD_FIFO);
	dev_dbg(swrm->dev, "%s: reg: 0x%x, cmd_id: 0x%x, rcmd_id: 0x%x, \
		dev_num: 0x%x, cmd_data: 0x%x\n", __func__, reg_addr,
		cmd_id, swrm->rcmd_id, dev_addr, *cmd_data);
	if ((((*cmd_data) & 0xF00) >> 8) != swrm->rcmd_id) {
		if (retry_attempt < MAX_FIFO_RD_FAIL_RETRY) {
			/* wait 500 us before retry on fifo read failure */
			usleep_range(500, 505);
			if (retry_attempt == (MAX_FIFO_RD_FAIL_RETRY - 1)) {
				swr_master_write(swrm, SWRM_CMD_FIFO_CMD, 0x1);
				swr_master_write(swrm, SWRM_CMD_FIFO_RD_CMD, val);
			}
			retry_attempt++;
			goto retry_read;
		} else {
			dev_err_ratelimited(swrm->dev, "%s: reg: 0x%x, cmd_id: 0x%x, \
				rcmd_id: 0x%x, dev_num: 0x%x, cmd_data: 0x%x\n",
				__func__, reg_addr, cmd_id, swrm->rcmd_id,
				dev_addr, *cmd_data);

			dev_err_ratelimited(swrm->dev,
				"%s: failed to read fifo\n", __func__);
		}
	}
	mutex_unlock(&swrm->iolock);

	return 0;
}

static int swrm_cmd_fifo_wr_cmd(struct swr_mstr_ctrl *swrm, u8 cmd_data,
				 u8 dev_addr, u8 cmd_id, u16 reg_addr)
{
	u32 val;
	int ret = 0;

	mutex_lock(&swrm->iolock);
	if (!cmd_id)
		val = swrm_get_packed_reg_val(&swrm->wcmd_id, cmd_data,
					      dev_addr, reg_addr);
	else
		val = swrm_get_packed_reg_val(&cmd_id, cmd_data,
					      dev_addr, reg_addr);
	dev_dbg(swrm->dev, "%s: reg: 0x%x, cmd_id: 0x%x,wcmd_id: 0x%x, \
			dev_num: 0x%x, cmd_data: 0x%x\n", __func__,
			reg_addr, cmd_id, swrm->wcmd_id,dev_addr, cmd_data);
	/*
	 * Check for outstanding cmd wrt. write fifo depth to avoid
	 * overflow.
	 */
	swrm_wait_for_fifo_avail(swrm, SWRM_WR_CHECK_AVAIL);
	swr_master_write(swrm, SWRM_CMD_FIFO_WR_CMD, val);
	/*
	 * wait for FIFO WR command to complete to avoid overflow
	 * skip delay if write is handled in platform driver.
	 */
	if(!swrm->write)
		usleep_range(150, 155);
	if (cmd_id == 0xF) {
		/*
		 * sleep for 10ms for MSM soundwire variant to allow broadcast
		 * command to complete.
		 */
		if (swrm_is_msm_variant(swrm->version))
			usleep_range(10000, 10100);
		else
			wait_for_completion_timeout(&swrm->broadcast,
						    (2 * HZ/10));
	}
	mutex_unlock(&swrm->iolock);
	return ret;
}

static int swrm_read(struct swr_master *master, u8 dev_num, u16 reg_addr,
		     void *buf, u32 len)
{
	struct swr_mstr_ctrl *swrm = swr_get_ctrl_data(master);
	int ret = 0;
	int val;
	u8 *reg_val = (u8 *)buf;

	if (!swrm) {
		dev_err(&master->dev, "%s: swrm is NULL\n", __func__);
		return -EINVAL;
	}
	if (!dev_num) {
		dev_err(&master->dev, "%s: invalid slave dev num\n", __func__);
		return -EINVAL;
	}
	mutex_lock(&swrm->devlock);
	if (!swrm->dev_up) {
		mutex_unlock(&swrm->devlock);
		return 0;
	}
	mutex_unlock(&swrm->devlock);

	pm_runtime_get_sync(swrm->dev);
	if (swrm->req_clk_switch)
		swrm_runtime_resume(swrm->dev);
	ret = swrm_cmd_fifo_rd_cmd(swrm, &val, dev_num, 0, reg_addr, len);

	if (!ret)
		*reg_val = (u8)val;

	pm_runtime_put_autosuspend(swrm->dev);
	pm_runtime_mark_last_busy(swrm->dev);
	return ret;
}

static int swrm_write(struct swr_master *master, u8 dev_num, u16 reg_addr,
		      const void *buf)
{
	struct swr_mstr_ctrl *swrm = swr_get_ctrl_data(master);
	int ret = 0;
	u8 reg_val = *(u8 *)buf;

	if (!swrm) {
		dev_err(&master->dev, "%s: swrm is NULL\n", __func__);
		return -EINVAL;
	}
	if (!dev_num) {
		dev_err(&master->dev, "%s: invalid slave dev num\n", __func__);
		return -EINVAL;
	}
	mutex_lock(&swrm->devlock);
	if (!swrm->dev_up) {
		mutex_unlock(&swrm->devlock);
		return 0;
	}
	mutex_unlock(&swrm->devlock);

	pm_runtime_get_sync(swrm->dev);
	if (swrm->req_clk_switch)
		swrm_runtime_resume(swrm->dev);
	ret = swrm_cmd_fifo_wr_cmd(swrm, reg_val, dev_num, 0, reg_addr);

	pm_runtime_put_autosuspend(swrm->dev);
	pm_runtime_mark_last_busy(swrm->dev);
	return ret;
}

static int swrm_bulk_write(struct swr_master *master, u8 dev_num, void *reg,
			   const void *buf, size_t len)
{
	struct swr_mstr_ctrl *swrm = swr_get_ctrl_data(master);
	int ret = 0;
	int i;
	u32 *val;
	u32 *swr_fifo_reg;

	if (!swrm || !swrm->handle) {
		dev_err(&master->dev, "%s: swrm is NULL\n", __func__);
		return -EINVAL;
	}
	if (len <= 0)
		return -EINVAL;
	mutex_lock(&swrm->devlock);
	if (!swrm->dev_up) {
		mutex_unlock(&swrm->devlock);
		return 0;
	}
	mutex_unlock(&swrm->devlock);

	pm_runtime_get_sync(swrm->dev);
	if (dev_num) {
		swr_fifo_reg = kcalloc(len, sizeof(u32), GFP_KERNEL);
		if (!swr_fifo_reg) {
			ret = -ENOMEM;
			goto err;
		}
		val = kcalloc(len, sizeof(u32), GFP_KERNEL);
		if (!val) {
			ret = -ENOMEM;
			goto mem_fail;
		}

		for (i = 0; i < len; i++) {
			val[i] = swrm_get_packed_reg_val(&swrm->wcmd_id,
							 ((u8 *)buf)[i],
							 dev_num,
							 ((u16 *)reg)[i]);
			swr_fifo_reg[i] = SWRM_CMD_FIFO_WR_CMD;
		}
		ret = swr_master_bulk_write(swrm, swr_fifo_reg, val, len);
		if (ret) {
			dev_err(&master->dev, "%s: bulk write failed\n",
				__func__);
			ret = -EINVAL;
		}
	} else {
		dev_err(&master->dev,
			"%s: No support of Bulk write for master regs\n",
			__func__);
		ret = -EINVAL;
		goto err;
	}
	kfree(val);
mem_fail:
	kfree(swr_fifo_reg);
err:
	pm_runtime_put_autosuspend(swrm->dev);
	pm_runtime_mark_last_busy(swrm->dev);
	return ret;
}

static u8 get_inactive_bank_num(struct swr_mstr_ctrl *swrm)
{
	return (swr_master_read(swrm, SWRM_MCP_STATUS) & 0x01) ? 0 : 1;
}

static void enable_bank_switch(struct swr_mstr_ctrl *swrm, u8 bank,
				u8 row, u8 col)
{
	swrm_cmd_fifo_wr_cmd(swrm, ((row << 3) | col), 0xF, 0xF,
			SWRS_SCP_FRAME_CTRL_BANK(bank));
}

static void swrm_switch_frame_shape(struct swr_mstr_ctrl *swrm, int mclk_freq)
{
	u8 bank;
	u32 n_row, n_col;
	u32 value = 0;
	u32 row = 0, col = 0;
	u8 ssp_period = 0;
	int frame_sync = SWRM_FRAME_SYNC_SEL;

	if (mclk_freq == MCLK_FREQ_NATIVE) {
		n_col = SWR_MAX_COL;
		col = SWRM_COL_16;
		n_row = SWR_ROW_64;
		row = SWRM_ROW_64;
		frame_sync = SWRM_FRAME_SYNC_SEL_NATIVE;
	} else {
		n_col = SWR_MIN_COL;
		col = SWRM_COL_02;
		n_row = SWR_ROW_50;
		row = SWRM_ROW_50;
		frame_sync = SWRM_FRAME_SYNC_SEL;
	}

	bank = get_inactive_bank_num(swrm);
	ssp_period = swrm_get_ssp_period(swrm, row, col, frame_sync);
	dev_dbg(swrm->dev, "%s: ssp_period: %d\n", __func__, ssp_period);
	value = ((n_row << SWRM_MCP_FRAME_CTRL_BANK_ROW_CTRL_SHFT) |
		  (n_col << SWRM_MCP_FRAME_CTRL_BANK_COL_CTRL_SHFT) |
		  ((ssp_period - 1) << SWRM_MCP_FRAME_CTRL_BANK_SSP_PERIOD_SHFT));
	swr_master_write(swrm, SWRM_MCP_FRAME_CTRL_BANK(bank), value);
	enable_bank_switch(swrm, bank, n_row, n_col);
}

static struct swr_port_info *swrm_get_port_req(struct swrm_mports *mport,
						   u8 slv_port, u8 dev_num)
{
	struct swr_port_info *port_req = NULL;

	list_for_each_entry(port_req, &mport->port_req_list, list) {
	/* Store dev_id instead of dev_num if enumeration is changed run_time */
		if ((port_req->slave_port_id == slv_port)
			&& (port_req->dev_num == dev_num))
			return port_req;
	}
	return NULL;
}

static bool swrm_remove_from_group(struct swr_master *master)
{
	struct swr_device *swr_dev;
	struct swr_mstr_ctrl *swrm = swr_get_ctrl_data(master);
	bool is_removed = false;

	if (!swrm)
		goto end;

	mutex_lock(&swrm->mlock);
	if ((swrm->num_rx_chs > 1) &&
	    (swrm->num_rx_chs == swrm->num_cfg_devs)) {
		list_for_each_entry(swr_dev, &master->devices,
				dev_list) {
			swr_dev->group_id = SWR_GROUP_NONE;
			master->gr_sid = 0;
		}
		is_removed = true;
	}
	mutex_unlock(&swrm->mlock);

end:
	return is_removed;
}

int swrm_get_clk_div_rate(int mclk_freq, int bus_clk_freq)
{
	if (!bus_clk_freq)
		return mclk_freq;

	if (mclk_freq == SWR_CLK_RATE_9P6MHZ) {
		if (bus_clk_freq <= SWR_CLK_RATE_0P6MHZ)
			bus_clk_freq = SWR_CLK_RATE_0P6MHZ;
		else if (bus_clk_freq <= SWR_CLK_RATE_1P2MHZ)
			bus_clk_freq = SWR_CLK_RATE_4P8MHZ;
		else if (bus_clk_freq <= SWR_CLK_RATE_2P4MHZ)
			bus_clk_freq = SWR_CLK_RATE_4P8MHZ;
		else if(bus_clk_freq <= SWR_CLK_RATE_4P8MHZ)
			bus_clk_freq = SWR_CLK_RATE_4P8MHZ;
		else if(bus_clk_freq <= SWR_CLK_RATE_9P6MHZ)
			bus_clk_freq = SWR_CLK_RATE_9P6MHZ;
		else
			bus_clk_freq = SWR_CLK_RATE_9P6MHZ;
	} else if (mclk_freq == SWR_CLK_RATE_11P2896MHZ)
		bus_clk_freq = SWR_CLK_RATE_11P2896MHZ;

	return bus_clk_freq;
}

static int swrm_update_bus_clk(struct swr_mstr_ctrl *swrm)
{
	int ret = 0;
	int agg_clk = 0;
	int i;

	for (i = 0; i < SWR_MSTR_PORT_LEN; i++)
		agg_clk += swrm->mport_cfg[i].ch_rate;

	if (agg_clk)
		swrm->bus_clk = swrm_get_clk_div_rate(swrm->mclk_freq,
							agg_clk);
	else
		swrm->bus_clk = swrm->mclk_freq;

	dev_dbg(swrm->dev, "%s: all_port_clk: %d, bus_clk: %d\n",
		__func__, agg_clk, swrm->bus_clk);

	return ret;
}

static void swrm_disable_ports(struct swr_master *master,
					     u8 bank)
{
	u32 value;
	struct swr_port_info *port_req;
	int i;
	struct swrm_mports *mport;
	struct swr_mstr_ctrl *swrm = swr_get_ctrl_data(master);

	if (!swrm) {
		pr_err("%s: swrm is null\n", __func__);
		return;
	}

	dev_dbg(swrm->dev, "%s: master num_port: %d\n", __func__,
		master->num_port);


	for (i = 0; i < SWR_MSTR_PORT_LEN ; i++) {

		mport = &(swrm->mport_cfg[i]);
		if (!mport->port_en)
			continue;

		list_for_each_entry(port_req, &mport->port_req_list, list) {
			/* skip ports with no change req's*/
			if (port_req->req_ch == port_req->ch_en)
				continue;

			swrm_cmd_fifo_wr_cmd(swrm, port_req->req_ch,
					port_req->dev_num, 0x00,
			SWRS_DP_CHANNEL_ENABLE_BANK(port_req->slave_port_id,
					bank));
			dev_dbg(swrm->dev, "%s: mport :%d, reg: 0x%x\n",
				__func__, i,
				(SWRM_DP_PORT_CTRL_BANK((i + 1), bank)));
		}
		value = ((mport->req_ch)
					<< SWRM_DP_PORT_CTRL_EN_CHAN_SHFT);
		value |= ((mport->offset2)
					<< SWRM_DP_PORT_CTRL_OFFSET2_SHFT);
		value |= ((mport->offset1)
				<< SWRM_DP_PORT_CTRL_OFFSET1_SHFT);
		value |= mport->sinterval;

		swr_master_write(swrm,
				SWRM_DP_PORT_CTRL_BANK((i + 1), bank),
				value);
		dev_dbg(swrm->dev, "%s: mport :%d, reg: 0x%x, val: 0x%x\n",
			__func__, i,
			(SWRM_DP_PORT_CTRL_BANK((i + 1), bank)), value);

		if (mport->stream_type == SWR_PCM)
			swrm_pcm_port_config(swrm, (i + 1), mport->dir, false);
	}
}

static void swrm_cleanup_disabled_port_reqs(struct swr_master *master)
{
	struct swr_port_info *port_req, *next;
	int i;
	struct swrm_mports *mport;
	struct swr_mstr_ctrl *swrm = swr_get_ctrl_data(master);

	if (!swrm) {
		pr_err("%s: swrm is null\n", __func__);
		return;
	}
	dev_dbg(swrm->dev, "%s: master num_port: %d\n", __func__,
		master->num_port);

	for (i = 0; i < SWR_MSTR_PORT_LEN; i++) {
		mport = &(swrm->mport_cfg[i]);
		list_for_each_entry_safe(port_req, next,
			&mport->port_req_list, list) {
			/* skip ports without new ch req */
			if (port_req->ch_en == port_req->req_ch)
				continue;

			/* remove new ch req's*/
			port_req->ch_en = port_req->req_ch;

			/* If no streams enabled on port, remove the port req */
			if (port_req->ch_en == 0) {
				list_del(&port_req->list);
				kfree(port_req);
			}
		}
		/* remove new ch req's on mport*/
		mport->ch_en = mport->req_ch;

		if (!(mport->ch_en)) {
			mport->port_en = false;
			master->port_en_mask &= ~i;
		}
	}
}

static u8 swrm_get_controller_offset1(struct swr_mstr_ctrl *swrm,
					u8* dev_offset, u8 off1)
{
	u8 offset1 = 0x0F;
	int i = 0;

	if (swrm->master_id == MASTER_ID_TX) {
		for (i = 1; i < SWRM_NUM_AUTO_ENUM_SLAVES; i++) {
			pr_debug("%s: dev offset: %d\n",
				__func__, dev_offset[i]);
			if (offset1 > dev_offset[i])
				offset1 = dev_offset[i];
		}
	} else {
		offset1 = off1;
	}

	pr_debug("%s: offset: %d\n", __func__, offset1);

	return offset1;
}

static void swrm_get_device_frame_shape(struct swr_mstr_ctrl *swrm,
					struct swrm_mports *mport,
					struct swr_port_info *port_req)
{
	u32 port_id = 0;
	u8 dev_num = 0;
	struct port_params *pp_dev;
	struct port_params *pp_port;

	if ((swrm->master_id == MASTER_ID_TX) &&
		((swrm->bus_clk == SWR_CLK_RATE_9P6MHZ) ||
		 (swrm->bus_clk == SWR_CLK_RATE_0P6MHZ) ||
		 (swrm->bus_clk == SWR_CLK_RATE_4P8MHZ))) {
		dev_num = swrm_get_device_id(swrm, port_req->dev_num);
		port_id = port_req->slave_port_id;
		if (swrm->bus_clk == SWR_CLK_RATE_9P6MHZ)
			pp_dev = swrdev_frame_params_9p6MHz[dev_num].pp;
		else if (swrm->bus_clk == SWR_CLK_RATE_0P6MHZ)
			pp_dev = swrdev_frame_params_0p6MHz[dev_num].pp;
		else
			pp_dev = swrdev_frame_params_4p8MHz[dev_num].pp;
		pp_port = &pp_dev[port_id];
		port_req->sinterval = pp_port->si;
		port_req->offset1 = pp_port->off1;
		port_req->offset2 = pp_port->off2;
		port_req->hstart = pp_port->hstart;
		port_req->hstop = pp_port->hstop;
		port_req->word_length = pp_port->wd_len;
		port_req->blk_pack_mode = pp_port->bp_mode;
		port_req->blk_grp_count = pp_port->bgp_ctrl;
		port_req->lane_ctrl = pp_port->lane_ctrl;
	} else {
		/* copy master port config to slave */
		port_req->sinterval = mport->sinterval;
		port_req->offset1 = mport->offset1;
		port_req->offset2 = mport->offset2;
		port_req->hstart = mport->hstart;
		port_req->hstop = mport->hstop;
		port_req->word_length = mport->word_length;
		port_req->blk_pack_mode = mport->blk_pack_mode;
		port_req->blk_grp_count = mport->blk_grp_count;
		port_req->lane_ctrl = mport->lane_ctrl;
	}
}

static void swrm_copy_data_port_config(struct swr_master *master, u8 bank)
{
	u32 value = 0, slv_id = 0;
	struct swr_port_info *port_req;
	int i;
	struct swrm_mports *mport;
	u32 reg[SWRM_MAX_PORT_REG];
	u32 val[SWRM_MAX_PORT_REG];
	int len = 0;
	u8 hparams = 0;
	u32 controller_offset = 0;
	struct swr_mstr_ctrl *swrm = swr_get_ctrl_data(master);
	u8 dev_offset[SWRM_NUM_AUTO_ENUM_SLAVES];

	if (!swrm) {
		pr_err("%s: swrm is null\n", __func__);
		return;
	}

	memset(dev_offset, 0xff, SWRM_NUM_AUTO_ENUM_SLAVES);
	dev_dbg(swrm->dev, "%s: master num_port: %d\n", __func__,
		master->num_port);

	for (i = 0; i < SWR_MSTR_PORT_LEN; i++) {
		mport = &(swrm->mport_cfg[i]);
		if (!mport->port_en)
			continue;

		if (mport->stream_type == SWR_PCM)
			swrm_pcm_port_config(swrm, (i + 1), mport->dir, true);

		list_for_each_entry(port_req, &mport->port_req_list, list) {
			slv_id = port_req->slave_port_id;
			/* Assumption: If different channels in the same port
			 * on master is enabled for different slaves, then each
			 * slave offset should be configured differently.
			 */
			swrm_get_device_frame_shape(swrm, mport, port_req);

			reg[len] = SWRM_CMD_FIFO_WR_CMD;
			val[len++] = SWR_REG_VAL_PACK(port_req->req_ch,
					port_req->dev_num, 0x00,
					SWRS_DP_CHANNEL_ENABLE_BANK(slv_id,
								bank));

			reg[len] = SWRM_CMD_FIFO_WR_CMD;
			val[len++] = SWR_REG_VAL_PACK(
					port_req->sinterval & 0xFF,
					port_req->dev_num, 0x00,
					SWRS_DP_SAMPLE_CONTROL_1_BANK(slv_id,
								bank));

			reg[len] = SWRM_CMD_FIFO_WR_CMD;
			val[len++] = SWR_REG_VAL_PACK(
					(port_req->sinterval >> 8)& 0xFF,
					port_req->dev_num, 0x00,
					SWRS_DP_SAMPLE_CONTROL_2_BANK(slv_id,
								bank));

			reg[len] = SWRM_CMD_FIFO_WR_CMD;
			val[len++] = SWR_REG_VAL_PACK(port_req->offset1,
					port_req->dev_num, 0x00,
					SWRS_DP_OFFSET_CONTROL_1_BANK(slv_id,
								bank));

			if (port_req->offset2 != SWR_INVALID_PARAM) {
				reg[len] = SWRM_CMD_FIFO_WR_CMD;
				val[len++] = SWR_REG_VAL_PACK(port_req->offset2,
						port_req->dev_num, 0x00,
						SWRS_DP_OFFSET_CONTROL_2_BANK(
							slv_id, bank));
			}
			if (port_req->hstart != SWR_INVALID_PARAM
				&& port_req->hstop != SWR_INVALID_PARAM) {
				hparams = (port_req->hstart << 4) |
						port_req->hstop;

				reg[len] = SWRM_CMD_FIFO_WR_CMD;
				val[len++] = SWR_REG_VAL_PACK(hparams,
						port_req->dev_num, 0x00,
						SWRS_DP_HCONTROL_BANK(slv_id,
									bank));
			}
			if (port_req->word_length != SWR_INVALID_PARAM) {
				reg[len] = SWRM_CMD_FIFO_WR_CMD;
				val[len++] =
					SWR_REG_VAL_PACK(port_req->word_length,
						port_req->dev_num, 0x00,
						SWRS_DP_BLOCK_CONTROL_1(slv_id));
			}
			if (port_req->blk_pack_mode != SWR_INVALID_PARAM
					&& swrm->master_id != MASTER_ID_WSA) {
				reg[len] = SWRM_CMD_FIFO_WR_CMD;
				val[len++] =
					SWR_REG_VAL_PACK(
					port_req->blk_pack_mode,
					port_req->dev_num, 0x00,
					SWRS_DP_BLOCK_CONTROL_3_BANK(slv_id,
									bank));
			}
			if (port_req->blk_grp_count != SWR_INVALID_PARAM) {
				reg[len] = SWRM_CMD_FIFO_WR_CMD;
				val[len++] =
					 SWR_REG_VAL_PACK(
						port_req->blk_grp_count,
						port_req->dev_num, 0x00,
						SWRS_DP_BLOCK_CONTROL_2_BANK(
								slv_id, bank));
			}
			if (port_req->lane_ctrl != SWR_INVALID_PARAM) {
				reg[len] = SWRM_CMD_FIFO_WR_CMD;
				val[len++] =
					SWR_REG_VAL_PACK(port_req->lane_ctrl,
						port_req->dev_num, 0x00,
						SWRS_DP_LANE_CONTROL_BANK(
								slv_id, bank));
			}
			port_req->ch_en = port_req->req_ch;
			dev_offset[port_req->dev_num] = port_req->offset1;
		}
		value = ((mport->req_ch)
				<< SWRM_DP_PORT_CTRL_EN_CHAN_SHFT);

		if (mport->offset2 != SWR_INVALID_PARAM)
			value |= ((mport->offset2)
					<< SWRM_DP_PORT_CTRL_OFFSET2_SHFT);
		controller_offset = (swrm_get_controller_offset1(swrm,
						dev_offset, mport->offset1));
		value |= (controller_offset << SWRM_DP_PORT_CTRL_OFFSET1_SHFT);
		mport->offset1 = controller_offset;
		value |= (mport->sinterval & 0xFF);

		reg[len] = SWRM_DP_PORT_CTRL_BANK((i + 1), bank);
		val[len++] = value;
		dev_dbg(swrm->dev, "%s: mport :%d, reg: 0x%x, val: 0x%x\n",
			__func__, (i + 1),
			(SWRM_DP_PORT_CTRL_BANK((i + 1), bank)), value);

		reg[len] = SWRM_DP_SAMPLECTRL2_BANK((i + 1), bank);
		val[len++] = ((mport->sinterval >> 8) & 0xFF);

		if (mport->lane_ctrl != SWR_INVALID_PARAM) {
			reg[len] = SWRM_DP_PORT_CTRL_2_BANK((i + 1), bank);
			val[len++] = mport->lane_ctrl;
		}
		if (mport->word_length != SWR_INVALID_PARAM) {
			reg[len] = SWRM_DP_BLOCK_CTRL_1((i + 1));
			val[len++] = mport->word_length;
		}

		if (mport->blk_grp_count != SWR_INVALID_PARAM) {
			reg[len] = SWRM_DP_BLOCK_CTRL2_BANK((i + 1), bank);
			val[len++] = mport->blk_grp_count;
		}
		if (mport->hstart != SWR_INVALID_PARAM
				&& mport->hstop != SWR_INVALID_PARAM) {
			reg[len] = SWRM_DP_PORT_HCTRL_BANK((i + 1), bank);
			hparams = (mport->hstop << 4) | mport->hstart;
			val[len++] = hparams;
		} else {
			reg[len] = SWRM_DP_PORT_HCTRL_BANK((i + 1), bank);
			hparams = (SWR_HSTOP_MAX_VAL << 4) | SWR_HSTART_MIN_VAL;
			val[len++] = hparams;
		}
		if (mport->blk_pack_mode != SWR_INVALID_PARAM) {
			reg[len] = SWRM_DP_BLOCK_CTRL3_BANK((i + 1), bank);
			val[len++] = mport->blk_pack_mode;
		}
		mport->ch_en = mport->req_ch;

	}
	swrm_reg_dump(swrm, reg, val, len, __func__);
	swr_master_bulk_write(swrm, reg, val, len);
}

static void swrm_apply_port_config(struct swr_master *master)
{
	u8 bank;
	struct swr_mstr_ctrl *swrm = swr_get_ctrl_data(master);

	if (!swrm) {
		pr_err("%s: Invalid handle to swr controller\n",
			__func__);
		return;
	}

	bank = get_inactive_bank_num(swrm);
	dev_dbg(swrm->dev, "%s: enter bank: %d master_ports: %d\n",
		__func__, bank, master->num_port);

	if (!swrm->disable_div2_clk_switch)
		swrm_cmd_fifo_wr_cmd(swrm, 0x01, 0xF, 0x00,
				SWRS_SCP_HOST_CLK_DIV2_CTL_BANK(bank));

	swrm_copy_data_port_config(master, bank);
}

static int swrm_slvdev_datapath_control(struct swr_master *master, bool enable)
{
	u8 bank;
	u32 value = 0, n_row = 0, n_col = 0;
	u32 row = 0, col = 0;
	int bus_clk_div_factor;
	int ret;
	u8 ssp_period = 0;
	struct swr_mstr_ctrl *swrm = swr_get_ctrl_data(master);
	int mask = (SWRM_ROW_CTRL_MASK | SWRM_COL_CTRL_MASK |
		    SWRM_CLK_DIV_MASK | SWRM_SSP_PERIOD_MASK);
	u8 inactive_bank;
	int frame_sync = SWRM_FRAME_SYNC_SEL;

	if (!swrm) {
		pr_err("%s: swrm is null\n", __func__);
		return -EFAULT;
	}

	mutex_lock(&swrm->mlock);

	/*
	 * During disable if master is already down, which implies an ssr/pdr
	 * scenario, just mark ports as disabled and exit
	 */
	if (swrm->state == SWR_MSTR_SSR && !enable) {
		if (!test_bit(DISABLE_PENDING, &swrm->port_req_pending)) {
			dev_dbg(swrm->dev, "%s:No pending disconn port req\n",
				__func__);
			goto exit;
		}
		clear_bit(DISABLE_PENDING, &swrm->port_req_pending);
		swrm_cleanup_disabled_port_reqs(master);
		if (!swrm_is_port_en(master)) {
			dev_dbg(&master->dev, "%s: pm_runtime auto suspend triggered\n",
				__func__);
			pm_runtime_mark_last_busy(swrm->dev);
			pm_runtime_put_autosuspend(swrm->dev);
		}
		goto exit;
	}
	bank = get_inactive_bank_num(swrm);

	if (enable) {
		if (!test_bit(ENABLE_PENDING, &swrm->port_req_pending)) {
			dev_dbg(swrm->dev, "%s:No pending connect port req\n",
				__func__);
			goto exit;
		}
		clear_bit(ENABLE_PENDING, &swrm->port_req_pending);
		ret = swrm_get_port_config(swrm);
		if (ret) {
			/* cannot accommodate ports */
			swrm_cleanup_disabled_port_reqs(master);
			mutex_unlock(&swrm->mlock);
			return -EINVAL;
		}
		swr_master_write(swrm, SWRM_CPU1_INTERRUPT_EN,
				 SWRM_INTERRUPT_STATUS_MASK);
		/* apply the new port config*/
		swrm_apply_port_config(master);
	} else {
		if (!test_bit(DISABLE_PENDING, &swrm->port_req_pending)) {
			dev_dbg(swrm->dev, "%s:No pending disconn port req\n",
				__func__);
			goto exit;
		}
		clear_bit(DISABLE_PENDING, &swrm->port_req_pending);
		swrm_disable_ports(master, bank);
	}
	dev_dbg(swrm->dev, "%s: enable: %d, cfg_devs: %d freq %d\n",
		__func__, enable, swrm->num_cfg_devs, swrm->mclk_freq);

	if (enable) {
		/* set col = 16 */
		n_col = SWR_MAX_COL;
		col = SWRM_COL_16;
		if (swrm->bus_clk == MCLK_FREQ_LP) {
			n_col = SWR_MIN_COL;
			col = SWRM_COL_02;
		}
	} else {
		/*
		 * Do not change to col = 2 if there are still active ports
		 */
		if (!master->num_port) {
			n_col = SWR_MIN_COL;
			col = SWRM_COL_02;
		} else {
			n_col = SWR_MAX_COL;
			col = SWRM_COL_16;
		}
	}
	/* Use default 50 * x, frame shape. Change based on mclk */
	if (swrm->mclk_freq == MCLK_FREQ_NATIVE) {
		dev_dbg(swrm->dev, "setting 64 x %d frameshape\n", col);
		n_row = SWR_ROW_64;
		row = SWRM_ROW_64;
		frame_sync = SWRM_FRAME_SYNC_SEL_NATIVE;
	} else {
		dev_dbg(swrm->dev, "setting 50 x %d frameshape\n", col);
		n_row = SWR_ROW_50;
		row = SWRM_ROW_50;
		frame_sync = SWRM_FRAME_SYNC_SEL;
	}
	ssp_period = swrm_get_ssp_period(swrm, row, col, frame_sync);
	bus_clk_div_factor = swrm_get_clk_div(swrm->mclk_freq, swrm->bus_clk);
	dev_dbg(swrm->dev, "%s: ssp_period: %d, bus_clk_div:%d \n", __func__,
					ssp_period, bus_clk_div_factor);
	value = swr_master_read(swrm, SWRM_MCP_FRAME_CTRL_BANK(bank));
	value &= (~mask);
	value |= ((n_row << SWRM_MCP_FRAME_CTRL_BANK_ROW_CTRL_SHFT) |
		  (n_col << SWRM_MCP_FRAME_CTRL_BANK_COL_CTRL_SHFT) |
		  (bus_clk_div_factor <<
			SWRM_MCP_FRAME_CTRL_BANK_CLK_DIV_VALUE_SHFT) |
		  ((ssp_period - 1) << SWRM_MCP_FRAME_CTRL_BANK_SSP_PERIOD_SHFT));
	swr_master_write(swrm, SWRM_MCP_FRAME_CTRL_BANK(bank), value);

	dev_dbg(swrm->dev, "%s: regaddr: 0x%x, value: 0x%x\n", __func__,
		SWRM_MCP_FRAME_CTRL_BANK(bank), value);

	enable_bank_switch(swrm, bank, n_row, n_col);
	inactive_bank = bank ? 0 : 1;

	if (enable)
		swrm_copy_data_port_config(master, inactive_bank);
	else {
		swrm_disable_ports(master, inactive_bank);
		swrm_cleanup_disabled_port_reqs(master);
	}
	if (!swrm_is_port_en(master)) {
		dev_dbg(&master->dev, "%s: pm_runtime auto suspend triggered\n",
			__func__);
		pm_runtime_mark_last_busy(swrm->dev);
		pm_runtime_put_autosuspend(swrm->dev);
	}
exit:
	mutex_unlock(&swrm->mlock);
return 0;
}

static int swrm_connect_port(struct swr_master *master,
			struct swr_params *portinfo)
{
	int i;
	struct swr_port_info *port_req;
	int ret = 0;
	struct swr_mstr_ctrl *swrm = swr_get_ctrl_data(master);
	struct swrm_mports *mport;
	u8 mstr_port_id, mstr_ch_msk;

	dev_dbg(&master->dev, "%s: enter\n", __func__);
	if (!portinfo)
		return -EINVAL;

	if (!swrm) {
		dev_err(&master->dev,
			"%s: Invalid handle to swr controller\n",
			__func__);
		return -EINVAL;
	}

	mutex_lock(&swrm->mlock);
	mutex_lock(&swrm->devlock);
	if (!swrm->dev_up) {
		swr_port_response(master, portinfo->tid);
		mutex_unlock(&swrm->devlock);
		mutex_unlock(&swrm->mlock);
		return -EINVAL;
	}
	mutex_unlock(&swrm->devlock);
	if (!swrm_is_port_en(master))
		pm_runtime_get_sync(swrm->dev);

	for (i = 0; i < portinfo->num_port; i++) {
		ret = swrm_get_master_port(swrm, &mstr_port_id, &mstr_ch_msk,
						portinfo->port_type[i],
						portinfo->port_id[i]);
		if (ret) {
			dev_err(&master->dev,
				"%s: mstr portid for slv port %d not found\n",
				__func__, portinfo->port_id[i]);
			goto port_fail;
		}

		mport = &(swrm->mport_cfg[mstr_port_id]);
		/* get port req */
		port_req = swrm_get_port_req(mport, portinfo->port_id[i],
					portinfo->dev_num);
		if (!port_req) {
			dev_dbg(&master->dev, "%s: new req:port id %d dev %d\n",
						 __func__, portinfo->port_id[i],
						portinfo->dev_num);
			port_req = kzalloc(sizeof(struct swr_port_info),
					GFP_KERNEL);
			if (!port_req) {
				ret = -ENOMEM;
				goto mem_fail;
			}
			port_req->dev_num = portinfo->dev_num;
			port_req->slave_port_id = portinfo->port_id[i];
			port_req->num_ch = portinfo->num_ch[i];
			port_req->ch_rate = portinfo->ch_rate[i];
			port_req->ch_en = 0;
			port_req->master_port_id = mstr_port_id;
			list_add(&port_req->list, &mport->port_req_list);
		}
		port_req->req_ch |= portinfo->ch_en[i];

		dev_dbg(&master->dev,
			"%s: mstr port %d, slv port %d ch_rate %d num_ch %d\n",
			__func__, port_req->master_port_id,
			port_req->slave_port_id, port_req->ch_rate,
			port_req->num_ch);
		/* Put the port req on master port */
		mport = &(swrm->mport_cfg[mstr_port_id]);
		mport->port_en = true;
		mport->req_ch |= mstr_ch_msk;
		master->port_en_mask |= (1 << mstr_port_id);
		if (swrm->clk_stop_mode0_supp &&
				swrm->dynamic_port_map_supported) {
			mport->ch_rate += portinfo->ch_rate[i];
			swrm_update_bus_clk(swrm);
		}
	}
	master->num_port += portinfo->num_port;
	set_bit(ENABLE_PENDING, &swrm->port_req_pending);
	swr_port_response(master, portinfo->tid);

	mutex_unlock(&swrm->mlock);
	return 0;

port_fail:
mem_fail:
	swr_port_response(master, portinfo->tid);
	/* cleanup  port reqs in error condition */
	swrm_cleanup_disabled_port_reqs(master);
	mutex_unlock(&swrm->mlock);
	return ret;
}

static int swrm_disconnect_port(struct swr_master *master,
			struct swr_params *portinfo)
{
	int i, ret = 0;
	struct swr_port_info *port_req;
	struct swrm_mports *mport;
	struct swr_mstr_ctrl *swrm = swr_get_ctrl_data(master);
	u8 mstr_port_id, mstr_ch_mask;

	if (!swrm) {
		dev_err(&master->dev,
			"%s: Invalid handle to swr controller\n",
			__func__);
		return -EINVAL;
	}

	if (!portinfo) {
		dev_err(&master->dev, "%s: portinfo is NULL\n", __func__);
		return -EINVAL;
	}
	mutex_lock(&swrm->mlock);

	for (i = 0; i < portinfo->num_port; i++) {

		ret = swrm_get_master_port(swrm, &mstr_port_id, &mstr_ch_mask,
				portinfo->port_type[i], portinfo->port_id[i]);
		if (ret) {
			dev_err(&master->dev,
				"%s: mstr portid for slv port %d not found\n",
				__func__, portinfo->port_id[i]);
			goto err;
		}
		mport = &(swrm->mport_cfg[mstr_port_id]);
		/* get port req */
		port_req = swrm_get_port_req(mport, portinfo->port_id[i],
					portinfo->dev_num);

		if (!port_req) {
			dev_err(&master->dev, "%s:port not enabled : port %d\n",
					 __func__, portinfo->port_id[i]);
			goto err;
		}
		port_req->req_ch &= ~portinfo->ch_en[i];
		mport->req_ch &= ~mstr_ch_mask;
		if (swrm->clk_stop_mode0_supp &&
				swrm->dynamic_port_map_supported &&
				!mport->req_ch) {
			mport->ch_rate = 0;
			swrm_update_bus_clk(swrm);
		}
	}
	master->num_port -= portinfo->num_port;
	set_bit(DISABLE_PENDING, &swrm->port_req_pending);
	swr_port_response(master, portinfo->tid);
	mutex_unlock(&swrm->mlock);

	return 0;

err:
	swr_port_response(master, portinfo->tid);
	mutex_unlock(&swrm->mlock);
	return -EINVAL;
}

static int swrm_find_alert_slave(struct swr_mstr_ctrl *swrm,
					int status, u8 *devnum)
{
	int i;
	bool found = false;

	for (i = 0; i < (swrm->master.num_dev + 1); i++) {
		if ((status & SWRM_MCP_SLV_STATUS_MASK) == SWR_ALERT) {
			*devnum = i;
			found = true;
			break;
		}
		status >>= 2;
	}
	if (found)
		return 0;
	else
		return -EINVAL;
}

static void swrm_enable_slave_irq(struct swr_mstr_ctrl *swrm)
{
	int i;
	int status = 0;
	u32 temp;

	status = swr_master_read(swrm, SWRM_MCP_SLV_STATUS);
	if (!status) {
		dev_dbg_ratelimited(swrm->dev, "%s: slaves status is 0x%x\n",
					__func__, status);
		return;
	}
	dev_dbg(swrm->dev, "%s: slave status: 0x%x\n", __func__, status);
	for (i = 0; i < (swrm->master.num_dev + 1); i++) {
		if (status & SWRM_MCP_SLV_STATUS_MASK) {
			swrm_cmd_fifo_rd_cmd(swrm, &temp, i, 0x0,
					SWRS_SCP_INT_STATUS_CLEAR_1, 1);
			swrm_cmd_fifo_wr_cmd(swrm, 0xFF, i, 0x0,
					SWRS_SCP_INT_STATUS_CLEAR_1);
			swrm_cmd_fifo_wr_cmd(swrm, 0x4, i, 0x0,
					SWRS_SCP_INT_STATUS_MASK_1);
		}
		status >>= 2;
	}
}

static int swrm_check_slave_change_status(struct swr_mstr_ctrl *swrm,
					int status, u8 *devnum)
{
	int i;
	int new_sts = status;
	int ret = SWR_NOT_PRESENT;

	if (status != swrm->slave_status) {
		for (i = 0; i < (swrm->master.num_dev + 1); i++) {
			if ((status & SWRM_MCP_SLV_STATUS_MASK) !=
			    (swrm->slave_status & SWRM_MCP_SLV_STATUS_MASK)) {
				ret = (status & SWRM_MCP_SLV_STATUS_MASK);
				*devnum = i;
				break;
			}
			status >>= 2;
			swrm->slave_status >>= 2;
		}
		swrm->slave_status = new_sts;
	}
	return ret;
}

static irqreturn_t swr_mstr_interrupt(int irq, void *dev)
{
	struct swr_mstr_ctrl *swrm = dev;
	u32 value, intr_sts, intr_sts_masked;
	u32 temp = 0;
	u32 status, chg_sts, i;
	u8 devnum = 0;
	int ret = IRQ_HANDLED;
	struct swr_device *swr_dev;
	struct swr_master *mstr = &swrm->master;
	int retry = 5;

	trace_printk("%s enter\n", __func__);
	if (unlikely(swrm_lock_sleep(swrm) == false)) {
		dev_err(swrm->dev, "%s Failed to hold suspend\n", __func__);
		return IRQ_NONE;
	}

	mutex_lock(&swrm->reslock);
	if (swrm_request_hw_vote(swrm, LPASS_HW_CORE, true)) {
		ret = IRQ_NONE;
		goto exit;
	}
	if (swrm_request_hw_vote(swrm, LPASS_AUDIO_CORE, true)) {
		ret = IRQ_NONE;
		goto err_audio_hw_vote;
	}
	ret = swrm_clk_request(swrm, true);
	if (ret) {
		dev_err(dev, "%s: swrm clk failed\n", __func__);
		ret = IRQ_NONE;
		goto err_audio_core_vote;
	}
	mutex_unlock(&swrm->reslock);

	intr_sts = swr_master_read(swrm, SWRM_INTERRUPT_STATUS);
	intr_sts_masked = intr_sts & swrm->intr_mask;

	dev_dbg(swrm->dev, "%s: status: 0x%x \n", __func__, intr_sts_masked);
	trace_printk("%s: status: 0x%x \n", __func__, intr_sts_masked);
handle_irq:
	for (i = 0; i < SWRM_INTERRUPT_MAX; i++) {
		value = intr_sts_masked & (1 << i);
		if (!value)
			continue;

		switch (value) {
		case SWRM_INTERRUPT_STATUS_SLAVE_PEND_IRQ:
			dev_dbg(swrm->dev, "%s: Trigger irq to slave device\n",
				__func__);
			status = swr_master_read(swrm, SWRM_MCP_SLV_STATUS);
			ret = swrm_find_alert_slave(swrm, status, &devnum);
			if (ret) {
				dev_err_ratelimited(swrm->dev,
				   "%s: no slave alert found.spurious interrupt\n",
					__func__);
				break;
			}
			swrm_cmd_fifo_rd_cmd(swrm, &temp, devnum, 0x0,
						SWRS_SCP_INT_STATUS_CLEAR_1, 1);
			swrm_cmd_fifo_wr_cmd(swrm, 0x4, devnum, 0x0,
						SWRS_SCP_INT_STATUS_CLEAR_1);
			swrm_cmd_fifo_wr_cmd(swrm, 0x0, devnum, 0x0,
						SWRS_SCP_INT_STATUS_CLEAR_1);


			list_for_each_entry(swr_dev, &mstr->devices, dev_list) {
				if (swr_dev->dev_num != devnum)
					continue;
				if (swr_dev->slave_irq) {
					do {
						swr_dev->slave_irq_pending = 0;
						handle_nested_irq(
							irq_find_mapping(
							swr_dev->slave_irq, 0));
					} while (swr_dev->slave_irq_pending);
				}

			}
			break;
		case SWRM_INTERRUPT_STATUS_NEW_SLAVE_ATTACHED:
			dev_dbg(swrm->dev, "%s: SWR new slave attached\n",
				__func__);
			break;
		case SWRM_INTERRUPT_STATUS_CHANGE_ENUM_SLAVE_STATUS:
			status = swr_master_read(swrm, SWRM_MCP_SLV_STATUS);
			swrm_enable_slave_irq(swrm);
			if (status == swrm->slave_status) {
				dev_dbg(swrm->dev,
					"%s: No change in slave status: 0x%x\n",
					__func__, status);
				break;
			}
			chg_sts = swrm_check_slave_change_status(swrm, status,
								&devnum);
			switch (chg_sts) {
			case SWR_NOT_PRESENT:
				dev_dbg(swrm->dev,
					"%s: device %d got detached\n",
					__func__, devnum);
				if (devnum == 0) {
					/*
					 * enable host irq if device 0 detached
					 * as hw will mask host_irq at slave
					 * but will not unmask it afterwards.
					 */
					swrm->enable_slave_irq = true;
				}
				break;
			case SWR_ATTACHED_OK:
				dev_dbg(swrm->dev,
					"%s: device %d got attached\n",
					__func__, devnum);
				/* enable host irq from slave device*/
				swrm->enable_slave_irq = true;
				break;
			case SWR_ALERT:
				dev_dbg(swrm->dev,
					"%s: device %d has pending interrupt\n",
					__func__, devnum);
				break;
			}
			break;
		case SWRM_INTERRUPT_STATUS_MASTER_CLASH_DET:
			dev_err_ratelimited(swrm->dev,
					"%s: SWR bus clsh detected\n",
					__func__);
			swrm->intr_mask &=
				~SWRM_INTERRUPT_STATUS_MASTER_CLASH_DET;
			swr_master_write(swrm,
				SWRM_CPU1_INTERRUPT_EN,
				swrm->intr_mask);
			break;
		case SWRM_INTERRUPT_STATUS_RD_FIFO_OVERFLOW:
			value = swr_master_read(swrm, SWRM_CMD_FIFO_STATUS);
			dev_err(swrm->dev,
				"%s: SWR read FIFO overflow fifo status %x\n",
				__func__, value);
			break;
		case SWRM_INTERRUPT_STATUS_RD_FIFO_UNDERFLOW:
			value = swr_master_read(swrm, SWRM_CMD_FIFO_STATUS);
			dev_err(swrm->dev,
				"%s: SWR read FIFO underflow fifo status %x\n",
				__func__, value);
			break;
		case SWRM_INTERRUPT_STATUS_WR_CMD_FIFO_OVERFLOW:
			value = swr_master_read(swrm, SWRM_CMD_FIFO_STATUS);
			dev_err(swrm->dev,
				"%s: SWR write FIFO overflow fifo status %x\n",
				__func__, value);
			swr_master_write(swrm, SWRM_CMD_FIFO_CMD, 0x1);
			break;
		case SWRM_INTERRUPT_STATUS_CMD_ERROR:
			value = swr_master_read(swrm, SWRM_CMD_FIFO_STATUS);
			dev_err_ratelimited(swrm->dev,
			"%s: SWR CMD error, fifo status 0x%x, flushing fifo\n",
					__func__, value);
			swr_master_write(swrm, SWRM_CMD_FIFO_CMD, 0x1);
			break;
		case SWRM_INTERRUPT_STATUS_DOUT_PORT_COLLISION:
			dev_err_ratelimited(swrm->dev,
					"%s: SWR Port collision detected\n",
					__func__);
			swrm->intr_mask &= ~SWRM_INTERRUPT_STATUS_DOUT_PORT_COLLISION;
			swr_master_write(swrm,
				SWRM_CPU1_INTERRUPT_EN, swrm->intr_mask);
			break;
		case SWRM_INTERRUPT_STATUS_READ_EN_RD_VALID_MISMATCH:
			dev_dbg(swrm->dev,
				"%s: SWR read enable valid mismatch\n",
				__func__);
			swrm->intr_mask &=
				~SWRM_INTERRUPT_STATUS_READ_EN_RD_VALID_MISMATCH;
			swr_master_write(swrm,
				 SWRM_CPU1_INTERRUPT_EN, swrm->intr_mask);
			break;
		case SWRM_INTERRUPT_STATUS_SPECIAL_CMD_ID_FINISHED:
			complete(&swrm->broadcast);
			dev_dbg(swrm->dev, "%s: SWR cmd id finished\n",
				__func__);
			break;
		case SWRM_INTERRUPT_STATUS_AUTO_ENUM_FAILED:
			swr_master_write(swrm, SWRM_ENUMERATOR_CFG, 0);
			while (swr_master_read(swrm, SWRM_ENUMERATOR_STATUS)) {
				if (!retry) {
					dev_dbg(swrm->dev,
						"%s: ENUM status is not idle\n",
						__func__);
					break;
				}
				retry--;
			}
			swr_master_write(swrm, SWRM_ENUMERATOR_CFG, 1);
			break;
		case SWRM_INTERRUPT_STATUS_AUTO_ENUM_TABLE_IS_FULL:
			break;
		case SWRM_INTERRUPT_STATUS_BUS_RESET_FINISHED:
			swrm_check_link_status(swrm, 0x1);
			break;
		case SWRM_INTERRUPT_STATUS_CLK_STOP_FINISHED:
			break;
		case SWRM_INTERRUPT_STATUS_EXT_CLK_STOP_WAKEUP:
			if (swrm->state == SWR_MSTR_UP) {
				dev_dbg(swrm->dev,
					"%s:SWR Master is already up\n",
					__func__);
			} else {
				dev_err_ratelimited(swrm->dev,
					"%s: SWR wokeup during clock stop\n",
					__func__);
				/* It might be possible the slave device gets
				 * reset and slave interrupt gets missed. So
				 * re-enable Host IRQ and process slave pending
				 * interrupts, if any.
				 */
				swrm_enable_slave_irq(swrm);
			}
			break;
		default:
			dev_err_ratelimited(swrm->dev,
					"%s: SWR unknown interrupt value: %d\n",
					__func__, value);
			ret = IRQ_NONE;
			break;
		}
	}
	swr_master_write(swrm, SWRM_INTERRUPT_CLEAR, intr_sts);
	swr_master_write(swrm, SWRM_INTERRUPT_CLEAR, 0x0);

	if (swrm->enable_slave_irq) {
		/* Enable slave irq here */
		swrm_enable_slave_irq(swrm);
		swrm->enable_slave_irq = false;
	}

	intr_sts = swr_master_read(swrm, SWRM_INTERRUPT_STATUS);
	intr_sts_masked = intr_sts & swrm->intr_mask;

	if (intr_sts_masked) {
		dev_dbg(swrm->dev, "%s: new interrupt received 0x%x\n",
			__func__, intr_sts_masked);
		goto handle_irq;
	}

	mutex_lock(&swrm->reslock);
	swrm_clk_request(swrm, false);
err_audio_core_vote:
	swrm_request_hw_vote(swrm, LPASS_AUDIO_CORE, false);

err_audio_hw_vote:
	swrm_request_hw_vote(swrm, LPASS_HW_CORE, false);
exit:
	mutex_unlock(&swrm->reslock);
	swrm_unlock_sleep(swrm);
	trace_printk("%s exit\n", __func__);
	return ret;
}

static irqreturn_t swrm_wakeup_interrupt(int irq, void *dev)
{
	struct swr_mstr_ctrl *swrm = dev;
	int ret = IRQ_HANDLED;

	if (!swrm || !(swrm->dev)) {
		pr_err("%s: swrm or dev is null\n", __func__);
		return IRQ_NONE;
	}

	trace_printk("%s enter\n", __func__);
	mutex_lock(&swrm->devlock);
	if (swrm->state == SWR_MSTR_SSR || !swrm->dev_up) {
		if (swrm->wake_irq > 0) {
			if (unlikely(!irq_get_irq_data(swrm->wake_irq))) {
				pr_err("%s: irq data is NULL\n", __func__);
				mutex_unlock(&swrm->devlock);
				return IRQ_NONE;
			}
			mutex_lock(&swrm->irq_lock);
			if (!irqd_irq_disabled(
			    irq_get_irq_data(swrm->wake_irq)))
				disable_irq_nosync(swrm->wake_irq);
			mutex_unlock(&swrm->irq_lock);
		}
		mutex_unlock(&swrm->devlock);
		return ret;
	}
	mutex_unlock(&swrm->devlock);
	if (unlikely(swrm_lock_sleep(swrm) == false)) {
		dev_err(swrm->dev, "%s Failed to hold suspend\n", __func__);
		goto exit;
	}
	if (swrm->wake_irq > 0) {
		if (unlikely(!irq_get_irq_data(swrm->wake_irq))) {
			pr_err("%s: irq data is NULL\n", __func__);
			return IRQ_NONE;
		}
		mutex_lock(&swrm->irq_lock);
		if (!irqd_irq_disabled(
		    irq_get_irq_data(swrm->wake_irq)))
			disable_irq_nosync(swrm->wake_irq);
		mutex_unlock(&swrm->irq_lock);
	}
	pm_runtime_get_sync(swrm->dev);
	pm_runtime_mark_last_busy(swrm->dev);
	pm_runtime_put_autosuspend(swrm->dev);
	swrm_unlock_sleep(swrm);
exit:
	trace_printk("%s exit\n", __func__);
	return ret;
}

static void swrm_wakeup_work(struct work_struct *work)
{
	struct swr_mstr_ctrl *swrm;

	swrm = container_of(work, struct swr_mstr_ctrl,
			     wakeup_work);
	if (!swrm || !(swrm->dev)) {
		pr_err("%s: swrm or dev is null\n", __func__);
		return;
	}

	trace_printk("%s enter\n", __func__);
	mutex_lock(&swrm->devlock);
	if (!swrm->dev_up) {
		mutex_unlock(&swrm->devlock);
		goto exit;
	}
	mutex_unlock(&swrm->devlock);
	if (unlikely(swrm_lock_sleep(swrm) == false)) {
		dev_err(swrm->dev, "%s Failed to hold suspend\n", __func__);
		goto exit;
	}
	pm_runtime_get_sync(swrm->dev);
	pm_runtime_mark_last_busy(swrm->dev);
	pm_runtime_put_autosuspend(swrm->dev);
	swrm_unlock_sleep(swrm);
exit:
	trace_printk("%s exit\n", __func__);
	pm_relax(swrm->dev);
}

static int swrm_get_device_status(struct swr_mstr_ctrl *swrm, u8 devnum)
{
	u32 val;

	swrm->slave_status = swr_master_read(swrm, SWRM_MCP_SLV_STATUS);
	val = (swrm->slave_status >> (devnum * 2));
	val &= SWRM_MCP_SLV_STATUS_MASK;
	return val;
}

static int swrm_get_logical_dev_num(struct swr_master *mstr, u64 dev_id,
				u8 *dev_num)
{
	int i;
	u64 id = 0;
	int ret = -EINVAL;
	struct swr_mstr_ctrl *swrm = swr_get_ctrl_data(mstr);
	struct swr_device *swr_dev;
	u32 num_dev = 0;

	if (!swrm) {
		pr_err("%s: Invalid handle to swr controller\n",
			__func__);
		return ret;
	}
	if (swrm->num_dev)
		num_dev = swrm->num_dev;
	else
		num_dev = mstr->num_dev;

	mutex_lock(&swrm->devlock);
	if (!swrm->dev_up) {
		mutex_unlock(&swrm->devlock);
		return ret;
	}
	mutex_unlock(&swrm->devlock);

	pm_runtime_get_sync(swrm->dev);
	for (i = 1; i < (num_dev + 1); i++) {
		id = ((u64)(swr_master_read(swrm,
			    SWRM_ENUMERATOR_SLAVE_DEV_ID_2(i))) << 32);
		id |= swr_master_read(swrm,
					SWRM_ENUMERATOR_SLAVE_DEV_ID_1(i));

		/*
		 * As pm_runtime_get_sync() brings all slaves out of reset
		 * update logical device number for all slaves.
		 */
		list_for_each_entry(swr_dev, &mstr->devices, dev_list) {
			if (swr_dev->addr == (id & SWR_DEV_ID_MASK)) {
				u32 status = swrm_get_device_status(swrm, i);

				if ((status == 0x01) || (status == 0x02)) {
					swr_dev->dev_num = i;
					if ((id & SWR_DEV_ID_MASK) == dev_id) {
						*dev_num = i;
						ret = 0;
						dev_info(swrm->dev,
							"%s: devnum %d assigned for dev %llx\n",
							__func__, i,
							swr_dev->addr);
						swrm->logical_dev[i] = swr_dev->addr;
					}
				}
			}
		}
	}
	if (ret)
		dev_err_ratelimited(swrm->dev,
				"%s: device 0x%llx is not ready\n",
				__func__, dev_id);

	pm_runtime_mark_last_busy(swrm->dev);
	pm_runtime_put_autosuspend(swrm->dev);
	return ret;
}

static void swrm_device_wakeup_vote(struct swr_master *mstr)
{
	struct swr_mstr_ctrl *swrm = swr_get_ctrl_data(mstr);

	if (!swrm) {
		pr_err("%s: Invalid handle to swr controller\n",
			__func__);
		return;
	}
	if (unlikely(swrm_lock_sleep(swrm) == false)) {
		dev_err(swrm->dev, "%s Failed to hold suspend\n", __func__);
		return;
	}
	if (swrm_request_hw_vote(swrm, LPASS_HW_CORE, true))
		dev_err(swrm->dev, "%s:lpass core hw enable failed\n",
			__func__);
	if (swrm_request_hw_vote(swrm, LPASS_AUDIO_CORE, true))
		dev_err(swrm->dev, "%s:lpass audio hw enable failed\n",
			__func__);

	pm_runtime_get_sync(swrm->dev);
}

static void swrm_device_wakeup_unvote(struct swr_master *mstr)
{
	struct swr_mstr_ctrl *swrm = swr_get_ctrl_data(mstr);

	if (!swrm) {
		pr_err("%s: Invalid handle to swr controller\n",
			__func__);
		return;
	}
	pm_runtime_mark_last_busy(swrm->dev);
	pm_runtime_put_autosuspend(swrm->dev);

	swrm_request_hw_vote(swrm, LPASS_AUDIO_CORE, false);
	swrm_request_hw_vote(swrm, LPASS_HW_CORE, false);

	swrm_unlock_sleep(swrm);
}

static int swrm_master_init(struct swr_mstr_ctrl *swrm)
{
	int ret = 0, i = 0;
	u32 val;
	u8 row_ctrl = SWR_ROW_50;
	u8 col_ctrl = SWR_MIN_COL;
	u8 ssp_period = 1;
	u8 retry_cmd_num = 3;
	u32 reg[SWRM_MAX_INIT_REG];
	u32 value[SWRM_MAX_INIT_REG];
	u32 temp = 0;
	int len = 0;

	/* SW workaround to gate hw_ctl for SWR version >=1.6 */
	if (swrm->version >= SWRM_VERSION_1_6) {
		if (swrm->swrm_hctl_reg) {
			temp = ioread32(swrm->swrm_hctl_reg);
			temp &= 0xFFFFFFFD;
			iowrite32(temp, swrm->swrm_hctl_reg);
			usleep_range(500, 505);
			temp = ioread32(swrm->swrm_hctl_reg);
			dev_dbg(swrm->dev, "%s: hctl_reg val: 0x%x\n",
				__func__, temp);
		}
	}
	ssp_period = swrm_get_ssp_period(swrm, SWRM_ROW_50,
					SWRM_COL_02, SWRM_FRAME_SYNC_SEL);
	dev_dbg(swrm->dev, "%s: ssp_period: %d\n", __func__, ssp_period);

	/* Clear Rows and Cols */
	val = ((row_ctrl << SWRM_MCP_FRAME_CTRL_BANK_ROW_CTRL_SHFT) |
		(col_ctrl << SWRM_MCP_FRAME_CTRL_BANK_COL_CTRL_SHFT) |
		((ssp_period - 1) << SWRM_MCP_FRAME_CTRL_BANK_SSP_PERIOD_SHFT));

	reg[len] = SWRM_MCP_FRAME_CTRL_BANK(0);
	value[len++] = val;

	/* Set Auto enumeration flag */
	reg[len] = SWRM_ENUMERATOR_CFG;
	value[len++] = 1;

	/* Configure No pings */
	val = swr_master_read(swrm, SWRM_MCP_CFG);
	val &= ~SWRM_NUM_PINGS_MASK;
	val |= (0x1f << SWRM_NUM_PINGS_POS);
	reg[len] = SWRM_MCP_CFG;
	value[len++] = val;

	/* Configure number of retries of a read/write cmd */
	val = (retry_cmd_num);
	reg[len] = SWRM_CMD_FIFO_CFG;
	value[len++] = val;

	reg[len] = SWRM_MCP_BUS_CTRL;
	value[len++] = 0x2;

	/* Set IRQ to PULSE */
	reg[len] = SWRM_COMP_CFG;
	value[len++] = 0x02;

	reg[len] = SWRM_INTERRUPT_CLEAR;
	value[len++] = 0xFFFFFFFF;

	swrm->intr_mask = SWRM_INTERRUPT_STATUS_MASK;
	/* Mask soundwire interrupts */
	reg[len] = SWRM_INTERRUPT_EN;
	value[len++] = swrm->intr_mask;

	reg[len] = SWRM_CPU1_INTERRUPT_EN;
	value[len++] = swrm->intr_mask;

	reg[len] = SWRM_COMP_CFG;
	value[len++] = 0x03;

	swr_master_bulk_write(swrm, reg, value, len);

	if (!swrm_check_link_status(swrm, 0x1)) {
		dev_err(swrm->dev,
			"%s: swr link failed to connect\n",
			__func__);
		for (i = 0; i < len; i++) {
			usleep_range(50, 55);
			dev_err(swrm->dev,
				"%s:reg:0x%x val:0x%x\n",
				__func__,
				reg[i], swr_master_read(swrm, reg[i]));
		}
		return -EINVAL;
	}

	/* Execute it for versions >= 1.5.1 */
	if (swrm->version >= SWRM_VERSION_1_5_1)
		swr_master_write(swrm, SWRM_CMD_FIFO_CFG,
				(swr_master_read(swrm,
					SWRM_CMD_FIFO_CFG) | 0x80000000));

	return ret;
}

static int swrm_event_notify(struct notifier_block *self,
			     unsigned long action, void *data)
{
	struct swr_mstr_ctrl *swrm = container_of(self, struct swr_mstr_ctrl,
						  event_notifier);

	if (!swrm || !(swrm->dev)) {
		pr_err("%s: swrm or dev is NULL\n", __func__);
		return -EINVAL;
	}
	switch (action) {
	case MSM_AUD_DC_EVENT:
		schedule_work(&(swrm->dc_presence_work));
		break;
	case SWR_WAKE_IRQ_EVENT:
		if (swrm->ipc_wakeup && !swrm->ipc_wakeup_triggered) {
			swrm->ipc_wakeup_triggered = true;
			pm_stay_awake(swrm->dev);
			schedule_work(&swrm->wakeup_work);
		}
		break;
	default:
		dev_err(swrm->dev, "%s: invalid event type: %lu\n",
			__func__, action);
		return -EINVAL;
	}

	return 0;
}

static void swrm_notify_work_fn(struct work_struct *work)
{
	struct swr_mstr_ctrl *swrm = container_of(work, struct swr_mstr_ctrl,
						  dc_presence_work);

	if (!swrm || !swrm->pdev) {
		pr_err("%s: swrm or pdev is NULL\n", __func__);
		return;
	}
	swrm_wcd_notify(swrm->pdev, SWR_DEVICE_DOWN, NULL);
}

static int swrm_probe(struct platform_device *pdev)
{
	struct swr_mstr_ctrl *swrm;
	struct swr_ctrl_platform_data *pdata;
	u32 i, num_ports, port_num, port_type, ch_mask, swrm_hctl_reg = 0;
	u32 *temp, map_size, map_length, ch_iter = 0, old_port_num = 0;
	int ret = 0;
	struct clk *lpass_core_hw_vote = NULL;
	struct clk *lpass_core_audio = NULL;
	u32 is_wcd937x = 0;

	/* Allocate soundwire master driver structure */
	swrm = devm_kzalloc(&pdev->dev, sizeof(struct swr_mstr_ctrl),
			GFP_KERNEL);
	if (!swrm) {
		ret = -ENOMEM;
		goto err_memory_fail;
	}
	swrm->pdev = pdev;
	swrm->dev = &pdev->dev;
	platform_set_drvdata(pdev, swrm);
	swr_set_ctrl_data(&swrm->master, swrm);
	pdata = dev_get_platdata(&pdev->dev);
	if (!pdata) {
		dev_err(&pdev->dev, "%s: pdata from parent is NULL\n",
			__func__);
		ret = -EINVAL;
		goto err_pdata_fail;
	}
	swrm->handle = (void *)pdata->handle;
	if (!swrm->handle) {
		dev_err(&pdev->dev, "%s: swrm->handle is NULL\n",
			__func__);
		ret = -EINVAL;
		goto err_pdata_fail;
	}
	ret = of_property_read_u32(pdev->dev.of_node, "qcom,swr_master_id",
				&swrm->master_id);
	if (ret) {
		dev_err(&pdev->dev, "%s: failed to get master id\n", __func__);
		goto err_pdata_fail;
	}
	/* update the physical device address if wcd937x. */
	ret = of_property_read_u32(pdev->dev.of_node, "qcom,is_wcd937x",
				&is_wcd937x);
	if (ret)
		dev_dbg(&pdev->dev, "%s: failed to get wcd info\n", __func__);
	else if (is_wcd937x)
		swrm_phy_dev[1] = 0xa01170223;

	ret = of_property_read_u32(pdev->dev.of_node, "qcom,dynamic-port-map-supported",
				&swrm->dynamic_port_map_supported);
	if (ret) {
		dev_dbg(&pdev->dev,
			"%s: failed to get dynamic port map support, use default\n",
			__func__);
		swrm->dynamic_port_map_supported = 1;
	}

	if (!(of_property_read_u32(pdev->dev.of_node,
			"swrm-io-base", &swrm->swrm_base_reg)))
		ret = of_property_read_u32(pdev->dev.of_node,
			"swrm-io-base", &swrm->swrm_base_reg);
	if (!swrm->swrm_base_reg) {
		swrm->read = pdata->read;
		if (!swrm->read) {
			dev_err(&pdev->dev, "%s: swrm->read is NULL\n",
				__func__);
			ret = -EINVAL;
			goto err_pdata_fail;
		}
		swrm->write = pdata->write;
		if (!swrm->write) {
			dev_err(&pdev->dev, "%s: swrm->write is NULL\n",
				__func__);
			ret = -EINVAL;
			goto err_pdata_fail;
		}
		swrm->bulk_write = pdata->bulk_write;
		if (!swrm->bulk_write) {
			dev_err(&pdev->dev, "%s: swrm->bulk_write is NULL\n",
				__func__);
			ret = -EINVAL;
			goto err_pdata_fail;
		}
	} else {
		swrm->swrm_dig_base = devm_ioremap(&pdev->dev,
					swrm->swrm_base_reg, SWRM_MAX_REGISTER);
	}

	swrm->core_vote = pdata->core_vote;
	if (!(of_property_read_u32(pdev->dev.of_node,
			"qcom,swrm-hctl-reg", &swrm_hctl_reg)))
		swrm->swrm_hctl_reg = devm_ioremap(&pdev->dev,
						swrm_hctl_reg, 0x4);
	swrm->clk = pdata->clk;
	if (!swrm->clk) {
		dev_err(&pdev->dev, "%s: swrm->clk is NULL\n",
			__func__);
		ret = -EINVAL;
		goto err_pdata_fail;
	}
	if (of_property_read_u32(pdev->dev.of_node,
			"qcom,swr-clock-stop-mode0",
			&swrm->clk_stop_mode0_supp)) {
		swrm->clk_stop_mode0_supp = FALSE;
	}

	ret = of_property_read_u32(swrm->dev->of_node, "qcom,swr-num-dev",
				   &swrm->num_dev);
	if (ret) {
		dev_dbg(&pdev->dev, "%s: Looking up %s property failed\n",
			__func__, "qcom,swr-num-dev");
	} else {
		if (swrm->num_dev > SWRM_NUM_AUTO_ENUM_SLAVES) {
			dev_err(&pdev->dev, "%s: num_dev %d > max limit %d\n",
				__func__, swrm->num_dev,
				SWRM_NUM_AUTO_ENUM_SLAVES);
			ret = -EINVAL;
			goto err_pdata_fail;
		} else {
			swrm->master.num_dev = swrm->num_dev;
		}
	}

	/* Parse soundwire port mapping */
	ret = of_property_read_u32(pdev->dev.of_node, "qcom,swr-num-ports",
				&num_ports);
	if (ret) {
		dev_err(swrm->dev, "%s: Failed to get num_ports\n", __func__);
		goto err_pdata_fail;
	}
	swrm->num_ports = num_ports;

	if (!of_find_property(pdev->dev.of_node, "qcom,swr-port-mapping",
				&map_size)) {
		dev_err(swrm->dev, "missing port mapping\n");
		goto err_pdata_fail;
	}

	map_length = map_size / (3 * sizeof(u32));
	if (num_ports > SWR_MSTR_PORT_LEN) {
		dev_err(&pdev->dev, "%s:invalid number of swr ports\n",
			__func__);
		ret = -EINVAL;
		goto err_pdata_fail;
	}
	temp = devm_kzalloc(&pdev->dev, map_size, GFP_KERNEL);

	if (!temp) {
		ret = -ENOMEM;
		goto err_pdata_fail;
	}
	ret = of_property_read_u32_array(pdev->dev.of_node,
				"qcom,swr-port-mapping", temp, 3 * map_length);
	if (ret) {
		dev_err(swrm->dev, "%s: Failed to read port mapping\n",
					__func__);
		goto err_pdata_fail;
	}

	for (i = 0; i < map_length; i++) {
		port_num = temp[3 * i];
		port_type = temp[3 * i + 1];
		ch_mask = temp[3 * i + 2];

		if (port_num != old_port_num)
			ch_iter = 0;
		swrm->port_mapping[port_num][ch_iter].port_type = port_type;
		swrm->port_mapping[port_num][ch_iter++].ch_mask = ch_mask;
		old_port_num = port_num;
	}
	devm_kfree(&pdev->dev, temp);

	ret = of_property_read_u32(pdev->dev.of_node, "qcom,is-always-on",
				&swrm->is_always_on);
	if (ret)
		dev_dbg(&pdev->dev, "%s: failed to get is_always_on flag\n", __func__);

	swrm->reg_irq = pdata->reg_irq;
	swrm->master.read = swrm_read;
	swrm->master.write = swrm_write;
	swrm->master.bulk_write = swrm_bulk_write;
	swrm->master.get_logical_dev_num = swrm_get_logical_dev_num;
	swrm->master.connect_port = swrm_connect_port;
	swrm->master.disconnect_port = swrm_disconnect_port;
	swrm->master.slvdev_datapath_control = swrm_slvdev_datapath_control;
	swrm->master.remove_from_group = swrm_remove_from_group;
	swrm->master.device_wakeup_vote = swrm_device_wakeup_vote;
	swrm->master.device_wakeup_unvote = swrm_device_wakeup_unvote;
	swrm->master.dev.parent = &pdev->dev;
	swrm->master.dev.of_node = pdev->dev.of_node;
	swrm->master.num_port = 0;
	swrm->rcmd_id = 0;
	swrm->wcmd_id = 0;
	swrm->slave_status = 0;
	swrm->num_rx_chs = 0;
	swrm->clk_ref_count = 0;
	swrm->swr_irq_wakeup_capable = 0;
	swrm->mclk_freq = MCLK_FREQ;
	swrm->bus_clk = MCLK_FREQ;
	swrm->dev_up = true;
	swrm->state = SWR_MSTR_UP;
	swrm->ipc_wakeup = false;
	swrm->ipc_wakeup_triggered = false;
	swrm->disable_div2_clk_switch = FALSE;
	init_completion(&swrm->reset);
	init_completion(&swrm->broadcast);
	init_completion(&swrm->clk_off_complete);
	mutex_init(&swrm->irq_lock);
	mutex_init(&swrm->mlock);
	mutex_init(&swrm->reslock);
	mutex_init(&swrm->force_down_lock);
	mutex_init(&swrm->iolock);
	mutex_init(&swrm->clklock);
	mutex_init(&swrm->devlock);
	mutex_init(&swrm->pm_lock);
	swrm->wlock_holders = 0;
	swrm->pm_state = SWRM_PM_SLEEPABLE;
	init_waitqueue_head(&swrm->pm_wq);
	pm_qos_add_request(&swrm->pm_qos_req,
			   PM_QOS_CPU_DMA_LATENCY,
			   PM_QOS_DEFAULT_VALUE);

	for (i = 0 ; i < SWR_MSTR_PORT_LEN; i++)
		INIT_LIST_HEAD(&swrm->mport_cfg[i].port_req_list);

	if (of_property_read_u32(pdev->dev.of_node,
			"qcom,disable-div2-clk-switch",
			&swrm->disable_div2_clk_switch)) {
		swrm->disable_div2_clk_switch = FALSE;
	}

	/* Register LPASS core hw vote */
	lpass_core_hw_vote = devm_clk_get(&pdev->dev, "lpass_core_hw_vote");
	if (IS_ERR(lpass_core_hw_vote)) {
		ret = PTR_ERR(lpass_core_hw_vote);
		dev_dbg(&pdev->dev, "%s: clk get %s failed %d\n",
			__func__, "lpass_core_hw_vote", ret);
		lpass_core_hw_vote = NULL;
		ret = 0;
	}
	swrm->lpass_core_hw_vote = lpass_core_hw_vote;

	/* Register LPASS audio core vote */
	lpass_core_audio = devm_clk_get(&pdev->dev, "lpass_audio_hw_vote");
	if (IS_ERR(lpass_core_audio)) {
		ret = PTR_ERR(lpass_core_audio);
		dev_dbg(&pdev->dev, "%s: clk get %s failed %d\n",
			__func__, "lpass_core_audio", ret);
		lpass_core_audio = NULL;
		ret = 0;
	}
	swrm->lpass_core_audio = lpass_core_audio;

	if (swrm->reg_irq) {
		ret = swrm->reg_irq(swrm->handle, swr_mstr_interrupt, swrm,
			    SWR_IRQ_REGISTER);
		if (ret) {
			dev_err(&pdev->dev, "%s: IRQ register failed ret %d\n",
				__func__, ret);
			goto err_irq_fail;
		}
	} else {
		swrm->irq = platform_get_irq_byname(pdev, "swr_master_irq");
		if (swrm->irq < 0) {
			dev_err(swrm->dev, "%s() error getting irq hdle: %d\n",
					__func__, swrm->irq);
			goto err_irq_fail;
		}

		ret = request_threaded_irq(swrm->irq, NULL,
					   swr_mstr_interrupt,
					   IRQF_TRIGGER_RISING | IRQF_ONESHOT,
					   "swr_master_irq", swrm);
		if (ret) {
			dev_err(swrm->dev, "%s: Failed to request irq %d\n",
				__func__, ret);
			goto err_irq_fail;
		}

	}
	/* Make inband tx interrupts as wakeup capable for slave irq */
	ret = of_property_read_u32(pdev->dev.of_node,
				   "qcom,swr-mstr-irq-wakeup-capable",
				   &swrm->swr_irq_wakeup_capable);
	if (ret)
		dev_dbg(swrm->dev, "%s: swrm irq wakeup capable not defined\n",
			__func__);
	if (swrm->swr_irq_wakeup_capable)
		irq_set_irq_wake(swrm->irq, 1);
	ret = swr_register_master(&swrm->master);
	if (ret) {
		dev_err(&pdev->dev, "%s: error adding swr master\n", __func__);
		goto err_mstr_fail;
	}

	/* Add devices registered with board-info as the
	 * controller will be up now
	 */
	swr_master_add_boarddevices(&swrm->master);
	if (!swrm->is_always_on && swrm_request_hw_vote(swrm, LPASS_AUDIO_CORE, true))
		dev_dbg(&pdev->dev, "%s: Audio HW Vote is failed\n", __func__);
	mutex_lock(&swrm->mlock);
	swrm_clk_request(swrm, true);
	swrm->version = swr_master_read(swrm, SWRM_COMP_HW_VERSION);
	ret = swrm_master_init(swrm);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"%s: Error in master Initialization , err %d\n",
			__func__, ret);
		mutex_unlock(&swrm->mlock);
		ret = -EPROBE_DEFER;
		goto err_mstr_init_fail;
	}

	mutex_unlock(&swrm->mlock);
	INIT_WORK(&swrm->wakeup_work, swrm_wakeup_work);

	if (pdev->dev.of_node)
		of_register_swr_devices(&swrm->master);

	swrm->rd_fifo_depth = ((swr_master_read(swrm, SWRM_COMP_PARAMS)
				& SWRM_COMP_PARAMS_RD_FIFO_DEPTH) >> 15);
	swrm->wr_fifo_depth = ((swr_master_read(swrm, SWRM_COMP_PARAMS)
				& SWRM_COMP_PARAMS_WR_FIFO_DEPTH) >> 10);

#ifdef CONFIG_DEBUG_FS
	swrm->debugfs_swrm_dent = debugfs_create_dir(dev_name(&pdev->dev), 0);
	if (!IS_ERR(swrm->debugfs_swrm_dent)) {
		swrm->debugfs_peek = debugfs_create_file("swrm_peek",
				S_IFREG | 0444, swrm->debugfs_swrm_dent,
				(void *) swrm, &swrm_debug_read_ops);

		swrm->debugfs_poke = debugfs_create_file("swrm_poke",
				S_IFREG | 0444, swrm->debugfs_swrm_dent,
				(void *) swrm, &swrm_debug_write_ops);

		swrm->debugfs_reg_dump = debugfs_create_file("swrm_reg_dump",
				   S_IFREG | 0444, swrm->debugfs_swrm_dent,
				   (void *) swrm,
				   &swrm_debug_dump_ops);
	}
#endif
	ret = device_init_wakeup(swrm->dev, true);
	if (ret) {
		dev_err(swrm->dev, "Device wakeup init failed: %d\n", ret);
		goto err_irq_wakeup_fail;
	}

	pm_runtime_set_autosuspend_delay(&pdev->dev, auto_suspend_timer);
	pm_runtime_use_autosuspend(&pdev->dev);
	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);
	pm_runtime_mark_last_busy(&pdev->dev);

	INIT_WORK(&swrm->dc_presence_work, swrm_notify_work_fn);
	swrm->event_notifier.notifier_call  = swrm_event_notify;
	msm_aud_evt_register_client(&swrm->event_notifier);

	return 0;
err_irq_wakeup_fail:
	device_init_wakeup(swrm->dev, false);
err_mstr_init_fail:
	swr_unregister_master(&swrm->master);
err_mstr_fail:
	if (swrm->reg_irq) {
		swrm->reg_irq(swrm->handle, swr_mstr_interrupt,
				swrm, SWR_IRQ_FREE);
	} else if (swrm->irq) {
		if (irq_get_irq_data(swrm->irq) != NULL)
			irqd_set_trigger_type(
				irq_get_irq_data(swrm->irq),
				IRQ_TYPE_NONE);
		if (swrm->swr_irq_wakeup_capable)
			irq_set_irq_wake(swrm->irq, 0);
		free_irq(swrm->irq, swrm);
	}
err_irq_fail:
	mutex_destroy(&swrm->irq_lock);
	mutex_destroy(&swrm->mlock);
	mutex_destroy(&swrm->reslock);
	mutex_destroy(&swrm->force_down_lock);
	mutex_destroy(&swrm->iolock);
	mutex_destroy(&swrm->clklock);
	mutex_destroy(&swrm->pm_lock);
	pm_qos_remove_request(&swrm->pm_qos_req);

err_pdata_fail:
err_memory_fail:
	return ret;
}

static int swrm_remove(struct platform_device *pdev)
{
	struct swr_mstr_ctrl *swrm = platform_get_drvdata(pdev);

	if (swrm->reg_irq) {
		swrm->reg_irq(swrm->handle, swr_mstr_interrupt,
				swrm, SWR_IRQ_FREE);
	} else if (swrm->irq) {
		if (irq_get_irq_data(swrm->irq) != NULL)
			irqd_set_trigger_type(
				irq_get_irq_data(swrm->irq),
				IRQ_TYPE_NONE);
		if (swrm->swr_irq_wakeup_capable)
			irq_set_irq_wake(swrm->irq, 0);
		free_irq(swrm->irq, swrm);
	} else if (swrm->wake_irq > 0) {
		free_irq(swrm->wake_irq, swrm);
	}
	cancel_work_sync(&swrm->wakeup_work);
	pm_runtime_disable(&pdev->dev);
	pm_runtime_set_suspended(&pdev->dev);
	swr_unregister_master(&swrm->master);
	msm_aud_evt_unregister_client(&swrm->event_notifier);
	device_init_wakeup(swrm->dev, false);
	mutex_destroy(&swrm->irq_lock);
	mutex_destroy(&swrm->mlock);
	mutex_destroy(&swrm->reslock);
	mutex_destroy(&swrm->iolock);
	mutex_destroy(&swrm->clklock);
	mutex_destroy(&swrm->force_down_lock);
	mutex_destroy(&swrm->pm_lock);
	pm_qos_remove_request(&swrm->pm_qos_req);
	devm_kfree(&pdev->dev, swrm);
	return 0;
}

static int swrm_clk_pause(struct swr_mstr_ctrl *swrm)
{
	u32 val;

	dev_dbg(swrm->dev, "%s: state: %d\n", __func__, swrm->state);
	swr_master_write(swrm, SWRM_INTERRUPT_EN, 0x1FDFD);
	val = swr_master_read(swrm, SWRM_MCP_CFG);
	val |= 0x02;
	swr_master_write(swrm, SWRM_MCP_CFG, val);

	return 0;
}

#ifdef CONFIG_PM
static int swrm_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct swr_mstr_ctrl *swrm = platform_get_drvdata(pdev);
	int ret = 0;
	bool swrm_clk_req_err = false;
	bool hw_core_err = false, aud_core_err = false;
	struct swr_master *mstr = &swrm->master;
	struct swr_device *swr_dev;
	u32 temp = 0;

	dev_dbg(dev, "%s: pm_runtime: resume, state:%d\n",
		__func__, swrm->state);
	trace_printk("%s: pm_runtime: resume, state:%d\n",
		__func__, swrm->state);
	mutex_lock(&swrm->reslock);

	if (swrm_request_hw_vote(swrm, LPASS_HW_CORE, true)) {
		dev_err(dev, "%s:lpass core hw enable failed\n",
			__func__);
		hw_core_err = true;
	}
	if (swrm_request_hw_vote(swrm, LPASS_AUDIO_CORE, true)) {
		dev_err(dev, "%s:lpass audio hw enable failed\n",
			__func__);
		aud_core_err = true;
	}

	if ((swrm->state == SWR_MSTR_DOWN) ||
	    (swrm->state == SWR_MSTR_SSR && swrm->dev_up)) {
		if (swrm->clk_stop_mode0_supp) {
			if (swrm->wake_irq > 0) {
				if (unlikely(!irq_get_irq_data
				    (swrm->wake_irq))) {
					pr_err("%s: irq data is NULL\n",
						__func__);
					mutex_unlock(&swrm->reslock);
					return IRQ_NONE;
				}
				mutex_lock(&swrm->irq_lock);
				if (!irqd_irq_disabled(
				    irq_get_irq_data(swrm->wake_irq)))
					disable_irq_nosync(swrm->wake_irq);
				mutex_unlock(&swrm->irq_lock);
			}
			if (swrm->ipc_wakeup)
				msm_aud_evt_blocking_notifier_call_chain(
					SWR_WAKE_IRQ_DEREGISTER, (void *)swrm);
		}

		if (swrm_clk_request(swrm, true)) {
			/*
			 * Set autosuspend timer to 1 for
			 * master to enter into suspend.
			 */
			swrm_clk_req_err = true;
			goto exit;
		}
		if (!swrm->clk_stop_mode0_supp || swrm->state == SWR_MSTR_SSR) {
			list_for_each_entry(swr_dev, &mstr->devices, dev_list) {
				ret = swr_device_up(swr_dev);
				if (ret == -ENODEV) {
					dev_dbg(dev,
						"%s slave device up not implemented\n",
						__func__);
					trace_printk(
						"%s slave device up not implemented\n",
						__func__);
					ret = 0;
				} else if (ret) {
					dev_err(dev,
						"%s: failed to wakeup swr dev %d\n",
						__func__, swr_dev->dev_num);
					swrm_clk_request(swrm, false);
					goto exit;
				}
			}
			swr_master_write(swrm, SWRM_COMP_SW_RESET, 0x01);
			swr_master_write(swrm, SWRM_COMP_SW_RESET, 0x01);
			swr_master_write(swrm, SWRM_MCP_BUS_CTRL, 0x01);
			swrm_master_init(swrm);
			/* wait for hw enumeration to complete */
			usleep_range(100, 105);
			if (!swrm_check_link_status(swrm, 0x1))
				dev_dbg(dev, "%s:failed in connecting, ssr?\n",
					__func__);
			swrm_cmd_fifo_wr_cmd(swrm, 0x4, 0xF, 0x0,
						SWRS_SCP_INT_STATUS_MASK_1);
			if (swrm->state == SWR_MSTR_SSR) {
				mutex_unlock(&swrm->reslock);
				enable_bank_switch(swrm, 0, SWR_ROW_50, SWR_MIN_COL);
				mutex_lock(&swrm->reslock);
			}
		} else {
			if (swrm->swrm_hctl_reg) {
				temp = ioread32(swrm->swrm_hctl_reg);
				temp &= 0xFFFFFFFD;
				iowrite32(temp, swrm->swrm_hctl_reg);
			}
			/*wake up from clock stop*/
			swr_master_write(swrm, SWRM_MCP_BUS_CTRL, 0x2);
			/* clear and enable bus clash interrupt */
			swr_master_write(swrm, SWRM_INTERRUPT_CLEAR, 0x08);
			swrm->intr_mask |= 0x08;
			swr_master_write(swrm, SWRM_INTERRUPT_EN,
					 swrm->intr_mask);
			swr_master_write(swrm,
					 SWRM_CPU1_INTERRUPT_EN,
					 swrm->intr_mask);
			usleep_range(100, 105);
			if (!swrm_check_link_status(swrm, 0x1))
				dev_dbg(dev, "%s:failed in connecting, ssr?\n",
					__func__);
		}
		swrm->state = SWR_MSTR_UP;
	}
exit:
	if (swrm->is_always_on && !aud_core_err)
		swrm_request_hw_vote(swrm, LPASS_AUDIO_CORE, false);

	if (!hw_core_err)
		swrm_request_hw_vote(swrm, LPASS_HW_CORE, false);
	if (swrm_clk_req_err)
		pm_runtime_set_autosuspend_delay(&pdev->dev,
				ERR_AUTO_SUSPEND_TIMER_VAL);
	else
		pm_runtime_set_autosuspend_delay(&pdev->dev,
				auto_suspend_timer);
	if (swrm->req_clk_switch)
		swrm->req_clk_switch = false;
	mutex_unlock(&swrm->reslock);

	trace_printk("%s: pm_runtime: resume done, state:%d\n",
		__func__, swrm->state);
	return ret;
}

static int swrm_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct swr_mstr_ctrl *swrm = platform_get_drvdata(pdev);
	int ret = 0;
	bool hw_core_err = false, aud_core_err = false;
	struct swr_master *mstr = &swrm->master;
	struct swr_device *swr_dev;
	int current_state = 0;

	trace_printk("%s: pm_runtime: suspend state: %d\n",
		__func__, swrm->state);
	dev_dbg(dev, "%s: pm_runtime: suspend state: %d\n",
		__func__, swrm->state);
	mutex_lock(&swrm->reslock);
	mutex_lock(&swrm->force_down_lock);
	current_state = swrm->state;
	mutex_unlock(&swrm->force_down_lock);

	if (swrm_request_hw_vote(swrm, LPASS_HW_CORE, true)) {
		dev_err(dev, "%s:lpass core hw enable failed\n",
			__func__);
		hw_core_err = true;
	}

	if (swrm->is_always_on && swrm_request_hw_vote(swrm, LPASS_AUDIO_CORE, true))
		aud_core_err = true;
	if ((current_state == SWR_MSTR_UP) ||
	    (current_state == SWR_MSTR_SSR)) {

		if ((current_state != SWR_MSTR_SSR) &&
			swrm_is_port_en(&swrm->master)) {
			dev_dbg(dev, "%s ports are enabled\n", __func__);
			trace_printk("%s ports are enabled\n", __func__);
			ret = -EBUSY;
			goto exit;
		}
		if (!swrm->clk_stop_mode0_supp || swrm->state == SWR_MSTR_SSR) {
			dev_err(dev, "%s: clk stop mode not supported or SSR entry\n",
				__func__);
			mutex_unlock(&swrm->reslock);
			enable_bank_switch(swrm, 0, SWR_ROW_50, SWR_MIN_COL);
			mutex_lock(&swrm->reslock);
			swrm_clk_pause(swrm);
			swr_master_write(swrm, SWRM_COMP_CFG, 0x00);
			list_for_each_entry(swr_dev, &mstr->devices, dev_list) {
				ret = swr_device_down(swr_dev);
				if (ret == -ENODEV) {
					dev_dbg_ratelimited(dev,
						"%s slave device down not implemented\n",
						 __func__);
					trace_printk(
						"%s slave device down not implemented\n",
						 __func__);
					ret = 0;
				} else if (ret) {
					dev_err(dev,
						"%s: failed to shutdown swr dev %d\n",
						__func__, swr_dev->dev_num);
					trace_printk(
						"%s: failed to shutdown swr dev %d\n",
						__func__, swr_dev->dev_num);
					goto exit;
				}
			}
			trace_printk("%s: clk stop mode not supported or SSR exit\n",
				__func__);
		} else {
			/* Mask bus clash interrupt */
			swrm->intr_mask &= ~((u32)0x08);
			swr_master_write(swrm, SWRM_INTERRUPT_EN,
					 swrm->intr_mask);
			swr_master_write(swrm,
					 SWRM_CPU1_INTERRUPT_EN,
					 swrm->intr_mask);
			mutex_unlock(&swrm->reslock);
			/* clock stop sequence */
			swrm_cmd_fifo_wr_cmd(swrm, 0x2, 0xF, 0xF,
					SWRS_SCP_CONTROL);
			mutex_lock(&swrm->reslock);
			usleep_range(100, 105);
		}
		if (!swrm_check_link_status(swrm, 0x0))
			dev_dbg(dev, "%s:failed in disconnecting, ssr?\n",
				__func__);
		ret = swrm_clk_request(swrm, false);
		if (ret) {
			dev_err(dev, "%s: swrmn clk failed\n", __func__);
			ret = 0;
			goto exit;
		}

		if (swrm->clk_stop_mode0_supp) {
			if (swrm->wake_irq > 0) {
				enable_irq(swrm->wake_irq);
			} else if (swrm->ipc_wakeup) {
				msm_aud_evt_blocking_notifier_call_chain(
					SWR_WAKE_IRQ_REGISTER, (void *)swrm);
				swrm->ipc_wakeup_triggered = false;
			}
		}
	}

	/* Retain  SSR state until resume */
	if (current_state != SWR_MSTR_SSR)
		swrm->state = SWR_MSTR_DOWN;

exit:
	if (!swrm->is_always_on && swrm->state != SWR_MSTR_UP) {
		if (swrm_request_hw_vote(swrm, LPASS_AUDIO_CORE, false))
			dev_dbg(dev, "%s:lpass audio hw enable failed\n",
			__func__);
	} else if (swrm->is_always_on && !aud_core_err)
		swrm_request_hw_vote(swrm, LPASS_AUDIO_CORE, false);

	if (!hw_core_err)
		swrm_request_hw_vote(swrm, LPASS_HW_CORE, false);
	mutex_unlock(&swrm->reslock);
	trace_printk("%s: pm_runtime: suspend done state: %d\n",
		__func__, swrm->state);
	return ret;
}
#endif /* CONFIG_PM */

static int swrm_device_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct swr_mstr_ctrl *swrm = platform_get_drvdata(pdev);
	int ret = 0;

	dev_dbg(dev, "%s: swrm state: %d\n", __func__, swrm->state);
	trace_printk("%s: swrm state: %d\n", __func__, swrm->state);
	if (!pm_runtime_enabled(dev) || !pm_runtime_suspended(dev)) {
		ret = swrm_runtime_suspend(dev);
		if (!ret) {
			pm_runtime_disable(dev);
			pm_runtime_set_suspended(dev);
			pm_runtime_enable(dev);
		}
	}

	return 0;
}

static int swrm_device_down(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct swr_mstr_ctrl *swrm = platform_get_drvdata(pdev);

	dev_dbg(dev, "%s: swrm state: %d\n", __func__, swrm->state);
	trace_printk("%s: swrm state: %d\n", __func__, swrm->state);

	mutex_lock(&swrm->force_down_lock);
	swrm->state = SWR_MSTR_SSR;
	mutex_unlock(&swrm->force_down_lock);

	swrm_device_suspend(dev);
	return 0;
}

int swrm_register_wake_irq(struct swr_mstr_ctrl *swrm)
{
	int ret = 0;
	int irq, dir_apps_irq;

	if (!swrm->ipc_wakeup) {
		irq = of_get_named_gpio(swrm->dev->of_node,
					"qcom,swr-wakeup-irq", 0);
		if (gpio_is_valid(irq)) {
			swrm->wake_irq = gpio_to_irq(irq);
			if (swrm->wake_irq < 0) {
				dev_err(swrm->dev,
					"Unable to configure irq\n");
				return swrm->wake_irq;
			}
		} else {
			dir_apps_irq = platform_get_irq_byname(swrm->pdev,
							"swr_wake_irq");
			if (dir_apps_irq < 0) {
				dev_err(swrm->dev,
					"TLMM connect gpio not found\n");
				return -EINVAL;
			}
			swrm->wake_irq = dir_apps_irq;
		}
		ret = request_threaded_irq(swrm->wake_irq, NULL,
					   swrm_wakeup_interrupt,
					   IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
					   "swr_wake_irq", swrm);
		if (ret) {
			dev_err(swrm->dev, "%s: Failed to request irq %d\n",
				__func__, ret);
			return -EINVAL;
		}
		irq_set_irq_wake(swrm->wake_irq, 1);
	}
	return ret;
}

static int swrm_alloc_port_mem(struct device *dev, struct swr_mstr_ctrl *swrm,
				u32 uc, u32 size)
{
	if (!swrm->port_param) {
		swrm->port_param = devm_kzalloc(dev,
					sizeof(swrm->port_param) * SWR_UC_MAX,
					GFP_KERNEL);
		if (!swrm->port_param)
			return -ENOMEM;
	}
	if (!swrm->port_param[uc]) {
		swrm->port_param[uc] = devm_kcalloc(dev, size,
					sizeof(struct port_params),
					GFP_KERNEL);
		if (!swrm->port_param[uc])
			return -ENOMEM;
	} else {
		dev_err_ratelimited(swrm->dev, "%s: called more than once\n",
				    __func__);
	}

	return 0;
}

static int swrm_copy_port_config(struct swr_mstr_ctrl *swrm,
				struct swrm_port_config *port_cfg,
				u32 size)
{
	int idx;
	struct port_params *params;
	int uc = port_cfg->uc;
	int ret = 0;

	for (idx = 0; idx < size; idx++) {
		params = &((struct port_params *)port_cfg->params)[idx];
		if (!params) {
			dev_err(swrm->dev, "%s: Invalid params\n", __func__);
			ret = -EINVAL;
			break;
		}
		memcpy(&swrm->port_param[uc][idx], params,
					sizeof(struct port_params));
	}

	return ret;
}

/**
 * swrm_wcd_notify - parent device can notify to soundwire master through
 * this function
 * @pdev: pointer to platform device structure
 * @id: command id from parent to the soundwire master
 * @data: data from parent device to soundwire master
 */
int swrm_wcd_notify(struct platform_device *pdev, u32 id, void *data)
{
	struct swr_mstr_ctrl *swrm;
	int ret = 0;
	struct swr_master *mstr;
	struct swr_device *swr_dev;
	struct swrm_port_config *port_cfg;

	if (!pdev) {
		pr_err("%s: pdev is NULL\n", __func__);
		return -EINVAL;
	}
	swrm = platform_get_drvdata(pdev);
	if (!swrm) {
		dev_err(&pdev->dev, "%s: swrm is NULL\n", __func__);
		return -EINVAL;
	}
	mstr = &swrm->master;

	switch (id) {
	case SWR_REQ_CLK_SWITCH:
		/* This will put soundwire in clock stop mode and disable the
		 * clocks, if there is no active usecase running, so that the
		 * next activity on soundwire will request clock from new clock
		 * source.
		 */
		if (!data) {
			dev_err(swrm->dev, "%s: data is NULL for id:%d\n",
				__func__, id);
			ret = -EINVAL;
			break;
		}
		mutex_lock(&swrm->mlock);
		if (swrm->clk_src != *(int *)data) {
			if (swrm->state == SWR_MSTR_UP) {
				swrm->req_clk_switch = true;
				swrm_device_suspend(&pdev->dev);
				if (swrm->state == SWR_MSTR_UP)
					swrm->req_clk_switch = false;
			}
			swrm->clk_src = *(int *)data;
		}
		mutex_unlock(&swrm->mlock);
		break;
	case SWR_CLK_FREQ:
		if (!data) {
			dev_err(swrm->dev, "%s: data is NULL\n", __func__);
			ret = -EINVAL;
		} else {
			mutex_lock(&swrm->mlock);
			if (swrm->mclk_freq != *(int *)data) {
				dev_dbg(swrm->dev, "%s: freq change: force mstr down\n", __func__);
				if (swrm->state == SWR_MSTR_DOWN)
					dev_dbg(swrm->dev, "%s:SWR master is already Down:%d\n",
						__func__, swrm->state);
				else {
					swrm->mclk_freq = *(int *)data;
					swrm->bus_clk = swrm->mclk_freq;
					swrm_switch_frame_shape(swrm,
								swrm->bus_clk);
					swrm_device_suspend(&pdev->dev);
				}
				/*
				 * add delay to ensure clk release happen
				 * if interrupt triggered for clk stop,
				 * wait for it to exit
				 */
				usleep_range(10000, 10500);
			}
			swrm->mclk_freq = *(int *)data;
			swrm->bus_clk = swrm->mclk_freq;
			mutex_unlock(&swrm->mlock);
		}
		break;
	case SWR_DEVICE_SSR_DOWN:
		trace_printk("%s: swr device down called\n", __func__);
		mutex_lock(&swrm->mlock);
		if (swrm->state == SWR_MSTR_DOWN)
			dev_dbg(swrm->dev, "%s:SWR master is already Down:%d\n",
				__func__, swrm->state);
		else
			swrm_device_down(&pdev->dev);
		mutex_lock(&swrm->devlock);
		swrm->dev_up = false;
		swrm->hw_core_clk_en = 0;
		swrm->aud_core_clk_en = 0;
		mutex_unlock(&swrm->devlock);
		mutex_lock(&swrm->reslock);
		swrm->state = SWR_MSTR_SSR;
		mutex_unlock(&swrm->reslock);
		mutex_unlock(&swrm->mlock);
		break;
	case SWR_DEVICE_SSR_UP:
		/* wait for clk voting to be zero */
		trace_printk("%s: swr device up  called\n", __func__);
		reinit_completion(&swrm->clk_off_complete);
		if (swrm->clk_ref_count &&
			 !wait_for_completion_timeout(&swrm->clk_off_complete,
						   msecs_to_jiffies(500)))
			dev_err(swrm->dev, "%s: clock voting not zero\n",
				__func__);

		mutex_lock(&swrm->devlock);
		swrm->dev_up = true;
		mutex_unlock(&swrm->devlock);
		break;
	case SWR_DEVICE_DOWN:
		dev_dbg(swrm->dev, "%s: swr master down called\n", __func__);
		trace_printk("%s: swr master down called\n", __func__);
		mutex_lock(&swrm->mlock);
		if (swrm->state == SWR_MSTR_DOWN)
			dev_dbg(swrm->dev, "%s:SWR master is already Down:%d\n",
				__func__, swrm->state);
		else
			swrm_device_down(&pdev->dev);
		mutex_unlock(&swrm->mlock);
		break;
	case SWR_DEVICE_UP:
		dev_dbg(swrm->dev, "%s: swr master up called\n", __func__);
		trace_printk("%s: swr master up called\n", __func__);
		mutex_lock(&swrm->devlock);
		if (!swrm->dev_up) {
			dev_dbg(swrm->dev, "SSR not complete yet\n");
			mutex_unlock(&swrm->devlock);
			return -EBUSY;
		}
		mutex_unlock(&swrm->devlock);
		mutex_lock(&swrm->mlock);
		pm_runtime_mark_last_busy(&pdev->dev);
		pm_runtime_get_sync(&pdev->dev);
		mutex_lock(&swrm->reslock);
		list_for_each_entry(swr_dev, &mstr->devices, dev_list) {
			ret = swr_reset_device(swr_dev);
			if (ret == -ENODEV) {
				dev_dbg_ratelimited(swrm->dev,
					"%s slave reset not implemented\n",
					__func__);
				ret = 0;
			} else if (ret) {
				dev_err(swrm->dev,
					"%s: failed to reset swr device %d\n",
					__func__, swr_dev->dev_num);
				swrm_clk_request(swrm, false);
			}
		}
		pm_runtime_mark_last_busy(&pdev->dev);
		pm_runtime_put_autosuspend(&pdev->dev);
		mutex_unlock(&swrm->reslock);
		mutex_unlock(&swrm->mlock);
		break;
	case SWR_SET_NUM_RX_CH:
		if (!data) {
			dev_err(swrm->dev, "%s: data is NULL\n", __func__);
			ret = -EINVAL;
		} else {
			mutex_lock(&swrm->mlock);
			swrm->num_rx_chs = *(int *)data;
			if ((swrm->num_rx_chs > 1) && !swrm->num_cfg_devs) {
				list_for_each_entry(swr_dev, &mstr->devices,
						    dev_list) {
					ret = swr_set_device_group(swr_dev,
								SWR_BROADCAST);
					if (ret)
						dev_err(swrm->dev,
							"%s: set num ch failed\n",
							__func__);
				}
			} else {
				list_for_each_entry(swr_dev, &mstr->devices,
						    dev_list) {
					ret = swr_set_device_group(swr_dev,
								SWR_GROUP_NONE);
					if (ret)
						dev_err(swrm->dev,
							"%s: set num ch failed\n",
							__func__);
				}
			}
			mutex_unlock(&swrm->mlock);
		}
		break;
	case SWR_REGISTER_WAKE_IRQ:
		if (!data) {
			dev_err(swrm->dev, "%s: reg wake irq data is NULL\n",
				__func__);
			ret = -EINVAL;
		} else {
			mutex_lock(&swrm->mlock);
			swrm->ipc_wakeup = *(u32 *)data;
			ret = swrm_register_wake_irq(swrm);
			if (ret)
				dev_err(swrm->dev, "%s: register wake_irq failed\n",
					__func__);
			mutex_unlock(&swrm->mlock);
		}
		break;
	case SWR_REGISTER_WAKEUP:
		msm_aud_evt_blocking_notifier_call_chain(
					SWR_WAKE_IRQ_REGISTER, (void *)swrm);
		break;
	case SWR_DEREGISTER_WAKEUP:
		msm_aud_evt_blocking_notifier_call_chain(
					SWR_WAKE_IRQ_DEREGISTER, (void *)swrm);
		break;
	case SWR_SET_PORT_MAP:
		if (!data) {
			dev_err(swrm->dev, "%s: data is NULL for id=%d\n",
				__func__, id);
			ret = -EINVAL;
		} else {
			mutex_lock(&swrm->mlock);
			port_cfg = (struct swrm_port_config *)data;
			if (!port_cfg->size) {
				ret = -EINVAL;
				goto done;
			}
			ret = swrm_alloc_port_mem(&pdev->dev, swrm,
						port_cfg->uc, port_cfg->size);
			if (!ret)
				swrm_copy_port_config(swrm, port_cfg,
						      port_cfg->size);
done:
			mutex_unlock(&swrm->mlock);
		}
		break;
	default:
		dev_err(swrm->dev, "%s: swr master unknown id %d\n",
			__func__, id);
		break;
	}
	return ret;
}
EXPORT_SYMBOL(swrm_wcd_notify);

/*
 * swrm_pm_cmpxchg:
 *      Check old state and exchange with pm new state
 *      if old state matches with current state
 *
 * @swrm: pointer to wcd core resource
 * @o: pm old state
 * @n: pm new state
 *
 * Returns old state
 */
static enum swrm_pm_state swrm_pm_cmpxchg(
				struct swr_mstr_ctrl *swrm,
				enum swrm_pm_state o,
				enum swrm_pm_state n)
{
	enum swrm_pm_state old;

	if (!swrm)
		return o;

	mutex_lock(&swrm->pm_lock);
	old = swrm->pm_state;
	if (old == o)
		swrm->pm_state = n;
	mutex_unlock(&swrm->pm_lock);

	return old;
}

static bool swrm_lock_sleep(struct swr_mstr_ctrl *swrm)
{
	enum swrm_pm_state os;

	/*
	 * swrm_{lock/unlock}_sleep will be called by swr irq handler
	 * and slave wake up requests..
	 *
	 * If system didn't resume, we can simply return false so
	 * IRQ handler can return without handling IRQ.
	 */
	mutex_lock(&swrm->pm_lock);
	if (swrm->wlock_holders++ == 0) {
		dev_dbg(swrm->dev, "%s: holding wake lock\n", __func__);
		pm_qos_update_request(&swrm->pm_qos_req,
					  msm_cpuidle_get_deep_idle_latency());
		pm_stay_awake(swrm->dev);
	}
	mutex_unlock(&swrm->pm_lock);

	if (!wait_event_timeout(swrm->pm_wq,
				((os =  swrm_pm_cmpxchg(swrm,
				  SWRM_PM_SLEEPABLE,
				  SWRM_PM_AWAKE)) ==
					SWRM_PM_SLEEPABLE ||
					(os == SWRM_PM_AWAKE)),
					msecs_to_jiffies(
					SWRM_SYSTEM_RESUME_TIMEOUT_MS))) {
		dev_err(swrm->dev, "%s: system didn't resume within %dms, s %d, w %d\n",
			__func__, SWRM_SYSTEM_RESUME_TIMEOUT_MS, swrm->pm_state,
				swrm->wlock_holders);
		swrm_unlock_sleep(swrm);
		return false;
	}
	wake_up_all(&swrm->pm_wq);
	return true;
}

static void swrm_unlock_sleep(struct swr_mstr_ctrl *swrm)
{
	mutex_lock(&swrm->pm_lock);
	if (--swrm->wlock_holders == 0) {
		dev_dbg(swrm->dev, "%s: releasing wake lock pm_state %d -> %d\n",
			 __func__, swrm->pm_state, SWRM_PM_SLEEPABLE);
		/*
		 * if swrm_lock_sleep failed, pm_state would be still
		 * swrm_PM_ASLEEP, don't overwrite
		 */
		if (likely(swrm->pm_state == SWRM_PM_AWAKE))
			swrm->pm_state = SWRM_PM_SLEEPABLE;
		pm_qos_update_request(&swrm->pm_qos_req,
				  PM_QOS_DEFAULT_VALUE);
		pm_relax(swrm->dev);
	}
	mutex_unlock(&swrm->pm_lock);
	wake_up_all(&swrm->pm_wq);
}

#ifdef CONFIG_PM_SLEEP
static int swrm_suspend(struct device *dev)
{
	int ret = -EBUSY;
	struct platform_device *pdev = to_platform_device(dev);
	struct swr_mstr_ctrl *swrm = platform_get_drvdata(pdev);

	dev_dbg(dev, "%s: system suspend, state: %d\n", __func__, swrm->state);

	mutex_lock(&swrm->pm_lock);

	if (swrm->pm_state == SWRM_PM_SLEEPABLE) {
		dev_dbg(swrm->dev, "%s: suspending system, state %d, wlock %d\n",
			 __func__, swrm->pm_state,
			swrm->wlock_holders);
		swrm->pm_state = SWRM_PM_ASLEEP;
	} else if (swrm->pm_state == SWRM_PM_AWAKE) {
		/*
		 * unlock to wait for pm_state == SWRM_PM_SLEEPABLE
		 * then set to SWRM_PM_ASLEEP
		 */
		dev_dbg(swrm->dev, "%s: waiting to suspend system, state %d, wlock %d\n",
			 __func__, swrm->pm_state,
			 swrm->wlock_holders);
		mutex_unlock(&swrm->pm_lock);
		if (!(wait_event_timeout(swrm->pm_wq, swrm_pm_cmpxchg(
					 swrm, SWRM_PM_SLEEPABLE,
						 SWRM_PM_ASLEEP) ==
						   SWRM_PM_SLEEPABLE,
						   msecs_to_jiffies(
						   SWRM_SYS_SUSPEND_WAIT)))) {
			dev_dbg(swrm->dev, "%s: suspend failed state %d, wlock %d\n",
				 __func__, swrm->pm_state,
				 swrm->wlock_holders);
			return -EBUSY;
		} else {
			dev_dbg(swrm->dev,
				"%s: done, state %d, wlock %d\n",
				__func__, swrm->pm_state,
				swrm->wlock_holders);
		}
		mutex_lock(&swrm->pm_lock);
	} else if (swrm->pm_state == SWRM_PM_ASLEEP) {
		dev_dbg(swrm->dev, "%s: system is already suspended, state %d, wlock %d\n",
			__func__, swrm->pm_state,
			swrm->wlock_holders);
	}

	mutex_unlock(&swrm->pm_lock);

	if ((!pm_runtime_enabled(dev) || !pm_runtime_suspended(dev))) {
		ret = swrm_runtime_suspend(dev);
		if (!ret) {
			/*
			 * Synchronize runtime-pm and system-pm states:
			 * At this point, we are already suspended. If
			 * runtime-pm still thinks its active, then
			 * make sure its status is in sync with HW
			 * status. The three below calls let the
			 * runtime-pm know that we are suspended
			 * already without re-invoking the suspend
			 * callback
			 */
			pm_runtime_disable(dev);
			pm_runtime_set_suspended(dev);
			pm_runtime_enable(dev);
		}
	}
	if (ret == -EBUSY) {
		/*
		 * There is a possibility that some audio stream is active
		 * during suspend. We dont want to return suspend failure in
		 * that case so that display and relevant components can still
		 * go to suspend.
		 * If there is some other error, then it should be passed-on
		 * to system level suspend
		 */
		ret = 0;
	}
	return ret;
}

static int swrm_resume(struct device *dev)
{
	int ret = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct swr_mstr_ctrl *swrm = platform_get_drvdata(pdev);

	dev_dbg(dev, "%s: system resume, state: %d\n", __func__, swrm->state);
	if (!pm_runtime_enabled(dev) || !pm_runtime_suspend(dev)) {
		ret = swrm_runtime_resume(dev);
		if (!ret) {
			pm_runtime_mark_last_busy(dev);
			pm_request_autosuspend(dev);
		}
	}
	mutex_lock(&swrm->pm_lock);
	if (swrm->pm_state == SWRM_PM_ASLEEP) {
		dev_dbg(swrm->dev,
			"%s: resuming system, state %d, wlock %d\n",
			__func__, swrm->pm_state,
			swrm->wlock_holders);
		swrm->pm_state = SWRM_PM_SLEEPABLE;
	} else {
		dev_dbg(swrm->dev, "%s: system is already awake, state %d wlock %d\n",
			__func__, swrm->pm_state,
			swrm->wlock_holders);
	}
	mutex_unlock(&swrm->pm_lock);
	wake_up_all(&swrm->pm_wq);

	return ret;
}
#endif /* CONFIG_PM_SLEEP */

static const struct dev_pm_ops swrm_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(
		swrm_suspend,
		swrm_resume
	)
	SET_RUNTIME_PM_OPS(
		swrm_runtime_suspend,
		swrm_runtime_resume,
		NULL
	)
};

static const struct of_device_id swrm_dt_match[] = {
	{
		.compatible = "qcom,swr-mstr",
	},
	{}
};

static struct platform_driver swr_mstr_driver = {
	.probe = swrm_probe,
	.remove = swrm_remove,
	.driver = {
		.name = SWR_WCD_NAME,
		.owner = THIS_MODULE,
		.pm = &swrm_dev_pm_ops,
		.of_match_table = swrm_dt_match,
		.suppress_bind_attrs = true,
	},
};

static int __init swrm_init(void)
{
	return platform_driver_register(&swr_mstr_driver);
}
module_init(swrm_init);

static void __exit swrm_exit(void)
{
	platform_driver_unregister(&swr_mstr_driver);
}
module_exit(swrm_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("SoundWire Master Controller");
MODULE_ALIAS("platform:swr-mstr");
