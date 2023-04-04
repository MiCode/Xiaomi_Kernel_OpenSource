// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 MediaTek Inc.
 */

#define pr_fmt(fmt)    "[m4u_debug] " fmt

#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/export.h>
#include <dt-bindings/memory/mtk-memory-port.h>
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <aee.h>
#endif

#include "mtk_iommu.h"
#include "iommu_tz_sec.h"
#include "iommu_tz_debug.h"
#if IS_ENABLED(CONFIG_MTK_TRUSTED_MEMORY_SUBSYSTEM)
#include <public/trusted_mem_api.h>
#endif

#define ERROR_LARB_PORT_ID		0xFFFF
#define F_MMU_INT_TF_MSK		GENMASK(11, 2)

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#define m4u_aee_print(string, args...) do {\
		char m4u_name[100];\
		snprintf(m4u_name, 100, "[M4U]"string, ##args); \
	aee_kernel_warning_api(__FILE__, __LINE__, \
		DB_OPT_MMPROFILE_BUFFER | DB_OPT_DUMP_DISPLAY, \
		m4u_name, "[M4U] error"string, ##args); \
	pr_err("[M4U] error:"string, ##args);  \
	} while (0)

#else
#define m4u_aee_print(string, args...) do {\
		char m4u_name[100];\
		snprintf(m4u_name, 100, "[M4U]"string, ##args); \
	pr_err("[M4U] error:"string, ##args);  \
	} while (0)
#endif

#define M4U0_PORT_INIT(name, slave, larb, port)  {\
		name, 0, slave, larb, port, (((larb)<<7)|((port)<<2)),\
}

#define MM_IOMMU_PORT_INIT(name, m4u_id, larb_id, tf_larb, port_id)  {\
	name, m4u_id, larb_id, port_id, (((tf_larb)<<7)|((port_id)<<2)), 1\
}

#define mmu_translation_log_format \
	"CRDISPATCH_KEY:M4U_%s\ntranslation fault:port=%s,mva=0x%x,pa=0x%x\n"

#define iommu_dump(file, fmt, args...) \
	do {\
		if (file)\
			seq_printf(file, fmt, ##args);\
		else\
			pr_info(fmt, ##args);\
	} while (0)

struct mtk_iommu_cb {
	int port;
	mtk_iommu_fault_callback_t fault_fn;
	void *fault_data;
};

struct peri_iommu_data {
	enum peri_iommu id;
	u32 bus_id;
};

struct mtk_m4u_data {
	struct device			*dev;
	struct proc_dir_entry	*debug_root;
	struct mtk_iommu_cb		*m4u_cb;
	const struct mtk_m4u_plat_data	*plat_data;
};

struct mtk_iommu_port {
	char *name;
	unsigned m4u_id: 2;
	unsigned larb_id: 6;
	unsigned port_id: 8;
	unsigned tf_id: 14;     /* 14 bits */
	bool enable_tf;
	unsigned int port_type;
};

struct mtk_m4u_plat_data {
	struct peri_iommu_data		*peri_data;
	const struct mtk_iommu_port	*port_list[TYPE_NUM];
	u32				port_nr[TYPE_NUM];
	const struct mau_config_info	*mau_config;
	u32				mau_config_nr;
	u32				mm_tf_ccu_support;
	int (*mm_tf_is_gce_videoup)(u32 port_tf, u32 vld_tf);
	char *(*peri_tf_analyse)(enum peri_iommu bus_id, u32 id);
};

struct iova_info {
	struct device *dev;
	dma_addr_t iova;
	size_t size;
	struct list_head list_node;
};

struct iova_buf_list {
	struct list_head head;
	struct mutex lock;
};

static const struct mtk_iommu_port iommu_port_mt6765[] = {
	/*Larb0 */
	MM_IOMMU_PORT_INIT("L0_DISP_OVL0", 0, 0, 0x0, 0),
	MM_IOMMU_PORT_INIT("L0_DISP_2L_OVL0_LARB0", 0, 0, 0x0, 1),
	MM_IOMMU_PORT_INIT("L0_DISP_RDMA0", 0, 0, 0x0, 2),
	MM_IOMMU_PORT_INIT("L0_DISP_WDMA0", 0, 0, 0x0, 3),
	MM_IOMMU_PORT_INIT("L0_MDP_RDMA0", 0, 0, 0x0, 4),
	MM_IOMMU_PORT_INIT("L0_MDP_WDMA0", 0, 0, 0x0, 5),
	MM_IOMMU_PORT_INIT("L0_MDP_WROT0", 0, 0, 0x0, 6),
	MM_IOMMU_PORT_INIT("L0_DISP_FAKE0", 0, 0, 0x0, 7),
	/*Larb1 */
	MM_IOMMU_PORT_INIT("L1_VENC_RCPU", 0, 1, 0x1, 0),
	MM_IOMMU_PORT_INIT("L1_VENC_REC", 0, 1, 0x1, 1),
	MM_IOMMU_PORT_INIT("L1_VENC_BSDMA", 0, 1, 0x1, 2),
	MM_IOMMU_PORT_INIT("L1_VENC_SV_COMV", 0, 1, 0x1, 3),
	MM_IOMMU_PORT_INIT("L1_VENC_RD_COMV", 0, 1, 0x1, 4),
	MM_IOMMU_PORT_INIT("L1_JPGENC_RDMA", 0, 1, 0x1, 5),
	MM_IOMMU_PORT_INIT("L1_JPGENC_BSDMA", 0, 1, 0x1, 6),
	MM_IOMMU_PORT_INIT("L1_VENC_CUR_LUMA", 0, 1, 0x1, 7),
	MM_IOMMU_PORT_INIT("L1_VENC_CUR_CHROMA", 0, 1, 0x1, 8),
	MM_IOMMU_PORT_INIT("L1_VENC_REF_LUMA", 0, 1, 0x1, 9),
	MM_IOMMU_PORT_INIT("L1_VENC_REF_CHROMA", 0, 1, 0x1, 10),
	/*Larb2 */
	MM_IOMMU_PORT_INIT("L2_CAM_IMGI", 0, 2, 0x2, 0),
	MM_IOMMU_PORT_INIT("L2_CAM_IMG2O", 0, 2, 0x2, 1),
	MM_IOMMU_PORT_INIT("L2_CAM_IMG3O", 0, 2, 0x2, 2),
	MM_IOMMU_PORT_INIT("L2_CAM_VIPI", 0, 2, 0x2, 3),
	MM_IOMMU_PORT_INIT("L2_CAM_LCEI", 0, 2, 0x2, 4),
	MM_IOMMU_PORT_INIT("L2_CAM_FD_RP", 0, 2, 0x2, 5),
	MM_IOMMU_PORT_INIT("L2_CAM_FD_WR", 0, 2, 0x2, 6),
	MM_IOMMU_PORT_INIT("L2_CAM_FD_RB", 0, 2, 0x2, 7),
	MM_IOMMU_PORT_INIT("L2_CAM_DPE_RDMA", 0, 2, 0x2, 8),
	MM_IOMMU_PORT_INIT("L2_CAM_DPE_WDMA", 0, 2, 0x2, 9),
	MM_IOMMU_PORT_INIT("L2_CAM_RSC_RDMA0", 0, 2, 0x2, 10),
	MM_IOMMU_PORT_INIT("L2_CAM_RSC_WDMA", 0, 2, 0x2, 11),
	/*Larb3 */
	MM_IOMMU_PORT_INIT("L3_CAM_IMGO", 0, 3, 0x3, 0),
	MM_IOMMU_PORT_INIT("L3_CAM_RRZO", 0, 3, 0x3, 1),
	MM_IOMMU_PORT_INIT("L3_CAM_AAO", 0, 3, 0x3, 2),
	MM_IOMMU_PORT_INIT("L3_CAM_AFO", 0, 3, 0x3, 3),
	MM_IOMMU_PORT_INIT("L3_CAM_LSCI0", 0, 3, 0x3, 4),
	MM_IOMMU_PORT_INIT("L3_CAM_LSCI1", 0, 3, 0x3, 5),
	MM_IOMMU_PORT_INIT("L3_CAM_PDO", 0, 3, 0x3, 6),
	MM_IOMMU_PORT_INIT("L3_CAM_BPCI", 0, 3, 0x3, 7),
	MM_IOMMU_PORT_INIT("L3_CAM_LCSO", 0, 3, 0x3, 8),
	MM_IOMMU_PORT_INIT("L3_CAM_RSSO_A", 0, 3, 0x3, 9),
	MM_IOMMU_PORT_INIT("L3_CAM_RSSO_B", 0, 3, 0x3, 10),
	MM_IOMMU_PORT_INIT("L3_CAM_UFEO", 0, 3, 0x3, 11),
	MM_IOMMU_PORT_INIT("L3_CAM_SOCO", 0, 3, 0x3, 12),
	MM_IOMMU_PORT_INIT("L3_CAM_SOC1", 0, 3, 0x3, 13),
	MM_IOMMU_PORT_INIT("L3_CAM_SOC2", 0, 3, 0x3, 14),
	MM_IOMMU_PORT_INIT("L3_CAM_CCUI", 0, 3, 0x3, 15),
	MM_IOMMU_PORT_INIT("L3_CAM_CCUO", 0, 3, 0x3, 16),
	MM_IOMMU_PORT_INIT("L3_CAM_CACI", 0, 3, 0x3, 17),
	MM_IOMMU_PORT_INIT("L3_CAM_RAWI_A", 0, 3, 0x3, 18),
	MM_IOMMU_PORT_INIT("L3_CAM_RAWI_B", 0, 3, 0x3, 19),
	MM_IOMMU_PORT_INIT("L3_CAM_CCUG", 0, 3, 0x3, 20),
	MM_IOMMU_PORT_INIT("UNKNOWN", 0, 0, 0x0, 0)
};

static const struct mtk_iommu_port iommu_port_mt6768[] = {
	/*Larb0 */
	MM_IOMMU_PORT_INIT("L0_DISP_OVL0", 0, 0, 0x0, 0),
	MM_IOMMU_PORT_INIT("L0_DISP_2L_OVL0_LARB0", 0, 0, 0x0, 1),
	MM_IOMMU_PORT_INIT("L0_DISP_RDMA0", 0, 0, 0x0, 2),
	MM_IOMMU_PORT_INIT("L0_DISP_WDMA0", 0, 0, 0x0, 3),
	MM_IOMMU_PORT_INIT("L0_MDP_RDMA0", 0, 0, 0x0, 4),
	MM_IOMMU_PORT_INIT("L0_MDP_WDMA0", 0, 0, 0x0, 5),
	MM_IOMMU_PORT_INIT("L0_MDP_WROT0", 0, 0, 0x0, 6),
	MM_IOMMU_PORT_INIT("L0_DISP_FAKE0", 0, 0, 0x0, 7),
	/*Larb1 */
	MM_IOMMU_PORT_INIT("L1_VDEC_MC", 0, 1, 0x1, 0),
	MM_IOMMU_PORT_INIT("L1_VDEC_PP", 0, 1, 0x1, 1),
	MM_IOMMU_PORT_INIT("L1_VDEC_VLD", 0, 1, 0x1, 2),
	MM_IOMMU_PORT_INIT("L1_VDEC_VLD2", 0, 1, 0x1, 3),
	MM_IOMMU_PORT_INIT("L1_VDEC_AVC_MV", 0, 1, 0x1, 4),
	MM_IOMMU_PORT_INIT("L1_VDEC_PRED_RD", 0, 1, 0x1, 5),
	MM_IOMMU_PORT_INIT("L1_VDEC_PRED_WR", 0, 1, 0x1, 6),
	MM_IOMMU_PORT_INIT("L1_VDEC_PPWRAP", 0, 1, 0x1, 7),
	MM_IOMMU_PORT_INIT("L1_VDEC_TILE", 0, 1, 0x1, 8),
	/*Larb2 */
	MM_IOMMU_PORT_INIT("L2_CAM_IMGI", 0, 2, 0x2, 0),
	MM_IOMMU_PORT_INIT("L2_CAM_IMG2O", 0, 2, 0x2, 1),
	MM_IOMMU_PORT_INIT("L2_CAM_IMG3O", 0, 2, 0x2, 2),
	MM_IOMMU_PORT_INIT("L2_CAM_VIPI", 0, 2, 0x2, 3),
	MM_IOMMU_PORT_INIT("L2_CAM_LCEI", 0, 2, 0x2, 4),
	MM_IOMMU_PORT_INIT("L2_CAM_FD_RP", 0, 2, 0x2, 5),
	MM_IOMMU_PORT_INIT("L2_CAM_FD_WR", 0, 2, 0x2, 6),
	MM_IOMMU_PORT_INIT("L2_CAM_FD_RB", 0, 2, 0x2, 7),
	MM_IOMMU_PORT_INIT("L2_CAM_DPE_RDMA", 0, 2, 0x2, 8),
	MM_IOMMU_PORT_INIT("L2_CAM_DPE_WDMA", 0, 2, 0x2, 9),
	MM_IOMMU_PORT_INIT("L2_CAM_RSC_RDMA", 0, 2, 0x2, 10),
	MM_IOMMU_PORT_INIT("L2_CAM_RSC_WDMA", 0, 2, 0x2, 11),
	/*Larb3 */
	MM_IOMMU_PORT_INIT("L3_CAM_IMGO", 0, 3, 0x3, 0),
	MM_IOMMU_PORT_INIT("L3_CAM_RRZO", 0, 3, 0x3, 1),
	MM_IOMMU_PORT_INIT("L3_CAM_AAO", 0, 3, 0x3, 2),
	MM_IOMMU_PORT_INIT("L3_CAM_AFO", 0, 3, 0x3, 3),
	MM_IOMMU_PORT_INIT("L3_CAM_LSCI0", 0, 3, 0x3, 4),
	MM_IOMMU_PORT_INIT("L3_CAM_LSCI1", 0, 3, 0x3, 5),
	MM_IOMMU_PORT_INIT("L3_CAM_PDO", 0, 3, 0x3, 6),
	MM_IOMMU_PORT_INIT("L3_CAM_BPCI", 0, 3, 0x3, 7),
	MM_IOMMU_PORT_INIT("L3_CAM_LCSO", 0, 3, 0x3, 8),
	MM_IOMMU_PORT_INIT("L3_CAM_RSSO_A", 0, 3, 0x3, 9),
	MM_IOMMU_PORT_INIT("L3_CAM_RSSO_B", 0, 3, 0x3, 10),
	MM_IOMMU_PORT_INIT("L3_CAM_UFEO", 0, 3, 0x3, 11),
	MM_IOMMU_PORT_INIT("L3_CAM_SOCO", 0, 3, 0x3, 12),
	MM_IOMMU_PORT_INIT("L3_CAM_SOC1", 0, 3, 0x3, 13),
	MM_IOMMU_PORT_INIT("L3_CAM_SOC2", 0, 3, 0x3, 14),
	MM_IOMMU_PORT_INIT("L3_CAM_CCUI", 0, 3, 0x3, 15),
	MM_IOMMU_PORT_INIT("L3_CAM_CCUO", 0, 3, 0x3, 16),
	MM_IOMMU_PORT_INIT("L3_CAM_CACI", 0, 3, 0x3, 17),
	MM_IOMMU_PORT_INIT("L3_CAM_RAWI_A", 0, 3, 0x3, 18),
	MM_IOMMU_PORT_INIT("L3_CAM_RAWI_B", 0, 3, 0x3, 19),
	MM_IOMMU_PORT_INIT("L3_CAM_CCUG", 0, 3, 0x3, 20),
	/*Larb4 */
	MM_IOMMU_PORT_INIT("L4_VENC_RCPU", 0, 4, 0x4, 0),
	MM_IOMMU_PORT_INIT("L4_VENC_REC", 0, 4, 0x4, 1),
	MM_IOMMU_PORT_INIT("L4_VENC_BSDMA", 0, 4, 0x4, 2),
	MM_IOMMU_PORT_INIT("L4_VENC_SV_COMV", 0, 4, 0x4, 3),
	MM_IOMMU_PORT_INIT("L4_VENC_RD_COMV", 0, 4, 0x4, 4),
	MM_IOMMU_PORT_INIT("L4_JPGENC_RDMA", 0, 4, 0x4, 5),
	MM_IOMMU_PORT_INIT("L4_JPGENC_BSDMA", 0, 4, 0x4, 6),
	MM_IOMMU_PORT_INIT("L4_VENC_CUR_LUMA", 0, 4, 0x4, 7),
	MM_IOMMU_PORT_INIT("L4_VENC_CUR_CHROMA", 0, 4, 0x4, 8),
	MM_IOMMU_PORT_INIT("L4_VENC_REF_LUMA", 0, 4, 0x4, 9),
	MM_IOMMU_PORT_INIT("L4_VENC_REF_CHROMA", 0, 4, 0x4, 10),

	MM_IOMMU_PORT_INIT("UNKNOWN", 0, 0, 0x0, 0)
};

static struct mtk_m4u_data *m4u_data;
static struct iova_buf_list iova_list;

static int mtk_iommu_debug_help(struct seq_file *s)
{
	iommu_dump(s, "iommu TEE debug file:\n");
	iommu_dump(s, "help: description debug file and command\n");
	iommu_dump(s, "debug: iommu_tee main debug file, receive debug command\n");

	iommu_dump(s, "iommu debug command:\n");
	iommu_dump(s, "echo 1 > /proc/iommu_tee/debug: iommu debug help\n");
	iommu_dump(s, "echo 2 > /proc/iommu_tee/debug: mm translation fault test\n");
	iommu_dump(s, "echo 50 > /proc/iommu_tee/debug: TEE m4u_sec-init UT\n");
	return 0;
}

void mtk_iova_dbg_alloc(struct device *dev, dma_addr_t iova, size_t size)
{
	struct iova_info *iova_buf;

	iova_buf = kzalloc(sizeof(*iova_buf), GFP_KERNEL);
	if (!iova_buf)
		return;

	iova_buf->dev = dev;
	iova_buf->iova = iova;
	iova_buf->size = size;
	mutex_lock(&iova_list.lock);
	list_add(&iova_buf->list_node, &iova_list.head);
	mutex_unlock(&iova_list.lock);
}

void mtk_iova_dbg_free(dma_addr_t iova, size_t size)
{
	struct iova_info *plist;
	struct iova_info *tmp_plist;

	mutex_lock(&iova_list.lock);
	list_for_each_entry_safe(plist, tmp_plist,
				 &iova_list.head, list_node) {
		if (plist->iova == iova && plist->size == size) {
			list_del(&plist->list_node);
			kfree(plist);
			break;
		}
	}
	mutex_unlock(&iova_list.lock);
}

void mtk_iova_dbg_dump(struct seq_file *s)
{
	struct iova_info *plist = NULL;
	struct iova_info *n = NULL;

	mutex_lock(&iova_list.lock);

	if (s)
		seq_printf(s, "%18s %8s %18s\n", "iova", "size", "dev");
	else
		pr_info("%18s %8s %18s\n", "iova", "size", "dev");
	list_for_each_entry_safe(plist, n, &iova_list.head,
				 list_node) {
		if (s)
			seq_printf(s, "%pa %8zu %18s\n",
				   &plist->iova,
				   plist->size,
				   dev_name(plist->dev));
		else
			pr_info("%pa %8zu %18s\n",
				   &plist->iova,
				   plist->size,
				   dev_name(plist->dev));
	}
	mutex_unlock(&iova_list.lock);
}

static int m4u_debug_set(void *data, u64 val)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_MTK_TRUSTED_MEMORY_SUBSYSTEM)
	u32 sec_handle = 0;
	u32 refcount = 0;
	u32 size = 0x1000;
	//struct ssheap_buf_info *ssheap = NULL;
#endif
	pr_info("%s:val=%llu\n", __func__, val);

	switch (val) {
	case 1:	/* show help info */
		ret = mtk_iommu_debug_help(NULL);
		break;
	case 2: /* dump iova info */
		mtk_iova_dbg_dump(NULL);
		break;
#if IS_ENABLED(CONFIG_TRUSTONIC_TEE_SUPPORT) || \
	IS_ENABLED(CONFIG_MICROTRUST_TEE_SUPPORT)
	case 50:
#if IS_ENABLED(CONFIG_MTK_TRUSTED_MEMORY_SUBSYSTEM)
		pr_info("%s: iommu tee ut begin\n", __func__);
		ret = trusted_mem_api_alloc(0, 0, &size, &refcount,
					    &sec_handle, "iommu_tee_ut", 0, NULL);
		if (ret == -ENOMEM) {
			pr_err("%s[%d] UT FAIL: out of memory\n",
			       __func__, __LINE__);
			return ret;
		}
		if (sec_handle <= 0) {
			pr_err("%s[%d] sec memory alloc error: handle %u\n",
			       __func__, __LINE__, sec_handle);
			return sec_handle;
		}
		m4u_sec_init();
		pr_info("%s: iommu tee ut end, m4u_sec_init done.\n", __func__);
#endif
		break;
#endif
	default:
		pr_err("%s error,val=%llu\n", __func__, val);
	}

	return 0;
}

static int m4u_debug_get(void *data, u64 *val)
{
	*val = 0;
	return 0;
}

DEFINE_PROC_ATTRIBUTE(m4u_debug_fops, m4u_debug_get, m4u_debug_set, "%llu\n");

#if IS_ENABLED(CONFIG_PROC_FS)
/* Define proc_ops: *_proc_show function will be called when file is opened */
#define DEFINE_PROC_FOPS_RO(name)				\
	static int name ## _proc_open(struct inode *inode,	\
		struct file *file)				\
	{							\
		return single_open(file, name ## _proc_show,	\
			PDE_DATA(inode));			\
	}							\
	static const struct proc_ops name = {			\
		.proc_open		= name ## _proc_open,	\
		.proc_read		= seq_read,		\
		.proc_lseek		= seq_lseek,		\
		.proc_release	= single_release,		\
	}

static int mtk_iommu_help_fops_proc_show(struct seq_file *s, void *unused)
{
	mtk_iommu_debug_help(s);
	return 0;
}

/* adb shell cat /proc/iommu_debug/help */
DEFINE_PROC_FOPS_RO(mtk_iommu_help_fops);
#endif

static int m4u_debug_init(struct mtk_m4u_data *data)
{
	struct proc_dir_entry *debug_file;

	data->debug_root = proc_mkdir("iommu_tee", NULL);

		if (IS_ERR_OR_NULL(data->debug_root))
			pr_err("failed to create iommu_tee dir\n");

		debug_file = proc_create_data("debug",
			S_IFREG | 0644, data->debug_root, &m4u_debug_fops, NULL);

		if (IS_ERR_OR_NULL(debug_file))
			pr_err("failed to create debug file\n");

		debug_file = proc_create_data("help",
			S_IFREG | 0644, data->debug_root, &mtk_iommu_help_fops, NULL);
		if (IS_ERR_OR_NULL(debug_file))
			pr_err("failed to proc_create help file\n");

	return 0;
}

static int mtk_m4u_dbg_probe(struct platform_device *pdev)
{
struct device *dev = &pdev->dev;
	u32 total_port;

	pr_info("%s sjr start\n", __func__);
	m4u_data = devm_kzalloc(dev, sizeof(struct mtk_m4u_data), GFP_KERNEL);
	if (!m4u_data)
		return -ENOMEM;

	m4u_data->dev = dev;
	m4u_data->plat_data = of_device_get_match_data(dev);
	total_port = m4u_data->plat_data->port_nr[MM_IOMMU] +
		     m4u_data->plat_data->port_nr[APU_IOMMU] +
		     m4u_data->plat_data->port_nr[PERI_IOMMU];
	m4u_data->m4u_cb = devm_kzalloc(dev, total_port *
		sizeof(struct mtk_iommu_cb), GFP_KERNEL);
	if (!m4u_data->m4u_cb)
		return -ENOMEM;

	m4u_debug_init(m4u_data);

	pr_info("%s sjr done\n", __func__);
	return 0;
}

static const struct mtk_m4u_plat_data mt6765_data = {
	.port_list[MM_IOMMU] = iommu_port_mt6765,
	.port_nr[MM_IOMMU]   = ARRAY_SIZE(iommu_port_mt6765),
};

static const struct mtk_m4u_plat_data mt6768_data = {
	.port_list[MM_IOMMU] = iommu_port_mt6768,
	.port_nr[MM_IOMMU]   = ARRAY_SIZE(iommu_port_mt6768),
};

static const struct of_device_id iommu_tee_dbg_of_ids[] = {
	{ .compatible = "mediatek,mt6765-iommu-tee-debug", .data = &mt6765_data},
	{ .compatible = "mediatek,mt6768-iommu-tee-debug", .data = &mt6768_data},
	{}
};

static struct platform_driver iommu_tee_dbg_drv = {
	.probe	= mtk_m4u_dbg_probe,
	.driver	= {
		.name = "iommu-tee-debug",
		.of_match_table = of_match_ptr(iommu_tee_dbg_of_ids),
	}
};

module_platform_driver(iommu_tee_dbg_drv);

MODULE_LICENSE("GPL v2");
