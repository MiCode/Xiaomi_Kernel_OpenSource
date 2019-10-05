/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author:
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
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
#include <asm/dma-contiguous.h>

#include "mtk-hcp.h"
#include "mtk-hcp-support.h"

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

#define RED_PATH            "/dev/red"
#define HCP_DEVNAME         "mtk_hcp"

#define HCP_TIMEOUT_MS		4000U
#define HCP_FW_VER_LEN		16
#define HCP_SHARE_BUF_SIZE	288
#define MAX_REQUEST_SIZE	10

#define SYNC_SEND		1
#define ASYNC_SEND		0
/*
 * define magic number for reserved memory to use in mmap function.
 */
#define START_ISP_MEM_ADDR		0x12345000
#define START_DIP_MEM_FOR_HW_ADDR	0x12346000
#define START_MDP_MEM_ADDR		0x12347000
#define START_FD_MEM_ADDR		0x12348000
#define START_DIP_MEM_FOR_SW_ADDR	0x12349000


/*
 * define module register mmap address
 */
#define ISP_UNI_A_BASE_HW	0x1A003000
#define ISP_A_BASE_HW		0x1A004000
#define ISP_B_BASE_HW		0x1A006000
#define ISP_C_BASE_HW		0x1A008000
#define DIP_BASE_HW		0x15021000
#define FD_BASE_HW		0x1502B000

#define HCP_INIT		_IOWR('J', 0, struct share_buf)
#define HCP_GET_OBJECT		_IOWR('J', 1, struct share_buf)
#define HCP_NOTIFY		_IOWR('J', 2, struct share_buf)
#define HCP_COMPLETE		_IOWR('J', 3, struct share_buf)
#define HCP_WAKEUP		_IOWR('J', 4, struct share_buf)

#define COMPAT_HCP_INIT		_IOWR('J', 0, struct share_buf)
#define COMPAT_HCP_GET_OBJECT	_IOWR('J', 1, struct share_buf)
#define COMPAT_HCP_NOTIFY	_IOWR('J', 2, struct share_buf)
#define COMPAT_HCP_COMPLETE	_IOWR('J', 3, struct share_buf)
#define COMPAT_HCP_WAKEUP	_IOWR('J', 4, struct share_buf)

static struct mtk_hcp		*hcp_mtkdev;

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

/**
 * struct mtk_hcp - Hcp driver data
 * @extmem: Hcp extended memory information
 * @hcp_desc: Hcp descriptor
 * @dev: Hcp struct device
 * @hcp_mutex: Protect mtk_hcp (except recv_buf) and ensure only
 *                          one client to use hcp service at a time.
 * @file: Hcp daemon file pointer
 * @is_open: The flag to indicate if hcp device is open.
 * @ack_wq: The wait queue for each client. When sleeping
 *                     processes wake up, they will check the condition
 *                     "hcp_id_ack" to run the corresponding action or
 *                     go back to sleep.
 * @hcp_id_ack: The ACKs for registered HCP function.
 * @get_wq: When sleeping process waking up, it will check the
 *                    condition "ipi_got" to run the corresponding action or
 *                    go back to sleep.
 * @ipi_got: The flags for IPI message polling from user.
 * @ipi_done: The flags for IPI message polling from user again,
 *                       which means the previous messages has been dispatched
 *                       done in daemon.
 * @user_obj: Temporary share_buf used for hcp_msg_get.
 * @hcp_devno: The hcp_devno for hcp init hcp character device
 * @hcp_cdev: The point of hcp character device.
 * @hcp_class: The class_create for create hcp device
 * @hcp_device: hcp struct device
 * @hcpname: hcp struct device name in dtsi
 * @ cm4_support_list  To indicate which module can run in cm4 or it will send
 *                                      to user space for running action.
 *
 */
struct mtk_hcp {
	struct hcp_mem extmem;
	struct hcp_desc hcp_desc_table[HCP_MAX_ID];
	struct device *dev;
	struct mutex hcp_mutex[MODULE_MAX_ID];
	struct file *file;
	bool   is_open;
	wait_queue_head_t ack_wq[MODULE_MAX_ID];
	bool hcp_id_ack[HCP_MAX_ID];
	wait_queue_head_t get_wq[MODULE_MAX_ID];
	atomic_t ipi_got[MODULE_MAX_ID];
	atomic_t ipi_done[MODULE_MAX_ID];
	struct share_buf user_obj[MODULE_MAX_ID];
	dev_t hcp_devno;
	struct cdev hcp_cdev;
	struct class *hcp_class;
	struct device *hcp_device;
	const char *hcpname;
	bool cm4_support_list[MODULE_MAX_ID];
};

inline int hcp_id_to_ipi_id(struct mtk_hcp *hcp_dev, enum hcp_id id)
{
	int ipi_id = -EINVAL;

	if (id < HCP_INIT_ID || id > HCP_MAX_ID) {
		dev_dbg(hcp_dev->dev, "%s: Invalid hcp id %d\n", __func__, id);
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

	if (id < HCP_INIT_ID || id > HCP_MAX_ID) {
		dev_dbg(hcp_dev->dev, "%s: Invalid hcp id %d\n", __func__, id);
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

inline int ipi_id_to_hcp_id(enum ipi_id id)
{
	int hcp_id = HCP_INIT_ID;

	switch (id) {

	default:
		break;
	}
	pr_debug("%s hcp_id:%d\n", __func__, hcp_id);
	return hcp_id;
}

static inline bool mtk_hcp_running(struct mtk_hcp *hcp_dev)
{
	return hcp_dev->is_open;
}

static void hcp_ipi_handler(int id, void *data, unsigned int len)
{
	int hcp_id = ipi_id_to_hcp_id(id);

	if (hcp_mtkdev->hcp_desc_table[hcp_id].handler)
		hcp_mtkdev->hcp_desc_table[hcp_id].handler(data, len, NULL);
}

static void cm4_support_table_init(struct mtk_hcp *hcp)
{
	int i = 0;

	for (i = 0; i < MODULE_MAX_ID; i++)
		hcp->cm4_support_list[i] = false;

	i = 0;
	while (CM4_SUPPORT_TABLE[i][0] < MODULE_MAX_ID) {
		if (CM4_SUPPORT_TABLE[i][1] == 1) {
			hcp->cm4_support_list[CM4_SUPPORT_TABLE[i][0]]
				= true;
		}

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
		dev_dbg(hcp_dev->dev, "%s hcp device in not ready\n",
			__func__);
		return -EPROBE_DEFER;
	}

	if (id >= HCP_INIT_ID && id < HCP_MAX_ID && handler != NULL) {
		if (mtk_hcp_cm4_support(hcp_dev, id) == true) {
			int ipi_id = hcp_id_to_ipi_id(hcp_dev, id);

			scp_ipi_registration(ipi_id, hcp_ipi_handler, name);
		}
		hcp_dev->hcp_desc_table[id].name = name;
		hcp_dev->hcp_desc_table[id].handler = handler;
		hcp_dev->hcp_desc_table[id].priv = priv;
		return 0;
	}

	dev_dbg(&pdev->dev, "%s register hcp id %d with invalid arguments\n",
		__func__, id);

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(mtk_hcp_register);

int mtk_hcp_unregister(struct platform_device *pdev, enum hcp_id id)
{
	struct mtk_hcp *hcp_dev = platform_get_drvdata(pdev);

	if (hcp_dev == NULL) {
		dev_dbg(&pdev->dev, "%s hcp device in not ready\n", __func__);
		return -EPROBE_DEFER;
	}

	if (id >= HCP_INIT_ID && id < HCP_MAX_ID) {
		memset((void *)&hcp_dev->hcp_desc_table[id], 0,
			sizeof(struct hcp_desc));
		return 0;
	}

	dev_dbg(&pdev->dev, "%s register hcp id %d with invalid arguments\n",
		__func__, id);

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(mtk_hcp_unregister);

static int hcp_send_internal(struct platform_device *pdev,
		 enum hcp_id id, void *buf,
		 unsigned int len,
		 unsigned int wait)
{
	struct mtk_hcp *hcp_dev = platform_get_drvdata(pdev);
	struct share_buf send_obj;
	unsigned long timeout;
	int ret = 0;

	if (id <= HCP_INIT_ID || id >= HCP_MAX_ID ||
	    len > sizeof(send_obj.share_data) || buf == NULL) {
		dev_info(&pdev->dev, "%s failed to send hcp message\n",
			__func__);
		return -EINVAL;
	}

	if (mtk_hcp_running(hcp_dev) == false) {
		dev_info(&pdev->dev, "%s hcp is not running\n", __func__);
		return -EPERM;
	}

	if (mtk_hcp_cm4_support(hcp_dev, id) == true) {
		int ipi_id = hcp_id_to_ipi_id(hcp_dev, id);

		dev_dbg(&pdev->dev, "%s cm4 is support !!!\n", __func__);
		ret = scp_ipi_send(ipi_id, buf, len, 0, SCP_A_ID);
	} else {
		int module_id = hcp_id_to_module_id(hcp_dev, id);

		mutex_lock(&hcp_dev->hcp_mutex[module_id]);

		timeout = jiffies + msecs_to_jiffies(HCP_TIMEOUT_MS);
		do {
			if (time_after(jiffies, timeout)) {
				dev_dbg(&pdev->dev,
					"mtk_hcp_ipi_send:IPI timeout!\n");
				mutex_unlock(&hcp_dev->hcp_mutex[module_id]);
				return -EIO;
			}
		} while (!atomic_read(&hcp_dev->ipi_done[module_id]));

		hcp_dev->hcp_id_ack[id] = false;
		memcpy((void *)hcp_dev->user_obj[module_id].share_data,
			buf, len);
		hcp_dev->user_obj[module_id].len = len;
		hcp_dev->user_obj[module_id].id = (int)id;
		atomic_set(&hcp_dev->ipi_got[module_id], 1);
		atomic_set(&hcp_dev->ipi_done[module_id], 0);
		wake_up(&hcp_dev->get_wq[module_id]);
		mutex_unlock(&hcp_dev->hcp_mutex[module_id]);
		dev_info(&pdev->dev,
			"%s message(%d)size(%d) send to user space !!!\n",
			__func__, id, len);

		if (!wait)
			return 0;

		/* wait for RED's ACK */
		timeout = msecs_to_jiffies(HCP_TIMEOUT_MS);
		ret = wait_event_timeout(hcp_dev->ack_wq[module_id],
			hcp_dev->hcp_id_ack[id], timeout);
		hcp_dev->hcp_id_ack[id] = false;
		if (ret == 0) {
			dev_dbg(&pdev->dev, "%s hcp id:%d ack time out !\n",
				__func__, id);
		} else if (-ERESTARTSYS == ret) {
			dev_dbg(&pdev->dev,
				"%s hcp id:%d ack wait interrupted !\n",
				__func__, id);
		} else {
			ret = 0;
		}
	}
	return ret;
}


int mtk_hcp_send(struct platform_device *pdev,
		 enum hcp_id id, void *buf,
		 unsigned int len)
{
	return hcp_send_internal(pdev, id, buf, len, SYNC_SEND);
}
EXPORT_SYMBOL_GPL(mtk_hcp_send);

int mtk_hcp_send_async(struct platform_device *pdev,
		 enum hcp_id id, void *buf,
		 unsigned int len)
{
	return hcp_send_internal(pdev, id, buf, len, ASYNC_SEND);
}
EXPORT_SYMBOL_GPL(mtk_hcp_send_async);

struct platform_device *mtk_hcp_get_plat_device(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *hcp_node;
	struct platform_device *hcp_pdev;

	dev_dbg(&pdev->dev, "- E. hcp get platform device.\n");


	hcp_node = of_parse_phandle(dev->of_node, "mediatek,hcp", 0);
	if (hcp_node == NULL) {
		dev_dbg(&pdev->dev, "%s can't get hcp node.\n", __func__);
		return NULL;
	}

	hcp_pdev = of_find_device_by_node(hcp_node);
	if (WARN_ON(hcp_pdev == NULL) == true) {
		dev_dbg(&pdev->dev, "%s hcp pdev failed.\n", __func__);
		of_node_put(hcp_node);
		return NULL;
	}

	return hcp_pdev;
}
EXPORT_SYMBOL_GPL(mtk_hcp_get_plat_device);

#if HCP_RESERVED_MEM
phys_addr_t mtk_hcp_mem_base_phys;
phys_addr_t mtk_hcp_mem_base_virt;
phys_addr_t mtk_hcp_mem_size;

static struct mtk_hcp_reserve_mblock mtk_hcp_reserve_mblock[] = {
	{
		.num = ISP_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x200000,  /*need 20MB*/
		.is_dma_buf = true,
	},
	{
		.num = DIP_MEM_FOR_HW_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x400000,   /*need more than 4MB*/
		.is_dma_buf = true,
	},
	{
		.num = DIP_MEM_FOR_SW_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x100000,   /*1MB*/
		.is_dma_buf = true, /* Kurt ToDo: shall be false */
	},
	{
		.num = MDP_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x400000,   /*4MB*/
		.is_dma_buf = true,
	},
	{
		.num = FD_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x100000,   /*1MB*/
		.is_dma_buf = true,
	},
};

phys_addr_t mtk_hcp_get_reserve_mem_phys(enum mtk_hcp_reserve_mem_id_t id)
{
	if (id >= NUMS_MEM_ID) {
		pr_debug("[HCP] no reserve memory for %d", id);
		return 0;
	} else
		return mtk_hcp_reserve_mblock[id].start_phys;
}
EXPORT_SYMBOL_GPL(mtk_hcp_get_reserve_mem_phys);

phys_addr_t mtk_hcp_get_reserve_mem_virt(enum mtk_hcp_reserve_mem_id_t id)
{
	if (id >= NUMS_MEM_ID) {
		pr_debug("[HCP] no reserve memory for %d", id);
		return 0;
	} else
		return (phys_addr_t)mtk_hcp_reserve_mblock[id].start_virt;
}
EXPORT_SYMBOL_GPL(mtk_hcp_get_reserve_mem_virt);

phys_addr_t mtk_hcp_get_reserve_mem_dma(enum mtk_hcp_reserve_mem_id_t id)
{
	if (id >= NUMS_MEM_ID) {
		pr_debug("[HCP] no reserve memory for %d", id);
		return 0;
	} else
		return mtk_hcp_reserve_mblock[id].start_dma;
}
EXPORT_SYMBOL_GPL(mtk_hcp_get_reserve_mem_dma);

phys_addr_t mtk_hcp_get_reserve_mem_size(enum mtk_hcp_reserve_mem_id_t id)
{
	if (id >= NUMS_MEM_ID) {
		pr_debug("[HCP] no reserve memory for %d", id);
		return 0;
	} else
		return mtk_hcp_reserve_mblock[id].size;
}
EXPORT_SYMBOL_GPL(mtk_hcp_get_reserve_mem_size);
#endif

static int mtk_hcp_open(struct inode *inode, struct file *file)
{
	struct mtk_hcp *hcp_dev;
#if HCP_RESERVED_MEM
	enum mtk_hcp_reserve_mem_id_t id;
#ifdef CONFIG_MTK_M4U
	int ret = 0;
	struct m4u_client_t *m4u_client = NULL;
	uint32_t mva;
#endif

#endif

	hcp_dev = container_of(inode->i_cdev, struct mtk_hcp, hcp_cdev);
	dev_info(hcp_dev->dev, "open inode->i_cdev = 0x%p\n", inode->i_cdev);

	/*  */
	file->private_data = hcp_dev;

	hcp_dev->is_open = true;
	cm4_support_table_init(hcp_dev);

#if HCP_RESERVED_MEM

#ifdef CONFIG_MTK_M4U
	m4u_client = m4u_create_client();
#endif
	//allocate reserved memory
	for (id = 0; id < NUMS_MEM_ID; id++) {
#ifdef CONFIG_MTK_M4U
		if (m4u_client == NULL) {
			pr_debug("can not alloc mva for %d module", id);
			break;
		}

		ret = m4u_alloc_mva(m4u_client, 0,
			(unsigned long)mtk_hcp_reserve_mblock[id].start_virt,
			NULL,
			mtk_hcp_reserve_mblock[id].size,
			M4U_PROT_READ | M4U_PROT_WRITE,
			0,
			&mva);
		mtk_hcp_reserve_mblock[id].start_dma = mva;
 #endif
		pr_info("%s id:%d phys:0x%llx virt:0x%llx dma:0x%llx\n",
			__func__, id,
			mtk_hcp_get_reserve_mem_phys(id),
			mtk_hcp_get_reserve_mem_virt(id),
			mtk_hcp_get_reserve_mem_dma(id));
	}
#endif

	dev_info(hcp_dev->dev, "- X. hcp open.\n");

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
	int reserved_id = -1;
	long length = 0;
	unsigned int pfn = 0x0;

	/* dealing with register remap */
	length = vma->vm_end - vma->vm_start;
	/*  */
	pfn = vma->vm_pgoff << PAGE_SHIFT;
	switch (pfn) {
	case ISP_UNI_A_BASE_HW:
	case ISP_A_BASE_HW:
	case ISP_B_BASE_HW:
	case ISP_C_BASE_HW:
		break;
	case DIP_BASE_HW:
		break;
	case FD_BASE_HW:
		break;
#if HCP_RESERVED_MEM
	case START_ISP_MEM_ADDR:
		reserved_id = ISP_MEM_ID;
		break;
	case START_DIP_MEM_FOR_HW_ADDR:
		reserved_id = DIP_MEM_FOR_HW_ID;
		break;
	case START_DIP_MEM_FOR_SW_ADDR:
		reserved_id = DIP_MEM_FOR_SW_ID;
		break;
	case START_MDP_MEM_ADDR:
		reserved_id = MDP_MEM_ID;
		break;
	case START_FD_MEM_ADDR:
		reserved_id = FD_MEM_ID;
		break;
#endif
	}

	if (pfn) {
		if (reserved_id != -1) {
			int start_phys =
				mtk_hcp_reserve_mblock[reserved_id].start_phys;

			vma->vm_pgoff = (unsigned long)
				(start_phys >> PAGE_SHIFT);
			vma->vm_page_prot =
				pgprot_writecombine(vma->vm_page_prot);
		} else {
			vma->vm_page_prot =
				pgprot_noncached(vma->vm_page_prot);
		}
		goto remap;
	}

	/* dealing with share memory */
	hcp_dev->extmem.d_va = kmalloc(length, GFP_KERNEL);
	if (hcp_dev->extmem.d_va != NULL) {
		hcp_dev->extmem.d_pa = virt_to_phys(hcp_dev->extmem.d_va);
		vma->vm_pgoff = (unsigned long)
			(hcp_dev->extmem.d_pa >> PAGE_SHIFT);
		dev_dbg(hcp_dev->dev, "sharememory va:0x%p pa:0x%llx",
			hcp_dev->extmem.d_va, hcp_dev->extmem.d_pa);
	} else {
		dev_dbg(hcp_dev->dev, " %s Allocate share buffer fail !!!\n",
			__func__);
		return -ENOMEM;

	}

remap:
	if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
		length, vma->vm_page_prot) != 0) {
		return -EAGAIN;
	}

	return 0;
}

static int mtk_hcp_get_data(struct mtk_hcp *hcp_dev, unsigned long arg)
{
	struct share_buf share_buff_data;
	int module_id = 0;
	int ret = 0;

	ret = (long)copy_from_user(&share_buff_data, (unsigned char *)arg,
				   (unsigned long)sizeof(struct share_buf));
	module_id = share_buff_data.id;

	atomic_set(&hcp_dev->ipi_done[module_id], 1);
	dev_info(hcp_dev->dev, "%s ipi_done[%d] = %d\n", __func__, module_id,
		atomic_read(&hcp_dev->ipi_got[module_id]));
	ret = wait_event_freezable(hcp_dev->get_wq[module_id],
		atomic_read(&hcp_dev->ipi_got[module_id]));
	if (ret != 0) {
		dev_dbg(hcp_dev->dev, "%s wait event return %d\n",
			__func__, ret);
		return ret;
	}

	ret = copy_to_user((void *)arg, &hcp_dev->user_obj[module_id],
		(unsigned long)sizeof(struct share_buf));
	dev_info(hcp_dev->dev, "%s copy data to user %d\n", __func__, ret);
	if (ret != 0) {
		dev_info(hcp_dev->dev, "%s(%d) Copy data to user failed!\n",
			__func__, __LINE__);
		ret = -EINVAL;
	}
	atomic_set(&hcp_dev->ipi_got[module_id], 0);
	return ret;
}

static void module_notify(struct mtk_hcp *hcp_dev,
	struct share_buf *user_data_addr)
{
	dev_dbg(hcp_dev->dev, "%s with message id:%d\n",
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
	int module_id = hcp_id_to_module_id(hcp_dev, user_data_addr->id);

	dev_dbg(hcp_dev->dev, "%s\n", __func__);
	hcp_dev->hcp_id_ack[user_data_addr->id] = true;
	wake_up(&hcp_dev->ack_wq[module_id]);
}

static long mtk_hcp_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	long ret = -1;
	//void *mem_priv;
	//unsigned char *user_data_addr = NULL;
	struct mtk_hcp *hcp_dev = (struct mtk_hcp *)file->private_data;

	switch (cmd) {
	case HCP_GET_OBJECT:
		ret = mtk_hcp_get_data(hcp_dev, arg);
		break;
	case HCP_NOTIFY:
		{
			struct share_buf  user_data_addr;

			ret = (long)copy_from_user(&user_data_addr, (void *)arg,
				sizeof(struct share_buf));
			module_notify(hcp_dev, &user_data_addr);
			ret = 0;
		}
		break;
	case HCP_COMPLETE:
		{
			struct share_buf  user_data_addr;

			ret = (long)copy_from_user(&user_data_addr, (void *)arg,
				sizeof(struct share_buf));
			module_notify(hcp_dev, &user_data_addr);
			module_wake_up(hcp_dev, &user_data_addr);
			ret = 0;
		}
		break;
	case HCP_WAKEUP:
		{
			struct share_buf  user_data_addr;

			ret = (long)copy_from_user(&user_data_addr, (void *)arg,
				sizeof(struct share_buf));
			module_wake_up(hcp_dev, &user_data_addr);
			ret = 0;
		}
		break;
	default:
		dev_info(hcp_dev->dev, "Invalid cmd_type:%d cmd_number:%d.\n",
			_IOC_TYPE(cmd), _IOC_NR(cmd));
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
		dev_info(hcp_dev->dev, "%s: Invalid cmd_number %d.\n",
			__func__, _IOC_NR(cmd));
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

static int mtk_hcp_probe(struct platform_device *pdev)
{
	struct mtk_hcp *hcp_dev;
	int ret = 0;
	int i = 0;
#if HCP_RESERVED_MEM
	enum mtk_hcp_reserve_mem_id_t id;
	dma_addr_t dma_handle;
#endif
	dev_dbg(&pdev->dev, "- E. hcp driver probe.\n");

	hcp_dev = devm_kzalloc(&pdev->dev, sizeof(*hcp_dev), GFP_KERNEL);
	if (hcp_dev == NULL)
		return -ENOMEM;

	hcp_mtkdev = hcp_dev;
	hcp_dev->dev = &pdev->dev;
	platform_set_drvdata(pdev, hcp_dev);
	dev_set_drvdata(&pdev->dev, hcp_dev);


	hcp_dev->is_open = false;
	for (i = 0; i < MODULE_MAX_ID; i++) {
		mutex_init(&hcp_dev->hcp_mutex[i]);

		init_waitqueue_head(&hcp_dev->ack_wq[i]);
		init_waitqueue_head(&hcp_dev->get_wq[i]);
		memset(&hcp_dev->user_obj[i], 0,
			sizeof(struct share_buf));

		atomic_set(&hcp_dev->ipi_got[i], 0);
		atomic_set(&hcp_dev->ipi_done[i], 0);
	}
	/* init character device */

	ret = alloc_chrdev_region(&hcp_dev->hcp_devno, 0, 1, HCP_DEVNAME);
	if (ret < 0) {
		dev_dbg(&pdev->dev, "alloc_chrdev_region failed err= %d", ret);
		goto err_alloc;
	}

	cdev_init(&hcp_dev->hcp_cdev, &hcp_fops);
	hcp_dev->hcp_cdev.owner = THIS_MODULE;

	ret = cdev_add(&hcp_dev->hcp_cdev, hcp_dev->hcp_devno, 1);
	if (ret < 0) {
		dev_dbg(&pdev->dev, "cdev_add fail  err= %d", ret);
		goto err_add;
	}

	hcp_dev->hcp_class = class_create(THIS_MODULE, "mtk_hcp_driver");
	if (IS_ERR(hcp_dev->hcp_class) == true) {
		ret = (int)PTR_ERR(hcp_dev->hcp_class);
		dev_dbg(&pdev->dev, "class create fail  err= %d", ret);
		goto err_add;
	}

	hcp_dev->hcp_device = device_create(hcp_dev->hcp_class, NULL,
		hcp_dev->hcp_devno, NULL, HCP_DEVNAME);
	if (IS_ERR(hcp_dev->hcp_device) == true) {
		ret = (int)PTR_ERR(hcp_dev->hcp_device);
		dev_dbg(&pdev->dev, "device create fail  err= %d", ret);
		goto err_device;
	}

	dev_dbg(&pdev->dev, "- X. hcp driver probe success.\n");

#if HCP_RESERVED_MEM
	//allocate reserved memory
	for (id = 0; id < NUMS_MEM_ID; id++) {
		if (mtk_hcp_reserve_mblock[id].is_dma_buf) {
			mtk_hcp_reserve_mblock[id].start_virt =
				dma_alloc_coherent(hcp_dev->dev,
					mtk_hcp_reserve_mblock[id].size,
					&dma_handle, GFP_KERNEL);
			/*
			 *virt_to_phys() only valid for liinear address
			 * should not use in dma_alloc_coheret API.
			 */
			mtk_hcp_reserve_mblock[id].start_phys =
				(phys_addr_t)dma_handle;
			mtk_hcp_reserve_mblock[id].start_dma =
				(phys_addr_t)dma_handle;
		} else {
			mtk_hcp_reserve_mblock[id].start_virt =
				kzalloc(mtk_hcp_reserve_mblock[id].size,
					GFP_KERNEL);
			mtk_hcp_reserve_mblock[id].start_phys =
				virt_to_phys(
					mtk_hcp_reserve_mblock[id].start_virt);
			mtk_hcp_reserve_mblock[id].start_dma = 0;
		}
		pr_debug("%s id:%d phys:0x%llx virt:0x%llx dma:0x%llx\n",
			__func__, id,
			mtk_hcp_get_reserve_mem_phys(id),
			mtk_hcp_get_reserve_mem_virt(id),
			mtk_hcp_get_reserve_mem_dma(id));
	}
#endif
	return 0;

err_device:
	class_destroy(hcp_dev->hcp_class);
err_add:
	cdev_del(&hcp_dev->hcp_cdev);
err_alloc:
	unregister_chrdev_region(hcp_dev->hcp_devno, 1);
	for (i = 0; i < MODULE_MAX_ID; i++)
		mutex_destroy(&hcp_dev->hcp_mutex[i]);

	devm_kfree(&pdev->dev, hcp_dev);

	dev_dbg(&pdev->dev, "- X. hcp driver probe fail.\n");

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

	if (hcp_dev->is_open == true) {
		filp_close(hcp_dev->file, NULL);
		hcp_dev->is_open = false;
	}
	devm_kfree(&pdev->dev, hcp_dev);

	cdev_del(&hcp_dev->hcp_cdev);
	unregister_chrdev_region(hcp_dev->hcp_devno, 1);

	dev_dbg(&pdev->dev, "- X. hcp driver remove.\n");
	return 0;
}

static struct platform_driver mtk_hcp_driver = {
	.probe	= mtk_hcp_probe,
	.remove	= mtk_hcp_remove,
	.driver	= {
		.name	= HCP_DEVNAME,
		.owner	= THIS_MODULE,
		.of_match_table = mtk_hcp_match,
	},
};

module_platform_driver(mtk_hcp_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Mediatek hetero control process driver");
