/******************************************************************************
 * Copyright (C) 2020-2022 NXP
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 ******************************************************************************/
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/gpio.h>

#include "common_ese.h"

static void cold_reset_gaurd_timer_callback(struct timer_list *t)
{
	struct cold_reset *cold_reset = from_timer(cold_reset, t, timer);

	pr_debug("%s: entry\n", __func__);
	cold_reset->in_progress = false;
}

static long start_cold_reset_guard_timer(struct cold_reset *cold_reset)
{
	long ret = -EINVAL;

	if (timer_pending(&cold_reset->timer) == 1) {
		pr_debug("%s: delete pending timer\n", __func__);
		/* delete timer if already pending */
		del_timer(&cold_reset->timer);
	}
	cold_reset->in_progress = true;
	timer_setup(&cold_reset->timer, cold_reset_gaurd_timer_callback, 0);
	ret = mod_timer(&cold_reset->timer,
			jiffies + msecs_to_jiffies(ESE_CLD_RST_GUARD_TIME_MS));
	return ret;
}

static int send_cold_reset_protection_cmd(struct nfc_dev *nfc_dev,
					  bool requestType)
{
	int ret = 0;
	int cmd_length = 0;
	uint8_t *cmd = nfc_dev->write_kbuf;
	struct cold_reset *cold_reset = &nfc_dev->cold_reset;

	*cmd++ = NCI_PROP_MSG_CMD;

	if (requestType) {	/* reset protection */
		*cmd++ = RST_PROT_OID;
		*cmd++ = RST_PROT_PAYLOAD_SIZE;
		*cmd++ = (!cold_reset->reset_protection) ? 1 : 0;
	} else {		/* cold reset */
		*cmd++ = CLD_RST_OID;
		*cmd++ = CLD_RST_PAYLOAD_SIZE;
	}
	cmd_length = cmd - nfc_dev->write_kbuf;

	ret = nfc_dev->nfc_write(nfc_dev, nfc_dev->write_kbuf, cmd_length,
				 MAX_RETRY_COUNT);
	if (ret != cmd_length) {
		ret = -EIO;
		pr_err("%s: nfc_write returned %d\n", __func__, ret);
		goto exit;
	}
	cmd = nfc_dev->write_kbuf;
	if (requestType)
		pr_debug(" %s: NxpNciX: %d > 0x%02x%02x%02x%02x\n", __func__,
			 ret, cmd[NCI_HDR_IDX], cmd[NCI_HDR_OID_IDX],
			 cmd[NCI_PAYLOAD_LEN_IDX], cmd[NCI_PAYLOAD_IDX]);
	else
		pr_debug(" %s: NxpNciX: %d > 0x%02x%02x%02x\n", __func__, ret,
			 cmd[NCI_HDR_IDX], cmd[NCI_HDR_OID_IDX],
			 cmd[NCI_PAYLOAD_LEN_IDX]);
exit:
	return ret;
}

void wakeup_on_prop_rsp(struct nfc_dev *nfc_dev, uint8_t *buf)
{
	struct cold_reset *cold_reset = &nfc_dev->cold_reset;

	cold_reset->status = -EIO;
	if ((NCI_HDR_LEN + buf[NCI_PAYLOAD_LEN_IDX]) != NCI_PROP_MSG_RSP_LEN)
		pr_err("%s: invalid response for cold_reset/protection\n",
		       __func__);
	else
		cold_reset->status = buf[NCI_PAYLOAD_IDX];

	pr_debug(" %s: NxpNciR 0x%02x%02x%02x%02x\n", __func__,
		 buf[NCI_HDR_IDX], buf[NCI_HDR_OID_IDX],
		 buf[NCI_PAYLOAD_LEN_IDX], buf[NCI_PAYLOAD_IDX]);

	cold_reset->rsp_pending = false;
	wake_up_interruptible(&cold_reset->read_wq);
}

static int validate_cold_reset_protection_request(struct cold_reset *cold_reset,
						  unsigned long arg)
{
	int ret = 0;

	if (!cold_reset->reset_protection) {
		if (IS_RST_PROT_EN_REQ(arg) && IS_SRC_VALID_PROT(arg)) {
			pr_debug("%s: reset protection enable\n", __func__);
		} else if (IS_CLD_RST_REQ(arg) && IS_SRC_VALID(arg)) {
			pr_debug("%s: cold reset\n", __func__);
		} else if (IS_RST_PROT_DIS_REQ(arg) && IS_SRC_VALID_PROT(arg)) {
			pr_debug("%s: reset protection already disable\n",
				 __func__);
			ret = -EINVAL;
		} else {
			pr_err("%s: operation not permitted\n", __func__);
			ret = -EPERM;
		}
	} else {
		if (IS_RST_PROT_DIS_REQ(arg) &&
		    IS_SRC(arg, cold_reset->rst_prot_src)) {
			pr_debug("%s: disable reset protection from same src\n",
				 __func__);
		} else if (IS_CLD_RST_REQ(arg) &&
			   IS_SRC(arg, cold_reset->rst_prot_src)) {
			pr_debug("%s: cold reset from same source\n", __func__);
		} else if (IS_RST_PROT_EN_REQ(arg) &&
			   IS_SRC(arg, cold_reset->rst_prot_src)) {
			pr_debug("%s: enable reset protection from same src\n",
				 __func__);
			ret = -EINVAL;
		} else {
			pr_err("%s: operation not permitted\n", __func__);
			ret = -EPERM;
		}
	}
	return ret;
}

static int perform_cold_reset_protection(struct nfc_dev *nfc_dev,
					 unsigned long arg)
{
	int ret = 0;
	int timeout = 0;
	int retry_cnt = 0;
	char *rsp = nfc_dev->read_kbuf;
	struct cold_reset *cold_reset = &nfc_dev->cold_reset;

	/* check if NFCC not in the FW download or hard reset state */
	ret = validate_nfc_state_nci(nfc_dev);
	if (ret < 0) {
		pr_err("%s: invalid cmd\n", __func__);
		return ret;
	}

	/* check if NFCC not in the FW download or hard reset state */
	ret = validate_cold_reset_protection_request(cold_reset, arg);
	if (ret < 0) {
		goto err;
	}

	/* check if cold reset already in progress */
	if (IS_CLD_RST_REQ(arg) && cold_reset->in_progress) {
		pr_err("%s: cold reset already in progress\n", __func__);
		ret = -EBUSY;
		goto err;
	}

	/* enable interrupt if not enabled incase when devnode not opened by HAL */
	nfc_dev->nfc_enable_intr(nfc_dev);

	mutex_lock(&nfc_dev->write_mutex);
	/* write api has 15ms maximum wait to clear any pending read before */
	cold_reset->status = -EIO;
	cold_reset->rsp_pending = true;
	ret = send_cold_reset_protection_cmd(nfc_dev, IS_RST_PROT_REQ(arg));
	if (ret < 0) {
		mutex_unlock(&nfc_dev->write_mutex);
		cold_reset->rsp_pending = false;
		pr_err("%s: failed to send cold reset/protection cmd\n",
		       __func__);
		goto err;
	}

	ret = 0;
	/* start the cold reset guard timer */
	if (IS_CLD_RST_REQ(arg)) {
		/* Guard timer not needed when OSU over NFC */
		if (!(cold_reset->reset_protection && IS_SRC_NFC(arg))) {
			ret = start_cold_reset_guard_timer(cold_reset);
			if (ret) {
				mutex_unlock(&nfc_dev->write_mutex);
				pr_err("%s: error in mod_timer\n", __func__);
				goto err;
			}
		}
	}

	timeout = NCI_CMD_RSP_TIMEOUT_MS;
	mutex_lock(&nfc_dev->dev_ref_mutex);
	do {
		if (nfc_dev->cold_reset.is_nfc_read_pending) {
			if (!wait_event_interruptible_timeout
			    (cold_reset->read_wq,
			     cold_reset->rsp_pending == false,
			     msecs_to_jiffies(timeout))) {
				pr_err("%s: cold reset/prot response timeout\n",
				       __func__);
				if (retry_cnt <= 1) {
					retry_cnt = retry_cnt + 1;
					ret = -EAGAIN;
				} else {
					pr_debug("%s: Maximum retry reached",
						 __func__);
					ret = -ETIMEDOUT;
				}
			}
		} else {
			ret = nfc_dev->nfc_read(nfc_dev, rsp, 3, timeout);
			if (!ret)
				break;
			usleep_range(READ_RETRY_WAIT_TIME_US,
				     READ_RETRY_WAIT_TIME_US + 500);
		}
	} while (ret == -ERESTARTSYS || ret == -EFAULT || ret == -EAGAIN);
	mutex_unlock(&nfc_dev->dev_ref_mutex);
	mutex_unlock(&nfc_dev->write_mutex);

	timeout = ESE_CLD_RST_REBOOT_GUARD_TIME_MS;
	if (ret == 0) {		/* success case */
		ret = cold_reset->status;
		if (IS_RST_PROT_REQ(arg)) {
			cold_reset->reset_protection = IS_RST_PROT_EN_REQ(arg);
			cold_reset->rst_prot_src = IS_RST_PROT_EN_REQ(arg) ?
			    GET_SRC(arg) : SRC_NONE;
			/* wait for reboot guard timer */
		} else {
			if (wait_event_interruptible_timeout
			    (cold_reset->read_wq, true,
			     msecs_to_jiffies(timeout)) == 0) {
				pr_info("%s: reboot guard timer timeout\n",
					__func__);
			}
		}
	}
err:
	mutex_unlock(&nfc_dev->dev_ref_mutex);
	return ret;
}

/**
 * nfc_ese_pwr() - power control for ese
 * @nfc_dev:    nfc device data structure
 * @arg:    mode that we want to move to
 *
 * Device power control. Depending on the arg value, device moves to
 * different states, refer common_ese.h for args
 *
 * Return: -ENOIOCTLCMD if arg is not supported
 * 0 if Success(or no issue)
 * 0 or 1 in case of arg is ESE_POWER_STATE
 * and error ret code otherwise
 */
int nfc_ese_pwr(struct nfc_dev *nfc_dev, unsigned long arg)
{
	int ret = 0;
	struct platform_gpio *nfc_gpio = &nfc_dev->configs.gpio;

	if (arg == ESE_POWER_ON) {
		/*
		 * Let's store the NFC VEN pin state
		 * will check stored value in case of eSE power off request,
		 * to find out if NFC MW also sent request to set VEN HIGH
		 * VEN state will remain HIGH if NFC is enabled otherwise
		 * it will be set as LOW
		 */
		nfc_dev->nfc_ven_enabled = gpio_get_value(nfc_gpio->ven);
		if (!nfc_dev->nfc_ven_enabled) {
			pr_debug("%s: ese hal service setting ven high\n",
				 __func__);
			gpio_set_ven(nfc_dev, 1);
		} else {
			pr_debug("%s: ven already high\n", __func__);
		}
	} else if (arg == ESE_POWER_OFF) {
		if (!nfc_dev->nfc_ven_enabled) {
			pr_debug("%s: nfc not enabled, disabling ven\n",
				 __func__);
			gpio_set_ven(nfc_dev, 0);
		} else {
			pr_debug("%s: keep ven high as nfc is enabled\n",
				 __func__);
		}
	} else if (arg == ESE_POWER_STATE) {
		/* eSE get power state */
		ret = gpio_get_value(nfc_gpio->ven);
	} else if (IS_CLD_RST_REQ(arg) || IS_RST_PROT_REQ(arg)) {
		ret = perform_cold_reset_protection(nfc_dev, arg);
	} else {
		pr_err("%s: bad arg %lu\n", __func__, arg);
		ret = -ENOIOCTLCMD;
	}
	return ret;
}
EXPORT_SYMBOL(nfc_ese_pwr);

#define ESE_LEGACY_INTERFACE
#ifdef ESE_LEGACY_INTERFACE
static struct nfc_dev *nfc_dev_legacy;

/******************************************************************************
 * perform_ese_cold_reset() - It shall be called by others driver(not nfc/ese)
 * to perform cold reset only
 * @arg: request of cold reset from other drivers should be ESE_CLD_RST_OTHER
 *
 * Returns:- 0 in case of success and negative values in case of failure
 *****************************************************************************/
int perform_ese_cold_reset(unsigned long arg)
{
	int ret = 0;

	if (nfc_dev_legacy) {
		if (IS_CLD_RST_REQ(arg) && IS_SRC_OTHER(arg)) {
			ret = nfc_ese_pwr(nfc_dev_legacy, arg);
		} else {
			pr_err("%s: operation not permitted\n", __func__);
			return -EPERM;
		}
	}
	pr_debug("%s: arg = %d ret = %lu\n", __func__, arg, ret);
	return ret;
}
EXPORT_SYMBOL(perform_ese_cold_reset);
#endif /* ESE_LEGACY_INTERFACE */

void ese_cold_reset_release(struct nfc_dev *nfc_dev)
{
	struct cold_reset *cold_reset = &nfc_dev->cold_reset;

	cold_reset->rsp_pending = false;
	cold_reset->in_progress = false;
	if (timer_pending(&cold_reset->timer) == 1)
		del_timer(&cold_reset->timer);
}

void common_ese_init(struct nfc_dev *nfc_dev)
{
	struct cold_reset *cold_reset = &nfc_dev->cold_reset;

	cold_reset->reset_protection = false;
	cold_reset->rst_prot_src = SRC_NONE;
	init_waitqueue_head(&cold_reset->read_wq);
	ese_cold_reset_release(nfc_dev);
#ifdef ESE_LEGACY_INTERFACE
	nfc_dev_legacy = nfc_dev;
#endif /* ESE_LEGACY_INTERFACE */
}

void common_ese_exit(struct nfc_dev *nfc_dev)
{
#ifdef ESE_LEGACY_INTERFACE
	nfc_dev_legacy = NULL;
#endif /* ESE_LEGACY_INTERFACE */
}
