/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/completion.h>
#include <linux/platform_device.h>
#include <linux/mailbox_controller.h>
#include <linux/module.h>
#include <linux/of_irq.h>
#include <linux/kthread.h>
#include <linux/workqueue.h>
#include <linux/mailbox/qmp.h>

#define QMP_MAGIC	0x4d41494c	/* MAIL */
#define QMP_VERSION	0x1
#define QMP_FEATURES	0x0
#define QMP_NUM_CHANS	0x1
#define QMP_TOUT_MS	5000
#define QMP_TX_TOUT_MS	2000

#define QMP_MBOX_LINK_DOWN		0xFFFF0000
#define QMP_MBOX_LINK_UP		0x0000FFFF
#define QMP_MBOX_CH_DISCONNECTED	0xFFFF0000
#define QMP_MBOX_CH_CONNECTED		0x0000FFFF

#define MSG_RAM_ALIGN_BYTES 3

/**
 * enum qmp_local_state - definition of the local state machine
 * @LINK_DISCONNECTED:		Init state, waiting for ucore to start
 * @LINK_NEGOTIATION:		Set local link state to up, wait for ucore ack
 * @LINK_CONNECTED:		Link state up, channel not connected
 * @LOCAL_CONNECTING:		Channel opening locally, wait for ucore ack
 * @LOCAL_CONNECTED:		Channel opened locally
 * @CHANNEL_CONNECTED:		Channel fully opened
 * @LOCAL_DISCONNECTING:	Channel closing locally, wait for ucore ack
 */
enum qmp_local_state {
	LINK_DISCONNECTED,
	LINK_NEGOTIATION,
	LINK_CONNECTED,
	LOCAL_CONNECTING,
	LOCAL_CONNECTED,
	CHANNEL_CONNECTED,
	LOCAL_DISCONNECTING,
};

/**
 * struct channel_desc - description of a core's link, channel and mailbox state
 * @link_state		Current link state of core
 * @link_state_ack	Ack for other core to use when link state changes
 * @ch_state		Current channel state of core
 * @ch_state_ack	Ack for other core to use when channel state changes
 * @mailbox_size	Size of this core's mailbox
 * @mailbox_offset	Location of core's mailbox from a base smem location
 */
struct channel_desc {
	u32 link_state;
	u32 link_state_ack;
	u32 ch_state;
	u32 ch_state_ack;
	u32 mailbox_size;
	u32 mailbox_offset;
};

/**
 * struct mbox_desc - description of the protocol's mailbox state
 * @magic	Magic number field to be set by ucore
 * @version	Version field to be set by ucore
 * @features	Features field to be set by ucore
 * @ucore	Channel descriptor to hold state of ucore
 * @mcore	Channel descriptor to hold state of mcore
 * @reserved	Reserved in case of future use
 *
 * This structure resides in SMEM and contains the control information for the
 * mailbox channel. Each core in the link will have one channel descriptor
 */
struct mbox_desc {
	u32 magic;
	u32 version;
	u32 features;
	struct channel_desc ucore;
	struct channel_desc mcore;
	u32 reserved;
};

/**
 * struct qmp_core_version - local structure to hold version and features
 * @version	Version field to indicate what version the ucore supports
 * @features	Features field to indicate what features the ucore supports
 */
struct qmp_core_version {
	u32 version;
	u32 features;
};

/**
 * struct qmp_device - local information for managing a single mailbox
 * @dev:		The device that corresponds to this mailbox
 * @mbox:		The mbox controller for this mailbox
 * @name:		The name of this mailbox
 * @local_state:	Current state of the mailbox protocol
 * @link_complete:	Use to block until link negotiation with remote proc
 *			is complete
 * @ch_complete:	Use to block until the channel is fully opened
 * @tx_sent:		True if tx is sent and remote proc has not sent ack
 * @ch_in_use:		True if this mailbox's channel owned by a client
 * @rx_buf:		buffer to pass to client, holds copied data from mailbox
 * @version:		Version and features received during link negotiation
 * @mcore_mbox_offset:	Offset of mcore mbox from the msgram start
 * @mcore_mbox_size:	Size of the mcore mbox
 * @desc:		Reference to the mailbox descriptor in SMEM
 * @msgram:		Reference to the start of msgram
 * @irq_mask:		Mask written to @tx_irq_reg to trigger irq
 * @tx_irq_reg:		Reference to the register to send an irq to remote proc
 * @rx_reset_reg:	Reference to the register to reset the rx irq, if
 *			applicable
 * @rx_irq_line:	The incoming interrupt line
 * @tx_irq_count:	Number of tx interrupts triggered
 * @rx_irq_count:	Number of rx interrupts received
 * @kwork:		Work to be executed when an irq is received
 * @kworker:		Handle to entitiy to process incoming data
 * @task:		Handle to task context used to run @kworker
 * @state_lock:		Serialize mailbox state changes
 * @dwork:		Delayed work to detect timed out tx
 * @tx_lock:		Serialize access for writes to mailbox
 */
struct qmp_device {
	struct device *dev;
	struct mbox_controller *mbox;
	const char *name;
	enum qmp_local_state local_state;
	struct completion link_complete;
	struct completion ch_complete;
	bool tx_sent;
	bool ch_in_use;
	struct qmp_pkt rx_pkt;
	struct qmp_core_version version;
	u32 mcore_mbox_offset;
	u32 mcore_mbox_size;
	void __iomem *desc;
	void __iomem *msgram;
	u32 irq_mask;
	void __iomem *tx_irq_reg;
	void __iomem *rx_reset_reg;
	u32 rx_irq_line;
	u32 tx_irq_count;
	u32 rx_irq_count;
	struct kthread_work kwork;
	struct kthread_worker kworker;
	struct task_struct *task;
	struct mutex state_lock;
	struct delayed_work dwork;
	spinlock_t tx_lock;
};

/**
 * send_irq() - send an irq to a remote entity as an event signal.
 * @mdev:	Which remote entity that should receive the irq.
 */
static void send_irq(struct qmp_device *mdev)
{
	/*
	 * Any data associated with this event must be visable to the remote
	 * before the interrupt is triggered
	 */
	wmb();
	writel_relaxed(mdev->irq_mask, mdev->tx_irq_reg);
	mdev->tx_irq_count++;
}

/**
 * qmp_irq_handler() - handle irq from remote entitity.
 * @irq:	irq number for the trggered interrupt.
 * @priv:	private pointer to qmp mbox device.
 */
irqreturn_t qmp_irq_handler(int irq, void *priv)
{
	struct qmp_device *mdev = (struct qmp_device *)priv;

	if (mdev->rx_reset_reg)
		writel_relaxed(mdev->irq_mask, mdev->rx_reset_reg);

	kthread_queue_work(&mdev->kworker, &mdev->kwork);
	mdev->rx_irq_count++;

	return IRQ_HANDLED;
}

static void memcpy32_toio(void *dest, void *src, size_t size)
{
	u32 *dest_local = (u32 *)dest;
	u32 *src_local = (u32 *)src;

	WARN_ON(size & MSG_RAM_ALIGN_BYTES);
	size /= sizeof(u32);
	while (size--)
		iowrite32(*src_local++, dest_local++);
}

static void memcpy32_fromio(void *dest, void *src, size_t size)
{
	u32 *dest_local = (u32 *)dest;
	u32 *src_local = (u32 *)src;

	WARN_ON(size & MSG_RAM_ALIGN_BYTES);
	size /= sizeof(u32);
	while (size--)
		*dest_local++ = ioread32(src_local++);
}

/**
 * set_ucore_link_ack() - set the link ack in the ucore channel desc.
 * @mdev:	the mailbox for the field that is being set.
 * @state:	the value to set the ack field to.
 */
static void set_ucore_link_ack(struct qmp_device *mdev, u32 state)
{
	u32 offset;

	offset = offsetof(struct mbox_desc, ucore);
	offset += offsetof(struct channel_desc, link_state_ack);
	iowrite32(state, mdev->desc + offset);
}

/**
 * set_ucore_ch_ack() - set the channel ack in the ucore channel desc.
 * @mdev:	the mailbox for the field that is being set.
 * @state:	the value to set the ack field to.
 */
static void set_ucore_ch_ack(struct qmp_device *mdev, u32 state)
{
	u32 offset;

	offset = offsetof(struct mbox_desc, ucore);
	offset += offsetof(struct channel_desc, ch_state_ack);
	iowrite32(state, mdev->desc + offset);
}

/**
 * set_mcore_ch() - set the channel state in the mcore channel desc.
 * @mdev:	the mailbox for the field that is being set.
 * @state:	the value to set the channel field to.
 */
static void set_mcore_ch(struct qmp_device *mdev, u32 state)
{
	u32 offset;

	offset = offsetof(struct mbox_desc, mcore);
	offset += offsetof(struct channel_desc, ch_state);
	iowrite32(state, mdev->desc + offset);
}

/**
 * qmp_notify_timeout() - Notify client of tx timeout with -EIO
 * @work:	Structure for work that was scheduled.
 */
static void qmp_notify_timeout(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct qmp_device *mdev = container_of(dwork, struct qmp_device, dwork);
	struct mbox_chan *chan = &mdev->mbox->chans[0];
	int err = -EIO;

	pr_err("%s: qmp tx timeout for %s\n", __func__, mdev->name);
	mbox_chan_txdone(chan, err);
}

/**
 * qmp_startup() - Start qmp mailbox channel for communication. Waits for
 *			remote subsystem to open channel if link is not
 *			initated or until timeout.
 * @chan:	mailbox channel that is being opened.
 *
 * Return: 0 on succes or standard Linux error code.
 */
static int qmp_startup(struct mbox_chan *chan)
{
	struct qmp_device *mdev = chan->con_priv;

	if (!mdev)
		return -EINVAL;

	mutex_lock(&mdev->state_lock);
	if (mdev->local_state == CHANNEL_CONNECTED) {
		mutex_unlock(&mdev->state_lock);
		return -EINVAL;
	}
	if (!completion_done(&mdev->link_complete)) {
		mutex_unlock(&mdev->state_lock);
		return -EAGAIN;
	}

	set_mcore_ch(mdev, QMP_MBOX_CH_CONNECTED);
	mdev->local_state = LOCAL_CONNECTING;
	mutex_unlock(&mdev->state_lock);

	send_irq(mdev);
	wait_for_completion_interruptible_timeout(&mdev->ch_complete,
					msecs_to_jiffies(QMP_TOUT_MS));
	return 0;
}

static inline void qmp_schedule_tx_timeout(struct qmp_device *mdev)
{
	schedule_delayed_work(&mdev->dwork, msecs_to_jiffies(QMP_TX_TOUT_MS));
}

/**
 * qmp_send_data() - Copy the data to the channel's mailbox and notify
 *				remote subsystem of new data. This function will
 *				return an error if the previous message sent has
 *				not been read. Cannot Sleep.
 * @chan:	mailbox channel that data is to be sent over.
 * @data:	Data to be sent to remote processor, should be in the format of
 *		a qmp_pkt.
 *
 * Return: 0 on succes or standard Linux error code.
 */
static int qmp_send_data(struct mbox_chan *chan, void *data)
{
	struct qmp_device *mdev = chan->con_priv;
	struct qmp_pkt *pkt = (struct qmp_pkt *)data;
	void __iomem *addr;
	unsigned long flags;

	if (!mdev || !data || mdev->local_state != CHANNEL_CONNECTED)
		return -EINVAL;

	spin_lock_irqsave(&mdev->tx_lock, flags);
	addr = mdev->msgram + mdev->mcore_mbox_offset;
	if (ioread32(addr)) {
		spin_unlock_irqrestore(&mdev->tx_lock, flags);
		return -EBUSY;
	}

	if (pkt->size + sizeof(pkt->size) > mdev->mcore_mbox_size) {
		spin_unlock_irqrestore(&mdev->tx_lock, flags);
		return -EINVAL;
	}
	memcpy32_toio(addr + sizeof(pkt->size), pkt->data, pkt->size);
	iowrite32(pkt->size, addr);
	mdev->tx_sent = true;
	send_irq(mdev);
	qmp_schedule_tx_timeout(mdev);
	spin_unlock_irqrestore(&mdev->tx_lock, flags);
	return 0;
}

/**
 * qmp_shutdown() - Disconnect this mailbox channel so the client does not
 *				receive anymore data and can reliquish control
 *				of the channel
 * @chan:	mailbox channel to be shutdown.
 */
static void qmp_shutdown(struct mbox_chan *chan)
{
	struct qmp_device *mdev = chan->con_priv;

	mutex_lock(&mdev->state_lock);
	if (mdev->local_state != LINK_DISCONNECTED) {
		mdev->local_state = LOCAL_DISCONNECTING;
		set_mcore_ch(mdev, QMP_MBOX_CH_DISCONNECTED);
		send_irq(mdev);
	}
	mdev->ch_in_use = false;
	mutex_unlock(&mdev->state_lock);
}

/**
 * qmp_last_tx_done() - qmp does not support polling operations, print
 *				error of unexpected usage and return true to
 *				resume operation.
 * @chan:	Corresponding mailbox channel for requested last tx.
 *
 * Return: true
 */
static bool qmp_last_tx_done(struct mbox_chan *chan)
{
	pr_err("In %s, unexpected usage of last_tx_done\n", __func__);
	return true;
}

/**
 * qmp_recv_data() - received notification that data is available in the
 *			mailbox. Copy data from mailbox and pass to client.
 * @mdev:	mailbox device that received the notification.
 * @mbox_of:	offset of mailbox from msgram start.
 */
static void qmp_recv_data(struct qmp_device *mdev, u32 mbox_of)
{
	void __iomem *addr;
	struct qmp_pkt *pkt;

	addr = mdev->msgram + mbox_of;
	pkt = &mdev->rx_pkt;
	pkt->size = ioread32(addr);

	if (pkt->size > mdev->mcore_mbox_size)
		pr_err("%s: Invalid mailbox packet\n", __func__);
	else {
		memcpy32_fromio(pkt->data, addr + sizeof(pkt->size), pkt->size);
		mbox_chan_received_data(&mdev->mbox->chans[0], &pkt);
	}
	iowrite32(0, addr);
	send_irq(mdev);
}

/**
 * init_mcore_state() - initialize the mcore state of a mailbox.
 * @mdev:	mailbox device to be initialized.
 */
static void init_mcore_state(struct qmp_device *mdev)
{
	struct channel_desc mcore;
	u32 offset = offsetof(struct mbox_desc, mcore);

	mcore.link_state = QMP_MBOX_LINK_UP;
	mcore.link_state_ack = QMP_MBOX_LINK_DOWN;
	mcore.ch_state = QMP_MBOX_CH_DISCONNECTED;
	mcore.ch_state_ack = QMP_MBOX_CH_DISCONNECTED;
	mcore.mailbox_size = mdev->mcore_mbox_size;
	mcore.mailbox_offset = mdev->mcore_mbox_offset;
	memcpy32_toio(mdev->desc + offset, &mcore, sizeof(mcore));
}

/**
 * __qmp_rx_worker() - Handle incoming messages from remote processor.
 * @mdev:	mailbox device that received notification.
 */
static void __qmp_rx_worker(struct qmp_device *mdev)
{
	u32 msg_len;
	struct mbox_desc desc;

	memcpy_fromio(&desc, mdev->desc, sizeof(desc));
	if (desc.magic != QMP_MAGIC)
		return;

	mutex_lock(&mdev->state_lock);
	switch (mdev->local_state) {
	case LINK_DISCONNECTED:
		mdev->version.version = desc.version;
		mdev->version.features = desc.features;
		set_ucore_link_ack(mdev, desc.ucore.link_state);
		if (desc.mcore.mailbox_size) {
			mdev->mcore_mbox_size = desc.mcore.mailbox_size;
			mdev->mcore_mbox_offset = desc.mcore.mailbox_offset;
		}
		init_mcore_state(mdev);
		mdev->local_state = LINK_NEGOTIATION;
		mdev->rx_pkt.data = devm_kzalloc(mdev->dev,
						 desc.ucore.mailbox_size,
						 GFP_KERNEL);
		if (!mdev->rx_pkt.data) {
			pr_err("In %s: failed to allocate rx pkt\n", __func__);
			break;
		}
		send_irq(mdev);
		break;
	case LINK_NEGOTIATION:
		if (desc.mcore.link_state_ack != QMP_MBOX_LINK_UP ||
				desc.mcore.link_state != QMP_MBOX_LINK_UP) {
			pr_err("In %s: rx interrupt without negotiation ack\n",
					__func__);
			break;
		}
		mdev->local_state = LINK_CONNECTED;
		complete_all(&mdev->link_complete);
		break;
	case LINK_CONNECTED:
		if (desc.ucore.ch_state == desc.ucore.ch_state_ack) {
			pr_err("In %s: rx interrupt without channel open\n",
					__func__);
			break;
		}
		set_ucore_ch_ack(mdev, desc.ucore.ch_state);
		send_irq(mdev);
		break;
	case LOCAL_CONNECTING:
		if (desc.mcore.ch_state_ack == QMP_MBOX_CH_CONNECTED &&
				desc.mcore.ch_state == QMP_MBOX_CH_CONNECTED)
			mdev->local_state = LOCAL_CONNECTED;

		if (desc.ucore.ch_state != desc.ucore.ch_state_ack) {
			set_ucore_ch_ack(mdev, desc.ucore.ch_state);
			send_irq(mdev);
		}
		if (mdev->local_state == LOCAL_CONNECTED &&
				desc.mcore.ch_state == QMP_MBOX_CH_CONNECTED &&
				desc.ucore.ch_state == QMP_MBOX_CH_CONNECTED) {
			mdev->local_state = CHANNEL_CONNECTED;
			complete_all(&mdev->ch_complete);
		}
		break;
	case LOCAL_CONNECTED:
		if (desc.ucore.ch_state == desc.ucore.ch_state_ack) {
			pr_err("In %s: rx interrupt without remote channel open\n",
					__func__);
			break;
		}
		set_ucore_ch_ack(mdev, desc.ucore.ch_state);
		mdev->local_state = CHANNEL_CONNECTED;
		send_irq(mdev);
		complete_all(&mdev->ch_complete);
		break;
	case CHANNEL_CONNECTED:
		if (desc.ucore.ch_state == QMP_MBOX_CH_DISCONNECTED) {
			set_ucore_ch_ack(mdev, desc.ucore.ch_state);
			mdev->local_state = LOCAL_CONNECTED;
			send_irq(mdev);
		}

		msg_len = ioread32(mdev->msgram + desc.ucore.mailbox_offset);
		if (msg_len)
			qmp_recv_data(mdev, desc.ucore.mailbox_offset);

		if (mdev->tx_sent) {
			msg_len = ioread32(mdev->msgram +
						mdev->mcore_mbox_offset);
			if (msg_len == 0) {
				mdev->tx_sent = false;
				cancel_delayed_work(&mdev->dwork);
				mbox_chan_txdone(&mdev->mbox->chans[0], 0);
			}
		}
		break;
	case LOCAL_DISCONNECTING:
		if (desc.mcore.ch_state_ack == QMP_MBOX_CH_DISCONNECTED &&
				desc.mcore.ch_state == desc.mcore.ch_state_ack)
			mdev->local_state = LINK_CONNECTED;
		reinit_completion(&mdev->ch_complete);
		break;
	default:
		pr_err("In %s: Local Channel State corrupted\n", __func__);
	}
	mutex_unlock(&mdev->state_lock);
}

static void rx_worker(struct kthread_work *work)
{
	struct qmp_device *mdev;

	mdev = container_of(work, struct qmp_device, kwork);
	__qmp_rx_worker(mdev);
}

/**
 * qmp_mbox_of_xlate() - Returns a mailbox channel to be used for this mailbox
 *			device. Make sure the channel is not already in use.
 * @mbox:	Mailbox device controlls the requested channel.
 * @spec:	Device tree arguments to specify which channel is requested.
 */
static struct mbox_chan *qmp_mbox_of_xlate(struct mbox_controller *mbox,
		const struct of_phandle_args *spec)
{
	struct qmp_device *mdev = dev_get_drvdata(mbox->dev);
	unsigned int channel = spec->args[0];

	if (!mdev || channel >= mbox->num_chans)
		return ERR_PTR(-EINVAL);

	mutex_lock(&mdev->state_lock);
	if (mdev->ch_in_use) {
		pr_err("%s, mbox channel already in use %s\n", __func__,
								mdev->name);
		mutex_unlock(&mdev->state_lock);
		return ERR_PTR(-EBUSY);
	}
	mdev->ch_in_use = true;
	mutex_unlock(&mdev->state_lock);
	return &mbox->chans[0];
}

/**
 * parse_devicetree() - Parse the device tree information for QMP, map io
 *			memory and register for needed interrupts
 * @pdev:	platform device for this driver.
 * @mdev:	mailbox device to hold the device tree configuration.
 *
 * Return: 0 on succes or standard Linux error code.
 */
static int qmp_parse_devicetree(struct platform_device *pdev,
					struct qmp_device *mdev)
{
	struct device_node *node = pdev->dev.of_node;
	char *key;
	int rc;
	const char *subsys_name;
	u32 rx_irq_line, tx_irq_mask;
	u32 desc_of = 0;
	u32 mbox_of = 0;
	u32 mbox_size = 0;
	struct resource *msgram_r, *tx_irq_reg_r;

	key = "label";
	subsys_name = of_get_property(node, key, NULL);
	if (!subsys_name) {
		pr_err("%s: missing key %s\n", __func__, key);
		return -ENODEV;
	}

	key = "msgram";
	msgram_r = platform_get_resource_byname(pdev, IORESOURCE_MEM, key);
	if (!msgram_r) {
		pr_err("%s: missing key %s\n", __func__, key);
		return -ENODEV;
	}

	key = "irq-reg-base";
	tx_irq_reg_r = platform_get_resource_byname(pdev, IORESOURCE_MEM, key);
	if (!tx_irq_reg_r) {
		pr_err("%s: missing key %s\n", __func__, key);
		return -ENODEV;
	}

	key = "qcom,irq-mask";
	rc = of_property_read_u32(node, key, &tx_irq_mask);
	if (rc) {
		pr_err("%s: missing key %s\n", __func__, key);
		return -ENODEV;
	}

	key = "interrupts";
	rx_irq_line = irq_of_parse_and_map(node, 0);
	if (!rx_irq_line) {
		pr_err("%s: missing key %s\n", __func__, key);
		return -ENODEV;
	}

	key = "mbox-desc-offset";
	rc = of_property_read_u32(node, key, &desc_of);
	if (rc) {
		pr_err("%s: missing key %s\n", __func__, key);
		return -ENODEV;
	}

	key = "mbox-offset";
	rc = of_property_read_u32(node, key, &mbox_of);
	if (!rc)
		mdev->mcore_mbox_offset = mbox_of;

	key = "mbox-size";
	rc = of_property_read_u32(node, key, &mbox_size);
	if (!rc)
		mdev->mcore_mbox_size = mbox_size;

	mdev->name = subsys_name;
	mdev->msgram = devm_ioremap_nocache(&pdev->dev, msgram_r->start,
						resource_size(msgram_r));
	if (!mdev->msgram)
		return -ENOMEM;

	mdev->desc = mdev->msgram + desc_of;
	if (!mdev->desc)
		return -ENOMEM;

	mdev->irq_mask = tx_irq_mask;
	mdev->tx_irq_reg = devm_ioremap_nocache(&pdev->dev, tx_irq_reg_r->start,
						resource_size(tx_irq_reg_r));
	if (!mdev->tx_irq_reg)
		return -ENOMEM;

	mdev->rx_irq_line = rx_irq_line;
	return 0;
}

/**
 * cleanup_workqueue() - Flush all work and stop the thread for this mailbox.
 * @mdev:	mailbox device to cleanup.
 */
static void cleanup_workqueue(struct qmp_device *mdev)
{
	kthread_flush_worker(&mdev->kworker);
	kthread_stop(mdev->task);
	mdev->task = NULL;
}

static struct mbox_chan_ops qmp_mbox_ops = {
	.startup = qmp_startup,
	.shutdown = qmp_shutdown,
	.send_data = qmp_send_data,
	.last_tx_done = qmp_last_tx_done,
};

static const struct of_device_id qmp_mbox_match_table[] = {
	{ .compatible = "qcom,qmp-mbox" },
	{},
};

static int qmp_mbox_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct mbox_controller *mbox;
	struct qmp_device *mdev;
	struct mbox_chan *chans;
	int ret = 0;

	mdev = devm_kzalloc(&pdev->dev, sizeof(*mdev), GFP_KERNEL);
	if (!mdev)
		return -ENOMEM;
	platform_set_drvdata(pdev, mdev);

	ret = qmp_parse_devicetree(pdev, mdev);
	if (ret)
		return ret;

	mbox = devm_kzalloc(&pdev->dev, sizeof(*mbox), GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;

	chans = devm_kzalloc(&pdev->dev, sizeof(*chans) * QMP_NUM_CHANS,
								GFP_KERNEL);
	if (!chans)
		return -ENOMEM;

	mbox->dev = &pdev->dev;
	mbox->ops = &qmp_mbox_ops;
	mbox->chans = chans;
	mbox->chans[0].con_priv = mdev;
	mbox->num_chans = QMP_NUM_CHANS;
	mbox->txdone_irq = true;
	mbox->txdone_poll = false;
	mbox->of_xlate = qmp_mbox_of_xlate;

	mdev->dev = &pdev->dev;
	mdev->mbox = mbox;
	spin_lock_init(&mdev->tx_lock);
	mutex_init(&mdev->state_lock);
	mdev->local_state = LINK_DISCONNECTED;
	kthread_init_work(&mdev->kwork, rx_worker);
	kthread_init_worker(&mdev->kworker);
	mdev->task = kthread_run(kthread_worker_fn, &mdev->kworker, "qmp_%s",
								mdev->name);
	init_completion(&mdev->link_complete);
	init_completion(&mdev->ch_complete);
	mdev->tx_sent = false;
	mdev->ch_in_use = false;
	INIT_DELAYED_WORK(&mdev->dwork, qmp_notify_timeout);

	ret = mbox_controller_register(mbox);
	if (ret) {
		cleanup_workqueue(mdev);
		pr_err("%s: failed to register mbox controller %d\n", __func__,
									ret);
		return ret;
	}

	ret = devm_request_irq(&pdev->dev, mdev->rx_irq_line, qmp_irq_handler,
		IRQF_TRIGGER_RISING | IRQF_NO_SUSPEND | IRQF_SHARED,
		node->name, mdev);
	if (ret < 0) {
		cleanup_workqueue(mdev);
		mbox_controller_unregister(mdev->mbox);
		pr_err("%s: request irq on %d failed: %d\n", __func__,
							mdev->rx_irq_line, ret);
		return ret;
	}
	ret = enable_irq_wake(mdev->rx_irq_line);
	if (ret < 0)
		pr_err("%s: enable_irq_wake on %d failed: %d\n", __func__,
							mdev->rx_irq_line, ret);

	qmp_irq_handler(0, mdev);
	return 0;
}

static int qmp_mbox_remove(struct platform_device *pdev)
{
	struct qmp_device *mdev = platform_get_drvdata(pdev);

	cleanup_workqueue(mdev);
	mbox_controller_unregister(mdev->mbox);
	return 0;
}

static struct platform_driver qmp_mbox_driver = {
	.probe = qmp_mbox_probe,
	.remove = qmp_mbox_remove,
	.driver = {
		.name = "qmp_mbox",
		.owner = THIS_MODULE,
		.of_match_table = qmp_mbox_match_table,
	},
};

static int __init qmp_init(void)
{
	int rc = 0;

	rc = platform_driver_register(&qmp_mbox_driver);
	if (rc)
		pr_err("%s: qmp_mbox_driver reg failed %d\n", __func__, rc);
	return rc;
}
arch_initcall(qmp_init);

MODULE_DESCRIPTION("MSM QTI Mailbox Protocol");
MODULE_LICENSE("GPL v2");
