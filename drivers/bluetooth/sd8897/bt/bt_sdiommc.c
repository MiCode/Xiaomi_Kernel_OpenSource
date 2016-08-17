/** @file bt_sdiommc.c
 *  @brief This file contains SDIO IF (interface) module
 *  related functions.
 *
 * Copyright (C) 2007-2012, Marvell International Ltd.
 *
 * This software file (the "File") is distributed by Marvell International
 * Ltd. under the terms of the GNU General Public License Version 2, June 1991
 * (the "License").  You may use, redistribute and/or modify this File in
 * accordance with the terms and conditions of the License, a copy of which
 * is available along with the File in the gpl.txt file or by writing to
 * the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307 or on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
 *
 * THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE
 * ARE EXPRESSLY DISCLAIMED.  The License provides additional details about
 * this warranty disclaimer.
 *
 */

#include <linux/firmware.h>
#include <linux/mmc/sdio_func.h>

#include "bt_drv.h"
#include "bt_sdio.h"

/** define marvell vendor id */
#define MARVELL_VENDOR_ID 0x02df

/** Max retry number of CMD53 write */
#define MAX_WRITE_IOMEM_RETRY	2
/** Firmware name */
static char *fw_name = NULL;
/** request firmware nowait */
static int req_fw_nowait = 0;
static int multi_fn = BIT(2);
#define DEFAULT_FW_NAME "mrvl/sd8897_uapsta_a0.bin"

/** Function number 2 */
#define FN2			2
/** Device ID for SD8897 FN2 */
#define SD_DEVICE_ID_8897_BT_FN2    0x912E
/** Device ID for SD8897 FN3 */
#define SD_DEVICE_ID_8897_BT_FN3    0x912F

/** Array of SDIO device ids when multi_fn=0x12 */
static const struct sdio_device_id bt_ids[] = {
	{SDIO_DEVICE(MARVELL_VENDOR_ID, SD_DEVICE_ID_8897_BT_FN2)},
	{}
};

MODULE_DEVICE_TABLE(sdio, bt_ids);

/********************************************************
		Global Variables
********************************************************/
/** unregiser bus driver flag */
static u8 unregister = 0;
#ifdef SDIO_SUSPEND_RESUME
/** PM keep power */
extern int mbt_pm_keep_power;
#endif

/********************************************************
		Local Functions
********************************************************/

/**
 *  @brief This function gets rx_unit value
 *
 *  @param priv    A pointer to bt_private structure
 *  @return 	   BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
sd_get_rx_unit(bt_private * priv)
{
	int ret = BT_STATUS_SUCCESS;
	u8 reg;
	struct sdio_mmc_card *card = (struct sdio_mmc_card *)priv->bt_dev.card;

	ENTER();

	reg = sdio_readb(card->func, CARD_RX_UNIT_REG, &ret);
	if (ret == BT_STATUS_SUCCESS)
		priv->bt_dev.rx_unit = reg;

	LEAVE();
	return ret;
}

/**
 *  @brief This function reads fwstatus registers
 *
 *  @param priv    A pointer to bt_private structure
 *  @param dat	   A pointer to keep returned data
 *  @return 	   BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
static int
sd_read_firmware_status(bt_private * priv, u16 * dat)
{
	int ret = BT_STATUS_SUCCESS;
	u8 fws0;
	u8 fws1;
	struct sdio_mmc_card *card = (struct sdio_mmc_card *)priv->bt_dev.card;

	ENTER();

	fws0 = sdio_readb(card->func, CARD_FW_STATUS0_REG, &ret);
	if (ret < 0) {
		LEAVE();
		return BT_STATUS_FAILURE;
	}

	fws1 = sdio_readb(card->func, CARD_FW_STATUS1_REG, &ret);
	if (ret < 0) {
		LEAVE();
		return BT_STATUS_FAILURE;
	}

	*dat = (((u16) fws1) << 8) | fws0;

	LEAVE();
	return BT_STATUS_SUCCESS;
}

/**
 *  @brief This function reads rx length
 *
 *  @param priv    A pointer to bt_private structure
 *  @param dat	   A pointer to keep returned data
 *  @return 	   BT_STATUS_SUCCESS or other error no.
 */
static int
sd_read_rx_len(bt_private * priv, u16 * dat)
{
	int ret = BT_STATUS_SUCCESS;
	u8 reg;
	struct sdio_mmc_card *card = (struct sdio_mmc_card *)priv->bt_dev.card;

	ENTER();

	reg = sdio_readb(card->func, CARD_RX_LEN_REG, &ret);
	if (ret == BT_STATUS_SUCCESS)
		*dat = (u16) reg << priv->bt_dev.rx_unit;

	LEAVE();
	return ret;
}

/**
 *  @brief This function enables the host interrupts mask
 *
 *  @param priv    A pointer to bt_private structure
 *  @param mask	   the interrupt mask
 *  @return 	   BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
static int
sd_enable_host_int_mask(bt_private * priv, u8 mask)
{
	int ret = BT_STATUS_SUCCESS;
	struct sdio_mmc_card *card = (struct sdio_mmc_card *)priv->bt_dev.card;

	ENTER();

	sdio_writeb(card->func, mask, HOST_INT_MASK_REG, &ret);
	if (ret) {
		PRINTM(WARN, "BT: Unable to enable the host interrupt!\n");
		ret = BT_STATUS_FAILURE;
	}

	LEAVE();
	return ret;
}

/** @brief This function disables the host interrupts mask.
 *
 *  @param priv    A pointer to bt_private structure
 *  @param mask	   the interrupt mask
 *  @return 	   BT_STATUS_SUCCESS or other error no.
 */
static int
sd_disable_host_int_mask(bt_private * priv, u8 mask)
{
	int ret = BT_STATUS_FAILURE;
	u8 host_int_mask;
	struct sdio_mmc_card *card = (struct sdio_mmc_card *)priv->bt_dev.card;

	ENTER();

	/* Read back the host_int_mask register */
	host_int_mask = sdio_readb(card->func, HOST_INT_MASK_REG, &ret);
	if (ret)
		goto done;

	/* Update with the mask and write back to the register */
	host_int_mask &= ~mask;
	sdio_writeb(card->func, host_int_mask, HOST_INT_MASK_REG, &ret);
	if (ret < 0) {
		PRINTM(WARN, "BT: Unable to diable the host interrupt!\n");
		goto done;
	}
	ret = BT_STATUS_SUCCESS;
done:
	LEAVE();
	return ret;
}

/**
 *  @brief This function polls the card status register
 *
 *  @param priv    	A pointer to bt_private structure
 *  @param bits    	the bit mask
 *  @return 	   	BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
static int
sd_poll_card_status(bt_private * priv, u8 bits)
{
	int tries;
	int rval;
	struct sdio_mmc_card *card = (struct sdio_mmc_card *)priv->bt_dev.card;
	u8 cs;

	ENTER();

	for (tries = 0; tries < MAX_POLL_TRIES * 1000; tries++) {
		cs = sdio_readb(card->func, CARD_STATUS_REG, &rval);
		if (rval != 0)
			break;
		if (rval == 0 && (cs & bits) == bits) {
			LEAVE();
			return BT_STATUS_SUCCESS;
		}
		udelay(1);
	}
	PRINTM(ERROR,
	       "BT: sdio_poll_card_status failed (%d), tries = %d, cs = 0x%x\n",
	       rval, tries, cs);

	LEAVE();
	return BT_STATUS_FAILURE;
}

/**
 *  @brief This function reads updates the Cmd52 value in dev structure
 *
 *  @param priv    	A pointer to bt_private structure
 *  @return 	   	BT_STATUS_SUCCESS or other error no.
 */
int
sd_read_cmd52_val(bt_private * priv)
{
	int ret = BT_STATUS_SUCCESS;
	u8 func, reg, val;
	struct sdio_mmc_card *card = (struct sdio_mmc_card *)priv->bt_dev.card;

	ENTER();

	func = priv->bt_dev.cmd52_func;
	reg = priv->bt_dev.cmd52_reg;
	sdio_claim_host(card->func);
	if (func)
		val = sdio_readb(card->func, reg, &ret);
	else
		val = sdio_f0_readb(card->func, reg, &ret);
	sdio_release_host(card->func);
	if (ret) {
		PRINTM(ERROR, "BT: Cannot read value from func %d reg %d\n",
		       func, reg);
	} else {
		priv->bt_dev.cmd52_val = val;
	}

	LEAVE();
	return ret;
}

/**
 *  @brief This function updates card reg based on the Cmd52 value in dev structure
 *
 *  @param priv    	A pointer to bt_private structure
 *  @param func    	Stores func variable
 *  @param reg    	Stores reg variable
 *  @param val    	Stores val variable
 *  @return 	   	BT_STATUS_SUCCESS or other error no.
 */
int
sd_write_cmd52_val(bt_private * priv, int func, int reg, int val)
{
	int ret = BT_STATUS_SUCCESS;
	struct sdio_mmc_card *card = (struct sdio_mmc_card *)priv->bt_dev.card;

	ENTER();

	if (val >= 0) {
		/* Perform actual write only if val is provided */
		sdio_claim_host(card->func);
		if (func)
			sdio_writeb(card->func, val, reg, &ret);
		else
			sdio_f0_writeb(card->func, val, reg, &ret);
		sdio_release_host(card->func);
		if (ret) {
			PRINTM(ERROR,
			       "BT: Cannot write value (0x%x) to func %d reg %d\n",
			       val, func, reg);
			goto done;
		}
		priv->bt_dev.cmd52_val = val;
	}

	/* Save current func and reg for future read */
	priv->bt_dev.cmd52_func = func;
	priv->bt_dev.cmd52_reg = reg;

done:
	LEAVE();
	return ret;
}

/**
 *  @brief This function probes the card
 *
 *  @param func    A pointer to sdio_func structure.
 *  @param id	   A pointer to structure sdio_device_id
 *  @return 	   BT_STATUS_SUCCESS/BT_STATUS_FAILURE or other error no.
 */
static int
sd_probe_card(struct sdio_func *func, const struct sdio_device_id *id)
{
	int ret = BT_STATUS_SUCCESS;
	bt_private *priv = NULL;
	struct sdio_mmc_card *card = NULL;

	ENTER();

	PRINTM(INFO, "BT: vendor=0x%x,device=0x%x,class=%d,fn=%d\n", id->vendor,
	       id->device, id->class, func->num);
	card = kzalloc(sizeof(struct sdio_mmc_card), GFP_KERNEL);
	if (!card) {
		ret = -ENOMEM;
		goto done;
	}
	card->func = func;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
	/* wait for chip fully wake up */
	if (!func->enable_timeout)
		func->enable_timeout = 200;
#endif
	sdio_claim_host(func);
	ret = sdio_enable_func(func);
	if (ret) {
		sdio_disable_func(func);
		sdio_release_host(func);
		PRINTM(FATAL, "BT: sdio_enable_func() failed: ret=%d\n", ret);
		kfree(card);
		LEAVE();
		return -EIO;
	}
	sdio_release_host(func);
	priv = bt_add_card(card);
	if (!priv) {
		sdio_claim_host(func);
		sdio_disable_func(func);
		sdio_release_host(func);
		ret = BT_STATUS_FAILURE;
		kfree(card);
	}
done:
	LEAVE();
	return ret;
}

/**
 *  @brief This function checks if the firmware is ready to accept
 *  command or not.
 *
 *  @param priv     A pointer to bt_private structure
 *  @param pollnum  Number of times to poll fw status
 *  @return         BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
sd_verify_fw_download(bt_private * priv, int pollnum)
{
	int ret = BT_STATUS_SUCCESS;
	u16 firmwarestat;
	int tries;

	ENTER();

	/* Wait for firmware initialization event */
	for (tries = 0; tries < pollnum; tries++) {
		if (sd_read_firmware_status(priv, &firmwarestat) < 0)
			continue;
		if (firmwarestat == FIRMWARE_READY) {
			PRINTM(MSG, "BT FW is active(%d)\n", tries);
			ret = BT_STATUS_SUCCESS;
			break;
		} else {
			mdelay(100);
			ret = BT_STATUS_FAILURE;
		}
	}
	if (ret < 0)
		goto done;

	ret = BT_STATUS_SUCCESS;
done:
	LEAVE();
	return ret;
}

/**
 *  @brief Transfers firmware to card
 *
 *  @param priv      A Pointer to bt_private structure
 *  @return 	     BT_STATUS_SUCCESS/BT_STATUS_FAILURE or other error no.
 */
static int
sd_init_fw_dpc(bt_private * priv)
{
	struct sdio_mmc_card *card = (struct sdio_mmc_card *)priv->bt_dev.card;
	u8 *firmware = NULL;
	int firmwarelen;
	u8 base0;
	u8 base1;
	int ret = BT_STATUS_SUCCESS;
	int offset;
	void *tmpfwbuf = NULL;
	int tmpfwbufsz;
	u8 *fwbuf;
	u16 len;
	int txlen = 0;
	int tx_blocks = 0;
	int i = 0;
	int tries = 0;
#ifdef FW_DOWNLOAD_SPEED
	u32 tv1, tv2;
#endif

	ENTER();
	firmware = (u8 *) priv->firmware->data;
	firmwarelen = priv->firmware->size;

	PRINTM(INFO, "BT: Downloading FW image (%d bytes)\n", firmwarelen);

#ifdef FW_DOWNLOAD_SPEED
	tv1 = get_utimeofday();
#endif

	tmpfwbufsz = ALIGN_SZ(BT_UPLD_SIZE, DMA_ALIGNMENT);
	tmpfwbuf = kmalloc(tmpfwbufsz, GFP_KERNEL);
	if (!tmpfwbuf) {
		PRINTM(ERROR,
		       "BT: Unable to allocate buffer for firmware. Terminating download\n");
		ret = BT_STATUS_FAILURE;
		goto done;
	}
	memset(tmpfwbuf, 0, tmpfwbufsz);
	/* Ensure aligned firmware buffer */
	fwbuf = (u8 *) ALIGN_ADDR(tmpfwbuf, DMA_ALIGNMENT);

	/* Perform firmware data transfer */
	offset = 0;
	do {
		/* The host polls for the DN_LD_CARD_RDY and CARD_IO_READY bits
		 */
		ret = sd_poll_card_status(priv, CARD_IO_READY | DN_LD_CARD_RDY);
		if (ret < 0) {
			PRINTM(FATAL,
			       "BT: FW download with helper poll status timeout @ %d\n",
			       offset);
			goto done;
		}
		/* More data? */
		if (offset >= firmwarelen)
			break;

		for (tries = 0; tries < MAX_POLL_TRIES; tries++) {
			base0 = sdio_readb(card->func,
					   SQ_READ_BASE_ADDRESS_A0_REG, &ret);
			if (ret) {
				PRINTM(WARN, "Dev BASE0 register read failed:"
				       " base0=0x%04X(%d). Terminating download\n",
				       base0, base0);
				ret = BT_STATUS_FAILURE;
				goto done;
			}
			base1 = sdio_readb(card->func,
					   SQ_READ_BASE_ADDRESS_A1_REG, &ret);
			if (ret) {
				PRINTM(WARN, "Dev BASE1 register read failed:"
				       " base1=0x%04X(%d). Terminating download\n",
				       base1, base1);
				ret = BT_STATUS_FAILURE;
				goto done;
			}
			len = (((u16) base1) << 8) | base0;

			if (len != 0)
				break;
			udelay(10);
		}

		if (len == 0)
			break;
		else if (len > BT_UPLD_SIZE) {
			PRINTM(FATAL,
			       "BT: FW download failure @ %d, invalid length %d\n",
			       offset, len);
			ret = BT_STATUS_FAILURE;
			goto done;
		}

		txlen = len;

		if (len & BIT(0)) {
			i++;
			if (i > MAX_WRITE_IOMEM_RETRY) {
				PRINTM(FATAL,
				       "BT: FW download failure @ %d, over max retry count\n",
				       offset);
				ret = BT_STATUS_FAILURE;
				goto done;
			}
			PRINTM(ERROR,
			       "BT: FW CRC error indicated by the helper:"
			       " len = 0x%04X, txlen = %d\n", len, txlen);
			len &= ~BIT(0);

			PRINTM(ERROR, "BT: retry: %d, offset %d\n", i, offset);
			/* Setting this to 0 to resend from same offset */
			txlen = 0;
		} else {
			i = 0;

			/* Set blocksize to transfer - checking for last block */
			if (firmwarelen - offset < txlen)
				txlen = firmwarelen - offset;

			PRINTM(INFO, ".");

			tx_blocks =
				(txlen + SD_BLOCK_SIZE_FW_DL -
				 1) / SD_BLOCK_SIZE_FW_DL;

			/* Copy payload to buffer */
			memcpy(fwbuf, &firmware[offset], txlen);
		}

		/* Send data */
		ret = sdio_writesb(card->func, priv->bt_dev.ioport, fwbuf,
				   tx_blocks * SD_BLOCK_SIZE_FW_DL);

		if (ret < 0) {
			PRINTM(ERROR,
			       "BT: FW download, write iomem (%d) failed @ %d\n",
			       i, offset);
			sdio_writeb(card->func, 0x04, CONFIGURATION_REG, &ret);
			if (ret)
				PRINTM(ERROR, "write ioreg failed (CFG)\n");
		}

		offset += txlen;
	} while (TRUE);

	PRINTM(INFO, "\nBT: FW download over, size %d bytes\n", offset);

	ret = BT_STATUS_SUCCESS;
done:
#ifdef FW_DOWNLOAD_SPEED
	tv2 = get_utimeofday();
	PRINTM(INFO, "FW: %d.%03d.%03d ", tv1 / 1000000,
	       (tv1 % 1000000) / 1000, tv1 % 1000);
	PRINTM(INFO, " -> %d.%03d.%03d ", tv2 / 1000000,
	       (tv2 % 1000000) / 1000, tv2 % 1000);
	tv2 -= tv1;
	PRINTM(INFO, " == %d.%03d.%03d\n", tv2 / 1000000,
	       (tv2 % 1000000) / 1000, tv2 % 1000);
#endif
	if (tmpfwbuf)
		kfree(tmpfwbuf);
	LEAVE();
	return ret;
}

/**
 * @brief request_firmware callback
 *
 * @param fw_firmware  A pointer to firmware structure
 * @param context      A Pointer to bt_private structure
 * @return             BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
static int
sd_request_fw_dpc(const struct firmware *fw_firmware, void *context)
{
	int ret = BT_STATUS_SUCCESS;
	bt_private *priv = (bt_private *) context;
	struct sdio_mmc_card *card = NULL;
	struct m_dev *m_dev_bt = NULL;
	struct m_dev *m_dev_fm = NULL;
	struct m_dev *m_dev_nfc = NULL;
	struct timeval tstamp;

	ENTER();

	m_dev_bt = &priv->bt_dev.m_dev[BT_SEQ];
	m_dev_fm = &priv->bt_dev.m_dev[FM_SEQ];
	m_dev_nfc = &priv->bt_dev.m_dev[NFC_SEQ];

	if ((priv == NULL) || (priv->adapter == NULL) ||
	    (priv->bt_dev.card == NULL) || (m_dev_bt == NULL)) {
		LEAVE();
		return BT_STATUS_FAILURE;
	}

	card = (struct sdio_mmc_card *)priv->bt_dev.card;

	if (!fw_firmware) {
		do_gettimeofday(&tstamp);
		if (tstamp.tv_sec >
		    (priv->req_fw_time.tv_sec + REQUEST_FW_TIMEOUT)) {
			PRINTM(ERROR,
			       "BT: No firmware image found. Skipping download\n");
			ret = BT_STATUS_FAILURE;
			goto done;
		}
		PRINTM(ERROR,
		       "BT: No firmware image found! Retrying download\n");
		/* Wait a second here before calling the callback again */
		os_sched_timeout(1000);
		sd_download_firmware_w_helper(priv);
		LEAVE();
		return ret;
	}

	priv->firmware = fw_firmware;
	if (BT_STATUS_FAILURE == sd_init_fw_dpc(priv)) {
		PRINTM(ERROR,
		       "BT: sd_init_fw_dpc failed (download fw with nowait: %d). Terminating download\n",
		       req_fw_nowait);
		ret = BT_STATUS_FAILURE;
		goto done;
	}

	/* check if the fimware is downloaded successfully or not */
	if (sd_verify_fw_download(priv, MAX_FIRMWARE_POLL_TRIES)) {
		PRINTM(ERROR, "BT: FW failed to be active in time!\n");
		ret = BT_STATUS_FAILURE;
		goto done;
	}
	sdio_release_host(card->func);
	sd_enable_host_int(priv);
	if (BT_STATUS_FAILURE == sbi_register_conf_dpc(priv)) {
		PRINTM(ERROR,
		       "BT: sbi_register_conf_dpc failed. Terminating download\n");
		ret = BT_STATUS_FAILURE;
		goto done;
	}
	if (fw_firmware) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,32)
		if (!req_fw_nowait)
#endif
			release_firmware(fw_firmware);
	}
	LEAVE();
	return ret;

done:
	if (fw_firmware) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,32)
		if (!req_fw_nowait)
#endif
			release_firmware(fw_firmware);
	}
	/* For synchronous download cleanup will be done in add_card */
	if (!req_fw_nowait)
		return ret;
	sdio_release_host(card->func);
	PRINTM(INFO, "unregister device\n");
	sbi_unregister_dev(priv);
	((struct sdio_mmc_card *)card)->priv = NULL;
	/* Stop the thread servicing the interrupts */
	priv->adapter->SurpriseRemoved = TRUE;
	wake_up_interruptible(&priv->MainThread.waitQ);
	while (priv->MainThread.pid) {
		os_sched_timeout(1);
	}
	if (m_dev_bt->dev_pointer) {
		if (m_dev_bt->spec_type == IANYWHERE_SPEC)
			free_m_dev(m_dev_bt);
	}
	if (m_dev_fm->dev_pointer)
		free_m_dev(m_dev_fm);
	if (m_dev_nfc->dev_pointer)
		free_m_dev(m_dev_nfc);
	if (priv->adapter)
		bt_free_adapter(priv);
	kfree(priv);
	LEAVE();
	return ret;
}

/**
 * @brief request_firmware callback
 *        This function is invoked by request_firmware_nowait system call
 *
 * @param firmware     A pointer to firmware structure
 * @param context      A Pointer to bt_private structure
 * @return             None
 **/
static void
sd_request_fw_callback(const struct firmware *firmware, void *context)
{
	ENTER();
	sd_request_fw_dpc(firmware, context);
	LEAVE();
	return;
}

/**
 *  @brief This function downloads firmware image to the card.
 *
 *  @param priv    	A pointer to bt_private structure
 *  @return 	   	BT_STATUS_SUCCESS/BT_STATUS_FAILURE or other error no.
 */
int
sd_download_firmware_w_helper(bt_private * priv)
{
	int ret = BT_STATUS_SUCCESS;
	int err;
	char *cur_fw_name = NULL;

	ENTER();

	cur_fw_name = fw_name;

	if (req_fw_nowait) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,32)
		if ((ret =
		     request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
					     cur_fw_name, priv->hotplug_device,
					     GFP_KERNEL, priv,
					     sd_request_fw_callback)) < 0)
#else
		if ((ret =
		     request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
					     cur_fw_name, priv->hotplug_device,
					     priv, sd_request_fw_callback)) < 0)
#endif
			PRINTM(FATAL,
			       "BT: request_firmware_nowait() failed, error code = %#x\n",
			       ret);
	} else {
		if ((err =
		     request_firmware(&priv->firmware, cur_fw_name,
				      priv->hotplug_device)) < 0) {
			PRINTM(FATAL,
			       "BT: request_firmware() failed, error code = %#x\n",
			       err);
			ret = BT_STATUS_FAILURE;
		} else
			ret = sd_request_fw_dpc(priv->firmware, priv);
	}

	LEAVE();
	return ret;
}

/**
 *  @brief This function reads data from the card.
 *
 *  @param priv    	A pointer to bt_private structure
 *  @return 	   	BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
static int
sd_card_to_host(bt_private * priv)
{
	int ret = BT_STATUS_SUCCESS;
	u16 buf_len = 0;
	int buf_block_len;
	int blksz;
	struct sk_buff *skb = NULL;
	u32 type;
	u8 *payload = NULL;
	struct mbt_dev *mbt_dev = NULL;
	struct m_dev *mdev_bt = &(priv->bt_dev.m_dev[BT_SEQ]);
	struct m_dev *mdev_fm = &(priv->bt_dev.m_dev[FM_SEQ]);
	struct m_dev *mdev_nfc = &(priv->bt_dev.m_dev[NFC_SEQ]);
	struct nfc_dev *nfc_dev =
		(struct nfc_dev *)priv->bt_dev.m_dev[NFC_SEQ].dev_pointer;
	struct fm_dev *fm_dev =
		(struct fm_dev *)priv->bt_dev.m_dev[FM_SEQ].dev_pointer;
	struct m_dev *mdev_debug = &(priv->bt_dev.m_dev[DEBUG_SEQ]);
	struct debug_dev *debug_dev =
		(struct debug_dev *)priv->bt_dev.m_dev[DEBUG_SEQ].dev_pointer;
	struct sdio_mmc_card *card = priv->bt_dev.card;

	ENTER();
	mbt_dev = (struct mbt_dev *)priv->bt_dev.m_dev[BT_SEQ].dev_pointer;
	if (!card || !card->func) {
		PRINTM(ERROR, "BT: card or function is NULL!\n");
		ret = BT_STATUS_FAILURE;
		goto exit;
	}

	/* Read the length of data to be transferred */
	ret = sd_read_rx_len(priv, &buf_len);
	if (ret < 0) {
		PRINTM(ERROR, "BT: card_to_host, read scratch reg failed\n");
		ret = BT_STATUS_FAILURE;
		goto exit;
	}

	/* Allocate buffer */
	blksz = SD_BLOCK_SIZE;
	buf_block_len = (buf_len + blksz - 1) / blksz;
	if (buf_len <= BT_HEADER_LEN ||
	    (buf_block_len * blksz) > ALLOC_BUF_SIZE) {
		PRINTM(ERROR, "BT: card_to_host, invalid packet length: %d\n",
		       buf_len);
		ret = BT_STATUS_FAILURE;
		goto exit;
	}
	skb = bt_skb_alloc(buf_block_len * blksz + DMA_ALIGNMENT, GFP_ATOMIC);
	if (skb == NULL) {
		PRINTM(WARN, "BT: No free skb\n");
		goto exit;
	}
	if ((t_ptr) skb->data & (DMA_ALIGNMENT - 1)) {
		skb_put(skb,
			DMA_ALIGNMENT -
			((t_ptr) skb->data & (DMA_ALIGNMENT - 1)));
		skb_pull(skb,
			 DMA_ALIGNMENT -
			 ((t_ptr) skb->data & (DMA_ALIGNMENT - 1)));
	}

	payload = skb->data;
	ret = sdio_readsb(card->func, payload, priv->bt_dev.ioport,
			  buf_block_len * blksz);
	if (ret < 0) {
		PRINTM(ERROR, "BT: card_to_host, read iomem failed: %d\n", ret);
		kfree_skb(skb);
		skb = NULL;
		ret = BT_STATUS_FAILURE;
		goto exit;
	}
	/* This is SDIO specific header length: byte[2][1][0], * type: byte[3]
	   (HCI_COMMAND = 1, ACL_DATA = 2, SCO_DATA = 3, 0xFE = Vendor) */
	buf_len = payload[0];
	buf_len |= (u16) payload[1] << 8;
	type = payload[3];
	PRINTM(DATA, "BT: SDIO Blk Rd %s: len=%d type=%d\n", mbt_dev->name,
	       buf_len, type);
	if (buf_len > buf_block_len * blksz) {
		PRINTM(ERROR,
		       "BT: Drop invalid rx pkt, len in hdr=%d, cmd53 length=%d\n",
		       buf_len, buf_block_len * blksz);
		ret = BT_STATUS_FAILURE;
		kfree_skb(skb);
		skb = NULL;
		goto exit;
	}
	DBG_HEXDUMP(DAT_D, "BT: SDIO Blk Rd", payload, buf_len);
	switch (type) {
	case HCI_ACLDATA_PKT:
		bt_cb(skb)->pkt_type = type;
		skb_put(skb, buf_len);
		skb_pull(skb, BT_HEADER_LEN);
		if (mbt_dev) {
			skb->dev = (void *)mdev_bt;
			mdev_recv_frame(skb);
			mdev_bt->stat.byte_rx += buf_len;
		}
		break;
	case HCI_SCODATA_PKT:
		bt_cb(skb)->pkt_type = type;
		skb_put(skb, buf_len);
		skb_pull(skb, BT_HEADER_LEN);
		if (mbt_dev) {
			skb->dev = (void *)mdev_bt;
			mdev_recv_frame(skb);
			mdev_bt->stat.byte_rx += buf_len;
		}
		break;
	case HCI_EVENT_PKT:
		/** add EVT Demux */
		bt_cb(skb)->pkt_type = type;
		skb_put(skb, buf_len);
		skb_pull(skb, BT_HEADER_LEN);
		if (BT_STATUS_SUCCESS == check_evtpkt(priv, skb))
			break;
		switch (skb->data[0]) {
		case 0x0E:
			/** cmd complete */
			if (priv->debug_device_pending) {
				if (priv->debug_ocf_ogf[0] == skb->data[3] &&
				    priv->debug_ocf_ogf[1] == skb->data[4]) {
					priv->debug_device_pending = 0;
					priv->debug_ocf_ogf[0] = 0;
					priv->debug_ocf_ogf[1] = 0;
					/** debug cmd complete */
					if (debug_dev) {
						skb->dev = (void *)mdev_debug;
						mdev_recv_frame(skb);
						mdev_debug->stat.byte_rx +=
							buf_len;
					}
					break;
				}
			}
			if (skb->data[3] == 0x80 && skb->data[4] == 0xFE) {
				/** FM cmd complete */
				if (fm_dev) {
					skb->dev = (void *)mdev_fm;
					mdev_recv_frame(skb);
					mdev_fm->stat.byte_rx += buf_len;
				}
			} else if (skb->data[3] == 0x81 && skb->data[4] == 0xFE) {
				/** NFC cmd complete */
				if (nfc_dev) {
					skb->dev = (void *)mdev_nfc;
					mdev_recv_frame(skb);
					mdev_nfc->stat.byte_rx += buf_len;
				}
			} else {
				if (mbt_dev) {
					skb->dev = (void *)mdev_bt;
					mdev_recv_frame(skb);
					mdev_bt->stat.byte_rx += buf_len;
				}
			}
			break;
		case 0x0F:
			/** cmd status */
			if (skb->data[4] == 0x80 && skb->data[5] == 0xFE) {
				/** FM cmd ststus */
				if (fm_dev) {
					skb->dev = (void *)mdev_fm;
					mdev_recv_frame(skb);
					mdev_fm->stat.byte_rx += buf_len;
				}
			} else if (skb->data[4] == 0x81 && skb->data[5] == 0xFE) {
				/** NFC cmd ststus */
				if (nfc_dev) {
					skb->dev = (void *)mdev_nfc;
					mdev_recv_frame(skb);
					mdev_nfc->stat.byte_rx += buf_len;
				}
			} else {
				/** BT cmd status */
				if (mbt_dev) {
					skb->dev = (void *)mdev_bt;
					mdev_recv_frame(skb);
					mdev_bt->stat.byte_rx += buf_len;
				}
			}
			break;
		case 0xFF:
			/** Vendor specific pkt */
			if (skb->data[2] == 0xC0) {
				/** NFC EVT */
				if (nfc_dev) {
					skb->dev = (void *)mdev_nfc;
					mdev_recv_frame(skb);
					mdev_nfc->stat.byte_rx += buf_len;
				}
			} else if (skb->data[2] >= 0x80 && skb->data[2] <= 0xAF) {
				/** FM EVT */
				if (fm_dev) {
					skb->dev = (void *)mdev_fm;
					mdev_recv_frame(skb);
					mdev_fm->stat.byte_rx += buf_len;
				}
			} else {
				/** BT EVT */
				if (mbt_dev) {
					skb->dev = (void *)mdev_bt;
					mdev_recv_frame(skb);
					mdev_bt->stat.byte_rx += buf_len;
				}
			}
			break;
		default:
			/** BT EVT */
			if (mbt_dev) {
				skb->dev = (void *)mdev_bt;
				mdev_recv_frame(skb);
				mdev_bt->stat.byte_rx += buf_len;
			}
			break;
		}
		break;
	case MRVL_VENDOR_PKT:
		// Just think here need to back compatible FM
		bt_cb(skb)->pkt_type = HCI_VENDOR_PKT;
		skb_put(skb, buf_len);
		skb_pull(skb, BT_HEADER_LEN);
		if (mbt_dev) {
			if (BT_STATUS_SUCCESS != bt_process_event(priv, skb)) {
				skb->dev = (void *)mdev_bt;
				mdev_recv_frame(skb);
				mdev_bt->stat.byte_rx += buf_len;
			}
		}

		break;
	default:
		/* Driver specified event and command resp should be handle
		   here */
		PRINTM(INFO, "BT: Unknown PKT type:%d\n", type);
		kfree_skb(skb);
		skb = NULL;
		break;
	}
exit:
	if (ret) {
		if (mbt_dev)
			mdev_bt->stat.err_rx++;
		PRINTM(ERROR, "error when recv pkt!\n");
	}

	LEAVE();
	return ret;
}

/**
 *  @brief This function removes the card
 *
 *  @param func    A pointer to sdio_func structure
 *  @return        N/A
 */
static void
sd_remove_card(struct sdio_func *func)
{
	struct sdio_mmc_card *card;

	ENTER();

	if (func) {
		card = sdio_get_drvdata(func);
		if (card) {
			if (!unregister && card->priv) {
				PRINTM(INFO, "BT: card removed from sd slot\n");
				((bt_private *) (card->priv))->adapter->
					SurpriseRemoved = TRUE;
			}
			bt_remove_card(card->priv);
			kfree(card);
		}
	}

	LEAVE();
}

/**
 *  @brief This function handles the interrupt.
 *
 *  @param func  A pointer to sdio_func structure
 *  @return      N/A
 */
static void
sd_interrupt(struct sdio_func *func)
{
	bt_private *priv;
	struct m_dev *m_dev = NULL;
	struct sdio_mmc_card *card;
	int ret = BT_STATUS_SUCCESS;
	u8 ireg = 0;

	ENTER();

	card = sdio_get_drvdata(func);
	if (!card || !card->priv) {
		PRINTM(INFO,
		       "BT: %s: sbi_interrupt(%p) card or priv is NULL, card=%p\n",
		       __FUNCTION__, func, card);
		LEAVE();
		return;
	}
	priv = card->priv;
	m_dev = &(priv->bt_dev.m_dev[BT_SEQ]);
	ireg = sdio_readb(card->func, HOST_INTSTATUS_REG, &ret);
	if (ret) {
		PRINTM(WARN,
		       "BT: sdio_read_ioreg: read int status register failed\n");
		goto done;
	}
	if (ireg != 0) {
		/*
		 * DN_LD_HOST_INT_STATUS and/or UP_LD_HOST_INT_STATUS
		 * Clear the interrupt status register and re-enable the interrupt
		 */
		PRINTM(INTR, "BT: INT %s: sdio_ireg = 0x%x\n", m_dev->name,
		       ireg);
		priv->adapter->irq_recv = ireg;
		sdio_writeb(card->func,
			    ~(ireg) & (DN_LD_HOST_INT_STATUS |
				       UP_LD_HOST_INT_STATUS),
			    HOST_INTSTATUS_REG, &ret);
		if (ret) {
			PRINTM(WARN,
			       "BT: sdio_write_ioreg: clear int status register failed\n");
			goto done;
		}
	} else {
		PRINTM(ERROR, "BT: ERR: ireg=0\n");
	}
	OS_INT_DISABLE;
	priv->adapter->sd_ireg |= ireg;
	OS_INT_RESTORE;
	bt_interrupt(m_dev);
done:
	LEAVE();
}

/**
 *  @brief This function checks if the interface is ready to download
 *  or not while other download interfaces are present
 *
 *  @param priv   A pointer to bt_private structure
 *  @param val    Winner status (0: winner)
 *  @return       BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
sd_check_winner_status(bt_private * priv, u8 * val)
{

	int ret = BT_STATUS_SUCCESS;
	u8 winner = 0;
	struct sdio_mmc_card *cardp = (struct sdio_mmc_card *)priv->bt_dev.card;

	ENTER();
	winner = sdio_readb(cardp->func, CARD_FW_STATUS0_REG, &ret);
	if (ret != BT_STATUS_SUCCESS) {
		LEAVE();
		return BT_STATUS_FAILURE;
	}
	*val = winner;

	LEAVE();
	return ret;
}

#ifdef SDIO_SUSPEND_RESUME
#ifdef MMC_PM_KEEP_POWER
#ifdef MMC_PM_FUNC_SUSPENDED
/** @brief This function tells lower driver that BT is suspended
 *
 *  @param priv    A pointer to bt_private structure
 *  @return        None
 */
void
bt_is_suspended(bt_private * priv)
{
	struct sdio_mmc_card *card = priv->bt_dev.card;
	priv->adapter->is_suspended = TRUE;
	sdio_func_suspended(card->func);
}
#endif

/** @brief This function handles client driver suspend
 *
 *  @param dev	   A pointer to device structure
 *  @return 	   BT_STATUS_SUCCESS or other error no.
 */
int
bt_sdio_suspend(struct device *dev)
{
	struct sdio_func *func = dev_to_sdio_func(dev);
	mmc_pm_flag_t pm_flags = 0;
	bt_private *priv = NULL;
	struct sdio_mmc_card *cardp;
	struct m_dev *m_dev = NULL;

	ENTER();

	if (func) {
		pm_flags = sdio_get_host_pm_caps(func);
		PRINTM(CMD, "BT: %s: suspend: PM flags = 0x%x\n",
		       sdio_func_id(func), pm_flags);
		if (!(pm_flags & MMC_PM_KEEP_POWER)) {
			PRINTM(ERROR,
			       "BT: %s: cannot remain alive while host is suspended\n",
			       sdio_func_id(func));
			return -ENOSYS;
		}
		cardp = sdio_get_drvdata(func);
		if (!cardp || !cardp->priv) {
			PRINTM(ERROR,
			       "BT: Card or priv structure is not valid\n");
			LEAVE();
			return BT_STATUS_SUCCESS;
		}
	} else {
		PRINTM(ERROR, "BT: sdio_func is not specified\n");
		LEAVE();
		return BT_STATUS_SUCCESS;
	}
	priv = cardp->priv;

	if ((mbt_pm_keep_power) && (priv->adapter->hs_state != HS_ACTIVATED)) {
		/* disable FM event mask */
		if ((priv->bt_dev.m_dev[FM_SEQ].dev_type == FM_TYPE) &&
		    test_bit(HCI_RUNNING, &(priv->bt_dev.m_dev[FM_SEQ].flags)))
			fm_set_intr_mask(priv, FM_DISABLE_INTR_MASK);
		if (BT_STATUS_SUCCESS != bt_enable_hs(priv)) {
			PRINTM(CMD, "BT: HS not actived, suspend fail!\n");
			LEAVE();
			return -EBUSY;
		}
	}
	m_dev = &(priv->bt_dev.m_dev[BT_SEQ]);
	PRINTM(CMD, "BT %s: SDIO suspend\n", m_dev->name);
	mbt_hci_suspend_dev(m_dev);
	skb_queue_purge(&priv->adapter->tx_queue);

	priv->adapter->is_suspended = TRUE;
	LEAVE();
	/* We will keep the power when hs enabled successfully */
	if ((mbt_pm_keep_power) && (priv->adapter->hs_state == HS_ACTIVATED)) {
#ifdef MMC_PM_SKIP_RESUME_PROBE
		PRINTM(CMD, "BT: suspend with MMC_PM_KEEP_POWER and "
		       "MMC_PM_SKIP_RESUME_PROBE\n");
		return sdio_set_host_pm_flags(func,
					      MMC_PM_KEEP_POWER |
					      MMC_PM_SKIP_RESUME_PROBE);
#else
		PRINTM(CMD, "BT: suspend with MMC_PM_KEEP_POWER\n");
		return sdio_set_host_pm_flags(func, MMC_PM_KEEP_POWER);
#endif
	} else {
		PRINTM(CMD, "BT: suspend without MMC_PM_KEEP_POWER\n");
		return BT_STATUS_SUCCESS;
	}
}

/** @brief This function handles client driver resume
 *
 *  @param dev	   A pointer to device structure
 *  @return 	   BT_STATUS_SUCCESS
 */
int
bt_sdio_resume(struct device *dev)
{
	struct sdio_func *func = dev_to_sdio_func(dev);
	mmc_pm_flag_t pm_flags = 0;
	bt_private *priv = NULL;
	struct sdio_mmc_card *cardp;
	struct m_dev *m_dev = NULL;

	ENTER();
	if (func) {
		pm_flags = sdio_get_host_pm_caps(func);
		PRINTM(CMD, "BT: %s: resume: PM flags = 0x%x\n",
		       sdio_func_id(func), pm_flags);
		cardp = sdio_get_drvdata(func);
		if (!cardp || !cardp->priv) {
			PRINTM(ERROR,
			       "BT: Card or priv structure is not valid\n");
			LEAVE();
			return BT_STATUS_SUCCESS;
		}
	} else {
		PRINTM(ERROR, "BT: sdio_func is not specified\n");
		LEAVE();
		return BT_STATUS_SUCCESS;
	}
	priv = cardp->priv;
	priv->adapter->is_suspended = FALSE;
	m_dev = &(priv->bt_dev.m_dev[BT_SEQ]);
	PRINTM(CMD, "BT %s: SDIO resume\n", m_dev->name);
	mbt_hci_resume_dev(m_dev);
	sbi_wakeup_firmware(priv);
	/* enable FM event mask */
	if ((priv->bt_dev.m_dev[FM_SEQ].dev_type == FM_TYPE) &&
	    test_bit(HCI_RUNNING, &(priv->bt_dev.m_dev[FM_SEQ].flags)))
		fm_set_intr_mask(priv, FM_DEFAULT_INTR_MASK);
	priv->adapter->hs_state = HS_DEACTIVATED;
	PRINTM(CMD, "BT:%s: HS DEACTIVATED in Resume!\n", m_dev->name);
	LEAVE();
	return BT_STATUS_SUCCESS;
}
#endif
#endif

/********************************************************
		Global Functions
********************************************************/
#ifdef SDIO_SUSPEND_RESUME
#ifdef MMC_PM_KEEP_POWER
static struct dev_pm_ops bt_sdio_pm_ops = {
	.suspend = bt_sdio_suspend,
	.resume = bt_sdio_resume,
};
#endif
#endif
static struct sdio_driver sdio_bt = {
	.name = "sdio_bt",
	.id_table = bt_ids,
	.probe = sd_probe_card,
	.remove = sd_remove_card,
#ifdef SDIO_SUSPEND_RESUME
#ifdef MMC_PM_KEEP_POWER
	.drv = {
		.pm = &bt_sdio_pm_ops,
		}
#endif
#endif
};

/**
 *  @brief This function registers the bt module in bus driver.
 *
 *  @return	   An int pointer that keeps returned value
 */
int *
sbi_register(void)
{
	int *ret;

	ENTER();

	if (sdio_register_driver(&sdio_bt) != 0) {
		PRINTM(FATAL, "BT: SD Driver Registration Failed \n");
		LEAVE();
		return NULL;
	} else
		ret = (int *)1;

	LEAVE();
	return ret;
}

/**
 *  @brief This function de-registers the bt module in bus driver.
 *
 *  @return 	   N/A
 */
void
sbi_unregister(void)
{
	ENTER();
	unregister = TRUE;
	sdio_unregister_driver(&sdio_bt);
	LEAVE();
}

/**
 *  @brief This function registers the device.
 *
 *  @param priv    A pointer to bt_private structure
 *  @return 	   BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
sbi_register_dev(bt_private * priv)
{
	int ret = BT_STATUS_SUCCESS;
	u8 reg;
	u8 chiprev;
	struct sdio_mmc_card *card = priv->bt_dev.card;
	struct sdio_func *func;

	ENTER();

	if (!card || !card->func) {
		PRINTM(ERROR, "BT: Error: card or function is NULL!\n");
		goto failed;
	}
	func = card->func;
	priv->hotplug_device = &func->dev;
	if (fw_name == NULL)
		fw_name = DEFAULT_FW_NAME;

	/* Initialize the private structure */
	strncpy(priv->bt_dev.name, "bt_sdio0", sizeof(priv->bt_dev.name));
	priv->bt_dev.ioport = 0;
	priv->bt_dev.fn = func->num;

	sdio_claim_host(func);
	ret = sdio_claim_irq(func, sd_interrupt);
	if (ret) {
		PRINTM(FATAL, ": sdio_claim_irq failed: ret=%d\n", ret);
		goto release_host;
	}
	ret = sdio_set_block_size(card->func, SD_BLOCK_SIZE);
	if (ret) {
		PRINTM(FATAL, ": %s: cannot set SDIO block size\n",
		       __FUNCTION__);
		goto release_irq;
	}

	/* read Revision Register to get the chip revision number */
	chiprev = sdio_readb(func, CARD_REVISION_REG, &ret);
	if (ret) {
		PRINTM(FATAL, ": cannot read CARD_REVISION_REG\n");
		goto release_irq;
	}
	priv->adapter->chip_rev = chiprev;
	PRINTM(INFO, "revision=%#x\n", chiprev);

	/*
	 * Read the HOST_INTSTATUS_REG for ACK the first interrupt got
	 * from the bootloader. If we don't do this we get a interrupt
	 * as soon as we register the irq.
	 */
	reg = sdio_readb(func, HOST_INTSTATUS_REG, &ret);
	if (ret < 0)
		goto release_irq;

	/* Read the IO port */
	reg = sdio_readb(func, IO_PORT_0_REG, &ret);
	if (ret < 0)
		goto release_irq;
	else
		priv->bt_dev.ioport |= reg;

	reg = sdio_readb(func, IO_PORT_1_REG, &ret);
	if (ret < 0)
		goto release_irq;
	else
		priv->bt_dev.ioport |= (reg << 8);

	reg = sdio_readb(func, IO_PORT_2_REG, &ret);
	if (ret < 0)
		goto release_irq;
	else
		priv->bt_dev.ioport |= (reg << 16);

	PRINTM(INFO, ": SDIO FUNC%d IO port: 0x%x\n", priv->bt_dev.fn,
	       priv->bt_dev.ioport);

	sdio_set_drvdata(func, card);
	sdio_release_host(func);

	LEAVE();
	return BT_STATUS_SUCCESS;
release_irq:
	sdio_release_irq(func);
release_host:
	sdio_release_host(func);
failed:

	LEAVE();
	return BT_STATUS_FAILURE;
}

/**
 *  @brief This function de-registers the device.
 *
 *  @param priv    A pointer to bt_private structure
 *  @return 	   BT_STATUS_SUCCESS
 */
int
sbi_unregister_dev(bt_private * priv)
{
	struct sdio_mmc_card *card = priv->bt_dev.card;

	ENTER();

	if (card && card->func) {
		sdio_claim_host(card->func);
		sdio_release_irq(card->func);
		sdio_disable_func(card->func);
		sdio_release_host(card->func);
		sdio_set_drvdata(card->func, NULL);
	}

	LEAVE();
	return BT_STATUS_SUCCESS;
}

/**
 *  @brief This function enables the host interrupts.
 *
 *  @param priv    A pointer to bt_private structure
 *  @return 	   BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
sd_enable_host_int(bt_private * priv)
{
	struct sdio_mmc_card *card = priv->bt_dev.card;
	int ret;

	ENTER();

	if (!card || !card->func) {
		LEAVE();
		return BT_STATUS_FAILURE;
	}
	sdio_claim_host(card->func);
	ret = sd_enable_host_int_mask(priv, HIM_ENABLE);
	sd_get_rx_unit(priv);
	sdio_release_host(card->func);

	LEAVE();
	return ret;
}

/**
 *  @brief This function disables the host interrupts.
 *
 *  @param priv    A pointer to bt_private structure
 *  @return 	   BT_STATUS_SUCCESS/BT_STATUS_FAILURE or other error no.
 */
int
sd_disable_host_int(bt_private * priv)
{
	struct sdio_mmc_card *card = priv->bt_dev.card;
	int ret;

	ENTER();

	if (!card || !card->func) {
		LEAVE();
		return BT_STATUS_FAILURE;
	}
	sdio_claim_host(card->func);
	ret = sd_disable_host_int_mask(priv, HIM_DISABLE);
	sdio_release_host(card->func);

	LEAVE();
	return ret;
}

/**
 *  @brief This function sends data to the card.
 *
 *  @param priv    A pointer to bt_private structure
 *  @param payload A pointer to the data/cmd buffer
 *  @param nb	   Length of data/cmd
 *  @return 	   BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
sbi_host_to_card(bt_private * priv, u8 * payload, u16 nb)
{
	struct sdio_mmc_card *card = priv->bt_dev.card;
	struct m_dev *m_dev = &(priv->bt_dev.m_dev[BT_SEQ]);
	int ret = BT_STATUS_SUCCESS;
	int buf_block_len;
	int blksz;
	int i = 0;
	u8 *buf = NULL;
	void *tmpbuf = NULL;
	int tmpbufsz;

	ENTER();

	if (!card || !card->func) {
		PRINTM(ERROR, "BT: card or function is NULL!\n");
		LEAVE();
		return BT_STATUS_FAILURE;
	}
	buf = payload;

	/* Allocate buffer and copy payload */
	if ((t_ptr) payload & (DMA_ALIGNMENT - 1)) {
		tmpbufsz = ALIGN_SZ(nb, DMA_ALIGNMENT);
		tmpbuf = kmalloc(tmpbufsz, GFP_KERNEL);
		if (!tmpbuf) {
			LEAVE();
			return BT_STATUS_FAILURE;
		}
		memset(tmpbuf, 0, tmpbufsz);
		/* Ensure 8-byte aligned CMD buffer */
		buf = (u8 *) ALIGN_ADDR(tmpbuf, DMA_ALIGNMENT);
		memcpy(buf, payload, nb);
	}
	blksz = SD_BLOCK_SIZE;
	buf_block_len = (nb + blksz - 1) / blksz;
	sdio_claim_host(card->func);
#define MAX_WRITE_IOMEM_RETRY	2
	do {
		/* Transfer data to card */
		ret = sdio_writesb(card->func, priv->bt_dev.ioport, buf,
				   buf_block_len * blksz);
		if (ret < 0) {
			i++;
			PRINTM(ERROR,
			       "BT: host_to_card, write iomem (%d) failed: %d\n",
			       i, ret);
			sdio_writeb(card->func, HOST_WO_CMD53_FINISH_HOST,
				    CONFIGURATION_REG, &ret);
			udelay(20);
			ret = BT_STATUS_FAILURE;
			if (i > MAX_WRITE_IOMEM_RETRY)
				goto exit;
		} else {
			PRINTM(DATA, "BT: SDIO Blk Wr %s: len=%d\n",
			       m_dev->name, nb);
			DBG_HEXDUMP(DAT_D, "BT: SDIO Blk Wr", payload, nb);
		}
	} while (ret == BT_STATUS_FAILURE);
	priv->bt_dev.tx_dnld_rdy = FALSE;
exit:
	sdio_release_host(card->func);
	if (tmpbuf)
		kfree(tmpbuf);
	LEAVE();
	return ret;
}

/**
 *  @brief This function downloads firmware
 *
 *  @param priv    A pointer to bt_private structure
 *  @return 	   BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
sbi_download_fw(bt_private * priv)
{
	struct sdio_mmc_card *card = priv->bt_dev.card;
	int ret = BT_STATUS_SUCCESS;
	struct m_dev *m_dev_bt = &(priv->bt_dev.m_dev[BT_SEQ]);
	struct m_dev *m_dev_fm = &(priv->bt_dev.m_dev[FM_SEQ]);
	struct m_dev *m_dev_nfc = &(priv->bt_dev.m_dev[NFC_SEQ]);
	int poll_num = MAX_FIRMWARE_POLL_TRIES;
	u8 winner = 0;

	ENTER();

	if (!card || !card->func) {
		PRINTM(ERROR, "BT: card or function is NULL!\n");
		ret = BT_STATUS_FAILURE;
		goto exit;
	}

	sdio_claim_host(card->func);
	if (BT_STATUS_SUCCESS == sd_verify_fw_download(priv, 1)) {
		PRINTM(MSG, "BT: FW already downloaded!\n");
		sdio_release_host(card->func);
		sd_enable_host_int(priv);
		if (BT_STATUS_FAILURE == sbi_register_conf_dpc(priv)) {
			PRINTM(ERROR,
			       "BT: sbi_register_conf_dpc failed. Terminating download\n");
			ret = BT_STATUS_FAILURE;
			goto err_register;
		}
		goto exit;
	}
	/* Check if other interface is downloading */
	ret = sd_check_winner_status(priv, &winner);
	if (ret == BT_STATUS_FAILURE) {
		PRINTM(FATAL, "BT read winner status failed!\n");
		goto done;
	}
	if (winner) {
		PRINTM(MSG, "BT is not the winner (0x%x). Skip FW download\n",
		       winner);
		poll_num = MAX_MULTI_INTERFACE_POLL_TRIES;
		/* check if the fimware is downloaded successfully or not */
		if (sd_verify_fw_download(priv, poll_num)) {
			PRINTM(FATAL, "BT: FW failed to be active in time!\n");
			ret = BT_STATUS_FAILURE;
			goto done;
		}
		sdio_release_host(card->func);
		sd_enable_host_int(priv);
		if (BT_STATUS_FAILURE == sbi_register_conf_dpc(priv)) {
			PRINTM(ERROR,
			       "BT: sbi_register_conf_dpc failed. Terminating download\n");
			ret = BT_STATUS_FAILURE;
			goto err_register;
		}
		goto exit;
	}

	do_gettimeofday(&priv->req_fw_time);
	/* Download the main firmware via the helper firmware */
	if (sd_download_firmware_w_helper(priv)) {
		PRINTM(INFO, "BT: FW download failed!\n");
		ret = BT_STATUS_FAILURE;
		goto done;
	}
	goto exit;
done:
	sdio_release_host(card->func);
exit:
	LEAVE();
	return ret;
err_register:
	if (m_dev_bt->dev_pointer) {
		if (m_dev_bt->spec_type == IANYWHERE_SPEC)
			free_m_dev(m_dev_bt);
	}
	if (m_dev_fm->dev_pointer)
		free_m_dev(m_dev_fm);
	if (m_dev_nfc->dev_pointer)
		free_m_dev(m_dev_nfc);
	if (priv->adapter)
		bt_free_adapter(priv);
	LEAVE();
	return ret;
}

/**
 *  @brief This function checks the interrupt status and handle it accordingly.
 *
 *  @param priv    A pointer to bt_private structure
 *  @return 	   BT_STATUS_SUCCESS
 */
int
sbi_get_int_status(bt_private * priv)
{
	int ret = BT_STATUS_SUCCESS;
	u8 sdio_ireg = 0;
	struct sdio_mmc_card *card = priv->bt_dev.card;

	ENTER();

	OS_INT_DISABLE;
	sdio_ireg = priv->adapter->sd_ireg;
	priv->adapter->sd_ireg = 0;
	OS_INT_RESTORE;
	sdio_claim_host(card->func);
	priv->adapter->irq_done = sdio_ireg;
	if (sdio_ireg & DN_LD_HOST_INT_STATUS) {	/* tx_done INT */
		if (priv->bt_dev.tx_dnld_rdy) {	/* tx_done already received */
			PRINTM(INFO,
			       "BT: warning: tx_done already received: tx_dnld_rdy=0x%x int status=0x%x\n",
			       priv->bt_dev.tx_dnld_rdy, sdio_ireg);
		} else {
			priv->bt_dev.tx_dnld_rdy = TRUE;
		}
	}
	if (sdio_ireg & UP_LD_HOST_INT_STATUS)
		sd_card_to_host(priv);

	ret = BT_STATUS_SUCCESS;
	sdio_release_host(card->func);
	LEAVE();
	return ret;
}

/**
 *  @brief This function wakeup firmware
 *
 *  @param priv    A pointer to bt_private structure
 *  @return 	   BT_STATUS_SUCCESS/BT_STATUS_FAILURE or other error no.
 */
int
sbi_wakeup_firmware(bt_private * priv)
{
	struct sdio_mmc_card *card = priv->bt_dev.card;
	int ret = BT_STATUS_SUCCESS;

	ENTER();

	if (!card || !card->func) {
		PRINTM(ERROR, "BT: card or function is NULL!\n");
		LEAVE();
		return BT_STATUS_FAILURE;
	}
	sdio_claim_host(card->func);
	sdio_writeb(card->func, HOST_POWER_UP, CONFIGURATION_REG, &ret);
	sdio_release_host(card->func);
	PRINTM(CMD, "BT wake up firmware\n");

	LEAVE();
	return ret;
}

module_param(fw_name, charp, 0);
MODULE_PARM_DESC(fw_name, "Firmware name");
module_param(req_fw_nowait, int, 0);
MODULE_PARM_DESC(req_fw_nowait,
		 "0: Use request_firmware API; 1: Use request_firmware_nowait API");
module_param(multi_fn, int, 4);
MODULE_PARM_DESC(multi_fn, "Bit 2: FN2;");
