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
//#include <asm/dma-contiguous.h>
#include <linux/videodev2.h>
#include "mem/hcp_videobuf2-dma-contig.h"

#include "mtk-hcp.h"
#include "mtk-hcp-support.h"
#include "mtk-hcp_isp71.h"


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

#include "mtk_heap.h"
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
/*
 * define magic number for reserved memory to use in mmap function.
 */
#define START_ISP_MEM_ADDR        0x12345000
#define START_DIP_MEM_FOR_HW_ADDR 0x12346000
#define START_MDP_MEM_ADDR        0x12347000
#define START_FD_MEM_ADDR         0x12348000
#define START_DIP_MEM_FOR_SW_ADDR 0x12349000

/*
 * define module register mmap address
 */
#define ISP_UNI_A_BASE_HW 0x1A003000
#define ISP_A_BASE_HW           0x1A004000
#define ISP_B_BASE_HW           0x1A006000
#define ISP_C_BASE_HW           0x1A008000
#define DIP_BASE_HW             0x15021000
#define FD_BASE_HW              0x1502B000

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
	uintptr_t value;
} __attribute__ ((__packed__));

#define HCP_INIT                _IOWR('H', 0, struct share_buf)
#define HCP_GET_OBJECT          _IOWR('H', 1, struct share_buf)
#define HCP_NOTIFY              _IOWR('H', 2, struct share_buf)
#define HCP_COMPLETE            _IOWR('H', 3, struct share_buf)
#define HCP_WAKEUP              _IOWR('H', 4, struct share_buf)
#define HCP_TIMEOUT             _IO('H', 5)

#define COMPAT_HCP_INIT         _IOWR('H', 0, struct share_buf)
#define COMPAT_HCP_GET_OBJECT   _IOWR('H', 1, struct share_buf)
#define COMPAT_HCP_NOTIFY       _IOWR('H', 2, struct share_buf)
#define COMPAT_HCP_COMPLETE     _IOWR('H', 3, struct share_buf)
#define COMPAT_HCP_WAKEUP       _IOWR('H', 4, struct share_buf)
#define COMPAT_HCP_TIMEOUT      _IO('H', 5)

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

static struct msg *msg_pool_get(struct mtk_hcp *hcp_dev)
{
	unsigned long flag, empty;
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
	unsigned long flag;
	struct msg *msg, *tmp;
	int i = 0, seq_id, req_fd, hcp_id;

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
	unsigned long flag, empty;
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
	unsigned long flag, empty;

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
		unsigned int idx = (unsigned int)id;

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

	if (hcp_dev == NULL) {
		dev_info(&pdev->dev, "%s hcp device in not ready\n", __func__);
		return -EPROBE_DEFER;
	}

	if (id < HCP_MAX_ID) {
		memset((void *)&hcp_dev->hcp_desc_table[id], 0,
						sizeof(struct hcp_desc));
		return 0;
	}

	dev_info(&pdev->dev, "%s register hcp id %d with invalid arguments\n",
								__func__, id);

	return -EINVAL;
}
EXPORT_SYMBOL(mtk_hcp_unregister);

static int hcp_send_internal(struct platform_device *pdev,
		 enum hcp_id id, void *buf,
		 unsigned int len, int req_fd,
		 unsigned int wait)
{
	struct mtk_hcp *hcp_dev = platform_get_drvdata(pdev);
	struct share_buf send_obj;
	unsigned long timeout, flag;
	struct msg *msg;
	int ret = 0;
	unsigned int no;

	dev_dbg(&pdev->dev, "%s id:%d len %d\n",
				__func__, id, len);

	if (id >= HCP_MAX_ID || len > sizeof(send_obj.share_data) || buf == NULL) {
		dev_info(&pdev->dev,
			"%s failed to send hcp message (Invalid arg.), len/sz(%d/%d)\n",
			__func__, len, sizeof(send_obj.share_data));
		return -EINVAL;
	}

	if (mtk_hcp_running(hcp_dev) == false) {
		dev_info(&pdev->dev, "%s hcp is not running\n", __func__);
		return -EPERM;
	}

	if (mtk_hcp_cm4_support(hcp_dev, id) == true) {
#if MTK_CM4_SUPPORT
		int ipi_id = hcp_id_to_ipi_id(hcp_dev, id);

		dev_dbg(&pdev->dev, "%s cm4 is support !!!\n", __func__);
		return scp_ipi_send(ipi_id, buf, len, 0, SCP_A_ID);
#endif
	} else {
		int module_id = hcp_id_to_module_id(hcp_dev, id);
		if (module_id < MODULE_ISP || module_id >= MODULE_MAX_ID) {
			dev_info(&pdev->dev, "%s invalid module id %d", __func__, module_id);
			return -EINVAL;
		}

		timeout = msecs_to_jiffies(HCP_TIMEOUT_MS);
		ret = wait_event_timeout(hcp_dev->msg_wq,
			((msg = msg_pool_get(hcp_dev)) != NULL), timeout);
		if (ret == 0) {
			dev_info(&pdev->dev, "%s id:%d refill time out !\n",
				__func__, id);
			return -EIO;
		} else if (-ERESTARTSYS == ret) {
			dev_info(&pdev->dev, "%s id:%d refill interrupted !\n",
				__func__, id);
			return -ERESTARTSYS;
		}

		if (msg == NULL) {
			dev_info(&pdev->dev, "%s id:%d msg poll is full!\n",
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

		dev_dbg(&pdev->dev,
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
			dev_info(&pdev->dev, "%s hcp id:%d ack time out !\n",
				__func__, id);
			/*
			 * clear un-success event to prevent unexpected flow
			 * cauesd be remaining data
			 */
			return -EIO;
		} else if (-ERESTARTSYS == ret) {
			dev_info(&pdev->dev, "%s hcp id:%d ack wait interrupted !\n",
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
	return hcp_send_internal(pdev, id, buf, len, req_fd, SYNC_SEND);
}
EXPORT_SYMBOL(mtk_hcp_send);

int mtk_hcp_send_async(struct platform_device *pdev,
		 enum hcp_id id, void *buf,
		 unsigned int len, int req_fd)
{
	return hcp_send_internal(pdev, id, buf, len, req_fd, ASYNC_SEND);
}
EXPORT_SYMBOL(mtk_hcp_send_async);

int mtk_hcp_set_apu_dc(struct platform_device *pdev,
	int32_t value, size_t size)
{
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

			dev_dbg(hcp_dev->dev, "%s: SLB buffer base(0x%x), size(%ld): %x",
				__func__, (uintptr_t)slb.paddr, slb.size);

			ctrl.id    = CTRL_ID_SLB_BASE;
			ctrl.value = ((slb.size << 32) |
				((uintptr_t)slb.paddr & 0x0FFFFFFFFULL));

			return hcp_send_internal(pdev,
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

			return hcp_send_internal(pdev,
				HCP_IMGSYS_SET_CONTROL_ID, &ctrl, sizeof(ctrl), 0, 0);
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_hcp_set_apu_dc);

struct platform_device *mtk_hcp_get_plat_device(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *hcp_node;
	struct platform_device *hcp_pdev;

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
phys_addr_t mtk_hcp_mem_base_phys;
phys_addr_t mtk_hcp_mem_base_virt;
phys_addr_t mtk_hcp_mem_size;

/*static */struct mtk_hcp_reserve_mblock mtk_hcp_reserve_mblock[] = {
	/* NEED_LEGACY_MEM not defined */
	#ifdef NEED_LEGACY_MEM
	{
		.num = ISP_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x200000,  /*need 20MB*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
	{
		.num = DIP_MEM_FOR_HW_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x400000,   /*need more than 4MB*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
	{
		.num = DIP_MEM_FOR_SW_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x100000,   /*1MB*/
		.is_dma_buf = false, /* Kurt ToDo: shall be false */
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
	{
		.num = MDP_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x400000,   /*4MB*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
	{
		.num = FD_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x100000,   /*1MB*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL

	},
	#else
	{
		/*share buffer for frame setting, to be sw usage*/
		.num = IMG_MEM_FOR_HW_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x400000,   /*need more than 4MB*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
	#endif
	{
		.num = WPE_MEM_C_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x100000,   /*1MB*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
	{
		.num = WPE_MEM_T_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x100000,   /*1MB*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
	{
		.num = TRAW_MEM_C_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x400000,   /*4MB*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
	{
		.num = TRAW_MEM_T_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x1400000,   /*20MB*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
	{
		.num = DIP_MEM_C_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x1700000,   /*23MB*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
	{
		.num = DIP_MEM_T_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x1D00000,   /*29MB*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
	{
		.num = PQDIP_MEM_C_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x300000,   /*3MB*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
	{
		.num = PQDIP_MEM_T_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x200000,   /*2MB*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
	{
		.num = ADL_MEM_C_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x100000,   /*1MB*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL
	},
	{
		.num = ADL_MEM_T_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x100000,   /*1MB*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL
	},
	{
		.num = IMG_MEM_G_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x1188000,   /*15MB GCE + 2MB TPIPE + 30KB BW*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
};

#ifdef NEED_LEGACY_MEM
int mtk_hcp_reserve_mem_of_init(struct reserved_mem *rmem)
{
	unsigned int id;
	phys_addr_t accumlate_memory_size = 0;

	mtk_hcp_mem_base_phys = (phys_addr_t) rmem->base;
	mtk_hcp_mem_size = (phys_addr_t) rmem->size;

	pr_debug("[HCP] phys:0x%llx - 0x%llx (0x%llx)\n",
			(phys_addr_t)rmem->base,
			(phys_addr_t)rmem->base + (phys_addr_t)rmem->size,
						(phys_addr_t)rmem->size);
	accumlate_memory_size = 0;
	for (id = 0; id < NUMS_MEM_ID; id++) {
		mtk_hcp_reserve_mblock[id].start_phys =
			mtk_hcp_mem_base_phys + accumlate_memory_size;
		accumlate_memory_size += mtk_hcp_reserve_mblock[id].size;
		pr_debug(
		"[HCP][reserve_mem:%d]: phys:0x%llx - 0x%llx (0x%llx)\n", id,
			mtk_hcp_reserve_mblock[id].start_phys,
			mtk_hcp_reserve_mblock[id].start_phys +
			mtk_hcp_reserve_mblock[id].size,
			mtk_hcp_reserve_mblock[id].size);
	}
	return 0;
}
RESERVEDMEM_OF_DECLARE(mtk_hcp_reserve_mem_init, MTK_HCP_MEM_RESERVED_KEY,
						mtk_hcp_reserve_mem_of_init);


static int mtk_hcp_reserve_memory_ioremap(void)
{
	unsigned int id;
	phys_addr_t accumlate_memory_size;

	accumlate_memory_size = 0;

	mtk_hcp_mem_base_virt =
			(phys_addr_t)(size_t)ioremap_wc(mtk_hcp_mem_base_phys,
							mtk_hcp_mem_size);
	pr_debug("[HCP]reserve mem: virt:0x%llx - 0x%llx (0x%llx)\n",
		(phys_addr_t)mtk_hcp_mem_base_virt,
		(phys_addr_t)mtk_hcp_mem_base_virt +
						(phys_addr_t)mtk_hcp_mem_size,
		mtk_hcp_mem_size);
	for (id = 0; id < NUMS_MEM_ID; id++) {
		mtk_hcp_reserve_mblock[id].start_virt =
			mtk_hcp_mem_base_virt + accumlate_memory_size;
		accumlate_memory_size += mtk_hcp_reserve_mblock[id].size;
	}
	/* the reserved memory should be larger then expected memory
	 * or vpu_reserve_mblock does not match dts
	 */
	WARN_ON(accumlate_memory_size > mtk_hcp_mem_size);
#ifdef DEBUG
	for (id = 0; id < NUMS_MEM_ID; id++) {
		pr_info("[HCP][mem_reserve-%d] phys:0x%llx,virt:0x%llx,size:0x%llx\n",
			id, mtk_hcp_get_reserve_mem_phys(id),
			mtk_hcp_get_reserve_mem_virt(id),
			mtk_hcp_get_reserve_mem_size(id));
	}
#endif
	return 0;
}
#endif

phys_addr_t mtk_hcp_get_reserve_mem_phys(unsigned int id)
{
	if (id >= NUMS_MEM_ID) {
		pr_info("[HCP] no reserve memory for %d", id);
		return 0;
	} else {
		return mtk_hcp_reserve_mblock[id].start_phys;
	}
}
EXPORT_SYMBOL(mtk_hcp_get_reserve_mem_phys);

void mtk_hcp_set_reserve_mem_virt(unsigned int id,
	void *virmem)
{
	if (id >= NUMS_MEM_ID)
		pr_info("[HCP] no reserve memory for %d in set_reserve_mem_virt", id);
	else
		mtk_hcp_reserve_mblock[id].start_virt = virmem;
}
EXPORT_SYMBOL(mtk_hcp_set_reserve_mem_virt);

void *mtk_hcp_get_reserve_mem_virt(unsigned int id)
{
	if (id >= NUMS_MEM_ID) {
		pr_info("[HCP] no reserve memory for %d", id);
		return 0;
	} else
		return mtk_hcp_reserve_mblock[id].start_virt;
}
EXPORT_SYMBOL(mtk_hcp_get_reserve_mem_virt);

phys_addr_t mtk_hcp_get_reserve_mem_dma(unsigned int id)
{
	if (id >= NUMS_MEM_ID) {
		pr_info("[HCP] no reserve memory for %d", id);
		return 0;
	} else {
		return mtk_hcp_reserve_mblock[id].start_dma;
	}
}
EXPORT_SYMBOL(mtk_hcp_get_reserve_mem_dma);

phys_addr_t mtk_hcp_get_reserve_mem_size(unsigned int id)
{
	if (id >= NUMS_MEM_ID) {
		pr_info("[HCP] no reserve memory for %d", id);
		return 0;
	} else {
		return mtk_hcp_reserve_mblock[id].size;
	}
}
EXPORT_SYMBOL(mtk_hcp_get_reserve_mem_size);

void mtk_hcp_set_reserve_mem_fd(unsigned int id, uint32_t fd)
{
	if (id >= NUMS_MEM_ID)
		pr_info("[HCP] no reserve memory for %d", id);
	else
		mtk_hcp_reserve_mblock[id].fd = fd;
}
EXPORT_SYMBOL(mtk_hcp_set_reserve_mem_fd);

uint32_t mtk_hcp_get_reserve_mem_fd(unsigned int id)
{
	if (id >= NUMS_MEM_ID) {
		pr_info("[HCP] no reserve memory for %d", id);
		return 0;
	} else
		return mtk_hcp_reserve_mblock[id].fd;
}
EXPORT_SYMBOL(mtk_hcp_get_reserve_mem_fd);

void *mtk_hcp_get_gce_mem_virt(struct platform_device *pdev)
{
	struct mtk_hcp *hcp_dev = platform_get_drvdata(pdev);
	void *buffer;

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

phys_addr_t mtk_hcp_get_gce_mem_size(struct platform_device *pdev)
{
	struct mtk_hcp *hcp_dev = platform_get_drvdata(pdev);
	phys_addr_t mem_sz;

	if (!hcp_dev->data->get_gce_mem_size) {
		dev_info(&pdev->dev, "%s: not supported\n", __func__);
		return 0;
	}

	mem_sz = hcp_dev->data->get_gce_mem_size();

	return mem_sz;
}
EXPORT_SYMBOL(mtk_hcp_get_gce_mem_size);

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
	struct mtk_hcp *hcp_dev;

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
	struct slbc_data slb;
	int ret;

	dev_dbg(hcp_dev->dev, "- E. hcp release.\n");

	hcp_dev->is_open = false;

	if (atomic_read(&(hcp_dev->have_slb)) > 0) {
		slb.uid  = UID_SH_P2;
		slb.type = TP_BUFFER;
		ret = slbc_release(&slb);
		if (ret < 0) {
			dev_info(hcp_dev->dev, "Failed to release SLB buffer");
			return -1;
		}
	}

	kfree(hcp_dev->extmem.d_va);
	return 0;
}

static int mtk_hcp_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct mtk_hcp *hcp_dev = (struct mtk_hcp *)file->private_data;
	int reserved_memory_id = -1;
	long length = 0;
	unsigned long pfn = 0x0;
	int mem_id = 0;
	struct mtk_hcp_reserve_mblock *mblock = NULL;

	/* dealing with register remap */
	length = vma->vm_end - vma->vm_start;
	dev_dbg(hcp_dev->dev,
		"start:0x%llx end:0x%llx offset:0x%llx, legth:0x%llx",
		vma->vm_start, vma->vm_end, vma->vm_pgoff, length);
	/*  */
	pfn = vma->vm_pgoff << PAGE_SHIFT;
	switch (pfn) {
#if HCP_RESERVED_MEM
	#ifdef NEED_LEGACY_MEM
	case START_ISP_MEM_ADDR:
		reserved_memory_id = ISP_MEM_ID;
		break;
	case START_MDP_MEM_ADDR:
		reserved_memory_id = MDP_MEM_ID;
		break;
	case START_FD_MEM_ADDR:
		reserved_memory_id = FD_MEM_ID;
		break;
	#endif
	default:
		for (mem_id = 0; mem_id < NUMS_MEM_ID; mem_id++) {
			if (pfn == mtk_hcp_reserve_mblock[mem_id].start_phys) {
				reserved_memory_id = mem_id;
				break;
			}
		}
		break;
#endif
	}

	if (reserved_memory_id < 0 || reserved_memory_id >= NUMS_MEM_ID) {
		dev_info(hcp_dev->dev, " %s invalid reserved memory id %d", __func__,
			reserved_memory_id);
		return -EPERM;
	}

	mblock = &mtk_hcp_reserve_mblock[reserved_memory_id];
	if ((length <= 0) || (length != (long)mblock->size)) {
		dev_info(hcp_dev->dev,
				" %s size is not allowed: id:%d, pfn:0x%llx, length: 0x%lx != 0x%lx\n",
				__func__, reserved_memory_id, pfn, length,
				mblock->size);
		return -EPERM;
	}

	if (pfn) {
		mblock->mmap_cnt += 1;
		dev_dbg(hcp_dev->dev, "reserved_memory_id:%d, pfn:0x%llx, mmap_cnt:%d\n",
			reserved_memory_id, pfn,
			mblock->mmap_cnt);

		if (reserved_memory_id != -1) {
			vma->vm_pgoff =
				(unsigned long)(mblock->start_phys >> PAGE_SHIFT);
			vma->vm_page_prot =
			pgprot_writecombine(vma->vm_page_prot);

			switch (reserved_memory_id) {
			case IMG_MEM_FOR_HW_ID:
				vma->vm_pgoff =
					(unsigned long)(mblock->start_phys >> PAGE_SHIFT);
				vma->vm_page_prot =
						pgprot_writecombine(vma->vm_page_prot);
				break;
			case IMG_MEM_G_ID:
			default:
				hcp_dev->mem_ops->mmap(mblock->mem_priv, vma);
				pr_info("%s: [HCP][%d] after mem_ops->mmap vb2_dc_buf refcount(%d)\n",
					__func__, reserved_memory_id,
					hcp_dev->mem_ops->num_users(mblock->mem_priv));
				goto dma_buf_out;
			}
		} else {
			/*vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);*/
			dev_info(hcp_dev->dev,
				" %s ONLY SUPPORT MMAP FOR RESERVED MEM: id:%d, pfn:0x%llx !\n",
				__func__, reserved_memory_id, pfn);
			return -EPERM;
		}
		goto remap;
	} else {
		dev_info(hcp_dev->dev,
				" %s wrong pfn: id:%d, pfn:0x%llx !\n",
				__func__, reserved_memory_id, pfn);
		return -EPERM;
	}

	/* dealing with share memory, not supported */
	#ifdef EXTMEM_SUPPORT
	hcp_dev->extmem.d_va = kmalloc(length, GFP_KERNEL);
	if (hcp_dev->extmem.d_va == NULL) {
		int ret;

		ret = -ENOMEM;
		dev_info(hcp_dev->dev, " %s Allocate share buffer fail !!!\n",
								__func__);
		return ret;
	}

	hcp_dev->extmem.d_pa = virt_to_phys(hcp_dev->extmem.d_va);
	vma->vm_pgoff = (unsigned long) (hcp_dev->extmem.d_pa >> PAGE_SHIFT);
	dev_info(hcp_dev->dev, "sharememory va:0x%p pa:0x%llx",
		hcp_dev->extmem.d_va, hcp_dev->extmem.d_pa);
		#endif
remap:
	dev_dbg(hcp_dev->dev, "remap info id(%d) start:0x%llx pgoff:0x%llx page_prot:0x%llx length:0x%llx",
		reserved_memory_id, vma->vm_start, vma->vm_pgoff, vma->vm_page_prot, length);
	if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
		length, vma->vm_page_prot) != 0) {
		dev_info(hcp_dev->dev, " %s remap_pfn_range fail !!!\n",
								__func__);
		return -EAGAIN;
	}
dma_buf_out:
	return 0;
}

static void module_notify(struct mtk_hcp *hcp_dev,
					struct share_buf *user_data_addr)
{
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
	int module_id;

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

	dev_dbg(hcp_dev->dev, " %s\n", __func__);
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
			dev_info(hcp_dev->dev, "Get_OBJ # of buf:%d in cmd:%d exceed %u",
				data.count, cmd, IPI_MAX_BUFFER_COUNT);
			return -EINVAL;
		}
		for (index = 0; index < IPI_MAX_BUFFER_COUNT; index++) {
			if (index >= data.count) {
				break;
			}

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
				pr_info("[HCP] Unknown command of packet[%u] cmd:%d", index,
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
				// pr_info("[HCP] Copy to user+: %d", index);
				ret = copy_to_user((void *)data.buffer[index++], &msg->user_obj,
					(unsigned long)sizeof(struct share_buf));
				// pr_info("[HCP] Copy to user-");

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
	struct share_buf __user *share_data32;

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

static phys_addr_t rmem_base_phys;
static phys_addr_t rmem_size;


int hcp_reserve_mem_of_init(struct reserved_mem *rmem)
{
	rmem_base_phys = (phys_addr_t) rmem->base;
	rmem_size = (phys_addr_t) rmem->size;
	pr_debug("%s: phys_addr is 0x%lx with size(0x%lx)",
				__func__, rmem_base_phys, rmem_size);

	return 0;
}
#define HCP_RESERVE_MEM_KEY "mediatek,imgsys-reserve-memory"
RESERVEDMEM_OF_DECLARE(hcp_reserve_mem_init, HCP_RESERVE_MEM_KEY, hcp_reserve_mem_of_init);


int allocate_working_buffer_helper(struct platform_device *pdev)
{
	unsigned int id;
	struct mtk_hcp_reserve_mblock *mblock;
	unsigned int block_num;
	struct sg_table *sgt;
	struct dma_buf_attachment *attach;
	struct mtk_hcp *hcp_dev = platform_get_drvdata(pdev);
	struct dma_heap *pdma_heap;
	void *buf_ptr;

	mblock = hcp_dev->data->mblock;
	block_num = hcp_dev->data->block_num;

	/* allocate reserved memory */
	for (id = 0; id < block_num; id++) {
		if (mblock[id].is_dma_buf) {
			switch (id) {
			case IMG_MEM_FOR_HW_ID:
				/*allocated at probe via dts*/
				break;
			case IMG_MEM_G_ID:
				/* all supported heap name you can find with cmd */
				/* (ls /dev/dma_heap/) in shell */
				pdma_heap = dma_heap_find("mtk_mm");
				if (!pdma_heap) {
					pr_info("pdma_heap find fail\n");
					return -1;
				}
				mblock[id].d_buf = dma_heap_buffer_alloc(
					pdma_heap,
					mblock[id].size, O_RDWR | O_CLOEXEC,
					DMA_HEAP_VALID_HEAP_FLAGS);
				if (IS_ERR(mblock[id].d_buf)) {
					pr_info("dma_heap_buffer_alloc fail :%lld\n",
					PTR_ERR(mblock[id].d_buf));
					return -1;
				}
				mtk_dma_buf_set_name(mblock[id].d_buf, mblock[id].name);

				mblock[id].attach = dma_buf_attach(
				mblock[id].d_buf, hcp_dev->dev);
				attach = mblock[id].attach;
				if (IS_ERR(attach)) {
					pr_info("dma_buf_attach fail :%lld\n",
					PTR_ERR(attach));
					return -1;
				}

				mblock[id].sgt = dma_buf_map_attachment(attach,
				DMA_TO_DEVICE);
				sgt = mblock[id].sgt;
				if (IS_ERR(sgt)) {
					dma_buf_detach(mblock[id].d_buf, attach);
					pr_info("dma_buf_map_attachment fail sgt:%lld\n",
					PTR_ERR(sgt));
					return -1;
				}
				mblock[id].start_phys = sg_dma_address(sgt->sgl);
				mblock[id].start_dma =
				mblock[id].start_phys;
				buf_ptr = dma_buf_vmap(mblock[id].d_buf);
				if (!buf_ptr) {
					pr_info("sg_dma_address fail\n");
					return -1;
				}
				mblock[id].start_virt = buf_ptr;
				get_dma_buf(mblock[id].d_buf);
				mblock[id].fd =
				dma_buf_fd(mblock[id].d_buf,
				O_RDWR | O_CLOEXEC);
				break;
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
					mblock[id].size, O_RDWR | O_CLOEXEC,
					DMA_HEAP_VALID_HEAP_FLAGS);
				if (IS_ERR(mblock[id].d_buf)) {
					pr_info("dma_heap_buffer_alloc fail :%lld\n",
					PTR_ERR(mblock[id].d_buf));
					return -1;
				}
				mtk_dma_buf_set_name(mblock[id].d_buf, mblock[id].name);
				mblock[id].attach = dma_buf_attach(
				mblock[id].d_buf, hcp_dev->dev);
				attach = mblock[id].attach;
				if (IS_ERR(attach)) {
					pr_info("dma_buf_attach fail :%lld\n",
					PTR_ERR(attach));
					return -1;
				}

				mblock[id].sgt = dma_buf_map_attachment(attach,
				DMA_TO_DEVICE);
				sgt = mblock[id].sgt;
				if (IS_ERR(sgt)) {
					dma_buf_detach(mblock[id].d_buf, attach);
					pr_info("dma_buf_map_attachment fail sgt:%lld\n",
					PTR_ERR(sgt));
					return -1;
				}
				mblock[id].start_phys = sg_dma_address(sgt->sgl);
				mblock[id].start_dma =
				mblock[id].start_phys;
				buf_ptr = dma_buf_vmap(mblock[id].d_buf);
				if (!buf_ptr) {
					pr_info("sg_dma_address fail\n");
					return -1;
				}
				mblock[id].start_virt = buf_ptr;
				get_dma_buf(mblock[id].d_buf);
				mblock[id].fd =
				dma_buf_fd(mblock[id].d_buf,
				O_RDWR | O_CLOEXEC);
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
	unsigned int id;
	struct mtk_hcp_reserve_mblock *mblock;
	unsigned int block_num;
	struct mtk_hcp *hcp_dev = platform_get_drvdata(pdev);
	#ifdef NEED_FORCE_MMAP_PAIR
	int i = 0;
	#endif

	mblock = hcp_dev->data->mblock;
	block_num = hcp_dev->data->block_num;

	/* release reserved memory */
	for (id = 0; id < block_num; id++) {
		if (mblock[id].is_dma_buf) {
			switch (id) {
			case IMG_MEM_FOR_HW_ID:
				/*allocated at probe via dts*/
				break;
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

	if (!hcp_dev->data->allocate) {
		dev_info(&pdev->dev, "%s:allocate not supported\n", __func__);
		return allocate_working_buffer_helper(pdev);
	}

	return hcp_dev->data->allocate(hcp_dev, mode);
}
EXPORT_SYMBOL(mtk_hcp_allocate_working_buffer);

int mtk_hcp_release_working_buffer(struct platform_device *pdev)
{
	struct mtk_hcp *hcp_dev = platform_get_drvdata(pdev);

	if (!hcp_dev->data->release) {
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

	if (!hcp_dev->data->get_init_info || !info) {
		dev_info(&pdev->dev, "%s:not supported\n", __func__);
		return -1;
	}

	return hcp_dev->data->get_init_info(info);
}
EXPORT_SYMBOL(mtk_hcp_get_init_info);

void mtk_hcp_purge_msg(struct platform_device *pdev)
{
	struct mtk_hcp *hcp_dev = platform_get_drvdata(pdev);
	unsigned long flag;
	int i;
	struct msg *msg, *tmp;

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
	struct mtk_hcp *hcp_dev;
	struct msg *msgs;
	int ret = 0;
	int i = 0;

	dev_info(&pdev->dev, "- E. hcp driver probe.\n");
	hcp_dev = devm_kzalloc(&pdev->dev, sizeof(*hcp_dev), GFP_KERNEL);
	if (hcp_dev == NULL)
		return -ENOMEM;

	hcp_mtkdev = hcp_dev;
	hcp_dev->dev = &pdev->dev;
	hcp_dev->mem_ops = &hcp_vb2_dma_contig_memops;

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

	mtk_hcp_reserve_mblock[IMG_MEM_FOR_HW_ID].size = rmem_size;

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
	{}
};
MODULE_DEVICE_TABLE(of, mtk_hcp_match);

static int mtk_hcp_remove(struct platform_device *pdev)
{

	struct mtk_hcp *hcp_dev = platform_get_drvdata(pdev);
	int i = 0;
#if HCP_RESERVED_MEM
	unsigned int id;
#endif

	dev_dbg(&pdev->dev, "- E. hcp driver remove.\n");

#if HCP_RESERVED_MEM
	//remove reserved memory
	for (id = 0; id < NUMS_MEM_ID; id++) {
		dma_free_coherent(hcp_dev->dev, mtk_hcp_reserve_mblock[id].size,
			mtk_hcp_reserve_mblock[id].start_virt,
			mtk_hcp_reserve_mblock[id].start_dma);
	}
#endif

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
