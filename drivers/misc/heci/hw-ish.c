/*
 * H/W layer of HECI provider device (ISS)
 *
 * Copyright (c) 2014-2015, Intel Corporation.
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/pci.h>
#include <linux/sched.h>
#include "client.h"
#include "hw-ish.h"
#include "utils.h"
#include "heci_dev.h"
#include "hbm.h"
#include <linux/spinlock.h>
#include <linux/jiffies.h>

#ifdef dev_dbg
#undef dev_dbg
#endif
static void no_dev_dbg(void *v, char *s, ...)
{
}
/*#define dev_dbg dev_err*/
#define dev_dbg no_dev_dbg

#include <linux/delay.h>

/**
 * ish_reg_read - reads 32bit register
 *
 * @dev: the device structure
 * @offset: offset from which to read the data
 */
static inline u32 ish_reg_read(const struct heci_device *dev,
	unsigned long offset)
{
	struct ish_hw *hw = to_ish_hw(dev);
	return readl(hw->mem_addr + offset);
}

/**
 * ish_reg_write - Writes 32bit register
 *
 * @dev: the device structure
 * @offset: offset from which to write the data
 * @value: the byte to write
 */
static inline void ish_reg_write(struct heci_device *dev, unsigned long offset,
	u32 value)
{
	struct ish_hw *hw = to_ish_hw(dev);
	writel(value, hw->mem_addr + offset);
}

static inline u32 ish_read_fw_sts_reg(struct heci_device *dev)
{
	return ish_reg_read(dev, IPC_REG_ISH_HOST_FWSTS);
}

bool check_generated_interrupt(struct heci_device *dev)
{
	bool interrupt_generated = true;
	u32 pisr_val = 0;

	pisr_val = ish_reg_read(dev, IPC_REG_PISR);
	interrupt_generated = IPC_INT_FROM_ISH_TO_HOST(pisr_val);

	return interrupt_generated;
}


u32 ipc_output_payload_read(struct heci_device *dev, unsigned long index)
{
	return ish_reg_read(dev, IPC_REG_ISH2HOST_MSG +	(index * sizeof(u32)));
}

/**
 * ish_read - reads a message from heci device.
 *
 * @dev: the device structure
 * @buffer: message buffer will be written
 * @buffer_length: message size will be read
 */
static int ish_read(struct heci_device *dev, unsigned char *buffer,
	unsigned long buffer_length)
{
	u32	i;
	u32	*r_buf = (u32 *)buffer;
	u32	msg_offs;

	dev_dbg(&dev->pdev->dev, "buffer-length = %lu buf[0]0x%08X\n",
		buffer_length, ipc_output_payload_read(dev, 0));

	msg_offs = IPC_REG_ISH2HOST_MSG + sizeof(struct heci_msg_hdr);
	for (i = 0; i < buffer_length; i += sizeof(u32))
		*r_buf++ = ish_reg_read(dev, msg_offs + i);

	return 0;
}

/**
 * ish_is_input_ready - check if ISS FW is ready for receiving data
 *
 * @dev: the device structure
 */
static bool ish_is_input_ready(struct heci_device *dev)
{
	u32 doorbell_val;

	doorbell_val = ish_reg_read(dev, IPC_REG_HOST2ISH_DRBL);
	return !IPC_IS_BUSY(doorbell_val);
}

/**
 * ish_intr_enable - enables heci device interrupts
 *
 * @dev: the device structure
 */
void ish_intr_enable(struct heci_device *dev)
{
/*	u32 host_status = 0; */

	dev_dbg(&dev->pdev->dev, "ish_intr_enable\n");
	if (dev->pdev->revision == REVISION_ID_CHT_A0 ||
			(dev->pdev->revision & REVISION_ID_SI_MASK) ==
			REVISION_ID_CHT_A0_SI)
		ish_reg_write(dev, IPC_REG_HOST_COMM, 0x81);
	else if (dev->pdev->revision == REVISION_ID_CHT_B0 ||
			(dev->pdev->revision & REVISION_ID_SI_MASK) ==
			REVISION_ID_CHT_Bx_SI ||
			(dev->pdev->revision & REVISION_ID_SI_MASK) ==
			REVISION_ID_CHT_Kx_SI ||
			(dev->pdev->revision & REVISION_ID_SI_MASK) ==
			REVISION_ID_CHT_Dx_SI) {
		uint32_t	host_comm_val;

		host_comm_val = ish_reg_read(dev, IPC_REG_HOST_COMM);
		host_comm_val |= IPC_HOSTCOMM_INT_EN_BIT | 0x81;
		ish_reg_write(dev, IPC_REG_HOST_COMM, host_comm_val);
	}
}

/**
 * ish_intr_disable - disables heci device interrupts
 *
 * @dev: the device structure
 */
void ish_intr_disable(struct heci_device *dev)
{
	dev_dbg(&dev->pdev->dev, "ish_intr_disable\n");
	if (dev->pdev->revision == REVISION_ID_CHT_A0 ||
			(dev->pdev->revision & REVISION_ID_SI_MASK) ==
			REVISION_ID_CHT_A0_SI)
		/*ish_reg_write(dev, IPC_REG_HOST_COMM, 0xC1)*/;
	else if (dev->pdev->revision == REVISION_ID_CHT_B0 ||
			(dev->pdev->revision & REVISION_ID_SI_MASK) ==
			REVISION_ID_CHT_Bx_SI ||
			(dev->pdev->revision & REVISION_ID_SI_MASK) ==
			REVISION_ID_CHT_Kx_SI ||
			(dev->pdev->revision & REVISION_ID_SI_MASK) ==
			REVISION_ID_CHT_Dx_SI) {
		uint32_t	host_comm_val;

		host_comm_val = ish_reg_read(dev, IPC_REG_HOST_COMM);
		host_comm_val &= ~IPC_HOSTCOMM_INT_EN_BIT;
		host_comm_val |= 0xC1;
		ish_reg_write(dev, IPC_REG_HOST_COMM, host_comm_val);
	}
}

/*
 * BH processing work function (instead of thread handler)
 */
static void	bh_hbm_work_fn(struct work_struct *work)
{
	unsigned long	flags;
	struct heci_device	*dev;
	unsigned char	hbm[IPC_PAYLOAD_SIZE];

	ISH_DBG_PRINT(KERN_ALERT "%s(): work=%p +++\n", __func__, work);
	dev = container_of(work, struct heci_device, bh_hbm_work);
	spin_lock_irqsave(&dev->rd_msg_spinlock, flags);
	if (dev->rd_msg_fifo_head != dev->rd_msg_fifo_tail) {
		memcpy(hbm, dev->rd_msg_fifo + dev->rd_msg_fifo_head,
			IPC_PAYLOAD_SIZE);
		dev->rd_msg_fifo_head =
			(dev->rd_msg_fifo_head + IPC_PAYLOAD_SIZE) %
			(RD_INT_FIFO_SIZE * IPC_PAYLOAD_SIZE);
		spin_unlock_irqrestore(&dev->rd_msg_spinlock, flags);
		heci_hbm_dispatch(dev, (struct heci_bus_message *)hbm);
	} else {
		spin_unlock_irqrestore(&dev->rd_msg_spinlock, flags);
	}
	ISH_DBG_PRINT(KERN_ALERT "%s(): ---\n", __func__);
}
/*#####################################################*/

/*
 * Got msg with IPC (and upper protocol) header
 * and add it to the device Tx-to-write list
 * then try to send the first IPC waiting msg (if DRBL is cleared)
 * RETURN VALUE:	negative -	fail (means free links list is empty,
 *					or msg too long)
 *			0 -	succeed
 */
static int write_ipc_to_queue(struct heci_device *dev,
	void (*ipc_send_compl)(void *), void *ipc_send_compl_prm,
	unsigned char *msg, int length)
{
	struct wr_msg_ctl_info *ipc_link;
	unsigned long   flags;

	ISH_DBG_PRINT(KERN_ALERT "%s(): +++ length=%u\n", __func__, length);
	if (length > IPC_FULL_MSG_SIZE)
		return -EMSGSIZE;

	spin_lock_irqsave(&dev->wr_processing_spinlock, flags);
	if (list_empty(&dev->wr_free_list_head.link)) {
		spin_unlock_irqrestore(&dev->wr_processing_spinlock, flags);
		return -ENOMEM;
	}
	ipc_link = list_entry(dev->wr_free_list_head.link.next,
		struct wr_msg_ctl_info, link);
	list_del_init(&ipc_link->link);

	ipc_link->ipc_send_compl = ipc_send_compl;
	ipc_link->ipc_send_compl_prm = ipc_send_compl_prm;
	ipc_link->length = length;
	memcpy(ipc_link->inline_data, msg, length);

	list_add_tail(&ipc_link->link, &dev->wr_processing_list_head.link);
	spin_unlock_irqrestore(&dev->wr_processing_spinlock, flags);

	write_ipc_from_queue(dev);
	ISH_DBG_PRINT(KERN_ALERT "%s(): ---\n", __func__);
	return 0;
}

/* check if DRBL is cleared. if it is - write the first IPC msg,
 * then call the callback function (if it isn't NULL)
 */
int write_ipc_from_queue(struct heci_device *dev)
{
	u32	doorbell_val;
	unsigned long length;
	unsigned long rem;
	u32	*r_buf;
	int i;
	struct wr_msg_ctl_info	*ipc_link;
	u32	reg_addr;
	unsigned long	flags;
	void	(*ipc_send_compl)(void *);
	void	*ipc_send_compl_prm;
	static int	out_ipc_locked;
	unsigned long	out_ipc_flags;

	ISH_DBG_PRINT(KERN_ALERT "%s(): +++\n", __func__);

	if (dev->dev_state == HECI_DEV_DISABLED)
		return -EINVAL;

	spin_lock_irqsave(&dev->out_ipc_spinlock, out_ipc_flags);
	if (out_ipc_locked) {
		spin_unlock_irqrestore(&dev->out_ipc_spinlock, out_ipc_flags);
		return -EBUSY;
	}
	out_ipc_locked = 1;
	if (!ish_is_input_ready(dev)) {
		ISH_DBG_PRINT(KERN_ALERT "%s(): --- EBUSY\n", __func__);
		out_ipc_locked = 0;
		spin_unlock_irqrestore(&dev->out_ipc_spinlock, out_ipc_flags);
		return -EBUSY;
	}
	spin_unlock_irqrestore(&dev->out_ipc_spinlock, out_ipc_flags);

	spin_lock_irqsave(&dev->wr_processing_spinlock, flags);
	/*
	 * if empty list - return 0; may happen, as RX_COMPLETE handler doesn't
	 * check list emptiness
	 */
	if (list_empty(&dev->wr_processing_list_head.link)) {
		spin_unlock_irqrestore(&dev->wr_processing_spinlock, flags);
		ISH_DBG_PRINT(KERN_ALERT "%s(): --- empty\n", __func__);
		out_ipc_locked = 0;
		return	0;
	}

	ipc_link = list_entry(dev->wr_processing_list_head.link.next,
		struct wr_msg_ctl_info, link);
	length = ipc_link->length - sizeof(u32);
	/*first 4 bytes of the data is the doorbell value (IPC header)*/
	doorbell_val = *(u32 *)ipc_link->inline_data;
	r_buf = (u32 *)(ipc_link->inline_data + sizeof(u32));

	/* If sending MNG_SYNC_FW_CLOCK, update clock again */
	if (IPC_HEADER_GET_PROTOCOL(doorbell_val) == IPC_PROTOCOL_MNG &&
		IPC_HEADER_GET_MNG_CMD(doorbell_val) == MNG_SYNC_FW_CLOCK) {

		struct timespec ts;
		uint64_t usec;

		get_monotonic_boottime(&ts);
		usec = (uint64_t)ts.tv_sec * 1000000 +
			(uint64_t)ts.tv_nsec / 1000;
		r_buf[0] = (u32)(usec & 0xFFFFFFFF);
		r_buf[1] = (u32)(usec >> 32);
	}

	for (i = 0, reg_addr = IPC_REG_HOST2ISH_MSG; i < length >> 2; i++,
			reg_addr += 4)
		ish_reg_write(dev, reg_addr, r_buf[i]);

	rem = length & 0x3;
	if (rem > 0) {
		u32 reg = 0;
		memcpy(&reg, &r_buf[length >> 2], rem);
		ish_reg_write(dev, reg_addr, reg);
	}

	/* HID client debug */
	if (doorbell_val == 0x8000040C &&
		ish_reg_read(dev, IPC_REG_HOST2ISH_MSG) == 0x80080000 &&
		ish_reg_read(dev, IPC_REG_HOST2ISH_MSG+4) == 0x00030508) {
			++dev->ipc_hid_out_fc;
			++dev->ipc_hid_out_fc_cnt;
		}
	else if ((doorbell_val & 0xFFFFFC00) == 0x80000400 &&
		(ish_reg_read(dev, IPC_REG_HOST2ISH_MSG) & 0x8000FFFF) ==
				0x80000305)
			--dev->ipc_hid_in_fc;

	/* Update IPC counters */
	++dev->ipc_tx_cnt;
	dev->ipc_tx_bytes_cnt += IPC_HEADER_GET_LENGTH(doorbell_val);

	ish_reg_write(dev, IPC_REG_HOST2ISH_DRBL, doorbell_val);
	out_ipc_locked = 0;
	ISH_DBG_PRINT(KERN_ALERT
		"%s(): in msg. registers: %08X ! %08X %08X %08X %08X... hostcomm reg: %08X\n",
		__func__, ish_reg_read(dev, IPC_REG_HOST2ISH_DRBL),
		ish_reg_read(dev, IPC_REG_HOST2ISH_MSG),
		ish_reg_read(dev, IPC_REG_HOST2ISH_MSG + 4),
		ish_reg_read(dev, IPC_REG_HOST2ISH_MSG + 8),
		ish_reg_read(dev, IPC_REG_HOST2ISH_MSG + 0xC),
		ish_reg_read(dev, IPC_REG_HOST_COMM));

	ipc_send_compl = ipc_link->ipc_send_compl;
	ipc_send_compl_prm = ipc_link->ipc_send_compl_prm;
	list_del_init(&ipc_link->link);
	list_add_tail(&ipc_link->link, &dev->wr_free_list_head.link);
	spin_unlock_irqrestore(&dev->wr_processing_spinlock, flags);

	/*
	 * callback will be called out of spinlock,
	 * after ipc_link returned to free list
	 */
	if (ipc_send_compl)
		ipc_send_compl(ipc_send_compl_prm);
	ISH_DBG_PRINT(KERN_ALERT
		"%s(): --- written %lu bytes [%08X ! %08X %08X %08X %08X...]\n",
		__func__, length, *(u32 *)ipc_link->inline_data, r_buf[0],
		r_buf[1], r_buf[2], r_buf[3]);
	return 0;
}

/*#####################################################*/

static int	ish_fw_reset_handler(struct heci_device *dev)
{
	uint32_t	reset_id;
	unsigned long	flags;
	struct wr_msg_ctl_info *processing, *next;
	/* Read reset ID */
	reset_id = ish_reg_read(dev, IPC_REG_ISH2HOST_MSG) & 0xFFFF;

	/* Handle FW-initiated reset */
	dev->dev_state = HECI_DEV_RESETTING;

	/* Clear HOST2ISH.ILUP (what's it?) */
	/*ish_clr_host_rdy(dev);*/

	/* Clear IPC output queue */
	spin_lock_irqsave(&dev->wr_processing_spinlock, flags);
	list_for_each_entry_safe(processing, next,
			&dev->wr_processing_list_head.link, link) {
		list_del(&processing->link);
		list_add_tail(&processing->link, &dev->wr_free_list_head.link);
	}
	spin_unlock_irqrestore(&dev->wr_processing_spinlock, flags);

	/* Clear BH processing queue - no further HBMs */
	spin_lock_irqsave(&dev->rd_msg_spinlock, flags);
	dev->rd_msg_fifo_head = dev->rd_msg_fifo_tail = 0;
	spin_unlock_irqrestore(&dev->rd_msg_spinlock, flags);

	/* Handle ISS FW reset against upper layers */
	heci_bus_remove_all_clients(dev);			/* Remove all client devices */

	/* Send RESET_NOTIFY_ACK (with reset_id) */
/*#####################################*/
	if (!ish_is_input_ready(dev))
		timed_wait_for_timeout(WAIT_FOR_SEND_SLICE,
			ish_is_input_ready(dev), (2 * HZ));

	/* ISS FW is dead (?) */
	if (!ish_is_input_ready(dev)) {
		return	-EPIPE;
	} else {
		/*
		 * Set HOST2ISH.ILUP. Apparently we need this BEFORE sending
		 * RESET_NOTIFY_ACK - FW will be checking for it
		 */
		ish_set_host_rdy(dev);
		ipc_send_mng_msg(dev, MNG_RESET_NOTIFY_ACK, &reset_id,
			sizeof(uint32_t));
	}
/*####################################*/

	/* Wait for ISS FW'es ILUP and HECI_READY */
	timed_wait_for_timeout(WAIT_FOR_SEND_SLICE, ish_hw_is_ready(dev),
		(2 * HZ));
	if (!ish_hw_is_ready(dev)) {
		/* ISS FW is dead */
		uint32_t	ish_status;
		ish_status = ish_reg_read(dev, IPC_REG_ISH_HOST_FWSTS);
		dev_err(&dev->pdev->dev,
		"[heci-ish]: completed reset, ISS is dead (FWSTS = %08X)\n",
		ish_status);
		return -ENODEV;
	}

	return	0;
}

struct work_struct	fw_reset_work;
struct heci_device	*heci_dev;

static void	fw_reset_work_fn(struct work_struct *unused)
{
	int	rv;
	static int reset_cnt;

	ISH_DBG_PRINT(KERN_ALERT "%s(): +++\n", __func__);
	reset_cnt++;

	rv = ish_fw_reset_handler(heci_dev);
	if (!rv) {
		/* ISS is ILUP & HECI-ready. Restart HECI */
	/* bug fix here: when reset flow occurs, sometimes, the sysfs entries
		which were removed in ish_fw_reset_handler were still up,
		but the driver tried to create the same entries and failed.
		so wait some time here and then the sysfs entries removal will
		be done */
		if (reset_cnt != 0) /* not the boot flow */
			schedule_timeout(HZ / 3);
		heci_dev->recvd_hw_ready = 1;
		if (waitqueue_active(&heci_dev->wait_hw_ready))
			wake_up(&heci_dev->wait_hw_ready);

		heci_dev->dev_state = HECI_DEV_INIT_CLIENTS;
		heci_dev->hbm_state = HECI_HBM_START;
		heci_hbm_start_req(heci_dev);
		ISH_DBG_PRINT(KERN_ALERT "%s(): after heci_hbm_start_req()\n",
			__func__);

	} else
		printk(KERN_ERR "[heci-ish]: FW reset failed (%d)\n", rv);
}


static void sync_fw_clock(struct heci_device *dev)
{
	static unsigned long prev_sync;
	struct timespec ts;
	uint64_t usec;

	if (prev_sync && jiffies - prev_sync < 20 * HZ)
		return;

	prev_sync = jiffies;
	get_monotonic_boottime(&ts);
	usec = (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000;
	ipc_send_mng_msg(dev, MNG_SYNC_FW_CLOCK, &usec, sizeof(uint64_t));
}


/*
 *	Receive and process IPC management messages
 *
 *	NOTE: Any other mng command than reset_notify and reset_notify_ack
 *	won't wake BH handler
 */
static void	recv_ipc(struct heci_device *dev, uint32_t doorbell_val)
{
	uint32_t	mng_cmd;

	mng_cmd = IPC_HEADER_GET_MNG_CMD(doorbell_val);
	ISH_DBG_PRINT(KERN_ALERT "%s(): handled IPC mng_cmd=%08X\n", __func__,
		mng_cmd);

	switch (mng_cmd) {
	default:
		break;

	case MNG_RX_CMPL_INDICATION:
		ISH_DBG_PRINT(KERN_ALERT
			"%s(): RX_COMPLETE -- IPC_REG_ISH2HOST_MSG[0] = %08X\n",
			__func__, ish_reg_read(dev, IPC_REG_ISH2HOST_MSG));
		if (suspend_flag) {
			suspend_flag = 0;
			if (waitqueue_active(&suspend_wait))
				wake_up(&suspend_wait);
		}
		write_ipc_from_queue(dev);
		break;

	case MNG_RESET_NOTIFY:
		ISH_DBG_PRINT(KERN_ALERT "%s(): MNG_RESET_NOTIFY\n", __func__);
		if (!heci_dev) {
			heci_dev = dev;
			INIT_WORK(&fw_reset_work, fw_reset_work_fn);
		}
		schedule_work(&fw_reset_work);
		break;

	case MNG_RESET_NOTIFY_ACK:
		ISH_DBG_PRINT(KERN_ALERT "%s(): MNG_RESET_NOTIFY_ACK\n",
			__func__);
		dev->recvd_hw_ready = 1;
		if (waitqueue_active(&dev->wait_hw_ready))
			wake_up(&dev->wait_hw_ready);
		break;
	}
}




/**
 * ish_irq_handler - ISR of the HECI device
 *
 * @irq: irq number
 * @dev_id: pointer to the device structure
 *
 * returns irqreturn_t
 */
irqreturn_t ish_irq_handler(int irq, void *dev_id)
{
	struct heci_device *dev = dev_id;
	uint32_t	doorbell_val;
	struct heci_msg_hdr	*heci_hdr;
	bool interrupt_generated;
	u32 pisr_val;
	u32	msg_hdr;

	ISH_DBG_PRINT(KERN_ALERT "%s(): irq=%d +++\n", __func__, irq);

	/* Check that it's interrupt from ISH (may be shared) */
	pisr_val = ish_reg_read(dev, IPC_REG_PISR);
	interrupt_generated = IPC_INT_FROM_ISH_TO_HOST(pisr_val);

	ISH_DBG_PRINT(KERN_ALERT "%s(): interrupt_generated=%d [PIMR=%08X]\n",
		__func__, (int)interrupt_generated,
		ish_reg_read(dev, IPC_REG_PIMR));
	if (!interrupt_generated)
		return IRQ_NONE;

	doorbell_val = ish_reg_read(dev, IPC_REG_ISH2HOST_DRBL);
	ISH_DBG_PRINT(KERN_ALERT "%s(): IPC_IS_BUSY=%d\n", __func__,
		(int)IPC_IS_BUSY(doorbell_val));
	if (!IPC_IS_BUSY(doorbell_val))
		return IRQ_HANDLED;

	ISH_DBG_PRINT("%s(): doorbell is busy - YES\n", __func__);

	/* CHECKME: double check this */
	if (dev->dev_state == HECI_DEV_DISABLED)
		return IRQ_HANDLED;

	ish_intr_disable(dev);

	/* Sanity check: IPC dgram length in header */
	if (IPC_HEADER_GET_LENGTH(doorbell_val) > IPC_PAYLOAD_SIZE) {
		dev_err(&dev->pdev->dev,
			"%s(): IPC hdr - bad length: %u; dropped\n",
			__func__,
			(unsigned)IPC_HEADER_GET_LENGTH(doorbell_val));
		goto	eoi;
	}

	ISH_DBG_PRINT(KERN_ALERT "[heci-ish] %s(): protocol=%u\n", __func__,
		IPC_HEADER_GET_PROTOCOL(doorbell_val));

	/* IPC message */
	if (IPC_HEADER_GET_PROTOCOL(doorbell_val) == IPC_PROTOCOL_MNG) {
		recv_ipc(dev, doorbell_val);
		goto	eoi;
	}

	if (IPC_HEADER_GET_PROTOCOL(doorbell_val) != IPC_PROTOCOL_HECI)
		goto	eoi;

	/* Read HECI header dword */
	msg_hdr = ish_read_hdr(dev);
	if (!msg_hdr)
		goto	eoi;

	sync_fw_clock(dev);

	heci_hdr = (struct heci_msg_hdr *)&msg_hdr;

	/* Sanity check: HECI frag. length in header */
	if (heci_hdr->length > dev->mtu) {
		dev_err(&dev->pdev->dev,
			"%s(): HECI hdr - bad length: %u; dropped [%08X]\n",
			__func__,
			(unsigned)heci_hdr->length, msg_hdr);
		goto	eoi;
	}

	/* HECI bus message */
	if (!heci_hdr->host_addr && !heci_hdr->me_addr) {
		recv_hbm(dev, heci_hdr);
		goto	eoi;

	/* HECI fixed-client message */
	} else if (!heci_hdr->host_addr) {
		recv_fixed_cl_msg(dev, heci_hdr);
		goto	eoi;
	} else {
		/* HECI client message */
		recv_heci_cl_msg(dev, heci_hdr);
		goto	eoi;
	}

eoi:
	ISH_DBG_PRINT(KERN_ALERT
		"%s(): Doorbell cleared, busy reading cleared\n", __func__);
	/* Update IPC counters */
	++dev->ipc_rx_cnt;
	dev->ipc_rx_bytes_cnt += IPC_HEADER_GET_LENGTH(doorbell_val);

	ish_reg_write(dev, IPC_REG_ISH2HOST_DRBL, 0);
	/*
	 * Here and above: we need to actually read this register
	 * in order to unblock further interrupts on CHT A0
	 */
	ish_intr_enable(dev);
	return	IRQ_HANDLED;
}


static int	ipc_send_mng_msg(struct heci_device *dev, uint32_t msg_code,
	void *msg, size_t size)
{
	unsigned char	ipc_msg[IPC_FULL_MSG_SIZE];
	uint32_t	drbl_val = IPC_BUILD_MNG_MSG(msg_code, size);

	memcpy(ipc_msg, &drbl_val, sizeof(uint32_t));
	memcpy(ipc_msg + sizeof(uint32_t), msg, size);
	return	write_ipc_to_queue(dev, NULL, NULL, ipc_msg,
		sizeof(uint32_t) + size);
}


static int	ipc_send_heci_msg(struct heci_device *dev,
	struct heci_msg_hdr *hdr, void *msg, void(*ipc_send_compl)(void *),
	void *ipc_send_compl_prm)
{
	unsigned char	ipc_msg[IPC_FULL_MSG_SIZE];
	uint32_t	drbl_val;

	drbl_val = IPC_BUILD_HEADER(hdr->length + sizeof(struct heci_msg_hdr),
		IPC_PROTOCOL_HECI, 1);

	memcpy(ipc_msg, &drbl_val, sizeof(uint32_t));
	memcpy(ipc_msg + sizeof(uint32_t), hdr, sizeof(uint32_t));
	memcpy(ipc_msg + 2 * sizeof(uint32_t), msg, hdr->length);
	return	write_ipc_to_queue(dev, ipc_send_compl, ipc_send_compl_prm,
		ipc_msg, 2 * sizeof(uint32_t) + hdr->length);
}


/**
 * ish_hw_is_ready - check if the hw is ready
 *
 * @dev: the device structure
 */
bool ish_hw_is_ready(struct heci_device *dev)
{
	u32 ish_status =  ish_reg_read(dev, IPC_REG_ISH_HOST_FWSTS);
	return IPC_IS_ISH_ILUP(ish_status) && IPC_IS_ISH_HECI_READY(ish_status);
}

/**
 * ish_host_is_ready - check if the host is ready
 *
 * @dev: the device structure
 */
bool ish_host_is_ready(struct heci_device *dev)
{
	return true;
}

void ish_set_host_rdy(struct heci_device *dev)
{
	u32  host_status = ish_reg_read(dev, IPC_REG_HOST_COMM);
	dev_dbg(&dev->pdev->dev, "before HOST start host_status=%08X\n",
		host_status);
	IPC_SET_HOST_READY(host_status);
	ish_reg_write(dev, IPC_REG_HOST_COMM, host_status);
	host_status = ish_reg_read(dev, IPC_REG_HOST_COMM);
	dev_dbg(&dev->pdev->dev, "actually sent HOST start host_status=%08X\n",
		host_status);
}

void ish_clr_host_rdy(struct heci_device *dev)
{
	u32  host_status = ish_reg_read(dev, IPC_REG_HOST_COMM);
	dev_dbg(&dev->pdev->dev, "before HOST start host_status=%08X\n",
		host_status);
	IPC_CLEAR_HOST_READY(host_status);
	ish_reg_write(dev, IPC_REG_HOST_COMM, host_status);
	host_status = ish_reg_read(dev, IPC_REG_HOST_COMM);
	dev_dbg(&dev->pdev->dev, "actually sent HOST start host_status=%08X\n",
		host_status);
}


static int ish_hw_reset(struct heci_device *dev)
{
	struct pci_dev *pdev = dev->pdev;
	struct ish_hw *hw = to_ish_hw(dev);
	int rv;
	u16 csr;

#define MAX_DMA_DELAY	20
	unsigned dma_delay;

	if (!pdev)
		return -ENODEV;

	rv = pci_reset_function(pdev);
	if (!rv)
		dev->dev_state = HECI_DEV_RESETTING;

	if (!pdev->pm_cap) {
		dev_err(&pdev->dev, "Can't reset - no PM caps\n");
		return -EINVAL;
	}

	/* Now trigger reset to FW */
	writel(0, hw->mem_addr + IPC_REG_ISH_RMP2);

	for (dma_delay = 0; dma_delay < MAX_DMA_DELAY &&
			ish_reg_read(dev, IPC_REG_ISH_HOST_FWSTS) & (IPC_ISH_IN_DMA);
			dma_delay += 5);
		mdelay(5);

	if (dma_delay >= MAX_DMA_DELAY) {
		dev_err(&pdev->dev,
			"Can't reset - stuck with DMA in-progress\n");
		return -EBUSY;
	}

	pci_read_config_word(pdev, pdev->pm_cap + PCI_PM_CTRL, &csr);

	csr &= ~PCI_PM_CTRL_STATE_MASK;
	csr |= PCI_D3hot;
	pci_write_config_word(pdev, pdev->pm_cap + PCI_PM_CTRL, csr);

	mdelay(pdev->d3_delay);

	csr &= ~PCI_PM_CTRL_STATE_MASK;
	csr |= PCI_D0;
	pci_write_config_word(pdev, pdev->pm_cap + PCI_PM_CTRL, csr);

	writel(IPC_RMP2_DMA_ENABLED, hw->mem_addr + IPC_REG_ISH_RMP2);

	/* Send 0 IPC message so that ISS FW wakes up if it was already
	 asleep */
	writel(IPC_DRBL_BUSY_BIT, hw->mem_addr + IPC_REG_HOST2ISH_DRBL);

	return 0;
}


/**
 * ish_ipc_reset - resets host and fw IPC and upper layers.
 *
 * @dev: the device structure
 */
static int ish_ipc_reset(struct heci_device *dev)
{
	struct ipc_rst_payload_type ipc_mng_msg;
	int	rv = 0;

	ISH_DBG_PRINT(KERN_ALERT "%s():+++\n", __func__);
	dev_dbg(&dev->pdev->dev, "ish_hw_reset\n");
	/*temporary we'll send reset*/

	ipc_mng_msg.reset_id = 1;
	ipc_mng_msg.reserved = 0;

	ISH_DBG_PRINT(KERN_ALERT "%s(): before ish_intr_enable()\n", __func__);
	ish_intr_enable(dev);
	ISH_DBG_PRINT(KERN_ALERT "%s(): after ish_intr_enable()\n", __func__);

/* DEBUG: send self-interrupt and wait 100 (ms) for it to appear in klog */
/*	ish_reg_write(dev, IPC_REG_ISH2HOST_DRBL, 0x80000000);
	mdelay(100);
************************/

	/* Clear the incoming doorbell */
	ISH_DBG_PRINT(KERN_ALERT
		"%s(): Doorbell cleared, busy reading cleared\n", __func__);
	ish_reg_write(dev, IPC_REG_ISH2HOST_DRBL, 0);
	ISH_DBG_PRINT(KERN_ALERT "%s(): cleared doorbell reg.\n", __func__);

	/*
	 * Fixed: this should be set BEFORE writing RESET_NOTIFY,
	 * lest response will be received BEFORE this clearing...
	 */
	dev->recvd_hw_ready = 0;

	/*send message */
	rv = ipc_send_mng_msg(dev, MNG_RESET_NOTIFY, &ipc_mng_msg,
		sizeof(struct ipc_rst_payload_type));
	if (rv) {
		dev_err(&dev->pdev->dev, "Failed to send IPC MNG_RESET_NOTIFY\n");
		return	rv;
	}

	ISH_DBG_PRINT(KERN_ALERT "%s(): going to wait for hw_ready.\n",
		__func__);
	/*wait_event_interruptible(dev->wait_hw_ready, dev->recvd_hw_ready);*/
	wait_event_timeout(dev->wait_hw_ready, dev->recvd_hw_ready, 2*HZ);
	if (!dev->recvd_hw_ready) {
		dev_err(&dev->pdev->dev, "Timed out waiting for HW ready\n");
		rv = -ENODEV;
	}
	ISH_DBG_PRINT(KERN_ALERT "%s(): woke up from hw_ready.\n", __func__);

	dev_dbg(&dev->pdev->dev, "exit initial link wait\n");

	return rv;
}

/* Dummy. Do we need it? */
static void ish_hw_config(struct heci_device *dev)
{
	ISH_DBG_PRINT(KERN_ALERT "%s()+++ [ish_hw_reset=%p]\n",
		__func__, ish_hw_reset);
	dev_dbg(&dev->pdev->dev, "ish_hw_config\n");
}

static int ish_hw_start(struct heci_device *dev)
{
	struct ish_hw *hw = to_ish_hw(dev);

	dev_dbg(&dev->pdev->dev, "ish_hw_start\n");
	ish_set_host_rdy(dev);
#ifdef	D3_RCR
	/* After that we can enable ISH DMA operation */
	ISH_DBG_PRINT(KERN_ALERT "[heci-ish] %s(): writing DMA_ENABLED\n",
		__func__);
	writel(IPC_RMP2_DMA_ENABLED, hw->mem_addr + IPC_REG_ISH_RMP2);

	/* Send 0 IPC message so that ISS FW wakes up if it was already
	 asleep */
	writel(IPC_DRBL_BUSY_BIT, hw->mem_addr + IPC_REG_HOST2ISH_DRBL);
#endif /*D3_RCR*/
	ish_intr_enable(dev);
	return 0;
}


static u32 ish_read_hdr(const struct heci_device *dev)
{
	return ish_reg_read(dev, IPC_REG_ISH2HOST_MSG);
}


/**
 * ish_write - writes a message to heci device.
 *
 * @dev: the device structure
 * @header: header of message
 * @buf: message buffer will be written
 * returns 1 if success, 0 - otherwise.
 */

static int ish_write(struct heci_device *dev, struct heci_msg_hdr *header,
	unsigned char *buf)
{
/*#####################################################################*/
	unsigned char ipc_msg[IPC_FULL_MSG_SIZE];
	u32 doorbell_val;

	doorbell_val = IPC_BUILD_HEADER(header->length +
		sizeof(struct heci_msg_hdr), IPC_PROTOCOL_HECI, 1);
	memcpy(ipc_msg, (char *)&doorbell_val, sizeof(u32));
	memcpy(ipc_msg + sizeof(u32), (char *)header,
		sizeof(struct heci_msg_hdr));
	memcpy(ipc_msg + sizeof(u32) + sizeof(struct heci_msg_hdr), buf,
		header->length);

	return write_ipc_to_queue(dev, NULL, NULL, ipc_msg,
		sizeof(u32) + sizeof(struct heci_msg_hdr) + header->length);
/*#####################################################################*/
}


static const struct heci_hw_ops ish_hw_ops = {
	.host_is_ready = ish_host_is_ready,
	.hw_is_ready = ish_hw_is_ready,
	.hw_reset = ish_hw_reset,
	.ipc_reset = ish_ipc_reset,
	.hw_config = ish_hw_config,
	.hw_start = ish_hw_start,
	.read = ish_read,
	.write = ish_write,
	.write_ex = ipc_send_heci_msg,
	.get_fw_status = ish_read_fw_sts_reg
};


struct heci_device *ish_dev_init(struct pci_dev *pdev)
{

	struct heci_device *dev;

	dev = kzalloc(sizeof(struct heci_device) +  sizeof(struct ish_hw),
		GFP_KERNEL);
	if (!dev)
		return NULL;

	heci_device_init(dev);

	/* Rx INT->BH FIFO pointers */
	dev->rd_msg_fifo_head = 0;
	dev->rd_msg_fifo_tail = 0;
	spin_lock_init(&dev->rd_msg_spinlock);
	spin_lock_init(&dev->wr_processing_spinlock);
	spin_lock_init(&dev->out_ipc_spinlock);
	spin_lock_init(&dev->read_list_spinlock);
	spin_lock_init(&dev->device_lock);
	spin_lock_init(&dev->device_list_lock);
	spin_lock_init(&dev->cl_list_lock);
	spin_lock_init(&dev->me_clients_lock);
	INIT_WORK(&dev->bh_hbm_work, bh_hbm_work_fn);

	dev->ops = &ish_hw_ops;
	dev->pdev = pdev;
	dev->mtu = IPC_PAYLOAD_SIZE - sizeof(struct heci_msg_hdr);
	return dev;
}


void heci_device_disable(struct heci_device *dev)
{
	unsigned long flags;
	struct wr_msg_ctl_info *ipc_link;
	struct heci_cl *cl;

	dev->dev_state = HECI_DEV_DISABLED;
	ish_clr_host_rdy(dev);
	ish_intr_disable(dev);

	/* Free all other allocations */
	kfree(dev->me_clients);
}

