// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/wait.h>
#include <linux/lockdep.h>
#include <linux/time64.h>
#include <linux/kernel.h>

#include "apu.h"
#include "apu_config.h"
#include "apu_mbox.h"


static const char *ipi_lock_name[APU_IPI_MAX] = {
	"init",
	"name-service",
	"power-on-dummy",
	"apu-ctrl",
	"apu-mdw",
};
static struct lock_class_key ipi_lock_key[APU_IPI_MAX];

static unsigned int tx_serial_no = 0;
static unsigned int rx_serial_no = 0;

static inline void dump_msg_buf(struct mtk_apu *apu, void *data, uint32_t len)
{
	struct device *dev = apu->dev;
	uint32_t i;
	int size = 64, num;
	uint8_t buf[64], *ptr = buf;

	dev_info(dev, "===== dump message =====\n");
	for (i = 0; i < len; i++) {
		num = snprintf(ptr, size, "%02x ", ((uint8_t *)data)[i]);
		size -= num;
		ptr += num;

		if ((i + 1) % 4 == 0)
			snprintf(ptr++, size--, " ");

		if ((i + 1) % 16 == 0 || (i + 1) >= len) {
			dev_info(dev, "%s\n", buf);
			size = 64;
			ptr = buf;
		}
	}
	dev_info(dev, "========================\n");
}

static uint32_t calculate_csum(void *data, uint32_t len)
{
	uint32_t csum = 0, res = 0, i;
	uint8_t *ptr;

	for (i = 0; i < (len / sizeof(csum)); i++)
		csum += *(((uint32_t *)data) + i);

	ptr = (uint8_t *)data + len / sizeof(csum) * sizeof(csum);
	for (i = 0; i < (len % sizeof(csum)); i++) {
		res |= *(ptr + i) << i * 8;
	}

	csum += res;

	return csum;
}

int apu_ipi_send(struct mtk_apu *apu, u32 id, void *data, u32 len,
		 u32 wait_ms)
{
	struct timespec64 ts, te;
	struct device *dev;
	struct apu_mbox_hdr hdr;
	unsigned long timeout;
	int ret = 0;

	ktime_get_ts64(&ts);

	if ((!apu) || (id <= APU_IPI_INIT) ||
	    (id >= APU_IPI_MAX) || (id == APU_IPI_NS_SERVICE) ||
	    (len > APU_SHARE_BUFFER_SIZE) || (!data))
		return -EINVAL;

	dev = apu->dev;

	mutex_lock(&apu->send_lock);

	ret = apu_mbox_wait_inbox(apu);
	if (ret) {
		dev_info(apu->dev, "wait inbox fail, ret=%d\n", ret);
		goto unlock_mutex;
	}

	/* copy message payload to share buffer, need to do cache flush if
	 * the buffer is cacheable. currently not
	 */
	memcpy_toio(apu->send_buf, data, len);

	hdr.id = id;
	hdr.len = len;
	hdr.csum = calculate_csum(data, len);
	hdr.serial_no = tx_serial_no++;

	dev_info(apu->dev,
		 "%s: ipi_id=%d, len=%d, csum=%x, serial_no=%d,"
		 "rpmsg_id=%d, rpmsg_len=%d\n", __func__,
		 id, len, hdr.csum, hdr.serial_no, ((unsigned int *)data)[0],
		 ((unsigned int *)data)[1]);

	apu_mbox_write_inbox(apu, &hdr);

	apu->ipi_id_ack[id] = false;

	/* poll ack from remote processor if wait_ms specified */
	if (wait_ms) {
		timeout = jiffies + msecs_to_jiffies(wait_ms);
		ret = wait_event_timeout(apu->ack_wq,
					 &apu->ipi_id_ack[id],
					 timeout);

		apu->ipi_id_ack[id] = false;

		if (WARN(!ret, "apu ipi %d ack timeout!", id)) {
			ret = -EIO;
			goto unlock_mutex;
		} else {
			ret = 0;
		}
	}

unlock_mutex:
	mutex_unlock(&apu->send_lock);

	ktime_get_ts64(&te);
	ts = timespec64_sub(te, ts);

	dev_info(apu->dev,
		 "%s: ipi_id=%d, len=%d, csum=%x, serial_no=%d, "
		 "rpmsg_id=%d, rpmsg_len=%d elapse=%lld\n", __func__,
		 id, len, hdr.csum, hdr.serial_no, ((unsigned int *)data)[0],
		 ((unsigned int *)data)[1], timespec64_to_ns(&ts));

	return ret;
}

int apu_ipi_register(struct mtk_apu *apu, u32 id,
		     ipi_handler_t handler, void *priv)
{
	if (!apu || id >= APU_IPI_MAX || WARN_ON(handler == NULL)) {
		if (apu != NULL)
			dev_info(apu->dev,
				"%s failed. apu=%p, id=%d, handler=%p, priv=%p\n",
				__func__, apu, id, handler, priv);
		return -EINVAL;
	}

	dev_info(apu->dev, "%s: apu=%p, ipi=%d, handler=%p, priv=%p",
		 __func__, apu, id, handler, priv);

	mutex_lock(&apu->ipi_desc[id].lock);
	apu->ipi_desc[id].handler = handler;
	apu->ipi_desc[id].priv = priv;
	mutex_unlock(&apu->ipi_desc[id].lock);

	return 0;
}

void apu_ipi_unregister(struct mtk_apu *apu, u32 id)
{
	if (!apu || id >= APU_IPI_MAX) {
		if (apu != NULL)
			dev_info(apu->dev, "%s: invalid id=%d\n", __func__, id);
		return;
	}

	mutex_lock(&apu->ipi_desc[id].lock);
	apu->ipi_desc[id].handler = NULL;
	apu->ipi_desc[id].priv = NULL;
	mutex_unlock(&apu->ipi_desc[id].lock);
}

static void apu_init_ipi_handler(void *data, unsigned int len, void *priv)
{
	struct mtk_apu *apu = priv;
	struct apu_run *run = data;
	struct device *dev = apu->dev;

	strscpy(apu->run.fw_ver, data, APU_FW_VER_LEN);

	apu->run.signaled = 1;
	//apu->run.signaled = run->signaled; // signaled com from remote proc
	wake_up_interruptible(&apu->run.wq);

	dev_info(dev, "fw_ver: %s\n", run->fw_ver);
}

static irqreturn_t apu_ipi_handler(int irq, void *priv)
{
	struct timespec64 ts, te;
	struct mtk_apu *apu = priv;
	struct apu_mbox_hdr hdr;
	struct mtk_share_obj *recv_obj = apu->recv_buf;
	ipi_handler_t handler;
	u32 id, len, calc_csum;
	u32 temp_buf[APU_SHARE_BUFFER_SIZE / 4];

	ktime_get_ts64(&ts);

	apu_mbox_read_outbox(apu, &hdr);
	id = hdr.id;
	len = hdr.len;

	if (hdr.serial_no != rx_serial_no) {
		dev_info(apu->dev, "unmatched serial_no: curr=%u, recv=%u\n",
			rx_serial_no, hdr.serial_no);
	}
	rx_serial_no++;

	if (len > APU_SHARE_BUFFER_SIZE) {
		dev_info(apu->dev, "IPI message too long(len %d, max %d)",
			len, APU_SHARE_BUFFER_SIZE);
		goto ack_irq;
	}

	if (id >= APU_IPI_MAX) {
		dev_info(apu->dev, "no such IPI id = %d", id);
		goto ack_irq;
	}

	mutex_lock(&apu->ipi_desc[id].lock);
	handler = apu->ipi_desc[id].handler;
	if (!handler) {
		dev_info(apu->dev, "IPI id=%d is not registered", id);
		mutex_unlock(&apu->ipi_desc[id].lock);
		goto ack_irq;
	}

	memcpy_fromio(temp_buf, &recv_obj->share_buf, len);

	calc_csum = calculate_csum(temp_buf, len);
	if (calc_csum != hdr.csum) {
		dev_info(apu->dev, "csum error: recv=0x%08x, calc=0x%08x\n",
			hdr.csum, calc_csum);
		dump_msg_buf(apu, temp_buf, hdr.len);
	}

	handler(temp_buf, len, apu->ipi_desc[id].priv);
	mutex_unlock(&apu->ipi_desc[id].lock);

	apu->ipi_id_ack[id] = true;
	wake_up(&apu->ack_wq);

ack_irq:
	apu_mbox_ack_outbox(apu);

	ktime_get_ts64(&te);
	ts = timespec64_sub(te, ts);

	dev_info(apu->dev,
		 "%s: ipi_id=%d, len=%d, csum=%x, serial_no=%d, "
		 "rpmsg_id=%d, rpmsg_len=%d elapse=%lld\n", __func__,
		 id, len, hdr.csum, hdr.serial_no, temp_buf[0], temp_buf[1],
		 timespec64_to_ns(&ts));

	return IRQ_HANDLED;
}

static int apu_send_ipi(struct platform_device *pdev, u32 id, void *buf,
			unsigned int len, unsigned int wait)
{
	struct mtk_apu *apu = platform_get_drvdata(pdev);

	return apu_ipi_send(apu, id, buf, len, wait);
}

static int apu_register_ipi(struct platform_device *pdev, u32 id,
			    ipi_handler_t handler, void *priv)
{
	struct mtk_apu *apu = platform_get_drvdata(pdev);

	return apu_ipi_register(apu, id, handler, priv);
}

static void apu_unregister_ipi(struct platform_device *pdev, u32 id)
{
	struct mtk_apu *apu = platform_get_drvdata(pdev);

	apu_ipi_unregister(apu, id);
}

static struct mtk_apu_rpmsg_info mtk_apu_rpmsg_info = {
	.send_ipi = apu_send_ipi,
	.register_ipi = apu_register_ipi,
	.unregister_ipi = apu_unregister_ipi,
	.ns_ipi_id = APU_IPI_NS_SERVICE,
};

static void apu_add_rpmsg_subdev(struct mtk_apu *apu)
{
	apu->rpmsg_subdev =
		mtk_apu_rpmsg_create_rproc_subdev(to_platform_device(apu->dev),
						  &mtk_apu_rpmsg_info);

	if (apu->rpmsg_subdev)
		rproc_add_subdev(apu->rproc, apu->rpmsg_subdev);
}

static void apu_remove_rpmsg_subdev(struct mtk_apu *apu)
{
	if (apu->rpmsg_subdev) {
		rproc_remove_subdev(apu->rproc, apu->rpmsg_subdev);
		mtk_apu_rpmsg_destroy_rproc_subdev(apu->rpmsg_subdev);
		apu->rpmsg_subdev = NULL;
	}
}

void apu_ipi_config_remove(struct mtk_apu *apu)
{
	dma_free_coherent(apu->dev, APU_SHARE_BUF_SIZE,
			  apu->recv_buf, apu->recv_buf_da);
}

int apu_ipi_config_init(struct mtk_apu *apu)
{
	struct device *dev = apu->dev;
	struct apu_ipi_config *ipi_config;
	void *ipi_buf = NULL;
	dma_addr_t ipi_buf_da = 0;

	ipi_config = (struct apu_ipi_config *)
		get_apu_config_user_ptr(apu->conf_buf, eAPU_IPI_CONFIG);

	/* initialize shared buffer */
	ipi_buf = dma_alloc_coherent(dev, APU_SHARE_BUF_SIZE,
				     &ipi_buf_da, GFP_KERNEL);
	if (!ipi_buf || !ipi_buf_da) {
		dev_info(dev, "failed to allocate ipi share memory\n");
		return -ENOMEM;
	}

	dev_info(dev, "%s: ipi_buf=%p, ipi_buf_da=%llu\n",
		 __func__, ipi_buf, ipi_buf_da);

	memset_io(ipi_buf, 0, sizeof(struct mtk_share_obj)*2);

	apu->recv_buf = ipi_buf;
	apu->recv_buf_da = ipi_buf_da;
	apu->send_buf = ipi_buf + sizeof(struct mtk_share_obj);
	apu->send_buf_da = ipi_buf_da + sizeof(struct mtk_share_obj);

	ipi_config->in_buf_da = apu->send_buf_da;
	ipi_config->out_buf_da = apu->recv_buf_da;

	return 0;
}

void apu_ipi_remove(struct mtk_apu *apu)
{
	apu_mbox_hw_exit(apu);
	apu_remove_rpmsg_subdev(apu);
	apu_ipi_unregister(apu, APU_IPI_INIT);
}

int apu_ipi_init(struct platform_device *pdev, struct mtk_apu *apu)
{
	struct device *dev = apu->dev;
	int i, ret;

	mutex_init(&apu->send_lock);

	for (i = 0; i < APU_IPI_MAX; i++) {
		mutex_init(&apu->ipi_desc[i].lock);
		lockdep_set_class_and_name(&apu->ipi_desc[i].lock,
					   &ipi_lock_key[i],
					   ipi_lock_name[i]);
	}

	init_waitqueue_head(&apu->run.wq);
	init_waitqueue_head(&apu->ack_wq);

	/* APU initialization IPI register */
	ret = apu_ipi_register(apu, APU_IPI_INIT, apu_init_ipi_handler, apu);
	if (ret) {
		dev_info(dev, "failed to register ipi for init, ret=%d\n",
			ret);
		return ret;
	}

	/* add rpmsg subdev */
	apu_add_rpmsg_subdev(apu);

	/* register mailbox IRQ */
	apu->mbox0_irq_number = platform_get_irq_byname(pdev, "mbox0_irq");
	dev_info(&pdev->dev, "%s: mbox0_irq = %d\n", __func__,
		 apu->mbox0_irq_number);

	ret = devm_request_threaded_irq(&pdev->dev, apu->mbox0_irq_number,
				NULL, apu_ipi_handler, IRQF_ONESHOT,
				"apu_ipi", apu);
	if (ret < 0) {
		dev_info(&pdev->dev, "%s: failed to request irq %d, ret=%d\n",
			__func__, apu->mbox0_irq_number, ret);
		goto remove_rpmsg_subdev;
	}

	apu_mbox_hw_init(apu);

	return 0;

remove_rpmsg_subdev:
	apu_remove_rpmsg_subdev(apu);
	apu_ipi_unregister(apu, APU_IPI_INIT);

	return ret;
}
