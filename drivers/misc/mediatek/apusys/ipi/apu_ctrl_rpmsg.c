// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/completion.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/rpmsg.h>

#include "apu_ctrl_rpmsg.h"
#include "apusys_core.h"

enum {
	IPI_DBG_HW	= 0x1,
	IPI_DBG_FLW	= 0x2,
};

#define APU_IPI_PREFIX	"[apu_ipi]"

static u32 g_apu_ipi_klog;

static inline int ipi_debug_on(int mask)
{
	return g_apu_ipi_klog & mask;
}

#define ipi_debug(mask, x, ...)						\
	do {								\
		if (ipi_debug_on(mask))					\
			pr_info(APU_IPI_PREFIX " %s/%d " x,		\
				__func__, __LINE__, ##__VA_ARGS__);	\
	} while (0)

#define ipi_drv_info(x, args...) \
	pr_info(APU_IPI_PREFIX "%s " x, __func__, ##args)
#define ipi_drv_warn(x, args...) \
	pr_info(APU_IPI_PREFIX "[warn] %s " x, __func__, ##args)
#define ipi_drv_err(x, args...) \
	pr_info(APU_IPI_PREFIX "[error] %s " x, __func__, ##args)

#define ipi_hw_debug(x, ...)	ipi_debug(IPI_DBG_HW, x, ##__VA_ARGS__)
#define ipi_flw_debug(x, ...)	ipi_debug(IPI_DBG_FLW, x, ##__VA_ARGS__)

struct apu_ctrl_rpmsg {
	struct rpmsg_device *rpdev;
	struct rpmsg_endpoint *ept;
};

struct ctrl_msg_channel {
	ctrl_msg_handler handler;
	void *priv;

	void *recv_buf;
	unsigned int recv_buf_size;

	struct mutex lock;
	struct completion comp;
};

struct apu_ctrl_dev {
	struct device *dev;
	void *priv;

	struct mutex send_lock;
	struct ctrl_msg_channel ch[APU_CTRL_MSG_NUM];
};

struct apu_ctrl_msg {
	unsigned int ch_id;
	unsigned int len;
	unsigned char data[APU_CTRL_MSG_MAX_SIZE];
};

extern int apu_timesync_init(void);
extern int apu_timesync_remove(void);

static struct apu_ctrl_dev *ctldev;
static bool initialized;


#define APU_CTRL_RPMSG_TEST
#ifdef APU_CTRL_RPMSG_TEST
static uint32_t tx_msg[4], rx_msg[4];

static void apu_ctrl_rpmsg_test(void)
{
	int ret, i;
	bool err = false;

	ret = apu_ctrl_register_channel(APU_CTRL_MSG_TEST,
					NULL, NULL, rx_msg, 16);
	if (ret) {
		ipi_drv_err("failed to register channel for unittest, ret=%d\n",
			    ret);
		return;
	}

	/* test case: AP send, uP receive, invert and reply */
	tx_msg[0] = 0x55aa5a5a;
	tx_msg[1] = 0xaa55a5a5;
	tx_msg[2] = 0x66996969;
	tx_msg[3] = 0x99669696;
	ret = apu_ctrl_send_msg(APU_CTRL_MSG_TEST, tx_msg, 16, 1000);
	if(ret) {
		ipi_drv_err("send unit test msg failed, ret=%d\n", ret);
		goto unregister_test_channel;
	}

	for (i = 0; i < 4; i++) {
		if (rx_msg[i] != (tx_msg[i] ^ 0xffffffff)) {
			ipi_drv_err("error: tx[%d]=0x%08x, rx[%d]=0x%08x\n",
				    i, tx_msg[i], i, rx_msg[i]);
			err = true;
		}
	}

	if (err)
		ipi_drv_err("unit test failed\n");
	else
		ipi_drv_info("unit test passed.\n");

unregister_test_channel:
	apu_ctrl_unregister_channel(APU_CTRL_MSG_TEST);
}

#else /* APU_CTRL_RPMSG_TEST */

static void apu_ctrl_rpmsg_test(void) { }

#endif /* APU_CTRL_RPMSG_TEST */

int apu_ctrl_send_msg(u32 ch_id, void *data, unsigned int len,
		      unsigned int timeout)
{
	struct rpmsg_endpoint *ept =
			((struct apu_ctrl_rpmsg *)ctldev->priv)->ept;
	struct ctrl_msg_channel *ch = &ctldev->ch[ch_id];
	struct apu_ctrl_msg msg;
	int ret;

	if (!initialized || !ctldev) {
		ipi_drv_err("apu_ctrl_rpmsg dirver is not initialized\n");
		return -EPROBE_DEFER;
	}

	if (ch_id >= APU_CTRL_MSG_NUM) {
		ipi_drv_err("invalid ctrl msg channel id=%d\n", ch_id);
		return -EINVAL;
	}

	if (len > APU_CTRL_MSG_MAX_SIZE) {
		ipi_drv_err("message too long, len=%d\n", len);
		return -EINVAL;
	}

	//@@@ apu_dpidle_power_on_aputop();

	ipi_drv_info("ch_id=%d, data=%p, len=%d, timeout=%d\n", ch_id, data, len, timeout);

	msg.ch_id = ch_id;
	msg.len = len;
	memcpy(msg.data, data, len);

	mutex_lock(&ctldev->send_lock);

	reinit_completion(&ch->comp);

	ret = rpmsg_send(ept, (void *)&msg, len + APU_CTRL_MSG_HDR_SIZE);
	if (ret) {
		ipi_drv_err("failed to send msg. ch_id=%d, len=%d\n",
			    ch_id, len);
		goto unlock;
	}

	if (timeout > 0) {
		ret = wait_for_completion_interruptible_timeout(&ch->comp,
				msecs_to_jiffies(timeout));

		if (ret < 0) {
			ipi_drv_err("waiting for ack interrupted, ret=%d\n",
				    ret);
			goto unlock;
		}

		if (ret == 0) {
			ipi_drv_err("waiting for ack timeout.\n");
			ret = -ETIMEDOUT;
			goto unlock;
		}

		ret = 0;
	}

unlock:
	mutex_unlock(&ctldev->send_lock);

	return ret;
}
EXPORT_SYMBOL(apu_ctrl_send_msg);

int apu_ctrl_recv_msg(u32 ch_id)
{
	struct ctrl_msg_channel *ch = &ctldev->ch[ch_id];
	int ret;

	if (!initialized || !ctldev) {
		ipi_drv_err("apu_ctrl_rpmsg dirver is not initialized\n");
		return -EPROBE_DEFER;
	}

	if (ch_id >= APU_CTRL_MSG_NUM) {
		ipi_drv_err("invalid virtual channel id=%d\n", ch_id);
		return -EINVAL;
	}

	ret = wait_for_completion_interruptible(&ch->comp);

	if (ret < 0) {
		ipi_drv_err("waiting for ack interrupted, ret=%d\n", ret);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(apu_ctrl_recv_msg);

static int apu_ctrl_rpmsg_callback(struct rpmsg_device *rpdev, void *data,
				   int len, void *priv, u32 src)
{
	struct apu_ctrl_msg *msg = data;
	unsigned int ch_id = msg->ch_id;
	unsigned int msg_len = msg->len;
	struct ctrl_msg_channel *ch = &ctldev->ch[ch_id];

	if (!initialized || !ctldev) {
		ipi_drv_err("apu_ctrl_rpmsg dirver is not initialized\n");
		return -EPROBE_DEFER;
	}

	if (ch_id >= APU_CTRL_MSG_NUM) {
		ipi_drv_err("invalid apu ctrl channel id=%d\n", ch_id);
		return -EINVAL;
	}

	ipi_flw_debug("ch_id=%d, len=%d, buf=%p, handler=%p, size=%d\n",
		      ch_id, msg_len, ch->recv_buf, ch->handler,
		      ch->recv_buf_size);

	mutex_lock(&ch->lock);

	if (!ch->recv_buf || !ch->recv_buf_size)
		goto ack_comp;

	if (msg_len > ch->recv_buf_size) {
		ipi_drv_warn("receive over-sized message, ch_id=%d, size=%d\n",
			     ch_id, msg_len);
		msg_len = ch->recv_buf_size;
	}

	memcpy(ch->recv_buf, msg->data, msg_len);

	if (ch->handler)
		ch->handler(ch_id, ch->priv, ch->recv_buf, msg_len);

ack_comp:
	complete(&ch->comp);

	mutex_unlock(&ch->lock);

	return 0;
}

int apu_ctrl_register_channel(u32 ch_id, ctrl_msg_handler handler, void *priv,
			      void *recv_buf, unsigned int recv_buf_size)
{
	struct ctrl_msg_channel *ch = &ctldev->ch[ch_id];

	if (!initialized || !ctldev) {
		ipi_drv_err("apu_ctrl_rpmsg dirver is not initialized\n");
		return -EPROBE_DEFER;
	}

	if (ch_id >= APU_CTRL_MSG_NUM) {
		ipi_drv_err("invalid ctrl msg ch id=%d\n", ch_id);
		return -EINVAL;
	}

	mutex_lock(&ch->lock);
	if (ch->handler) {
		ipi_drv_err("ch_id=%d already registered\n", ch_id);
		mutex_unlock(&ch->lock);
		return -EBUSY;
	}

	ipi_drv_info("id=%d, handler=%p, priv=%p, recv_buf=%p, size=%d\n",
			ch_id, handler, priv, recv_buf, recv_buf_size);

	ch->handler = handler;
	ch->priv = priv;
	ch->recv_buf = recv_buf;
	ch->recv_buf_size = recv_buf_size;

	mutex_unlock(&ch->lock);

	return 0;
}
EXPORT_SYMBOL(apu_ctrl_register_channel);

void apu_ctrl_unregister_channel(u32 ch_id)
{
	struct ctrl_msg_channel *ch = &ctldev->ch[ch_id];

	if (!initialized || !ctldev || ch_id >= APU_CTRL_MSG_NUM)
		return;

	mutex_lock(&ch->lock);
	ch->handler = NULL;
	ch->priv = NULL;
	ch->recv_buf = NULL;
	ch->recv_buf_size = 0;
	mutex_unlock(&ch->lock);
}
EXPORT_SYMBOL(apu_ctrl_unregister_channel);

static struct rpmsg_endpoint *
apu_ctrl_create_ept(struct rpmsg_device *rpdev)
{
	struct rpmsg_channel_info chinfo = {};

	strscpy(chinfo.name, rpdev->id.name, RPMSG_NAME_SIZE);
	chinfo.src = rpdev->src;
	chinfo.dst = RPMSG_ADDR_ANY;

	return rpmsg_create_ept(rpdev, apu_ctrl_rpmsg_callback, NULL, chinfo);
}

static int apu_ctrl_rpmsg_probe(struct rpmsg_device *rpdev)
{
	struct device *dev = &rpdev->dev;
	struct apu_ctrl_rpmsg *ctrl_rpmsg;
	struct apu_ctrl_dev *ctrl_dev;
	int i;

	ctrl_rpmsg = devm_kzalloc(dev, sizeof(*ctrl_rpmsg), GFP_KERNEL);
	if (!ctrl_rpmsg)
		return -ENOMEM;

	ctrl_dev = devm_kzalloc(dev, sizeof(*ctrl_dev), GFP_KERNEL);
	if (!ctrl_dev)
		return -ENOMEM;

	ctrl_dev->dev = dev;
	ctrl_dev->priv = ctrl_rpmsg;
	dev_set_drvdata(dev, ctrl_dev);

	ctrl_rpmsg->rpdev = rpdev;
	ctrl_rpmsg->ept = apu_ctrl_create_ept(rpdev);

	mutex_init(&ctrl_dev->send_lock);
	for (i = 0; i < APU_CTRL_MSG_NUM; i++) {
		mutex_init(&ctrl_dev->ch[i].lock);
		init_completion(&ctrl_dev->ch[i].comp);
	}

	ctldev = ctrl_dev;
	initialized = true;

	apu_ctrl_rpmsg_test();
	apu_timesync_init();

	//@@@ apu_dpidle_init();

	return 0;
}

static void apu_ctrl_rpmsg_remove(struct rpmsg_device *rpdev)
{
	struct apu_ctrl_dev *ctrl_dev = dev_get_drvdata(&rpdev->dev);
	struct apu_ctrl_rpmsg *ctrl_rpmsg = ctrl_dev->priv;

	//@@@ apu_dpidle_exit();

	apu_timesync_remove();

	ctldev = NULL;
	initialized = false;

	rpmsg_destroy_ept(ctrl_rpmsg->ept);
}

static const struct of_device_id apu_ctrl_rpmsg_of_match[] = {
	{ .compatible = "mediatek,apu-ctrl-rpmsg", },
	{ },
};

static struct rpmsg_driver apu_ctrl_rpmsg_driver = {
	.drv	= {
		.name	= "apu-ctrl-rpmsg",
		.of_match_table = apu_ctrl_rpmsg_of_match,
	},
	.probe	= apu_ctrl_rpmsg_probe,
	.remove	= apu_ctrl_rpmsg_remove,
};

int apu_ctrl_rpmsg_init(void)
{
	int ret;

	ret = register_rpmsg_driver(&apu_ctrl_rpmsg_driver);
	if (ret)
		ipi_drv_err("failed to register apu_ctrl_rpmsg driver\n");

	return ret;
}

void apu_ctrl_rpmsg_exit(void)
{
	unregister_rpmsg_driver(&apu_ctrl_rpmsg_driver);
}
