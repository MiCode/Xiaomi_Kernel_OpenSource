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

#ifdef USE_ION
#include "ion_drv.h"
#include "ion_priv.h"
#include "mtk_ion.h"
#endif

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

/**
 * HCP (Hetero Control Processor ) is a tiny processor controlling
 * the methodology of register programming. If the module support
 * to run on CM4 then it will send data to CM4 to program register.
 * Or it will send the data to user library and let RED to program
 * register.
 *
 **/
#ifdef USE_ION
static struct ion_handle *_imgsys_ion_alloc(
	struct ion_client *client,
	unsigned int heap_id_mask, size_t align,
	unsigned int size, bool cached);
static void _imgsys_ion_free_handle(struct ion_client *client,
	struct ion_handle *handle);
#endif

#define RED_PATH                "/dev/red"
#define HCP_DEVNAME             "mtk_hcp"

#define HCP_TIMEOUT_MS          4000U
#define HCP_FW_VER_LEN          16
#define HCP_SHARE_BUF_SIZE      288
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

#define HCP_INIT                _IOWR('H', 0, struct share_buf)
#define HCP_GET_OBJECT          _IOWR('H', 1, struct share_buf)
#define HCP_NOTIFY              _IOWR('H', 2, struct share_buf)
#define HCP_COMPLETE            _IOWR('H', 3, struct share_buf)
#define HCP_WAKEUP              _IOWR('H', 4, struct share_buf)

#define COMPAT_HCP_INIT         _IOWR('H', 0, struct share_buf)
#define COMPAT_HCP_GET_OBJECT   _IOWR('H', 1, struct share_buf)
#define COMPAT_HCP_NOTIFY       _IOWR('H', 2, struct share_buf)
#define COMPAT_HCP_COMPLETE     _IOWR('H', 3, struct share_buf)
#define COMPAT_HCP_WAKEUP       _IOWR('H', 4, struct share_buf)

static struct mtk_hcp *hcp_mtkdev;

/**
 * struct hcp_mem - HCP memory information
 *
 * @d_va:    the kernel virtual memory address of HCP extended data memory
 * @d_pa:    the physical memory address of HCP extended data memory
 * @d_len:   the length of extended data
 */
struct hcp_mem {
	void *d_va;
	dma_addr_t d_pa;
	unsigned long d_len;
};

/**
 * struct hcp_desc - hcp descriptor
 *
 * @handler:      IPI handler
 * @name:         the name of IPI handler
 * @priv:         the private data of IPI handler
 */
struct hcp_desc {
	hcp_handler_t handler;
	const char *name;
	void *priv;
};

/**
 * struct request -
 *
 * @hcp_id:      id
 * @data:        meta data which to be processed in other module
 * @len:         data length
 */
struct request {
	enum hcp_id id;
	void *data;
	unsigned int len;
};

/**
 * struct share_buf - DTCM (Data Tightly-Coupled Memory) buffer shared with
 *                    RED and HCP
 *
 * @id:             hcp id
 * @len:            share buffer length
 * @share_buf:      share buffer data
 */
struct share_buf {
	s32 id;
	u32 len;
	unsigned char share_data[HCP_SHARE_BUF_SIZE];
};

struct msg {
	struct list_head entry;
	struct share_buf user_obj;
};
#define  MSG_NR (64)
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

/**
 * struct mtk_hcp - hcp driver data
 * @extmem:              hcp extended memory information
 * @hcp_desc:            hcp descriptor
 * @dev:                 hcp struct device
 * @mem_ops:             memory operations
 * @hcp_mutex:           protect mtk_hcp (except recv_buf) and ensure only
 *                       one client to use hcp service at a time.
 * @data_mutex:          protect shared buffer between kernel user send and
 *                       user thread get&read/copy
 * @file:                hcp daemon file pointer
 * @is_open:             the flag to indicate if hcp device is open.
 * @ack_wq:              the wait queue for each client. When sleeping
 *                       processes wake up, they will check the condition
 *                       "hcp_id_ack" to run the corresponding action or
 *                       go back to sleep.
 * @hcp_id_ack:         The ACKs for registered HCP function.
 * @get_wq:              When sleeping process waking up, it will check the
 *                       condition "ipi_got" to run the corresponding action or
 *                       go back to sleep.
 * @ipi_got:             The flags for IPI message polling from user.
 * @ipi_done:            The flags for IPI message polling from user again,
 *       which means the previous messages has been dispatched
 *                       done in daemon.
 * @user_obj:            Temporary share_buf used for hcp_msg_get.
 * @hcp_devno:           The hcp_devno for hcp init hcp character device
 * @hcp_cdev:            The point of hcp character device.
 * @hcp_class:           The class_create for create hcp device
 * @hcp_device:          hcp struct device
 * @hcpname:             hcp struct device name in dtsi
 * @ cm4_support_list    to indicate which module can run in cm4 or it will send
 *                       to user space for running action.
 * @ current_task        hcp current task struct
 */
struct mtk_hcp {
	struct hcp_mem extmem;
	struct hcp_desc hcp_desc_table[HCP_MAX_ID];
	struct device *dev;
	const struct vb2_mem_ops *mem_ops;
#ifdef USE_ION
	struct ion_client *pIonClient;
#endif
	/* for protecting vcu data structure */
	struct mutex hcp_mutex[MODULE_MAX_ID];
	struct mutex data_mutex[MODULE_MAX_ID];
	struct file *file;
	bool   is_open;
	wait_queue_head_t ack_wq[MODULE_MAX_ID];
	atomic_t hcp_id_ack[HCP_MAX_ID];
	wait_queue_head_t get_wq[MODULE_MAX_ID];
	atomic_t ipi_got[MODULE_MAX_ID];
	atomic_t ipi_done[MODULE_MAX_ID];
	struct share_buf user_obj[MODULE_MAX_ID];
	struct list_head chans[MODULE_MAX_ID];
	struct list_head msg_list;
	spinlock_t msglock;
	wait_queue_head_t msg_wq;
	dev_t hcp_devno;
	struct cdev hcp_cdev;
	struct class *hcp_class;
	struct device *hcp_device;
	const char *hcpname;
	bool cm4_support_list[MODULE_MAX_ID];
	struct task_struct *current_task;
	struct workqueue_struct *daemon_notify_wq[MODULE_MAX_ID];
};

static int msg_pool_available(struct mtk_hcp *hcp_dev)
{
	unsigned long flag, empty;

	spin_lock_irqsave(&hcp_dev->msglock, flag);
	empty = list_empty(&hcp_dev->msg_list);
	spin_unlock_irqrestore(&hcp_dev->msglock, flag);

	return !empty;
}

inline int hcp_id_to_ipi_id(struct mtk_hcp *hcp_dev, enum hcp_id id)
{
	int ipi_id = -EINVAL;

	if (id < HCP_INIT_ID || id >= HCP_MAX_ID) {
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

	if (id < HCP_INIT_ID || id >= HCP_MAX_ID) {
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
	case HCP_DIP_HW_TIMEOUT_ID:
	case HCP_DIP_DEQUE_DUMP_ID:
	case HCP_IMGSYS_DEQUE_DONE_ID:
	case HCP_IMGSYS_DEINIT_ID:
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
	case HCP_DIP_HW_TIMEOUT_ID:
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

	if (id >= HCP_INIT_ID && id < HCP_MAX_ID && handler != NULL) {
#if MTK_CM4_SUPPORT
		if (mtk_hcp_cm4_support(hcp_dev, id) == true) {
			int ipi_id = hcp_id_to_ipi_id(hcp_dev, id);

			scp_ipi_registration(ipi_id, hcp_ipi_handler, name);
		}
#endif
		hcp_dev->hcp_desc_table[id].name = name;
		hcp_dev->hcp_desc_table[id].handler = handler;
		hcp_dev->hcp_desc_table[id].priv = priv;
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

	if (id >= HCP_INIT_ID && id < HCP_MAX_ID) {
		memset((void *)&hcp_dev->hcp_desc_table[id], 0,
						sizeof(struct hcp_desc));
		return 0;
	}

	dev_info(&pdev->dev, "%s register hcp id %d with invalid arguments\n",
								__func__, id);

	return -EINVAL;
}
EXPORT_SYMBOL(mtk_hcp_unregister);
static atomic_t seq;

static int hcp_send_internal(struct platform_device *pdev,
		 enum hcp_id id, void *buf,
		 unsigned int len, int frame_no,
		 unsigned int wait)
{
	struct mtk_hcp *hcp_dev = platform_get_drvdata(pdev);
	struct share_buf send_obj;
	unsigned long timeout, flag;
	struct msg *msg;
	int ret = 0;
	unsigned int no;

	if (id < HCP_INIT_ID || id >= HCP_MAX_ID ||
			len > sizeof(send_obj.share_data) || buf == NULL) {
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

		mutex_lock(&hcp_dev->hcp_mutex[module_id]);
		timeout = msecs_to_jiffies(HCP_TIMEOUT_MS);
		ret = wait_event_timeout(hcp_dev->msg_wq,
				msg_pool_available(hcp_dev), timeout);
		if (ret == 0) {
			mutex_unlock(&hcp_dev->hcp_mutex[module_id]);
			dev_info(&pdev->dev, "%s id:%d refill time out !\n",
				__func__, id);
			return -EIO;
		} else if (-ERESTARTSYS == ret) {
			mutex_unlock(&hcp_dev->hcp_mutex[module_id]);
			dev_info(&pdev->dev, "%s id:%d refill interrupted !\n",
				__func__, id);
			return -ERESTARTSYS;
		}

		atomic_set(&hcp_dev->hcp_id_ack[id], 0);

		spin_lock_irqsave(&hcp_dev->msglock, flag);
		msg = list_first_entry(&hcp_dev->msg_list, struct msg, entry);
		list_del(&msg->entry);
		spin_unlock_irqrestore(&hcp_dev->msglock, flag);
		no = atomic_inc_return(&seq);

		memcpy((void *)msg->user_obj.share_data, buf, len);
		msg->user_obj.len = len;
		msg->user_obj.id = (int)id | (no << 16);

		spin_lock_irqsave(&hcp_dev->msglock, flag);
		list_add_tail(&msg->entry, &hcp_dev->chans[module_id]);
		spin_unlock_irqrestore(&hcp_dev->msglock, flag);

		atomic_inc(&hcp_dev->ipi_got[module_id]);
		atomic_set(&hcp_dev->ipi_done[module_id], 0);
		wake_up(&hcp_dev->get_wq[module_id]);
		mutex_unlock(&hcp_dev->hcp_mutex[module_id]);
		dev_dbg(&pdev->dev,
			"%s frame_no_%d, message(%d)size(%d) send to user space !!!\n",
			__func__, no, id, len);

		if (!wait)
			return 0;

		/* wait for RED's ACK */
		timeout = msecs_to_jiffies(HCP_TIMEOUT_MS);
		ret = wait_event_timeout(hcp_dev->ack_wq[module_id],
						atomic_read(&hcp_dev->hcp_id_ack[id]),
								timeout);
		atomic_set(&hcp_dev->hcp_id_ack[id], 0);
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
		 unsigned int len, int frame_no)
{
	return hcp_send_internal(pdev, id, buf, len, frame_no, SYNC_SEND);
}
EXPORT_SYMBOL(mtk_hcp_send);

int mtk_hcp_send_async(struct platform_device *pdev,
		 enum hcp_id id, void *buf,
		 unsigned int len, int frame_no)
{
	return hcp_send_internal(pdev, id, buf, len, frame_no, ASYNC_SEND);
}
EXPORT_SYMBOL(mtk_hcp_send_async);

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
		.pIonHandle = NULL
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
		.pIonHandle = NULL
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
		.pIonHandle = NULL
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
		.pIonHandle = NULL
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
		.pIonHandle = NULL
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
		.pIonHandle = NULL
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
		.pIonHandle = NULL
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
		.pIonHandle = NULL
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
		.pIonHandle = NULL
	},
	{
		.num = TRAW_MEM_T_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0xA00000,   /*10MB*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL
	},
	{
		.num = DIP_MEM_C_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x400000,   /*4MB*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL
	},
	{
		.num = DIP_MEM_T_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x1700000,   /*23MB*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL
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
		.pIonHandle = NULL
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
		.pIonHandle = NULL
	},
};

#ifdef NEED_LEGACY_MEM
int mtk_hcp_reserve_mem_of_init(struct reserved_mem *rmem)
{
	enum mtk_hcp_reserve_mem_id_t id;
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
	enum mtk_hcp_reserve_mem_id_t id;
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

#ifdef USE_ION
int hcp_get_ion_buffer_fd(struct platform_device *pdev, enum mtk_hcp_reserve_mem_id_t id)
{
	struct mtk_hcp *hcp_dev = platform_get_drvdata(pdev);
	int fd = -1;

	if ((id < 0) || (id >= NUMS_MEM_ID)) {
		pr_info("[HCP] no reserve memory for %d", id);
	} else if ((hcp_dev->pIonClient != NULL) &&
						 (mtk_hcp_reserve_mblock[id].pIonHandle != NULL)) {
		fd = ion_share_dma_buf_fd(hcp_dev->pIonClient,
			mtk_hcp_reserve_mblock[id].pIonHandle);
		mtk_hcp_set_reserve_mem_fd(id, fd);
		pr_info("%s: pIonClient:0x%x, mblock[%d].pIonHandle:%x, fd:%d\n",
			__func__, hcp_dev->pIonClient, id,
			mtk_hcp_reserve_mblock[id].pIonHandle, fd);
	} else {
		pr_info("Wrong, pIonClient is NULL in %s!!", __func__);
	}
	return fd;
}
EXPORT_SYMBOL(hcp_get_ion_buffer_fd);

void hcp_close_ion_buffer_fd(struct platform_device *pdev, enum mtk_hcp_reserve_mem_id_t id)
{
	struct mtk_hcp *hcp_dev = platform_get_drvdata(pdev);

	if ((id < 0) || (id >= NUMS_MEM_ID))
		pr_info("[HCP] no reserve memory for %d", id);
	else if ((hcp_dev->pIonClient != NULL) &&
					 (mtk_hcp_reserve_mblock[id].pIonHandle != NULL)) {
		mtk_hcp_set_reserve_mem_fd(id, -1);
	} else {
		pr_info("Wrong, pIonClient is NULL in %s!!", __func__);
	}
}
EXPORT_SYMBOL(hcp_close_ion_buffer_fd);

int hcp_allocate_buffer(struct platform_device *pdev, enum mtk_hcp_reserve_mem_id_t id,
	unsigned int size)
{
	struct mtk_hcp *hcp_dev = platform_get_drvdata(pdev);

	if ((id < 0) || (id >= NUMS_MEM_ID)) {
		pr_info("[HCP] no reserve memory for %d", id);
	} else if (hcp_dev->pIonClient != NULL) {
		mtk_hcp_reserve_mblock[id].pIonHandle =
			_imgsys_ion_alloc(hcp_dev->pIonClient,
		ION_HEAP_MULTIMEDIA_MASK, 0, size, true);
		if (mtk_hcp_reserve_mblock[id].pIonHandle != NULL) {
			mtk_hcp_set_reserve_mem_virt(id,
				ion_map_kernel(hcp_dev->pIonClient,
					mtk_hcp_reserve_mblock[id].pIonHandle));
			pr_info("%s: pIonClient:0x%x, mblock[%d].size:%x\n",
			__func__, hcp_dev->pIonClient, id, size);
		} else {
			pr_info("pIonHandle is NULL!! in %s", __func__);
		}
	} else {
		pr_info("Wrong, pIonClient is NULL in %s!!", __func__);
	}
	return 0;
}
EXPORT_SYMBOL(hcp_allocate_buffer);

int hcp_free_buffer(struct platform_device *pdev, enum mtk_hcp_reserve_mem_id_t id)
{
	struct mtk_hcp *hcp_dev = platform_get_drvdata(pdev);
	if ((id < 0) || (id >= NUMS_MEM_ID)) {
		pr_info("[HCP] no reserve memory for %d", id);
	} else if ((hcp_dev->pIonClient != NULL) &&
						 (mtk_hcp_reserve_mblock[id].pIonHandle != NULL)) {
		ion_unmap_kernel(hcp_dev->pIonClient, mtk_hcp_reserve_mblock[id].pIonHandle);
		_imgsys_ion_free_handle(hcp_dev->pIonClient, mtk_hcp_reserve_mblock[id].pIonHandle);
		mtk_hcp_reserve_mblock[id].mem_priv = NULL;
		mtk_hcp_reserve_mblock[id].mmap_cnt = 0;
		mtk_hcp_reserve_mblock[id].start_dma = 0x0;
		mtk_hcp_reserve_mblock[id].start_virt = 0x0;
		mtk_hcp_reserve_mblock[id].start_phys = 0x0;
		mtk_hcp_reserve_mblock[id].d_buf = NULL;
		mtk_hcp_reserve_mblock[id].fd = -1;
		mtk_hcp_reserve_mblock[id].pIonHandle = NULL;
	} else {
		pr_info("Wrong, pIonClient is NULL in %s!!", __func__);
	}
	return 0;
}
EXPORT_SYMBOL(hcp_free_buffer);
#endif

phys_addr_t mtk_hcp_get_reserve_mem_phys(enum mtk_hcp_reserve_mem_id_t id)
{
	if ((id < 0) || (id >= NUMS_MEM_ID)) {
		pr_info("[HCP] no reserve memory for %d", id);
		return 0;
	} else {
		return mtk_hcp_reserve_mblock[id].start_phys;
	}
}
EXPORT_SYMBOL(mtk_hcp_get_reserve_mem_phys);

void mtk_hcp_set_reserve_mem_virt(enum mtk_hcp_reserve_mem_id_t id,
	void *virmem)
{
	if ((id < 0) || (id >= NUMS_MEM_ID))
		pr_info("[HCP] no reserve memory for %d in set_reserve_mem_virt", id);
	else
		mtk_hcp_reserve_mblock[id].start_virt = virmem;
}
EXPORT_SYMBOL(mtk_hcp_set_reserve_mem_virt);

void *mtk_hcp_get_reserve_mem_virt(enum mtk_hcp_reserve_mem_id_t id)
{
	if ((id < 0) || (id >= NUMS_MEM_ID)) {
		pr_info("[HCP] no reserve memory for %d", id);
		return 0;
	} else
		return mtk_hcp_reserve_mblock[id].start_virt;
}
EXPORT_SYMBOL(mtk_hcp_get_reserve_mem_virt);

phys_addr_t mtk_hcp_get_reserve_mem_dma(enum mtk_hcp_reserve_mem_id_t id)
{
	if ((id < 0) || (id >= NUMS_MEM_ID)) {
		pr_info("[HCP] no reserve memory for %d", id);
		return 0;
	} else {
		return mtk_hcp_reserve_mblock[id].start_dma;
	}
}
EXPORT_SYMBOL(mtk_hcp_get_reserve_mem_dma);

phys_addr_t mtk_hcp_get_reserve_mem_size(enum mtk_hcp_reserve_mem_id_t id)
{
	if ((id < 0) || (id >= NUMS_MEM_ID)) {
		pr_info("[HCP] no reserve memory for %d", id);
		return 0;
	} else {
		return mtk_hcp_reserve_mblock[id].size;
	}
}
EXPORT_SYMBOL(mtk_hcp_get_reserve_mem_size);


void mtk_hcp_set_reserve_mem_fd(enum mtk_hcp_reserve_mem_id_t id, uint32_t fd)
{
	if ((id < 0) || (id >= NUMS_MEM_ID))
		pr_info("[HCP] no reserve memory for %d", id);
	else
		mtk_hcp_reserve_mblock[id].fd = fd;
}
EXPORT_SYMBOL(mtk_hcp_set_reserve_mem_fd);

uint32_t mtk_hcp_get_reserve_mem_fd(enum mtk_hcp_reserve_mem_id_t id)
{
	if ((id < 0) || (id >= NUMS_MEM_ID)) {
		pr_info("[HCP] no reserve memory for %d", id);
		return 0;
	} else
		return mtk_hcp_reserve_mblock[id].fd;
}
EXPORT_SYMBOL(mtk_hcp_get_reserve_mem_fd);
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

static int mtk_hcp_release(struct inode *inode, struct file *file)
{
	struct mtk_hcp *hcp_dev = (struct mtk_hcp *)file->private_data;

	dev_dbg(hcp_dev->dev, "- E. hcp release.\n");

	hcp_dev->is_open = false;
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

	/* dealing with register remap */
	length = vma->vm_end - vma->vm_start;
	dev_info(hcp_dev->dev,
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

	if ((length <= 0) || (length != (long)mtk_hcp_reserve_mblock[reserved_memory_id].size)) {
		dev_info(hcp_dev->dev,
				" %s size is not allowed: id:%d, pfn:0x%llx, length: 0x%lx != 0x%lx\n",
				__func__, reserved_memory_id, pfn, length,
				mtk_hcp_reserve_mblock[reserved_memory_id].size);
		return -EPERM;
	}

	if (pfn) {
		mtk_hcp_reserve_mblock[reserved_memory_id].mmap_cnt += 1;
		dev_info(hcp_dev->dev, "reserved_memory_id:%d, pfn:0x%llx, mmap_cnt:%d\n",
			reserved_memory_id, pfn,
			mtk_hcp_reserve_mblock[reserved_memory_id].mmap_cnt);

		if (reserved_memory_id != -1) {
			vma->vm_pgoff = (unsigned long)
	(mtk_hcp_reserve_mblock[reserved_memory_id].start_phys >> PAGE_SHIFT);
			vma->vm_page_prot =
			pgprot_writecombine(vma->vm_page_prot);

			switch (reserved_memory_id) {
			case IMG_MEM_FOR_HW_ID:
				vma->vm_pgoff = (unsigned long)
	(mtk_hcp_reserve_mblock[reserved_memory_id].start_phys >> PAGE_SHIFT);
				vma->vm_page_prot =
						pgprot_writecombine(vma->vm_page_prot);
				break;
			case IMG_MEM_G_ID:
			default:
		hcp_dev->mem_ops->mmap(mtk_hcp_reserve_mblock[reserved_memory_id].mem_priv,
					vma);
			pr_info("%s: [HCP][%d] after mem_ops->mmap vb2_dc_buf refcount(%d)\n",
			__func__, reserved_memory_id,
	hcp_dev->mem_ops->num_users(mtk_hcp_reserve_mblock[reserved_memory_id].mem_priv));
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
	dev_info(hcp_dev->dev, "remap info id(%d) start:0x%llx pgoff:0x%llx page_prot:0x%llx length:0x%llx",
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

static int mtk_hcp_get_data(struct mtk_hcp *hcp_dev, unsigned long arg)
{
	struct share_buf share_buff_data;
	int module_id = 0;
	unsigned long flag;
	struct msg *msg;
	int ret = 0;
	char *cmd;

	ret = (long)copy_from_user(&share_buff_data, (unsigned char *)arg,
					 (unsigned long)sizeof(struct share_buf));
	module_id = share_buff_data.id;
	if (module_id < MODULE_ISP || module_id >= MODULE_MAX_ID) {
		pr_info("[HCP] invalid module id %d", module_id);
		return -EINVAL;
	}

	atomic_set(&hcp_dev->ipi_done[module_id], 1);
	dev_dbg(hcp_dev->dev, "%s ipi_done[%d] = %d\n", __func__, module_id,
		hcp_dev->ipi_done[module_id]);

	mutex_lock(&hcp_dev->data_mutex[module_id]);
	ret = wait_event_freezable(hcp_dev->get_wq[module_id],
		atomic_read(&hcp_dev->ipi_got[module_id]));
	if (ret != 0) {
		mutex_unlock(&hcp_dev->data_mutex[module_id]);
		dev_info(hcp_dev->dev, "%s wait event return %d\n", __func__,
									ret);
		return ret;
	}
	atomic_dec(&hcp_dev->ipi_got[module_id]);
	mutex_unlock(&hcp_dev->data_mutex[module_id]);

	spin_lock_irqsave(&hcp_dev->msglock, flag);
	msg = list_first_entry(&hcp_dev->chans[module_id], struct msg, entry);
	list_del(&msg->entry);
	spin_unlock_irqrestore(&hcp_dev->msglock, flag);

	ret = copy_to_user((void *)arg, &msg->user_obj,
		(unsigned long)sizeof(struct share_buf));

	spin_lock_irqsave(&hcp_dev->msglock, flag);
	list_add_tail(&msg->entry, &hcp_dev->msg_list);
	spin_unlock_irqrestore(&hcp_dev->msglock, flag);
	wake_up(&hcp_dev->msg_wq);

	switch (msg->user_obj.id) {
	case HCP_DIP_FRAME_ID:
		cmd = "HCP_DIP_FRAME_ID";
		break;
	case HCP_IMGSYS_DEQUE_DONE_ID:
		cmd = "HCP_IMGSYS_DEQUE_DONE_ID";
		break;
	case HCP_IMGSYS_INIT_ID:
		cmd = "HCP_IMGSYS_INIT_ID";
		break;
	case HCP_IMGSYS_DEINIT_ID:
		cmd = "HCP_IMGSYS_DEINIT_ID";
		break;
	default:
		cmd = "unknown cmd";
		break;
	}
	dev_dbg(hcp_dev->dev, "%s copy data(id = %d) %s to user %d\n", __func__,
		msg->user_obj.id, cmd, ret);

	if (ret != 0) {
		dev_info(hcp_dev->dev, "%s(%d) Copy data to user failed!\n",
							__func__, __LINE__);
		ret = -EINVAL;
	}

	return ret;
}

static void module_notify(struct mtk_hcp *hcp_dev,
					struct share_buf *user_data_addr)
{
	if (!user_data_addr) {
		dev_info(hcp_dev->dev, "%s invalid null share buffer", __func__);
		return;
	}

	if ((user_data_addr->id < HCP_INIT_ID) || (user_data_addr->id >= HCP_MAX_ID)) {
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

	if ((user_data_addr->id < HCP_INIT_ID) || (user_data_addr->id >= HCP_MAX_ID)) {
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

static void hcp_handle_daemon_event(struct work_struct *work)
{
	struct my_wq_t *my_work_data = container_of(work,
		struct my_wq_t, task_work);

	switch (my_work_data->ioctl_event) {
	case HCP_NOTIFY:
		module_notify(my_work_data->hcp_dev, &(my_work_data->data_addr));
		break;
	case HCP_COMPLETE:
		module_notify(my_work_data->hcp_dev, &(my_work_data->data_addr));
		module_wake_up(my_work_data->hcp_dev, &(my_work_data->data_addr));
		break;
	case HCP_WAKEUP:
		module_wake_up(my_work_data->hcp_dev, &(my_work_data->data_addr));
		break;
	default:
		break;
	}

	vfree(my_work_data);
}

static long mtk_hcp_ioctl(struct file *file, unsigned int cmd,
							unsigned long arg)
{
	long ret = -1;
	//void *mem_priv;
	struct mtk_hcp *hcp_dev = (struct mtk_hcp *)file->private_data;

	switch (cmd) {
	case HCP_GET_OBJECT:
		ret = mtk_hcp_get_data(hcp_dev, arg);
		break;
	case HCP_NOTIFY:
	case HCP_COMPLETE:
	case HCP_WAKEUP:
		{
			int module_id = 0;
			struct my_wq_t *my_wq_d = (struct my_wq_t *)
				vzalloc(sizeof(struct my_wq_t));

			my_wq_d->hcp_dev = (struct mtk_hcp *)file->private_data;
			my_wq_d->ioctl_event = cmd;
			copy_from_user(&(my_wq_d->data_addr), (void *)arg,
				sizeof(struct share_buf));
			module_id = hcp_id_to_module_id(hcp_dev, my_wq_d->data_addr.id);
			if (module_id < MODULE_ISP || module_id >= MODULE_MAX_ID) {
				pr_info("[HCP] invalid module id %d", module_id);
				return -EINVAL;
			}
			/*dev_info(hcp_dev->dev, "(%d) event: 0x%x.\n", module_id, cmd);*/
			INIT_WORK(&my_wq_d->task_work, hcp_handle_daemon_event);
			queue_work(hcp_dev->daemon_notify_wq[module_id], &my_wq_d->task_work);
			ret = 0;
		}
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
	pr_info("%s: phys_addr is 0x%lx with size(0x%lx)",
				__func__, rmem_base_phys, rmem_size);

	return 0;
}
#define HCP_RESERVE_MEM_KEY "mediatek,imgsys-reserve-memory"
RESERVEDMEM_OF_DECLARE(hcp_reserve_mem_init, HCP_RESERVE_MEM_KEY, hcp_reserve_mem_of_init);

#ifdef USE_ION
#define ION_FLAG_FREE_WITHOUT_DEFER (4)

static struct ion_handle *_imgsys_ion_alloc(struct ion_client *client,
	unsigned int heap_id_mask, size_t align, unsigned int size, bool cached)
{
	struct ion_handle *disp_handle = NULL;

	disp_handle = ion_alloc(client, size, align,
		heap_id_mask, ((cached)?3:0) | ION_FLAG_FREE_WITHOUT_DEFER);
	if (IS_ERR(disp_handle)) {
		pr_info("disp_ion_alloc 1error %p\n", disp_handle);
		return NULL;
	}

	return disp_handle;
}

static void _imgsys_ion_free_handle(struct ion_client *client,
	struct ion_handle *handle)
{
	if (!client) {
		pr_info("invalid ion client!\n");
		return;
	}
	if (!handle)
		return;

	ion_free(client, handle);

}
#endif

int mtk_hcp_allocate_working_buffer(struct platform_device *pdev)
{
	enum mtk_hcp_reserve_mem_id_t id;
	void *va, *da;
#ifdef USE_ION
	int fd;
	struct ion_handle *pIonHandle = NULL;
#endif
	struct mtk_hcp *hcp_dev = platform_get_drvdata(pdev);

	/* allocate reserved memory */
	for (id = 0; id < NUMS_MEM_ID; id++) {
		if (mtk_hcp_reserve_mblock[id].is_dma_buf) {
			switch (id) {
			case IMG_MEM_FOR_HW_ID:
				/*allocated at probe via dts*/
				break;
			case IMG_MEM_G_ID:
#ifdef USE_ION
				if (hcp_dev->pIonClient != NULL) {
					pIonHandle = _imgsys_ion_alloc(hcp_dev->pIonClient,
						ION_HEAP_MULTIMEDIA_MASK, 0,
						(size_t)mtk_hcp_reserve_mblock[id].size, true);
					if (pIonHandle != NULL) {
						fd = ion_share_dma_buf_fd(hcp_dev->pIonClient,
							pIonHandle);
						mtk_hcp_reserve_mblock[id].start_virt =
							ion_map_kernel(hcp_dev->pIonClient,
							pIonHandle);
						mtk_hcp_reserve_mblock[id].fd = fd;
						mtk_hcp_reserve_mblock[id].pIonHandle = pIonHandle;
						pr_info("%s: pIonClient:%x, pIonHandle:%x, mblock[%d].size:%x, fd:%x\n",
						__func__, hcp_dev->pIonClient,
						pIonHandle, id,
						mtk_hcp_reserve_mblock[id].size, fd);
					} else {
						pr_info("pIonHandle is NULL!! in %s[%d]",
							__func__, id);
					}
				} else {
					pr_info("State machine is wrong, Because pIonClient is NULL!!");
				}
#endif
				break;
			default:
				mtk_hcp_reserve_mblock[id].mem_priv =
					hcp_dev->mem_ops->alloc(hcp_dev->dev, 0,
					mtk_hcp_reserve_mblock[id].size, 0, 0);
				pr_debug("%s: [HCP][%d] after mem_ops->alloc vb2_dc_buf refcount(%d)\n",
					__func__, id,
					hcp_dev->mem_ops->num_users(
					mtk_hcp_reserve_mblock[id].mem_priv));
				va = hcp_dev->mem_ops->vaddr(mtk_hcp_reserve_mblock[id].mem_priv);
				pr_debug("%s: [HCP][%d] after mem_ops->alloc vaddr refcount(%d)\n",
					__func__, id,
					hcp_dev->mem_ops->num_users(
					mtk_hcp_reserve_mblock[id].mem_priv));
				da = hcp_dev->mem_ops->cookie(mtk_hcp_reserve_mblock[id].mem_priv);
				pr_debug("%s: [HCP][%d] after mem_ops->cookie vaddr refcount(%d)\n",
					__func__, id,
					hcp_dev->mem_ops->num_users(
					mtk_hcp_reserve_mblock[id].mem_priv));
				mtk_hcp_reserve_mblock[id].start_dma = *(dma_addr_t *)da;
				mtk_hcp_reserve_mblock[id].start_virt = va;
				mtk_hcp_reserve_mblock[id].start_phys = *(dma_addr_t *)da;
				#ifdef NEED_WK_FD /*currently no used*/
				mtk_hcp_reserve_mblock[id].d_buf = hcp_dev->mem_ops->get_dmabuf(
					mtk_hcp_reserve_mblock[id].mem_priv, O_RDWR);
				mtk_hcp_reserve_mblock[id].fd = dma_buf_fd(
					mtk_hcp_reserve_mblock[id].d_buf, O_CLOEXEC);
				#endif
				break;
			}
		} else {
			mtk_hcp_reserve_mblock[id].start_virt =
				kzalloc(mtk_hcp_reserve_mblock[id].size,
					GFP_KERNEL);
			mtk_hcp_reserve_mblock[id].start_phys =
				virt_to_phys(
					mtk_hcp_reserve_mblock[id].start_virt);
			mtk_hcp_reserve_mblock[id].start_dma = 0;
		}
		pr_info(
			"%s: [HCP][mem_reserve-%d] phys:0x%llx, virt:0x%llx, dma:0x%llx, size:0x%llx, is_dma_buf:%d, fd:%d\n",
			__func__, id, mtk_hcp_get_reserve_mem_phys(id),
			mtk_hcp_get_reserve_mem_virt(id),
			mtk_hcp_get_reserve_mem_dma(id),
			mtk_hcp_get_reserve_mem_size(id),
			mtk_hcp_reserve_mblock[id].is_dma_buf,
			mtk_hcp_get_reserve_mem_fd(id));
	}

	return 0;
}
EXPORT_SYMBOL(mtk_hcp_allocate_working_buffer);

int mtk_hcp_release_working_buffer(struct platform_device *pdev)
{
	enum mtk_hcp_reserve_mem_id_t id;
	struct mtk_hcp *hcp_dev = platform_get_drvdata(pdev);
	#ifdef NEED_FORCE_MMAP_PAIR
	int i = 0;
	#endif

	/* release reserved memory */
	for (id = 0; id < NUMS_MEM_ID; id++) {
		if (mtk_hcp_reserve_mblock[id].is_dma_buf) {
			switch (id) {
			case IMG_MEM_FOR_HW_ID:
				/*allocated at probe via dts*/
				break;
			case IMG_MEM_G_ID:
#ifdef USE_ION
				if ((hcp_dev->pIonClient != NULL) &&
					(mtk_hcp_reserve_mblock[id].pIonHandle != NULL)) {
					_imgsys_ion_free_handle(hcp_dev->pIonClient,
						mtk_hcp_reserve_mblock[id].pIonHandle);
				} else {
					pr_info("pIonClient or pIonHandle is NULL!! in %s",
					__func__);
				}
				mtk_hcp_reserve_mblock[id].mem_priv = NULL;
				mtk_hcp_reserve_mblock[id].mmap_cnt = 0;
				mtk_hcp_reserve_mblock[id].start_dma = 0x0;
				mtk_hcp_reserve_mblock[id].start_virt = 0x0;
				mtk_hcp_reserve_mblock[id].start_phys = 0x0;
				mtk_hcp_reserve_mblock[id].d_buf = NULL;
				mtk_hcp_reserve_mblock[id].fd = -1;
				mtk_hcp_reserve_mblock[id].pIonHandle = NULL;
#endif
				break;
			default:
				#ifdef NEED_WK_FD /*currently no used*/
				dma_buf_put(mtk_hcp_reserve_mblock[id].d_buf);
				pr_info("%s: [HCP][%d] after dma_buf_put vb2_dc_buf refcount(%d)\n",
					__func__, id,
					hcp_dev->mem_ops->num_users(
					mtk_hcp_reserve_mblock[id].mem_priv));
				#endif
				/* pair with mem_ops->mmap
				 * could call vma->vm_ops->close(vma); instead
				 */
				/*
				 * we need to do the following steps to support our specific flow as
				 * below
				 *
				 * Flow:
				 * 1) do buffer alloc/release when imgsys streamon & streamoff
				 * 2) stream on/off might be repeatly called during image
				 *    post-processing without close hcp device and re-open hcp
				 *    device
				 * - The Flow would cause memory leakage due to refcount
				 *   increased by mmap would be handled when hcp device
				 *   (file descriptor) is close (calling vma->close)
				 *   . allocate: hcp_dev->mem_ops->alloc: refcount + 1
				 *   . mmap: hcp_dev->mem_ops->mmap: refcount + 1
				 *           (calling vma->open)
				 *   . release: hcp_dev->mem_ops->put: refcount - 1
				 *
				 * Solution:
				 * 1) do as usual under allocate & mmap stage
				 *   . allocate: hcp_dev->mem_ops->alloc: refcount + 1
				 *   . mmap: hcp_dev->mem_ops->mmap: refcount + 1
				 *           (calling vma->open)
				 * 2) do one more put ops in release stage
				 *   . release: hcp_dev->mem_ops->put: refcount - 1
				 *   . release: hcp_dev->mem_ops->put: refcount - 1
				 * 3) empty the hcp_vb2_common_vm_close (vma->close) function
				 *    to avoid exception due to function is called at mmap_exit
				 *    (hcp device close stage)
				 *
				 */
				#ifdef NEED_FORCE_MMAP_PAIR
				for (i = 0 ; i < mtk_hcp_reserve_mblock[id].mmap_cnt ; i++) {
					pr_info("%s: [mem-%d] call put for mmap_time(%d/%d)\n",
						__func__, id, i,
						mtk_hcp_reserve_mblock[id].mmap_cnt);
					hcp_dev->mem_ops->put(mtk_hcp_reserve_mblock[id].mem_priv);
				}
				#endif
				/* pair with mem_ops->alloc */
				hcp_dev->mem_ops->put(mtk_hcp_reserve_mblock[id].mem_priv);
				mtk_hcp_reserve_mblock[id].mem_priv = NULL;
				mtk_hcp_reserve_mblock[id].mmap_cnt = 0;
				mtk_hcp_reserve_mblock[id].start_dma = 0x0;
				mtk_hcp_reserve_mblock[id].start_virt = 0x0;
				mtk_hcp_reserve_mblock[id].start_phys = 0x0;
				mtk_hcp_reserve_mblock[id].d_buf = NULL;
				mtk_hcp_reserve_mblock[id].fd = -1;
				mtk_hcp_reserve_mblock[id].pIonHandle = NULL;
				break;
			}
		} else {
			kfree(mtk_hcp_reserve_mblock[id].start_virt);
			mtk_hcp_reserve_mblock[id].start_virt = 0x0;
			mtk_hcp_reserve_mblock[id].start_phys = 0x0;
			mtk_hcp_reserve_mblock[id].start_dma = 0x0;
			mtk_hcp_reserve_mblock[id].mmap_cnt = 0;
		}
		pr_info(
			"%s: [HCP][mem_reserve-%d] phys:0x%llx, virt:0x%llx, dma:0x%llx, size:0x%llx, is_dma_buf:%d, fd:%d\n",
			__func__, id, mtk_hcp_get_reserve_mem_phys(id),
			mtk_hcp_get_reserve_mem_virt(id),
			mtk_hcp_get_reserve_mem_dma(id),
			mtk_hcp_get_reserve_mem_size(id),
			mtk_hcp_reserve_mblock[id].is_dma_buf,
			mtk_hcp_get_reserve_mem_fd(id));
	}

	return 0;
}
EXPORT_SYMBOL(mtk_hcp_release_working_buffer);

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
		atomic_set(&hcp_dev->ipi_got[i], 0);
	}
	atomic_set(&seq, 0);
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
	platform_set_drvdata(pdev, hcp_dev);
	dev_set_drvdata(&pdev->dev, hcp_dev);


	hcp_dev->is_open = false;
	for (i = 0; i < MODULE_MAX_ID; i++) {
		mutex_init(&hcp_dev->hcp_mutex[i]);
		mutex_init(&hcp_dev->data_mutex[i]);

		init_waitqueue_head(&hcp_dev->ack_wq[i]);
		init_waitqueue_head(&hcp_dev->get_wq[i]);
		memset(&hcp_dev->user_obj[i], 0,
			sizeof(struct share_buf));

		INIT_LIST_HEAD(&hcp_dev->chans[i]);

		atomic_set(&hcp_dev->ipi_got[i], 0);
		atomic_set(&hcp_dev->ipi_done[i], 0);

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
#ifdef USE_ION
	hcp_dev->pIonClient = ion_client_create(g_ion_device, "imgsys");
	if (hcp_dev->pIonClient == NULL) {
		pr_info("hcp_dev->pIonClient is NULL!! in probe stage");
		goto err_device;
	}
#endif
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
		mutex_destroy(&hcp_dev->hcp_mutex[i]);
		mutex_destroy(&hcp_dev->data_mutex[i]);

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
	{.compatible = "mediatek,hcp",},
	{}
};
MODULE_DEVICE_TABLE(of, mtk_hcp_match);

static int mtk_hcp_remove(struct platform_device *pdev)
{

	struct mtk_hcp *hcp_dev = platform_get_drvdata(pdev);
	int i = 0;
#if HCP_RESERVED_MEM
	enum mtk_hcp_reserve_mem_id_t id;
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

#ifdef USE_ION
	if (hcp_dev->pIonClient != NULL) {
		ion_client_destroy(hcp_dev->pIonClient);
	}
#endif
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
