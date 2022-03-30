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
#include <linux/ratelimit.h>

#if IS_ENABLED(CONFIG_DEBUG_FS)
#include <linux/debugfs.h>
#endif

#include <mt-plat/aee.h>

#include "apu.h"
#include "apu_config.h"
#include "apu_mbox.h"
#include "apu_ipi_config.h"
#include "apu_excep.h"

static struct lock_class_key ipi_lock_key[APU_IPI_MAX];

static unsigned int tx_serial_no;
static unsigned int rx_serial_no;
unsigned int temp_buf[APU_SHARE_BUFFER_SIZE / 4];

/* for IRQ affinity tuning */
static struct mutex affin_lock;
static unsigned int affin_depth;
static struct mtk_apu *g_apu;

#define apu_ipi_info_ratelimited(dev, fmt, ...)  \
{                                                \
	static DEFINE_RATELIMIT_STATE(_rs,           \
				      HZ * 5,                    \
				      50);                       \
	if (__ratelimit(&_rs))                       \
		dev_info(dev, fmt, ##__VA_ARGS__);       \
}

static inline void dump_msg_buf(struct mtk_apu *apu, void *data, uint32_t len)
{
	struct device *dev = apu->dev;
	uint32_t i;
	int size = 64, num;
	uint8_t buf[64], *ptr = buf;
	int ret;

	dev_info(dev, "===== dump message =====\n");
	for (i = 0; i < len; i++) {
		num = snprintf(ptr, size, "%02x ", ((uint8_t *)data)[i]);
		if (num <= 0) {
			dev_info(dev, "%s: snprintf return error(num = %d)\n",
				__func__, num);
			return;
		}
		size -= num;
		ptr += num;

		if ((i + 1) % 4 == 0) {
			ret = snprintf(ptr++, size--, " ");
			if (ret <= 0) {
				dev_info(dev, "%s: snprintf return error(ret = %d)\n",
					__func__, ret);
				return;
			}
		}

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
	for (i = 0; i < (len % sizeof(csum)); i++)
		res |= *(ptr + i) << i * 8;

	csum += res;

	return csum;
}

static inline bool bypass_check(u32 id)
{
	/* whitelist IPI used in power off flow */
	return id == APU_IPI_DEEP_IDLE;
}

static void ipi_usage_cnt_update(struct mtk_apu *apu, u32 id, int diff)
{
	struct apu_ipi_desc *ipi = &apu->ipi_desc[id];

	if (ipi_attrs[id].ack != IPI_WITH_ACK)
		return;

	spin_lock(&apu->usage_cnt_lock);
	ipi->usage_cnt += diff;
	spin_unlock(&apu->usage_cnt_lock);
}

extern int apu_deepidle_power_on_aputop(struct mtk_apu *apu);

int apu_ipi_send(struct mtk_apu *apu, u32 id, void *data, u32 len,
		 u32 wait_ms)
{
	struct apu_ipi_desc *ipi;
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
	ipi = &apu->ipi_desc[id];

	mutex_lock(&apu->send_lock);

	if (ipi_attrs[id].direction == IPI_HOST_INITIATE &&
	    apu->ipi_inbound_locked == IPI_LOCKED && !bypass_check(id)) {
		dev_info(dev, "%s: ipi locked, ipi=%d\n", __func__, id);
		mutex_unlock(&apu->send_lock);
		return -EBUSY;
	}

	/* re-init inbox mask everytime for aoc */
	apu_mbox_inbox_init(apu);

	ret = apu_deepidle_power_on_aputop(apu);
	if (ret) {
		dev_info(dev, "apu_deepidle_power_on_aputop failed\n");
		mutex_unlock(&apu->send_lock);
		return -ESHUTDOWN;
	}

	ret = apu_mbox_wait_inbox(apu);
	if (ret) {
		dev_info(dev, "wait inbox fail, ret=%d\n", ret);
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

	ipi_usage_cnt_update(apu, id, 1);

unlock_mutex:
	mutex_unlock(&apu->send_lock);

	ktime_get_ts64(&te);
	ts = timespec64_sub(te, ts);

	apu_ipi_info_ratelimited(dev,
		 "%s: ipi_id=%d, len=%d, csum=%x, serial_no=%d, elapse=%lld\n",
		 __func__, id, len, hdr.csum, hdr.serial_no,
		 timespec64_to_ns(&ts));

	return ret;
}

int apu_ipi_lock(struct mtk_apu *apu)
{
	struct apu_ipi_desc *ipi;
	int i;
	bool ready_to_lock = true;

	if (mutex_trylock(&apu->send_lock) == 0)
		return -EBUSY;

	if (apu->ipi_inbound_locked == IPI_LOCKED) {
		dev_info(apu->dev, "%s: ipi already locked\n", __func__);
		mutex_unlock(&apu->send_lock);
		return 0;
	}

	spin_lock(&apu->usage_cnt_lock);
	for (i = 0; i < APU_IPI_MAX; i++) {
		ipi = &apu->ipi_desc[i];

		if (ipi_attrs[i].ack == IPI_WITH_ACK &&
		    ipi->usage_cnt != 0 &&
		    !bypass_check(i)) {
			spin_unlock(&apu->usage_cnt_lock);
			dev_info(apu->dev, "%s: ipi %d is still in use %d\n",
				 __func__, i, ipi->usage_cnt);
			spin_lock(&apu->usage_cnt_lock);
			ready_to_lock = false;
		}

	}

	if (!ready_to_lock) {
		spin_unlock(&apu->usage_cnt_lock);
		mutex_unlock(&apu->send_lock);
		return -EBUSY;
	}

	apu->ipi_inbound_locked = IPI_LOCKED;
	spin_unlock(&apu->usage_cnt_lock);

	mutex_unlock(&apu->send_lock);

	return 0;
}

void apu_ipi_unlock(struct mtk_apu *apu)
{
	mutex_lock(&apu->send_lock);

	if (apu->ipi_inbound_locked == IPI_UNLOCKED)
		dev_info(apu->dev, "%s: ipi already unlocked\n", __func__);

	spin_lock(&apu->usage_cnt_lock);
	apu->ipi_inbound_locked = IPI_UNLOCKED;
	spin_unlock(&apu->usage_cnt_lock);

	mutex_unlock(&apu->send_lock);
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

	strscpy(apu->run.fw_ver, data, APU_FW_VER_LEN);

	apu->run.signaled = 1;
	//apu->run.signaled = run->signaled; // signaled com from remote proc
	wake_up_interruptible(&apu->run.wq);
}

static irqreturn_t apu_ipi_handler(int irq, void *priv)
{
	struct timespec64 ts, te, tl;
	struct mtk_apu *apu = priv;
	struct device *dev = apu->dev;
	ipi_handler_t handler;
	u32 id, len;

	id = apu->hdr.id;
	len = apu->hdr.len;

	/* get the latency of threaded irq */
	ktime_get_ts64(&ts);
	tl = timespec64_sub(ts, apu->intr_ts);

	mutex_lock(&apu->ipi_desc[id].lock);

	handler = apu->ipi_desc[id].handler;
	if (!handler) {
		dev_info(dev, "IPI id=%d is not registered", id);
		mutex_unlock(&apu->ipi_desc[id].lock);
		goto out;
	}

	handler(temp_buf, len, apu->ipi_desc[id].priv);

	ipi_usage_cnt_update(apu, id, -1);

	mutex_unlock(&apu->ipi_desc[id].lock);

	apu->ipi_id_ack[id] = true;
	wake_up(&apu->ack_wq);

out:
	ktime_get_ts64(&te);
	ts = timespec64_sub(te, ts);

	apu_ipi_info_ratelimited(dev,
		 "%s: ipi_id=%d, len=%d, csum=%x, serial_no=%d, latency=%lld, elapse=%lld\n",
		 __func__, id, len, apu->hdr.csum, apu->hdr.serial_no,
		 timespec64_to_ns(&tl),
		 timespec64_to_ns(&ts));

	return IRQ_HANDLED;
}

irqreturn_t apu_ipi_int_handler(int irq, void *priv)
{
	struct mtk_apu *apu = priv;
	struct device *dev = apu->dev;
	struct mtk_share_obj *recv_obj = apu->recv_buf;
	u32 id, len, calc_csum;
	bool finish = false;
	uint32_t status;

	status = ioread32(apu->apu_mbox + 0xc4);
	if (status != ((1 << APU_MBOX_HDR_SLOTS) - 1)) {
		dev_info(dev, "abnormal isr call(0x%x), skip\n", status);
		return IRQ_HANDLED;
	}

	apu_mbox_read_outbox(apu, &apu->hdr);
	id = apu->hdr.id;
	len = apu->hdr.len;

	if (id >= APU_IPI_MAX) {
		dev_info(dev, "no such IPI id = %d", id);
		finish = true;
	}

	if (len > APU_SHARE_BUFFER_SIZE) {
		dev_info(dev, "IPI message too long(len %d, max %d)",
			len, APU_SHARE_BUFFER_SIZE);
		finish = true;
	}

	if (apu->hdr.serial_no != rx_serial_no) {
		dev_info(dev, "unmatched serial_no: curr=%u, recv=%u\n",
			rx_serial_no, apu->hdr.serial_no);
		/* correct the serial no. */
		rx_serial_no = apu->hdr.serial_no;
		apusys_rv_aee_warn("APUSYS_RV", "IPI rx_serial_no unmatch");
	}
	rx_serial_no++;

	if (finish)
		goto done;

	memcpy_fromio(temp_buf, &recv_obj->share_buf, len);

	/* ack after data copied */
	apu_mbox_ack_outbox(apu);

	calc_csum = calculate_csum(temp_buf, len);
	if (calc_csum != apu->hdr.csum) {
		dev_info(dev, "csum error: recv=0x%08x, calc=0x%08x\n",
			apu->hdr.csum, calc_csum);
		dump_msg_buf(apu, temp_buf, apu->hdr.len);
		apusys_rv_aee_warn("APUSYS_RV", "IPI rx csum error");
	}

	ktime_get_ts64(&apu->intr_ts);

	return IRQ_WAKE_THREAD;

done:
	apu_mbox_ack_outbox(apu);

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

#if IS_ENABLED(CONFIG_DEBUG_FS)

extern int apu_keep_awake;

#define APU_IPI_DNAME	"apu_ipi"
#define APU_IPI_FNAME	"ipi_dbg"
static struct dentry *ipi_dbg_root, *ipi_dbg_file;

static int apu_ipi_dbg_show(struct seq_file *s, void *unused)
{
	seq_printf(s, "keep_awake = %d\n", apu_keep_awake);

	return 0;
}

static int apu_ipi_dbg_open(struct inode *inode, struct file *file)
{
	return single_open(file, apu_ipi_dbg_show, inode->i_private);
}

enum {
	CMD_KEEP_AWAKE = 0,
};

static void apu_ipi_dbg_set_params(int cmd, unsigned int *args,
				   int cnt)
{
	switch (cmd) {
	case CMD_KEEP_AWAKE:
		apu_keep_awake = args[0];
		break;
	default:
		pr_info("%s: unknown cmd %d\n", __func__, cmd);
	}
}

#define IPI_DBG_MAX_ARGS	(1)
static ssize_t apu_ipi_dbg_write(struct file *flip, const char __user *buffer,
				 size_t count, loff_t *f_pos)
{
	char *tmp, *ptr, *token;
	unsigned int args[IPI_DBG_MAX_ARGS];
	int cmd;
	int ret, i;

	tmp = kzalloc(count + 1, GFP_KERNEL);
	if (!tmp)
		return -ENOMEM;

	ret = copy_from_user(tmp, buffer, count);
	if (ret) {
		pr_info("%s: failed to copy user data, ret=%d\n",
			__func__, ret);
		goto out;
	}

	tmp[count] = '\0';
	ptr = tmp;

	token = strsep(&ptr, " ");
	if (strcmp(token, "keep_awake") == 0) {
		cmd = CMD_KEEP_AWAKE;
	} else {
		ret = -EINVAL;
		pr_info("%s: unknown ipi dbg cmd: %s\n", __func__, token);
		goto out;
	}

	for (i = 0; i < IPI_DBG_MAX_ARGS && (token = strsep(&ptr, " ")); i++) {
		ret = kstrtoint(token, 10, &args[i]);
		if (ret) {
			pr_info("%s: invalid parameter i=%d, p=%s\n",
				__func__, i, token);
			goto out;
		}
	}

	apu_ipi_dbg_set_params(cmd, args, i);
	ret = count;

out:
	kfree(tmp);

	return ret;
}

static const struct file_operations apu_ipi_dbg_fops = {
	.open = apu_ipi_dbg_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = apu_ipi_dbg_write,
};

static int apu_ipi_dbg_init(void)
{
	ipi_dbg_root = debugfs_create_dir(APU_IPI_DNAME, NULL);
	if (IS_ERR_OR_NULL(ipi_dbg_root)) {
		pr_info("%s: failed to create debug dir %s\n",
			__func__, APU_IPI_DNAME);
		return -EINVAL;
	}

	ipi_dbg_file = debugfs_create_file(APU_IPI_FNAME, (0644), ipi_dbg_root,
					   NULL, &apu_ipi_dbg_fops);
	if (IS_ERR_OR_NULL(ipi_dbg_file)) {
		pr_info("%s: failed to create debug file %s\n",
			__func__, APU_IPI_FNAME);
		debugfs_remove_recursive(ipi_dbg_root);
		return -EINVAL;
	}

	return 0;
}

static void apu_ipi_dbg_exit(void)
{
	debugfs_remove(ipi_dbg_file);
	debugfs_remove_recursive(ipi_dbg_root);
}

#else
static int apu_ipi_dbg_init(void) { return 0; }
static void apu_ipi_dbg_exit(void) { }
#endif

int apu_ipi_affin_enable(void)
{
	struct mtk_apu *apu = g_apu;
	struct mtk_apu_hw_ops *hw_ops = &apu->platdata->ops;
	int ret = 0;

	mutex_lock(&affin_lock);

	if (affin_depth == 0)
		ret = hw_ops->irq_affin_set(apu);

	affin_depth++;

	mutex_unlock(&affin_lock);

	return ret;
}

int apu_ipi_affin_disable(void)
{
	struct mtk_apu *apu = g_apu;
	struct mtk_apu_hw_ops *hw_ops = &apu->platdata->ops;
	int ret = 0;

	mutex_lock(&affin_lock);

	affin_depth--;

	if (affin_depth == 0)
		ret = hw_ops->irq_affin_unset(apu);

	mutex_unlock(&affin_lock);

	return ret;
}

void apu_ipi_remove(struct mtk_apu *apu)
{
	apu_ipi_dbg_exit();
	apu_mbox_hw_exit(apu);
	apu_remove_rpmsg_subdev(apu);
	apu_ipi_unregister(apu, APU_IPI_INIT);
}

int apu_ipi_init(struct platform_device *pdev, struct mtk_apu *apu)
{
	struct mtk_apu_hw_ops *hw_ops = &apu->platdata->ops;
	struct device *dev = apu->dev;
	int i, ret;

	tx_serial_no = 0;
	rx_serial_no = 0;

	mutex_init(&apu->send_lock);
	spin_lock_init(&apu->usage_cnt_lock);

	mutex_init(&affin_lock);
	affin_depth = 0;
	g_apu = apu;

	for (i = 0; i < APU_IPI_MAX; i++) {
		mutex_init(&apu->ipi_desc[i].lock);
		lockdep_set_class_and_name(&apu->ipi_desc[i].lock,
					   &ipi_lock_key[i],
					   ipi_attrs[i].name);
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
				apu_ipi_int_handler, apu_ipi_handler, IRQF_ONESHOT,
				"apu_ipi", apu);
	if (ret < 0) {
		dev_info(&pdev->dev, "%s: failed to request irq %d, ret=%d\n",
			__func__, apu->mbox0_irq_number, ret);
		goto remove_rpmsg_subdev;
	}

	hw_ops->irq_affin_init(apu);

	apu_mbox_hw_init(apu);

	apu_ipi_dbg_init();

	return 0;

remove_rpmsg_subdev:
	apu_remove_rpmsg_subdev(apu);
	apu_ipi_unregister(apu, APU_IPI_INIT);

	return ret;
}
