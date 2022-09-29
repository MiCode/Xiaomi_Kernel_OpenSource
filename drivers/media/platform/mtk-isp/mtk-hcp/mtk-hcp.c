// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <asm/cacheflush.h>
#include <linux/cdev.h>
#include <linux/dma-mapping.h>
#include <linux/file.h>
#include <linux/firmware.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/compat.h>
#include <linux/freezer.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/dma-iommu.h>
#include <linux/dma-mapping.h>
#include <linux/videodev2.h>
#include <videobuf2-dma-contig.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/module.h>

#include <aee.h>

#include "mtk_imgsys-dev.h"

#include "mtk-hcp.h"
#include "mtk-hcp-aee.h"
#include "mtk-hcp-support.h"
#include "mtk-hcp_isp71.h"
#include "mtk-hcp_isp7s.h"


#ifdef CONFIG_MTK_IOMMU_V2
#include "mtk_iommu_ext.h"
#elif defined(CONFIG_MTK_IOMMU)
#include "mach/mt_iommu.h"
#elif defined(CONFIG_MTK_M4U)
#include "m4u.h"
#endif

#ifdef CONFIG_MTK_M4U
#include "m4u.h"
#include "m4u_port.h"
#endif

#include <mtk_heap.h>

#include "slbc_ops.h"

#include <linux/dma-heap.h>
#include <uapi/linux/dma-heap.h>
#include <linux/dma-heap.h>
#include <linux/dma-direction.h>
#include <linux/scatterlist.h>
#include <linux/dma-buf.h>


/**
 * HCP (Hetero Control Processor ) is a tiny processor controlling
 * the methodology of register programming. If the module support
 * to run on CM4 then it will send data to CM4 to program register.
 * Or it will send the data to user library and let RED to program
 * register.
 *
 **/

#define RED_PATH                "/dev/red"
#define HCP_DEVNAME             "mtk_hcp"

#define HCP_TIMEOUT_MS          4000U
#define HCP_FW_VER_LEN          16
#define MAX_REQUEST_SIZE        10

#define SYNC_SEND               1
#define ASYNC_SEND              0

static struct mtk_hcp *hcp_mtkdev;

#define IPI_MAX_BUFFER_COUNT    (8)

struct packet {
	int32_t module;
	bool more;
	int32_t count;
	struct share_buf *buffer[IPI_MAX_BUFFER_COUNT];
};

#define CTRL_ID_SLB_BASE        (0x01)

struct ctrl_data {
	uint32_t id;
	uint64_t value;
} __packed;

#define HCP_INIT                _IOWR('H', 0, struct share_buf)
#define HCP_GET_OBJECT          _IOWR('H', 1, struct share_buf)
#define HCP_NOTIFY              _IOWR('H', 2, struct share_buf)
#define HCP_COMPLETE            _IOWR('H', 3, struct share_buf)
#define HCP_WAKEUP              _IOWR('H', 4, struct share_buf)
#define HCP_TIMEOUT             _IO('H', 5)

#if IS_ENABLED(CONFIG_COMPAT)
#define COMPAT_HCP_INIT         _IOWR('H', 0, struct share_buf)
#define COMPAT_HCP_GET_OBJECT   _IOWR('H', 1, struct share_buf)
#define COMPAT_HCP_NOTIFY       _IOWR('H', 2, struct share_buf)
#define COMPAT_HCP_COMPLETE     _IOWR('H', 3, struct share_buf)
#define COMPAT_HCP_WAKEUP       _IOWR('H', 4, struct share_buf)
#define COMPAT_HCP_TIMEOUT      _IO('H', 5)
#endif

struct msg {
	struct list_head entry;
	struct share_buf user_obj;
};
#define  MSG_NR (96)
/**
 * struct my_wq_t - work struct to handle daemon notification
 *
 * @hcp_dev:        hcp device
 * @ioctl_event:    ioctl event id
 * @data_addr:      addr about shared data
 * @task_work:      work struct
 */
struct my_wq_t {
	struct mtk_hcp *hcp_dev;
	unsigned int ioctl_event;
	struct share_buf  data_addr;
	struct work_struct task_work;
};

/*  function prototype declaration */
static void module_notify(struct mtk_hcp *hcp_dev,
						struct share_buf *user_data_addr);
static int hcp_send_internal(struct mtk_hcp *hcp_dev,
							enum hcp_id id, void *buf,
							unsigned int len, int req_fd,
							unsigned int wait);
/*  End */

static struct msg *msg_pool_get(struct mtk_hcp *hcp_dev)
{
	unsigned long flag = 0;
	unsigned long empty = 0;
	struct msg *msg = NULL;

	spin_lock_irqsave(&hcp_dev->msglock, flag);
	empty = list_empty(&hcp_dev->msg_list);
	if (!empty) {
		msg = list_first_entry(&hcp_dev->msg_list, struct msg, entry);
		list_del(&msg->entry);
	}
	spin_unlock_irqrestore(&hcp_dev->msglock, flag);

	return msg;
}

static void chans_pool_dump(struct mtk_hcp *hcp_dev)
{
	unsigned long flag = 0;
	struct msg *msg = NULL;
	struct msg *tmp = NULL;
	int i = 0;
	int seq_id = 0;
	int req_fd = 0;
	int hcp_id = 0;

	spin_lock_irqsave(&hcp_dev->msglock, flag);
	for (i = 0; i < MODULE_MAX_ID; i++) {
		dev_info(hcp_dev->dev, "HCP(%d) stalled IPI object+\n", i);

		list_for_each_entry_safe(msg, tmp,
				&hcp_dev->chans[i], entry){

			seq_id = msg->user_obj.info.send.seq;
			req_fd = msg->user_obj.info.send.req;
			hcp_id = msg->user_obj.info.send.hcp;

			dev_info(hcp_dev->dev, "req_fd(%d), seq_id(%d), hcp_id(%d)\n",
				req_fd, seq_id, hcp_id);
		}

		dev_info(hcp_dev->dev, "HCP(%d) stalled IPI object-\n", i);
	}
	spin_unlock_irqrestore(&hcp_dev->msglock, flag);
}

static struct msg *chan_pool_get
	(struct mtk_hcp *hcp_dev, unsigned int module_id)
{
	unsigned long flag = 0;
	unsigned long empty = 0;
	struct msg *msg = NULL;

	spin_lock_irqsave(&hcp_dev->msglock, flag);
	empty = list_empty(&hcp_dev->chans[module_id]);
	if (!empty) {
		msg = list_first_entry(&hcp_dev->chans[module_id], struct msg, entry);
		list_del(&msg->entry);
	}
	empty = list_empty(&hcp_dev->chans[module_id]);
	spin_unlock_irqrestore(&hcp_dev->msglock, flag);

	// dev_info(hcp_dev->dev, "chan pool empty(%d)\n", empty);

	return msg;
}

static bool chan_pool_available(struct mtk_hcp *hcp_dev, int module_id)
{
	unsigned long flag = 0;
	unsigned long empty = 0;

	spin_lock_irqsave(&hcp_dev->msglock, flag);
	empty = list_empty(&hcp_dev->chans[module_id]);
	spin_unlock_irqrestore(&hcp_dev->msglock, flag);

	// dev_info(hcp_dev->dev, "chan pool abailable(%d)\n", !empty);

	return (!empty);
}

inline int hcp_id_to_ipi_id(struct mtk_hcp *hcp_dev, enum hcp_id id)
{
	int ipi_id = -EINVAL;

	if (id >= HCP_MAX_ID) {
		dev_info(hcp_dev->dev, "%s: Invalid hcp id %d\n", __func__, id);
		return -EINVAL;
	}

	switch (id) {

	default:
		break;
	}

	dev_dbg(hcp_dev->dev, "ipi_id:%d\n", ipi_id);
	return ipi_id;
}

inline int hcp_id_to_module_id(struct mtk_hcp *hcp_dev, enum hcp_id id)
{
	int module_id = -EINVAL;

	if (id >= HCP_MAX_ID) {
		dev_info(hcp_dev->dev, "%s: Invalid hcp id %d\n", __func__, id);
		return -EINVAL;
	}

	switch (id) {
	case HCP_ISP_CMD_ID:
	case HCP_ISP_FRAME_ID:
		module_id = MODULE_ISP;
		break;
	case HCP_DIP_INIT_ID:
	case HCP_DIP_FRAME_ID:
	case HCP_IMGSYS_HW_TIMEOUT_ID:
	case HCP_IMGSYS_SW_TIMEOUT_ID:
	case HCP_DIP_DEQUE_DUMP_ID:
	case HCP_IMGSYS_DEQUE_DONE_ID:
	case HCP_IMGSYS_DEINIT_ID:
	case HCP_IMGSYS_IOVA_FDS_ADD_ID:
	case HCP_IMGSYS_IOVA_FDS_DEL_ID:
	case HCP_IMGSYS_UVA_FDS_ADD_ID:
	case HCP_IMGSYS_UVA_FDS_DEL_ID:
	case HCP_IMGSYS_SET_CONTROL_ID:
	case HCP_IMGSYS_GET_CONTROL_ID:
		module_id = MODULE_DIP;
		break;
	case HCP_RSC_INIT_ID:
	case HCP_RSC_FRAME_ID:
		module_id = MODULE_RSC;
		break;
	default:
		break;
	}

	dev_dbg(hcp_dev->dev, "module_id:%d\n", module_id);
	return module_id;
}

inline int ipi_id_to_hcp_id(int id)
{
	int hcp_id = HCP_INIT_ID;

	switch (id) {

	default:
		break;
	}
	pr_debug("[%s]hcp_id:%d\n", __func__, hcp_id);
	return hcp_id;
}

static inline bool mtk_hcp_running(struct mtk_hcp *hcp_dev)
{
	return hcp_dev->is_open;
}

#ifdef AED_SET_EXTRA_FUNC_READY_ON_K515
int hcp_notify_aee(void)
{
	struct msg *msg = NULL;
	char dummy = 0;

	pr_info("HCP trigger AEE dump+\n");
	msg = msg_pool_get(hcp_mtkdev);
	msg->user_obj.id = HCP_IMGSYS_AEE_DUMP_ID;
	msg->user_obj.len = 0;
	msg->user_obj.info.send.hcp = HCP_IMGSYS_AEE_DUMP_ID;
	msg->user_obj.info.send.req = 0;
	msg->user_obj.info.send.ack = 0;

	hcp_send_internal(hcp_mtkdev, HCP_IMGSYS_AEE_DUMP_ID, &dummy, 1, 0, 1);
	module_notify(hcp_mtkdev, &msg->user_obj);

	pr_info("HCP trigger AEE dump-\n");

	return 0;
}
#endif

int mtk_hcp_proc_open(struct inode *inode, struct file *file)
{
	struct mtk_hcp *hcp_dev = hcp_mtkdev;

	const char *name = NULL;

	if (!try_module_get(THIS_MODULE))
		return -ENODEV;

	name = file->f_path.dentry->d_name.name;
	if (!strcmp(name, "daemon")) {
		file->private_data
			= &hcp_dev->aee_info.data[HCP_AEE_PROC_FILE_DAEMON];
	} else if (!strcmp(name, "kernel")) {
		file->private_data
			= &hcp_dev->aee_info.data[HCP_AEE_PROC_FILE_KERNEL];
	} else if (!strcmp(name, "stream")) {
		file->private_data
			= &hcp_dev->aee_info.data[HCP_AEE_PROC_FILE_STREAM];
	} else {
		pr_info("unknown proc file(%s)", name);
		module_put(THIS_MODULE);
		return -EPERM;
	}

	if (file->private_data == NULL) {
		pr_info("failed to allocate proc file(%s) buffer", name);
		module_put(THIS_MODULE);
		return -ENOMEM;
	}

	pr_info("%s: %s opened\n", __func__, name);

	return 0;
}

static ssize_t mtk_hcp_proc_read(struct file *file, char __user *buf,
	size_t lbuf, loff_t *ppos)
{
	struct mtk_hcp *hcp_dev = hcp_mtkdev;
	struct hcp_proc_data *data = (struct hcp_proc_data *)file->private_data;
	int remain = 0;
	int len = 0;
	int ret = 0;

	ret = mutex_lock_killable(&data->mtx);
	if (ret == -EINTR) {
		pr_info("mtx lock failed due to process being killed");
		return ret;
	}

	remain = data->cnt - *ppos;
	len = (remain > lbuf) ? lbuf : remain;
	if (len == 0) {
		mutex_unlock(&data->mtx);
		dev_dbg(hcp_dev->dev, "Reached end of the device on a read");
		return 0;
	}

	len = len - copy_to_user(buf, data->buf + *ppos, len);
	*ppos += len;

	mutex_unlock(&data->mtx);

	dev_dbg(hcp_dev->dev, "Leaving the READ function, len=%d, pos=%d\n",
		len, (int)*ppos);

	return len;
}

ssize_t mtk_hcp_kernel_db_write(struct platform_device *pdev,
		const char *buf, size_t sz)
{
	struct mtk_hcp *hcp_dev = platform_get_drvdata(pdev);
	struct hcp_aee_info *info = &hcp_dev->aee_info;
	struct hcp_proc_data *data
		= (struct hcp_proc_data *)&info->data[HCP_AEE_PROC_FILE_KERNEL];
	size_t remain = 0;
	size_t len = 0;
	int ret = 0;

	ret = mutex_lock_killable(&data->mtx);
	if (ret == -EINTR) {
		pr_info("mtx lock failed due to process being killed");
		return ret;
	}

	remain = data->sz - data->cnt;
	len = (remain > sz) ? sz : remain;

	if (len == 0) {
		dev_dbg(hcp_dev->dev, "Reach end of the file on write");
		mutex_unlock(&data->mtx);
		return 0;
	}

	memcpy(data->buf + data->cnt, buf, len);
	data->cnt += len;

	mutex_unlock(&data->mtx);

	return len;
}
EXPORT_SYMBOL(mtk_hcp_kernel_db_write);

static ssize_t mtk_hcp_proc_write(struct file *file, const char __user *buf,
	size_t lbuf, loff_t *ppos)
{
	struct mtk_hcp *hcp_dev = hcp_mtkdev;
	struct hcp_proc_data *data = (struct hcp_proc_data *)file->private_data;
	ssize_t remain = 0;
	ssize_t len = 0;
	int ret = 0;

	ret = mutex_lock_killable(&data->mtx);
	if (ret == -EINTR) {
		pr_info("mtx lock failed due to process being killed");
		return 0;
	}

	remain = data->sz - data->cnt;
	len = (remain > lbuf) ? lbuf : remain;
	if (len == 0) {
		mutex_unlock(&data->mtx);
		dev_dbg(hcp_dev->dev, "Reached end of the device on a write");
		return 0;
	}

	len = len - copy_from_user(data->buf + data->cnt, buf, len);

	data->cnt += len;
	*ppos = data->cnt;

	mutex_unlock(&data->mtx);

	dev_dbg(hcp_dev->dev, "Leaving the WRITE function, len=%d, pos=%u\n",
		len, data->cnt);

	return len;
}

int mtk_hcp_proc_close(struct inode *inode, struct file *file)
{
	module_put(THIS_MODULE);
	return 0;
}

static const struct proc_ops aee_ops = {
	.proc_open = mtk_hcp_proc_open,
	.proc_read  = mtk_hcp_proc_read,
	.proc_write = mtk_hcp_proc_write,
	.proc_release = mtk_hcp_proc_close
};

static void hcp_aee_reset(struct mtk_hcp *hcp_dev)
{
	int i = 0;
	struct hcp_aee_info *info = &hcp_dev->aee_info;

	dev_dbg(hcp_dev->dev, "%s -s\n", __func__);

	if (info == NULL) {
		dev_info(hcp_dev->dev, " %s - aee_info is NULL\n", __func__);
		return;
	}

	for (i = 0 ; i < HCP_AEE_PROC_FILE_NUM ; i++) {
		memset(info->data[i].buf, 0, info->data[i].sz);
		info->data[i].cnt = 0;
	}

	dev_dbg(hcp_dev->dev, "%s -e\n", __func__);
}

int hcp_aee_init(struct mtk_hcp *hcp_dev)
{
	struct hcp_aee_info *info = NULL;
	struct hcp_proc_data *data = NULL;
	kuid_t uid;
	kgid_t gid;
	int i = 0;

	dev_dbg(hcp_dev->dev, "%s -s\n", __func__);
	info = &hcp_dev->aee_info;

#ifdef AED_SET_EXTRA_FUNC_READY_ON_K515
	aed_set_extra_func(hcp_notify_aee);
#endif
	info->entry = proc_mkdir("mtk_img_debug", NULL);
	if (info->entry == NULL) {
		pr_info("%s: failed to create imgsys debug node\n", __func__);
		return -1;
	}

	for (i = 0 ; i < HCP_AEE_PROC_FILE_NUM; i++) {
		data = &info->data[i];
		data->sz = HCP_AEE_MAX_BUFFER_SIZE;
		mutex_init(&data->mtx);
	}

	hcp_aee_reset(hcp_dev);

	info->daemon =
		proc_create("daemon", 0660, info->entry, &aee_ops);
	info->stream =
		proc_create("stream", 0660, info->entry, &aee_ops);
	info->kernel =
		proc_create("kernel", 0660, info->entry, &aee_ops);

	uid = make_kuid(&init_user_ns, 0);
	gid = make_kgid(&init_user_ns, 1000);

	if (info->daemon)
		proc_set_user(info->daemon, uid, gid);
	else
		pr_info("%s: mtk_img_dbg/daemon: failed to set u/g", __func__);

	if (info->stream)
		proc_set_user(info->stream, uid, gid);
	else
		pr_info("%s: mtk_img_dbg/stream: failed to set u/g", __func__);

	if (info->kernel)
		proc_set_user(info->kernel, uid, gid);
	else
		pr_info("%s: mtk_img_dbg/kernel: failed to set u/g", __func__);

	dev_dbg(hcp_dev->dev, "%s - e\n", __func__);
	return 0;
}

int hcp_aee_deinit(struct mtk_hcp *hcp_dev)
{
	struct hcp_aee_info *info = &hcp_dev->aee_info;
	struct hcp_proc_data *data = NULL;
	int i = 0;

	for (i = 0 ; i < HCP_AEE_PROC_FILE_NUM; i++) {
		data = &info->data[i];
		data->sz = HCP_AEE_MAX_BUFFER_SIZE;
		mutex_destroy(&data->mtx);
	}

	if (info->kernel)
		proc_remove(info->kernel);

	if (info->daemon)
		proc_remove(info->daemon);

	if (info->stream)
		proc_remove(info->stream);

	if (info->entry)
		proc_remove(info->entry);

	return 0;
}

#if MTK_CM4_SUPPORT
static void hcp_ipi_handler(int id, void *data, unsigned int len)
{
	int hcp_id = ipi_id_to_hcp_id(id);

	if (hcp_mtkdev->hcp_desc_table[hcp_id].handler)
		hcp_mtkdev->hcp_desc_table[hcp_id].handler(data, len, NULL);
}
#endif

static void cm4_support_table_init(struct mtk_hcp *hcp)
{
	int i = 0;

	for (i = 0; i < MODULE_MAX_ID; i++)
		hcp->cm4_support_list[i] = false;

	i = 0;
	while (CM4_SUPPORT_CONFIGURE_TABLE[i][0] < MODULE_MAX_ID) {
		if (CM4_SUPPORT_CONFIGURE_TABLE[i][1] == 1)
			hcp->cm4_support_list[CM4_SUPPORT_CONFIGURE_TABLE[i][0]]
									= true;
		i++;
	}
}

static bool mtk_hcp_cm4_support(struct mtk_hcp *hcp_dev, enum hcp_id id)
{
	bool is_cm4_support = false;

	switch (id) {
	case HCP_ISP_CMD_ID:
	case HCP_ISP_FRAME_ID:
		is_cm4_support = hcp_dev->cm4_support_list[MODULE_ISP];
		break;
	case HCP_DIP_INIT_ID:
	case HCP_DIP_FRAME_ID:
	case HCP_IMGSYS_HW_TIMEOUT_ID:
	case HCP_IMGSYS_SW_TIMEOUT_ID:
		is_cm4_support = hcp_dev->cm4_support_list[MODULE_DIP];
		break;
	case HCP_FD_CMD_ID:
	case HCP_FD_FRAME_ID:
		is_cm4_support = hcp_dev->cm4_support_list[MODULE_FD];
		break;
	case HCP_RSC_INIT_ID:
	case HCP_RSC_FRAME_ID:
		is_cm4_support = hcp_dev->cm4_support_list[MODULE_RSC];
		break;
	default:
		break;
	}

	dev_dbg(hcp_dev->dev, "cm4 support status:%d\n", is_cm4_support);
	return is_cm4_support;
}

int mtk_hcp_register(struct platform_device *pdev,
				 enum hcp_id id, hcp_handler_t handler,
				 const char *name, void *priv)
{
	struct mtk_hcp *hcp_dev = platform_get_drvdata(pdev);
	unsigned int idx = 0;

	if (hcp_dev == NULL) {
		pr_info("%s hcp device in not ready\n", __func__);
		return -EPROBE_DEFER;
	}

	if (id < HCP_MAX_ID && handler != NULL) {
#if MTK_CM4_SUPPORT
		if (mtk_hcp_cm4_support(hcp_dev, id) == true) {
			int ipi_id = hcp_id_to_ipi_id(hcp_dev, id);

			scp_ipi_registration(ipi_id, hcp_ipi_handler, name);
		}
#endif
		idx = (unsigned int)id;
		hcp_dev->hcp_desc_table[idx].name = name;
		hcp_dev->hcp_desc_table[idx].handler = handler;
		hcp_dev->hcp_desc_table[idx].priv = priv;
		return 0;
	}

	dev_info(&pdev->dev, "%s register hcp id %d with invalid arguments\n",
								__func__, id);

	return -EINVAL;
}
EXPORT_SYMBOL(mtk_hcp_register);

int mtk_hcp_unregister(struct platform_device *pdev, enum hcp_id id)
{
	struct mtk_hcp *hcp_dev = platform_get_drvdata(pdev);
	unsigned int idx = (unsigned int)id;

	if (hcp_dev == NULL) {
		dev_info(&pdev->dev, "%s hcp device in not ready\n", __func__);
		return -EPROBE_DEFER;
	}

	if (idx < HCP_MAX_ID) {
		memset((void *)&hcp_dev->hcp_desc_table[idx], 0,
						sizeof(struct hcp_desc));
		return 0;
	}

	dev_info(&pdev->dev, "%s register hcp id %u with invalid arguments\n",
		__func__, idx);

	return -EINVAL;
}
EXPORT_SYMBOL(mtk_hcp_unregister);

static int hcp_send_internal(struct mtk_hcp *hcp_dev,
		 enum hcp_id id, void *buf,
		 unsigned int len, int req_fd,
		 unsigned int wait)
{
	struct share_buf send_obj = {0};
	unsigned long timeout = 0;
	unsigned long flag = 0;
	struct msg *msg = NULL;
	int ret = 0;
	unsigned int no = 0;

	dev_dbg(hcp_dev->dev, "%s id:%d len %d\n",
		__func__, id, len);

	if (id >= HCP_MAX_ID || len > sizeof(send_obj.share_data) || buf == NULL) {
		dev_info(hcp_dev->dev,
			"%s failed to send hcp message (Invalid arg.), len/sz(%d/%d)\n",
			__func__, len, sizeof(send_obj.share_data));
		return -EINVAL;
	}

	if (mtk_hcp_running(hcp_dev) == false) {
		dev_info(hcp_dev->dev, "%s hcp is not running\n", __func__);
		return -EPERM;
	}

	if (mtk_hcp_cm4_support(hcp_dev, id) == true) {
	#if MTK_CM4_SUPPORT
		int ipi_id = hcp_id_to_ipi_id(hcp_dev, id);

		dev_dbg(hcp_dev->dev, "%s cm4 is support !!!\n", __func__);
		return scp_ipi_send(ipi_id, buf, len, 0, SCP_A_ID);
	#endif
	} else {
		int module_id = hcp_id_to_module_id(hcp_dev, id);
		if (module_id < MODULE_ISP || module_id >= MODULE_MAX_ID) {
			dev_info(hcp_dev->dev, "%s invalid module id %d", __func__, module_id);
			return -EINVAL;
		}

		timeout = msecs_to_jiffies(HCP_TIMEOUT_MS);
		ret = wait_event_timeout(hcp_dev->msg_wq,
			((msg = msg_pool_get(hcp_dev)) != NULL), timeout);
		if (ret == 0) {
			dev_info(hcp_dev->dev, "%s id:%d refill time out !\n",
				__func__, id);
			return -EIO;
		} else if (-ERESTARTSYS == ret) {
			dev_info(hcp_dev->dev, "%s id:%d refill interrupted !\n",
				__func__, id);
			return -ERESTARTSYS;
		}

		if (msg == NULL) {
			dev_info(hcp_dev->dev, "%s id:%d msg poll is full!\n",
				__func__, id);
			return -EAGAIN;
		}

		atomic_set(&hcp_dev->hcp_id_ack[id], 0);

		//spin_lock_irqsave(&hcp_dev->msglock, flag);
		//msg = list_first_entry(&hcp_dev->msg_list, struct msg, entry);
		//list_del(&msg->entry);
		//spin_unlock_irqrestore(&hcp_dev->msglock, flag);

		memcpy((void *)msg->user_obj.share_data, buf, len);
		msg->user_obj.len = len;

		spin_lock_irqsave(&hcp_dev->msglock, flag);

		no = atomic_inc_return(&hcp_dev->seq);

		msg->user_obj.info.send.hcp = id;
		msg->user_obj.info.send.req = req_fd;
		msg->user_obj.info.send.seq = no;
		msg->user_obj.info.send.ack = (wait ? 1 : 0);
		msg->user_obj.id = id;

		list_add_tail(&msg->entry, &hcp_dev->chans[module_id]);
		spin_unlock_irqrestore(&hcp_dev->msglock, flag);

		wake_up(&hcp_dev->poll_wq[module_id]);

		dev_dbg(hcp_dev->dev,
			"%s frame_no_%d, message(%d)size(%d) send to user space !!!\n",
			__func__, no, id, len);

		if (id == HCP_IMGSYS_SW_TIMEOUT_ID)
			chans_pool_dump(hcp_dev);

		if (!wait)
			return 0;

		/* wait for RED's ACK */
		timeout = msecs_to_jiffies(HCP_TIMEOUT_MS);
		ret = wait_event_timeout(hcp_dev->ack_wq[module_id],
			atomic_cmpxchg(&(hcp_dev->hcp_id_ack[id]), 1, 0), timeout);
		if (ret == 0) {
			dev_info(hcp_dev->dev, "%s hcp id:%d ack time out !\n",
				__func__, id);
			/*
			* clear un-success event to prevent unexpected flow
			* cauesd be remaining data
			*/
			return -EIO;
		} else if (-ERESTARTSYS == ret) {
			dev_info(hcp_dev->dev, "%s hcp id:%d ack wait interrupted !\n",
				__func__, id);
			return -ERESTARTSYS;
		} else {
			return 0;
		}
	}
	return 0;
}

struct task_struct *mtk_hcp_get_current_task(struct platform_device *pdev)
{
	struct mtk_hcp *hcp_dev = platform_get_drvdata(pdev);

	return hcp_dev->current_task;
}
EXPORT_SYMBOL(mtk_hcp_get_current_task);

int mtk_hcp_send(struct platform_device *pdev,
		 enum hcp_id id, void *buf,
		 unsigned int len, int req_fd)
{
	struct mtk_hcp *hcp_dev = platform_get_drvdata(pdev);

	return hcp_send_internal(hcp_dev, id, buf, len, req_fd, SYNC_SEND);
}
EXPORT_SYMBOL(mtk_hcp_send);

int mtk_hcp_send_async(struct platform_device *pdev,
		 enum hcp_id id, void *buf,
		 unsigned int len, int req_fd)
{
	struct mtk_hcp *hcp_dev = platform_get_drvdata(pdev);

	return hcp_send_internal(hcp_dev, id, buf, len, req_fd, ASYNC_SEND);
}
EXPORT_SYMBOL(mtk_hcp_send_async);

int mtk_hcp_set_apu_dc(struct platform_device *pdev,
	int32_t value, size_t size)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	struct mtk_hcp *hcp_dev = platform_get_drvdata(pdev);

	struct slbc_data slb = {0};
	struct ctrl_data ctrl = {0};
	int ret = 0;

	if (value) {
		if (atomic_inc_return(&(hcp_dev->have_slb)) == 1) {
			slb.uid = UID_SH_P2;
			slb.type = TP_BUFFER;
			ret = slbc_request(&slb);
			if (ret < 0) {
				dev_info(hcp_dev->dev, "%s: Failed to allocate SLB buffer",
					__func__);
				return -1;
			}

			dev_info(hcp_dev->dev, "%s: SLB buffer base(0x%x), size(%ld): %x",
				__func__, (uintptr_t)slb.paddr, slb.size);

			ctrl.id    = CTRL_ID_SLB_BASE;
			ctrl.value = ((slb.size << 32) |
				((uintptr_t)slb.paddr & 0x0FFFFFFFFULL));

			return hcp_send_internal(hcp_dev,
				HCP_IMGSYS_SET_CONTROL_ID, &ctrl, sizeof(ctrl), 0, 0);
		}
	} else {
		if (atomic_dec_return(&(hcp_dev->have_slb)) == 0) {
			slb.uid  = UID_SH_P2;
			slb.type = TP_BUFFER;
			ret = slbc_release(&slb);
			if (ret < 0) {
				dev_info(hcp_dev->dev, "Failed to release SLB buffer");
				return -1;
			}

			ctrl.id    = CTRL_ID_SLB_BASE;
			ctrl.value = 0;

			return hcp_send_internal(hcp_dev,
				HCP_IMGSYS_SET_CONTROL_ID, &ctrl, sizeof(ctrl), 0, 0);
		}
	}
#endif
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_hcp_set_apu_dc);

struct platform_device *mtk_hcp_get_plat_device(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *hcp_node = NULL;
	struct platform_device *hcp_pdev = NULL;

	dev_dbg(&pdev->dev, "- E. hcp get platform device.\n");

	hcp_node = of_parse_phandle(dev->of_node, "mediatek,hcp", 0);
	if (hcp_node == NULL) {
		dev_info(&pdev->dev, "%s can't get hcp node.\n", __func__);
		return NULL;
	}

	hcp_pdev = of_find_device_by_node(hcp_node);
	if (WARN_ON(hcp_pdev == NULL) == true) {
		dev_info(&pdev->dev, "%s hcp pdev failed.\n", __func__);
		of_node_put(hcp_node);
		return NULL;
	}

	return hcp_pdev;
}
EXPORT_SYMBOL(mtk_hcp_get_plat_device);

#if HCP_RESERVED_MEM
phys_addr_t mtk_hcp_get_reserve_mem_phys(unsigned int id)
{
	return 0;
}
EXPORT_SYMBOL(mtk_hcp_get_reserve_mem_phys);

void mtk_hcp_set_reserve_mem_virt(unsigned int id,
	void *virmem)
{
	return;
}
EXPORT_SYMBOL(mtk_hcp_set_reserve_mem_virt);

void *mtk_hcp_get_reserve_mem_virt(unsigned int id)
{
	return NULL;
}
EXPORT_SYMBOL(mtk_hcp_get_reserve_mem_virt);

phys_addr_t mtk_hcp_get_reserve_mem_dma(unsigned int id)
{
	return 0;
}
EXPORT_SYMBOL(mtk_hcp_get_reserve_mem_dma);

phys_addr_t mtk_hcp_get_reserve_mem_size(unsigned int id)
{
	return 0;
}
EXPORT_SYMBOL(mtk_hcp_get_reserve_mem_size);

void mtk_hcp_set_reserve_mem_fd(unsigned int id, uint32_t fd)
{
	return;
}
EXPORT_SYMBOL(mtk_hcp_set_reserve_mem_fd);

uint32_t mtk_hcp_get_reserve_mem_fd(unsigned int id)
{
	return 0;
}
EXPORT_SYMBOL(mtk_hcp_get_reserve_mem_fd);

void *mtk_hcp_get_gce_mem_virt(struct platform_device *pdev)
{
	struct mtk_hcp *hcp_dev = platform_get_drvdata(pdev);
	void *buffer = NULL;

	if (!hcp_dev->data->get_gce_virt) {
		dev_info(&pdev->dev, "%s: not supported\n", __func__);
		return NULL;
	}

	buffer = hcp_dev->data->get_gce_virt();
	if (!buffer)
		dev_info(&pdev->dev, "%s: gce buffer is null\n", __func__);

	return buffer;
}
EXPORT_SYMBOL(mtk_hcp_get_gce_mem_virt);

int mtk_hcp_get_gce_buffer(struct platform_device *pdev)
{
	struct mtk_hcp *hcp_dev = platform_get_drvdata(pdev);

	if (!hcp_dev->data->get_gce) {
		dev_info(&pdev->dev, "%s:not supported\n", __func__);
		return -1;
	}

	return hcp_dev->data->get_gce();
}
EXPORT_SYMBOL(mtk_hcp_get_gce_buffer);

int mtk_hcp_put_gce_buffer(struct platform_device *pdev)
{
	struct mtk_hcp *hcp_dev = platform_get_drvdata(pdev);

	if (!hcp_dev->data->put_gce) {
		dev_info(&pdev->dev, "%s:not supported\n", __func__);
		return -1;
	}

	return hcp_dev->data->put_gce();
}
EXPORT_SYMBOL(mtk_hcp_put_gce_buffer);

void *mtk_hcp_get_hwid_mem_virt(struct platform_device *pdev)
{
	struct mtk_hcp *hcp_dev = platform_get_drvdata(pdev);

	if (!hcp_dev->data->get_hwid_virt) {
		dev_info(&pdev->dev, "%s:not supported\n", __func__);
		return NULL;
	}

	return hcp_dev->data->get_hwid_virt();
}
EXPORT_SYMBOL(mtk_hcp_get_hwid_mem_virt);

#endif

static int mtk_hcp_open(struct inode *inode, struct file *file)
{
	struct mtk_hcp *hcp_dev = NULL;

	hcp_dev = container_of(inode->i_cdev, struct mtk_hcp, hcp_cdev);
	dev_dbg(hcp_dev->dev, "open inode->i_cdev = 0x%p\n", inode->i_cdev);

	/*  */
	file->private_data = hcp_dev;

	hcp_dev->is_open = true;
	cm4_support_table_init(hcp_dev);

	hcp_dev->current_task = current;

	dev_dbg(hcp_dev->dev, "- X. hcp open.\n");

	return 0;
}

static unsigned int mtk_hcp_poll(struct file *file, poll_table *wait)
{
	struct mtk_hcp *hcp_dev = (struct mtk_hcp *)file->private_data;

	// dev_info(hcp_dev->dev, "%s: poll start+", __func__);
	if (chan_pool_available(hcp_dev, MODULE_IMG)) {
		// dev_info(hcp_dev->dev, "%s: poll start-: %d", __func__, POLLIN);
		return POLLIN;
	}

	poll_wait(file, &hcp_dev->poll_wq[MODULE_IMG], wait);
	if (chan_pool_available(hcp_dev, MODULE_IMG)) {
		// dev_info(hcp_dev->dev, "%s: poll start-: %d", __func__, POLLIN);
		return POLLIN;
	}

	// dev_info(hcp_dev->dev, "%s: poll start-: 0", __func__);
	return 0;
}

static int mtk_hcp_release(struct inode *inode, struct file *file)
{
	struct mtk_hcp *hcp_dev = (struct mtk_hcp *)file->private_data;
#ifndef CONFIG_FPGA_EARLY_PORTING
	struct slbc_data slb = {0};
	int ret = 0;
#endif

	dev_dbg(hcp_dev->dev, "- E. hcp release.\n");

	hcp_dev->is_open = false;
#ifndef CONFIG_FPGA_EARLY_PORTING
	if (atomic_read(&(hcp_dev->have_slb)) > 0) {
		slb.uid  = UID_SH_P2;
		slb.type = TP_BUFFER;
		ret = slbc_release(&slb);
		if (ret < 0) {
			dev_info(hcp_dev->dev, "Failed to release SLB buffer");
			return -1;
		}
	}
#endif

	kfree(hcp_dev->extmem.d_va);
	return 0;
}

static int mtk_hcp_mmap(struct file *file, struct vm_area_struct *vma)
{
	return -EOPNOTSUPP;
}

static void module_notify(struct mtk_hcp *hcp_dev,
					struct share_buf *user_data_addr)
{
	void *gce_buf = NULL;
	int req_fd = 0;
	struct img_sw_buffer *swbuf_data = NULL;
	struct swfrm_info_t *swfrm_info = NULL;
	struct mtk_imgsys_request *req = NULL;
	u64 *req_stat = NULL;

	if (!user_data_addr) {
		dev_info(hcp_dev->dev, "%s invalid null share buffer", __func__);
		return;
	}

	if (user_data_addr->id >= HCP_MAX_ID) {
		dev_info(hcp_dev->dev, "%s invalid hcp id %d", __func__, user_data_addr->id);
		return;
	}

	dev_dbg(hcp_dev->dev, " %s with message id:%d\n",
				__func__, user_data_addr->id);

	swbuf_data = (struct img_sw_buffer *)user_data_addr->share_data;
	if (swbuf_data && user_data_addr->id == HCP_IMGSYS_FRAME_ID) {
		if (hcp_dev->data && hcp_dev->data->get_gce_virt)
			gce_buf = hcp_dev->data->get_gce_virt();

		if (gce_buf)
			swfrm_info = (struct swfrm_info_t *)(gce_buf + (swbuf_data->offset));

		if (swfrm_info && swfrm_info->is_lastfrm)
			req = (struct mtk_imgsys_request *)swfrm_info->req_vaddr;

		if (req) {
			req_fd = req->tstate.req_fd;
			req_stat = req->req_stat;
		}

		if (req_stat) {
			*req_stat = *req_stat + 1;
			dev_dbg(hcp_dev->dev, "req:%d req_stat(%p):%llu\n",
				req_fd, req_stat, *req_stat);
		}

	}
	if (hcp_dev->hcp_desc_table[user_data_addr->id].handler) {
		hcp_dev->hcp_desc_table[user_data_addr->id].handler(
			user_data_addr->share_data,
			user_data_addr->len,
			hcp_dev->hcp_desc_table[user_data_addr->id].priv);
	}
}

static void module_wake_up(struct mtk_hcp *hcp_dev,
					struct share_buf *user_data_addr)
{
	int module_id = 0;

	if (!user_data_addr) {
		dev_info(hcp_dev->dev, "%s invalid null share buffer", __func__);
		return;
	}

	if (user_data_addr->id >= HCP_MAX_ID) {
		dev_info(hcp_dev->dev, "%s invalid hcp id %d", __func__, user_data_addr->id);
		return;
	}

	module_id = hcp_id_to_module_id(hcp_dev, user_data_addr->id);
	if (module_id < MODULE_ISP || module_id >= MODULE_MAX_ID) {
		dev_info(hcp_dev->dev, "%s invalid module id %d", __func__, module_id);
		return;
	}

	atomic_set(&hcp_dev->hcp_id_ack[user_data_addr->id], 1);
	wake_up(&hcp_dev->ack_wq[module_id]);
}

static long mtk_hcp_ioctl(struct file *file, unsigned int cmd,
							unsigned long arg)
{
	long ret = -1;
	//void *mem_priv;
	struct mtk_hcp *hcp_dev = (struct mtk_hcp *)file->private_data;
	struct share_buf buffer = {0};
	struct packet data = {0};
	unsigned int index = 0;
	struct msg *msg = NULL;
	unsigned long flag = 0;

	switch (cmd) {
	case HCP_GET_OBJECT:
		(void)copy_from_user(&data, (void *)arg, sizeof(struct packet));
		if (data.count > IPI_MAX_BUFFER_COUNT || data.count < 0) {
			dev_info(hcp_dev->dev, "Get_OBJ # of buf:%u in cmd:%d exceed %u",
				data.count, cmd, IPI_MAX_BUFFER_COUNT);
			return -EINVAL;
		}
		for (index = 0; index < IPI_MAX_BUFFER_COUNT; index++) {
			if (index >= data.count)
				break;

			if (data.buffer[index] == NULL) {
				dev_info(hcp_dev->dev, "Get_OBJ buf[%u] is NULL", index);
				return -EINVAL;
			}
			(void)copy_from_user((void *)&buffer, (void *)data.buffer[index],
				sizeof(struct share_buf));
			if (buffer.info.cmd == HCP_COMPLETE) {
				module_notify(hcp_dev, &buffer);
				module_wake_up(hcp_dev, &buffer);
			} else if (buffer.info.cmd == HCP_NOTIFY) {
				module_notify(hcp_dev, &buffer);
			} else {
				pr_info("[HCP] Unknown commands 0x%p, %d", data.buffer[index],
					buffer.info.cmd);
				return ret;
			}
		}

		index = 0;
		while (chan_pool_available(hcp_dev, MODULE_IMG)) {
			if (index >= IPI_MAX_BUFFER_COUNT)
				break;

			msg = chan_pool_get(hcp_dev, MODULE_IMG);
			if (msg != NULL) {
				ret = copy_to_user((void *)data.buffer[index++], &msg->user_obj,
					(unsigned long)sizeof(struct share_buf));

				// dev_info(hcp_dev->dev, "copy req fd(%d), obj id(%d) to user",
				//	req_fd, hcp_id);

				spin_lock_irqsave(&hcp_dev->msglock, flag);
				list_add_tail(&msg->entry, &hcp_dev->msg_list);
				spin_unlock_irqrestore(&hcp_dev->msglock, flag);
				wake_up(&hcp_dev->msg_wq);
			} else {
				dev_info(hcp_dev->dev, "can't get msg from chan_pool");
			}
		}

		put_user(index, (int32_t *)(arg + offsetof(struct packet, count)));
		ret = chan_pool_available(hcp_dev, MODULE_IMG);
		put_user(ret, (bool *)(arg + offsetof(struct packet, more)));
		//ret = mtk_hcp_get_data(hcp_dev, arg);

		ret = 0;
		break;
	case HCP_COMPLETE:
		(void)copy_from_user(&buffer, (void *)arg, sizeof(struct share_buf));
		module_notify(hcp_dev, &buffer);
		module_wake_up(hcp_dev, &buffer);
		ret = 0;
		break;
	case HCP_NOTIFY:
		(void)copy_from_user(&buffer, (void *)arg, sizeof(struct share_buf));
		module_notify(hcp_dev, &buffer);
		ret = 0;
		break;
	case HCP_WAKEUP:
		//(void)copy_from_user(&buffer, (void*)arg, sizeof(struct share_buf));
		//module_wake_up(hcp_dev, &buffer);
		wake_up(&hcp_dev->poll_wq[MODULE_IMG]);
		ret = 0;
		break;
	case HCP_TIMEOUT:
		chans_pool_dump(hcp_dev);
		ret = 0;
		break;
	default:
		dev_info(hcp_dev->dev, "Invalid cmd_number 0x%x.\n", cmd);
		break;
	}

	return ret;
}

#if IS_ENABLED(CONFIG_COMPAT)
static long mtk_hcp_compat_ioctl(struct file *file, unsigned int cmd,
							unsigned long arg)
{
	struct mtk_hcp *hcp_dev = (struct mtk_hcp *)file->private_data;
	long ret = -1;
	struct share_buf __user *share_data32 = NULL;

	switch (cmd) {
	case COMPAT_HCP_GET_OBJECT:
	case COMPAT_HCP_NOTIFY:
	case COMPAT_HCP_COMPLETE:
	case COMPAT_HCP_WAKEUP:
		share_data32 = compat_ptr((uint32_t)arg);
		ret = file->f_op->unlocked_ioctl(file,
				cmd, (unsigned long)share_data32);
		break;
	default:
		dev_info(hcp_dev->dev, "Invalid cmd_number 0x%x.\n", cmd);
		break;
	}
	return ret;
}
#endif

static const struct file_operations hcp_fops = {
	.owner          = THIS_MODULE,
	.unlocked_ioctl = mtk_hcp_ioctl,
	.open           = mtk_hcp_open,
	.poll           = mtk_hcp_poll,
	.release        = mtk_hcp_release,
	.mmap           = mtk_hcp_mmap,
#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl   = mtk_hcp_compat_ioctl,
#endif
};

int allocate_working_buffer_helper(struct platform_device *pdev)
{
	unsigned int id = 0;
	struct mtk_hcp_reserve_mblock *mblock = NULL;
	unsigned int block_num = 0;
	struct sg_table *sgt = NULL;
	struct dma_buf_attachment *attach = NULL;
	struct mtk_hcp *hcp_dev = platform_get_drvdata(pdev);
	struct dma_heap *pdma_heap = NULL;
	struct dma_buf_map map = {0};
	int ret = 0;

	mblock = hcp_dev->data->mblock;
	block_num = hcp_dev->data->block_num;

	/* allocate reserved memory */
	for (id = 0; id < block_num; id++) {
		if (mblock[id].is_dma_buf) {
			switch (id) {
			default:

				/* all supported heap name you can find with cmd */
				/* (ls /dev/dma_heap/) in shell */
				pdma_heap = dma_heap_find("mtk_mm-uncached");
				if (!pdma_heap) {
					pr_info("pdma_heap find fail\n");
					return -1;
				}
				mblock[id].d_buf = dma_heap_buffer_alloc(
					pdma_heap,
					mblock[id].size,
					O_RDWR | O_CLOEXEC,
					DMA_HEAP_VALID_HEAP_FLAGS);
				if (IS_ERR(mblock[id].d_buf)) {
					pr_info("dma_heap_buffer_alloc fail :%lld\n",
					PTR_ERR(mblock[id].d_buf));
					return -1;
				}
				mtk_dma_buf_set_name(mblock[id].d_buf, mblock[id].name);
				mblock[id].attach =
					dma_buf_attach(mblock[id].d_buf, hcp_dev->dev);
				attach = mblock[id].attach;
				if (IS_ERR(attach)) {
					pr_info("dma_buf_attach fail :%lld\n",
					PTR_ERR(attach));
					return -1;
				}

				mblock[id].sgt = dma_buf_map_attachment(attach, DMA_TO_DEVICE);
				sgt = mblock[id].sgt;
				if (IS_ERR(sgt)) {
					dma_buf_detach(mblock[id].d_buf, attach);
					pr_info("dma_buf_map_attachment fail sgt:%lld\n",
					PTR_ERR(sgt));
					return -1;
				}
				mblock[id].start_phys = sg_dma_address(sgt->sgl);
				mblock[id].start_dma = mblock[id].start_phys;
				ret = dma_buf_vmap(mblock[id].d_buf, &map);
				if (ret) {
					pr_info("sg_dma_address fail\n");
					return ret;
				}
				mblock[id].start_virt = (void *)map.vaddr;
				get_dma_buf(mblock[id].d_buf);
				mblock[id].fd =
					dma_buf_fd(mblock[id].d_buf, O_RDWR | O_CLOEXEC);
				break;
			}
		} else {
			mblock[id].start_virt =
				kzalloc(mblock[id].size,
					GFP_KERNEL);
			mblock[id].start_phys =
				virt_to_phys(
					mblock[id].start_virt);
			mblock[id].start_dma = 0;
		}
	}

	return 0;
}
EXPORT_SYMBOL(allocate_working_buffer_helper);

int release_working_buffer_helper(struct platform_device *pdev)
{
	unsigned int id = 0;
	struct mtk_hcp_reserve_mblock *mblock = NULL;
	unsigned int block_num = 0;
	struct mtk_hcp *hcp_dev = platform_get_drvdata(pdev);

	mblock = hcp_dev->data->mblock;
	block_num = hcp_dev->data->block_num;

	/* release reserved memory */
	for (id = 0; id < block_num; id++) {
		if (mblock[id].is_dma_buf) {
			switch (id) {
			default:
				/* free va */
				dma_buf_vunmap(mblock[id].d_buf,
				mblock[id].start_virt);
				/* free iova */
				dma_buf_unmap_attachment(mblock[id].attach,
				mblock[id].sgt, DMA_TO_DEVICE);
				dma_buf_detach(mblock[id].d_buf,
				mblock[id].attach);
				dma_buf_put(mblock[id].d_buf);
				// close fd in user space driver, you can't close fd in kernel site
				// dma_heap_buffer_free(mblock[id].d_buf);
				//dma_buf_put(my_dma_buf);
				//also can use this api, but not recommended
				mblock[id].mem_priv = NULL;
				mblock[id].mmap_cnt = 0;
				mblock[id].start_dma = 0x0;
				mblock[id].start_virt = 0x0;
				mblock[id].start_phys = 0x0;
				mblock[id].d_buf = NULL;
				mblock[id].fd = -1;
				mblock[id].pIonHandle = NULL;
				mblock[id].attach = NULL;
				mblock[id].sgt = NULL;
				break;
			}
		} else {
			kfree(mblock[id].start_virt);
			mblock[id].start_virt = 0x0;
			mblock[id].start_phys = 0x0;
			mblock[id].start_dma = 0x0;
			mblock[id].mmap_cnt = 0;
		}
	}

	return 0;
}
EXPORT_SYMBOL(release_working_buffer_helper);

int mtk_hcp_allocate_working_buffer(struct platform_device *pdev, unsigned int mode)
{
	struct mtk_hcp *hcp_dev = platform_get_drvdata(pdev);

	if ((hcp_dev == NULL)
		|| (hcp_dev->data == NULL)
		|| (hcp_dev->data->allocate == NULL)) {
		dev_info(&pdev->dev, "%s:allocate not supported\n", __func__);
		return allocate_working_buffer_helper(pdev);
	}

	return hcp_dev->data->allocate(hcp_dev, mode);
}
EXPORT_SYMBOL(mtk_hcp_allocate_working_buffer);

int mtk_hcp_release_working_buffer(struct platform_device *pdev)
{
	struct mtk_hcp *hcp_dev = platform_get_drvdata(pdev);

	if ((hcp_dev == NULL)
		|| (hcp_dev->data == NULL)
		|| (hcp_dev->data->allocate == NULL)) {
		dev_info(&pdev->dev, "%s:release not supported\n", __func__);
		return release_working_buffer_helper(pdev);
	}

	return hcp_dev->data->release(hcp_dev);
}
EXPORT_SYMBOL(mtk_hcp_release_working_buffer);

int mtk_hcp_get_init_info(struct platform_device *pdev,
			struct img_init_info *info)
{
	struct mtk_hcp *hcp_dev = platform_get_drvdata(pdev);

	if ((hcp_dev == NULL)
		|| (hcp_dev->data == NULL)
		|| (hcp_dev->data->get_init_info == NULL)
		|| (info == NULL)) {
		dev_info(&pdev->dev, "%s:not supported\n", __func__);
		return -1;
	}

	return hcp_dev->data->get_init_info(info);
}
EXPORT_SYMBOL(mtk_hcp_get_init_info);

void mtk_hcp_purge_msg(struct platform_device *pdev)
{
	struct mtk_hcp *hcp_dev = platform_get_drvdata(pdev);
	unsigned long flag = 0;
	int i = 0;
	struct msg *msg = NULL;
	struct msg *tmp = NULL;

	spin_lock_irqsave(&hcp_dev->msglock, flag);
	for (i = 0; i < MODULE_MAX_ID; i++) {
		list_for_each_entry_safe(msg, tmp,
					&hcp_dev->chans[i], entry){
			list_del(&msg->entry);
			list_add_tail(&msg->entry, &hcp_dev->msg_list);
		}
	}
	atomic_set(&hcp_dev->seq, 0);
	spin_unlock_irqrestore(&hcp_dev->msglock, flag);
}
EXPORT_SYMBOL(mtk_hcp_purge_msg);

static int mtk_hcp_probe(struct platform_device *pdev)
{
	struct mtk_hcp *hcp_dev = NULL;
	struct msg *msgs = NULL;
	int ret = 0;
	int i = 0;

	dev_info(&pdev->dev, "- E. hcp driver probe.\n");
	hcp_dev = devm_kzalloc(&pdev->dev, sizeof(*hcp_dev), GFP_KERNEL);
	if (hcp_dev == NULL)
		return -ENOMEM;

	hcp_mtkdev = hcp_dev;
	hcp_dev->dev = &pdev->dev;
	hcp_dev->mem_ops = &vb2_dma_contig_memops;

	hcp_dev->data = of_device_get_match_data(&pdev->dev);

	platform_set_drvdata(pdev, hcp_dev);
	dev_set_drvdata(&pdev->dev, hcp_dev);

	if (dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(34)))
		dev_info(&pdev->dev, "%s:No DMA available\n", __func__);

	if (!pdev->dev.dma_parms) {
		pdev->dev.dma_parms =
			devm_kzalloc(hcp_dev->dev, sizeof(*hcp_dev->dev->dma_parms), GFP_KERNEL);
	}
	if (hcp_dev->dev->dma_parms) {
		ret = dma_set_max_seg_size(hcp_dev->dev, (unsigned int)DMA_BIT_MASK(34));
		if (ret)
			dev_info(hcp_dev->dev, "Failed to set DMA segment size\n");
	}

	atomic_set(&(hcp_dev->have_slb), 0);

	hcp_dev->is_open = false;
	for (i = 0; i < MODULE_MAX_ID; i++) {
		init_waitqueue_head(&hcp_dev->ack_wq[i]);
		init_waitqueue_head(&hcp_dev->poll_wq[i]);

		INIT_LIST_HEAD(&hcp_dev->chans[i]);

		switch (i) {
		case MODULE_ISP:
			hcp_dev->daemon_notify_wq[i] =
				alloc_ordered_workqueue("%s",
				__WQ_LEGACY | WQ_MEM_RECLAIM |
				WQ_FREEZABLE,
				"isp_daemon_notify_wq");
			break;
		case MODULE_IMG:
			hcp_dev->daemon_notify_wq[i] =
				alloc_ordered_workqueue("%s",
				__WQ_LEGACY | WQ_MEM_RECLAIM |
				WQ_FREEZABLE,
				"imgsys_daemon_notify_wq");
			break;
		case MODULE_FD:
			hcp_dev->daemon_notify_wq[i] =
				alloc_ordered_workqueue("%s",
				__WQ_LEGACY | WQ_MEM_RECLAIM |
				WQ_FREEZABLE,
				"fd_daemon_notify_wq");
			break;
		case MODULE_RSC:
			hcp_dev->daemon_notify_wq[i] =
				alloc_ordered_workqueue("%s",
				__WQ_LEGACY | WQ_MEM_RECLAIM |
				WQ_FREEZABLE,
				"rsc_daemon_notify_wq");
			break;
		default:
			hcp_dev->daemon_notify_wq[i] = NULL;
			break;
		}
	}
	spin_lock_init(&hcp_dev->msglock);
	init_waitqueue_head(&hcp_dev->msg_wq);
	INIT_LIST_HEAD(&hcp_dev->msg_list);
	msgs = devm_kzalloc(hcp_dev->dev, sizeof(*msgs) * MSG_NR, GFP_KERNEL);
	for (i = 0; i < MSG_NR; i++)
		list_add_tail(&msgs[i].entry, &hcp_dev->msg_list);

	/* init character device */

	ret = alloc_chrdev_region(&hcp_dev->hcp_devno, 0, 1, HCP_DEVNAME);
	if (ret < 0) {
		dev_info(&pdev->dev, "alloc_chrdev_region failed err= %d", ret);
		goto err_alloc;
	}

	cdev_init(&hcp_dev->hcp_cdev, &hcp_fops);
	hcp_dev->hcp_cdev.owner = THIS_MODULE;

	ret = cdev_add(&hcp_dev->hcp_cdev, hcp_dev->hcp_devno, 1);
	if (ret < 0) {
		dev_info(&pdev->dev, "cdev_add fail  err= %d", ret);
		goto err_add;
	}

	hcp_dev->hcp_class = class_create(THIS_MODULE, "mtk_hcp_driver");
	if (IS_ERR(hcp_dev->hcp_class) == true) {
		ret = (int)PTR_ERR(hcp_dev->hcp_class);
		dev_info(&pdev->dev, "class create fail  err= %d", ret);
		goto err_add;
	}

	hcp_dev->hcp_device = device_create(hcp_dev->hcp_class, NULL,
					hcp_dev->hcp_devno, NULL, HCP_DEVNAME);
	if (IS_ERR(hcp_dev->hcp_device) == true) {
		ret = (int)PTR_ERR(hcp_dev->hcp_device);
		dev_info(&pdev->dev, "device create fail  err= %d", ret);
		goto err_device;
	}

	hcp_aee_init(hcp_mtkdev);
	dev_dbg(&pdev->dev, "hcp aee init done\n");
	dev_dbg(&pdev->dev, "- X. hcp driver probe success.\n");

#if HCP_RESERVED_MEM
	/* allocate reserved memory */
	/* allocate shared memory about ipi_param at probe, allocate others at streamon */
		/* update size to be the same with dts */
	/* mtk_hcp_reserve_mblock[IMG_MEM_FOR_HW_ID].start_virt = */
	/* ioremap_wc(rmem_base_phys, rmem_size); */
	/* mtk_hcp_reserve_mblock[IMG_MEM_FOR_HW_ID].start_phys = */
	/* (phys_addr_t)rmem_base_phys; */
	/* mtk_hcp_reserve_mblock[IMG_MEM_FOR_HW_ID].start_dma = */
	/* (phys_addr_t)rmem_base_phys; */
	/* mtk_hcp_reserve_mblock[IMG_MEM_FOR_HW_ID].size = rmem_size; */
#endif
	return 0;

err_device:
	class_destroy(hcp_dev->hcp_class);
err_add:
	cdev_del(&hcp_dev->hcp_cdev);
err_alloc:
	unregister_chrdev_region(hcp_dev->hcp_devno, 1);
	for (i = 0; i < MODULE_MAX_ID; i++) {
		if (hcp_dev->daemon_notify_wq[i]) {
			destroy_workqueue(hcp_dev->daemon_notify_wq[i]);
			hcp_dev->daemon_notify_wq[i] = NULL;
		}
	}

	devm_kfree(&pdev->dev, hcp_dev);

	dev_info(&pdev->dev, "- X. hcp driver probe fail.\n");

	return ret;
}

static const struct of_device_id mtk_hcp_match[] = {
	{.compatible = "mediatek,hcp", .data = (void *)&isp71_hcp_data},
	{.compatible = "mediatek,hcp7s", .data = (void *)&isp7s_hcp_data},
	{}
};
MODULE_DEVICE_TABLE(of, mtk_hcp_match);

static int mtk_hcp_remove(struct platform_device *pdev)
{

	struct mtk_hcp *hcp_dev = platform_get_drvdata(pdev);
	int i = 0;

	dev_dbg(&pdev->dev, "- E. hcp driver remove.\n");

	hcp_aee_deinit(hcp_dev);

	for (i = 0; i < MODULE_MAX_ID; i++) {
		if (hcp_dev->daemon_notify_wq[i]) {
			flush_workqueue(hcp_dev->daemon_notify_wq[i]);
			destroy_workqueue(hcp_dev->daemon_notify_wq[i]);
			hcp_dev->daemon_notify_wq[i] = NULL;
		}
	}

	if (hcp_dev->is_open == true) {
		hcp_dev->is_open = false;
		dev_dbg(&pdev->dev, "%s: opened device found\n", __func__);
	}
	devm_kfree(&pdev->dev, hcp_dev);

	cdev_del(&hcp_dev->hcp_cdev);
	unregister_chrdev_region(hcp_dev->hcp_devno, 1);


	dev_dbg(&pdev->dev, "- X. hcp driver remove.\n");
	return 0;
}

static struct platform_driver mtk_hcp_driver = {
	.probe  = mtk_hcp_probe,
	.remove = mtk_hcp_remove,
	.driver = {
		.name = HCP_DEVNAME,
		.owner  = THIS_MODULE,
		.of_match_table = mtk_hcp_match,
	},
};

module_platform_driver(mtk_hcp_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Mediatek hetero control process driver");
