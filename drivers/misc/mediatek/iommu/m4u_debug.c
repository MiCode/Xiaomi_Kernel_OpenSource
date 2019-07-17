// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
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
#ifdef CONFIG_MTK_AEE_FEATURE
#include <aee.h>
#endif

#include "m4u_debug.h"

#define MTK_M4U_ID(larb, port)		(((larb) << 5) | (port))
#define MTK_M4U_TO_LARB(id)		(((id) >> 5) & 0xf)
/* PortID within the local arbiter */
#define MTK_M4U_TO_PORT(id)		((id) & 0x1f)

#define ERROR_LARB_PORT_ID		0xFFFF
#define F_MMU_INT_TF_MSK		GENMASK(11, 2)

#ifdef CONFIG_MTK_AEE_FEATURE
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

#define M4U0_PORT_INIT(name, slave, larb_id, smi_select_larb_id, port)  {\
		name, 0, slave, larb_id, port, \
		(((smi_select_larb_id)<<7)|((port)<<2)), 1\
}

#define M4U1_PORT_INIT(name, slave, larb_id, smi_select_larb_id, port)  {\
		name, 1, slave, larb_id, port, \
		(((smi_select_larb_id)<<7)|((port)<<2)), 1\
}

#define mmu_translation_log_format \
	"CRDISPATCH_KEY:M4U_%s\ntranslation fault:port=%s,mva=0x%x,pa=0x%x\n"

struct mtk_iommu_cb {
	mtk_iommu_fault_callback_t fault_fn;
	void *fault_data;
};

struct mtk_m4u_data {
	struct device			*dev;
	struct dentry			*debug_root;
	struct mtk_iommu_cb		*m4u_cb;
	const struct mtk_m4u_plat_data	*plat_data;
};

struct iova_info {
	struct device *dev;
	dma_addr_t iova;
	size_t size;
	struct list_head list_node;
};

struct mtk_iommu_port {
	char *name;
	unsigned m4u_id: 2;
	unsigned m4u_slave: 2;
	unsigned larb_id: 4;
	unsigned larb_port: 8;
	unsigned tf_id: 12;     /* 12 bits */
	bool enable_tf;
};

struct mtk_m4u_plat_data {
	const struct mtk_iommu_port	*port;
	u32				port_nr;
};

struct iova_buf_list {
	struct list_head head;
	struct mutex lock;
};

static const struct mtk_iommu_port iommu_port_mt6779[] = {
	/* larb0 -MMSYS-9 */
	M4U0_PORT_INIT("DISP_POSTMASK0", 0, 0, 0, 0),
	M4U0_PORT_INIT("DISP_OVL0_HDR", 0, 0, 0, 1),
	M4U0_PORT_INIT("DISP_OVL1_HDR", 0, 0, 0, 2),
	M4U0_PORT_INIT("DISP_OVL0", 0, 0, 0, 3),
	M4U0_PORT_INIT("DISP_OVL1", 0, 0, 0, 4),
	M4U0_PORT_INIT("DISP_PVRIC0", 0, 0, 0, 5),
	M4U0_PORT_INIT("DISP_RDMA0", 0, 0, 0, 6),
	M4U0_PORT_INIT("DISP_WDMA0", 0, 0, 0, 7),
	M4U0_PORT_INIT("DISP_FAKE0", 0, 0, 0, 8),
	/*larb1-MMSYS-14*/
	M4U0_PORT_INIT("DISP_OVL0_2L_HDR", 0, 1, 4, 0),
	M4U0_PORT_INIT("DISP_OVL1_2L_HDR", 0, 1, 4, 1),
	M4U0_PORT_INIT("DISP_OVL0_2L", 0, 1, 4, 2),
	M4U0_PORT_INIT("DISP_OVL1_2L", 0, 1, 4, 3),
	M4U0_PORT_INIT("DISP_RDMA1", 0, 1, 4, 4),
	M4U0_PORT_INIT("MDP_PVRIC0", 0, 1, 4, 5),
	M4U0_PORT_INIT("MDP_PVRIC1", 0, 1, 4, 6),
	M4U0_PORT_INIT("MDP_RDMA0", 0, 1, 4, 7),
	M4U0_PORT_INIT("MDP_RDMA1", 0, 1, 4, 8),
	M4U0_PORT_INIT("MDP_WROT0_R", 0, 1, 4, 9),
	M4U0_PORT_INIT("MDP_WROT0_W", 0, 1, 4, 10),
	M4U0_PORT_INIT("MDP_WROT1_R", 0, 1, 4, 11),
	M4U0_PORT_INIT("MDP_WROT1_W", 0, 1, 4, 12),
	M4U0_PORT_INIT("DISP_FAKE1", 0, 1, 4, 13),
	/*larb2-VDEC-12*/
	M4U0_PORT_INIT("VDEC_MC_EXT", 0, 2, 8, 0),
	M4U0_PORT_INIT("VDEC_UFO_EXT", 0, 2, 8, 1),
	M4U0_PORT_INIT("VDEC_PP_EXT", 0, 2, 8, 2),
	M4U0_PORT_INIT("VDEC_PRED_RD_EXT", 0, 2, 8, 3),
	M4U0_PORT_INIT("VDEC_PRED_WR_EXT", 0, 2, 8, 4),
	M4U0_PORT_INIT("VDEC_PPWRAP_EXT", 0, 2, 8, 5),
	M4U0_PORT_INIT("VDEC_TILE_EXT", 0, 2, 8, 6),
	M4U0_PORT_INIT("VDEC_VLD_EXT", 0, 2, 8, 7),
	M4U0_PORT_INIT("VDEC_VLD2_EXT", 0, 2, 8, 8),
	M4U0_PORT_INIT("VDEC_AVC_MV_EXT", 0, 2, 8, 9),
	M4U0_PORT_INIT("VDEC_UFO_ENC_EXT", 0, 2, 8, 10),
	M4U0_PORT_INIT("VDEC_RG_CTRL_DMA_EXT", 0, 2, 8, 11),
	/*larb3-VENC-19*/
	M4U0_PORT_INIT("VENC_RCPU", 0, 3, 12, 0),
	M4U0_PORT_INIT("VENC_REC", 0, 3, 12, 1),
	M4U0_PORT_INIT("VENC_BSDMA", 0, 3, 12, 2),
	M4U0_PORT_INIT("VENC_SV_COMV", 0, 3, 12, 3),
	M4U0_PORT_INIT("VENC_RD_COMV", 0, 3, 12, 4),

	M4U0_PORT_INIT("VENC_NBM_RDMA", 0, 3, 12, 5),
	M4U0_PORT_INIT("VENC_NBM_RDMA_LITE", 0, 3, 12, 6),
	M4U0_PORT_INIT("JPGENC_Y_RDMA", 0, 3, 12, 7),
	M4U0_PORT_INIT("JPGENC_C_RDMA", 0, 3, 12, 8),
	M4U0_PORT_INIT("JPGENC_Q_TABLE", 0, 3, 12, 9),

	M4U0_PORT_INIT("JPGENC_BSDMA", 0, 3, 12, 10),
	M4U0_PORT_INIT("JPGEDC_WDMA", 0, 3, 12, 11),
	M4U0_PORT_INIT("JPGEDC_BSDMA", 0, 3, 12, 12),
	M4U0_PORT_INIT("VENC_NBM_WDMA", 0, 3, 12, 13),
	M4U0_PORT_INIT("VENC_NBM_WDMA_LITE", 0, 3, 12, 14),

	M4U0_PORT_INIT("VENC_CUR_LUMA", 0, 3, 12, 15),
	M4U0_PORT_INIT("VENC_CUR_CHROMA", 0, 3, 12, 16),
	M4U0_PORT_INIT("VENC_REF_LUMA", 0, 3, 12, 17),
	M4U0_PORT_INIT("VENC_REF_CHROMA", 0, 3, 12, 18),
	/*larb4-dummy*/

	/*larb5-IMG-26*/
	M4U0_PORT_INIT("IMGI_D1", 0, 5, 16, 0),
	M4U0_PORT_INIT("IMGBI_D1", 0, 5, 16, 1),
	M4U0_PORT_INIT("DMGI_D1", 0, 5, 16, 2),
	M4U0_PORT_INIT("DEPI_D1", 0, 5, 16, 3),
	M4U0_PORT_INIT("LCEI_D1", 0, 5, 16, 4),
	M4U0_PORT_INIT("SMTI_D1", 0, 5, 16, 5),
	M4U0_PORT_INIT("SMTO_D2", 0, 5, 16, 6),
	M4U0_PORT_INIT("SMTO_D1", 0, 5, 16, 7),
	M4U0_PORT_INIT("CRZO_D1", 0, 5, 16, 8),
	M4U0_PORT_INIT("IMG3O_D1", 0, 5, 16, 9),

	M4U0_PORT_INIT("VIPI_D1", 0, 5, 16, 10),
	M4U0_PORT_INIT("WPE_A_RDMA1", 0, 5, 16, 11),
	M4U0_PORT_INIT("WPE_A_RDMA0", 0, 5, 16, 12),
	M4U0_PORT_INIT("WPE_A_WDMA", 0, 5, 16, 13),
	M4U0_PORT_INIT("TIMGO_D1", 0, 5, 16, 14),
	M4U0_PORT_INIT("MFB_RDMA0", 0, 5, 16, 15),
	M4U0_PORT_INIT("MFB_RDMA1", 0, 5, 16, 16),
	M4U0_PORT_INIT("MFB_RDMA2", 0, 5, 16, 17),
	M4U0_PORT_INIT("MFB_RDMA3", 0, 5, 16, 18),
	M4U0_PORT_INIT("MFB_WDMA", 0, 5, 16, 19),

	M4U0_PORT_INIT("RESERVED1", 0, 5, 16, 20),
	M4U0_PORT_INIT("RESERVED2", 0, 5, 16, 21),
	M4U0_PORT_INIT("RESERVED3", 0, 5, 16, 22),
	M4U0_PORT_INIT("RESERVED4", 0, 5, 16, 23),
	M4U0_PORT_INIT("RESERVED5", 0, 5, 16, 24),
	M4U0_PORT_INIT("RESERVED6", 0, 5, 16, 25),

	/*larb7-IPESYS-4*/
	M4U0_PORT_INIT("DVS_RDMA", 0, 7, 20, 0),
	M4U0_PORT_INIT("DVS_WDMA", 0, 7, 20, 1),
	M4U0_PORT_INIT("DVP_RDMA,", 0, 7, 20, 2),
	M4U0_PORT_INIT("DVP_WDMA,", 0, 7, 20, 3),

	/*larb8-IPESYS-10*/
	M4U0_PORT_INIT("FDVT_RDA", 0, 8, 21, 0),
	M4U0_PORT_INIT("FDVT_RDB", 0, 8, 21, 1),
	M4U0_PORT_INIT("FDVT_WRA", 0, 8, 21, 2),
	M4U0_PORT_INIT("FDVT_WRB", 0, 8, 21, 3),
	M4U0_PORT_INIT("FE_RD0", 0, 8, 21, 4),
	M4U0_PORT_INIT("FE_RD1", 0, 8, 21, 5),
	M4U0_PORT_INIT("FE_WR0", 0, 8, 21, 6),
	M4U0_PORT_INIT("FE_WR1", 0, 8, 21, 7),
	M4U0_PORT_INIT("RSC_RDMA0", 0, 8, 21, 8),
	M4U0_PORT_INIT("RSC_WDMA", 0, 8, 21, 9),

	/*larb9-CAM-24*/
	M4U0_PORT_INIT("CAM_IMGO_R1_C", 0, 9, 28, 0),
	M4U0_PORT_INIT("CAM_RRZO_R1_C", 0, 9, 28, 1),
	M4U0_PORT_INIT("CAM_LSCI_R1_C", 0, 9, 28, 2),
	M4U0_PORT_INIT("CAM_BPCI_R1_C", 0, 9, 28, 3),
	M4U0_PORT_INIT("CAM_YUVO_R1_C", 0, 9, 28, 4),
	M4U0_PORT_INIT("CAM_UFDI_R2_C", 0, 9, 28, 5),
	M4U0_PORT_INIT("CAM_RAWI_R2_C", 0, 9, 28, 6),
	M4U0_PORT_INIT("CAM_RAWI_R5_C", 0, 9, 28, 7),
	M4U0_PORT_INIT("CAM_CAMSV_1", 0, 9, 28, 8),
	M4U0_PORT_INIT("CAM_CAMSV_2", 0, 9, 28, 9),

	M4U0_PORT_INIT("CAM_CAMSV_3", 0, 9, 28, 10),
	M4U0_PORT_INIT("CAM_CAMSV_4", 0, 9, 28, 11),
	M4U0_PORT_INIT("CAM_CAMSV_5", 0, 9, 28, 12),
	M4U0_PORT_INIT("CAM_CAMSV_6", 0, 9, 28, 13),
	M4U0_PORT_INIT("CAM_AAO_R1_C", 0, 9, 28, 14),
	M4U0_PORT_INIT("CAM_AFO_R1_C", 0, 9, 28, 15),
	M4U0_PORT_INIT("CAM_FLKO_R1_C", 0, 9, 28, 16),
	M4U0_PORT_INIT("CAM_LCESO_R1_C", 0, 9, 28, 17),
	M4U0_PORT_INIT("CAM_CRZO_R1_C", 0, 9, 28, 18),
	M4U0_PORT_INIT("CAM_LTMSO_R1_C", 0, 9, 28, 19),

	M4U0_PORT_INIT("CAM_RSSO_R1_C", 0, 9, 28, 20),
	M4U0_PORT_INIT("CAM_CCUI", 0, 9, 28, 21),
	M4U0_PORT_INIT("CAM_CCUO", 0, 9, 28, 22),
	M4U0_PORT_INIT("CAM_FAKE", 0, 9, 28, 23),

	/*larb10-CAM-31*/
	M4U0_PORT_INIT("CAM_IMGO_R1_A", 0, 10, 25, 0),
	M4U0_PORT_INIT("CAM_RRZO_R1_A", 0, 10, 25, 1),
	M4U0_PORT_INIT("CAM_LSCI_R1_A", 0, 10, 25, 2),
	M4U0_PORT_INIT("CAM_BPCI_R1_A", 0, 10, 25, 3),
	M4U0_PORT_INIT("CAM_YUVO_R1_A", 0, 10, 25, 4),
	M4U0_PORT_INIT("CAM_UFDI_R2_A", 0, 10, 25, 5),
	M4U0_PORT_INIT("CAM_RAWI_R2_A", 0, 10, 25, 6),
	M4U0_PORT_INIT("CAM_RAWI_R5_A", 0, 10, 25, 7),
	M4U0_PORT_INIT("CAM_IMGO_R1_B", 0, 10, 25, 8),
	M4U0_PORT_INIT("CAM_RRZO_R1_B", 0, 10, 25, 9),

	M4U0_PORT_INIT("CAM_LSCI_R1_B", 0, 10, 25, 10),
	M4U0_PORT_INIT("CAM_BPCI_R1_B", 0, 10, 25, 11),
	M4U0_PORT_INIT("CAM_YUVO_R1_B,", 0, 10, 25, 12),
	M4U0_PORT_INIT("CAM_UFDI_R2_B", 0, 10, 25, 13),
	M4U0_PORT_INIT("CAM_RAWI_R2_B", 0, 10, 25, 14),
	M4U0_PORT_INIT("CAM_RAWI_R5_B", 0, 10, 25, 15),
	M4U0_PORT_INIT("CAM_CAMSV_0", 0, 10, 25, 16),
	M4U0_PORT_INIT("CAM_AAO_R1_A", 0, 10, 25, 17),
	M4U0_PORT_INIT("CAM_AFO_R1_A", 0, 10, 25, 18),
	M4U0_PORT_INIT("CAM_FLKO_R1_A", 0, 10, 25, 19),

	M4U0_PORT_INIT("CAM_LCESO_R1_A", 0, 10, 25, 20),
	M4U0_PORT_INIT("CAM_CRZO_R1_A", 0, 10, 25, 21),
	M4U0_PORT_INIT("CAM_AAO_R1_B", 0, 10, 25, 22),
	M4U0_PORT_INIT("CAM_AFO_R1_B", 0, 10, 25, 23),
	M4U0_PORT_INIT("CAM_FLKO_R1_B", 0, 10, 25, 24),
	M4U0_PORT_INIT("CAM_LCESO_R1_B", 0, 10, 25, 25),
	M4U0_PORT_INIT("CAM_CRZO_R1_B", 0, 10, 25, 26),
	M4U0_PORT_INIT("CAM_LTMSO_R1_A", 0, 10, 25, 27),
	M4U0_PORT_INIT("CAM_RSSO_R1_A", 0, 10, 25, 28),
	M4U0_PORT_INIT("CAM_LTMSO_R1_B", 0, 10, 25, 29),
	M4U0_PORT_INIT("CAM_RSSO_R1_B", 0, 10, 25, 30),

	M4U0_PORT_INIT("CCU0", 0, 9, 24, 0),
	M4U0_PORT_INIT("CCU1", 0, 9, 24, 1),

	M4U1_PORT_INIT("VPU", 0, 0, 0, 0),

	M4U0_PORT_INIT("UNKNOWN", 0, 0, 0, 0)
};

static struct mtk_m4u_data *m4u_data;
static struct iova_buf_list iova_list;

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

static inline int mtk_iommu_get_tf_port_idx(int tf_id)
{
	int i;

	tf_id &= F_MMU_INT_TF_MSK;
	for (i = 0; i < m4u_data->plat_data->port_nr; i++) {
		if (m4u_data->plat_data->port[i].tf_id == tf_id)
			return i;
	}

	return ERROR_LARB_PORT_ID;
}

static inline int mtk_iommu_port_idx(int id)
{
	int i;

	for (i = 0; i < m4u_data->plat_data->port_nr; i++) {
		if ((m4u_data->plat_data->port[i].larb_id ==
		     MTK_M4U_TO_LARB(id)) &&
		     (m4u_data->plat_data->port[i].larb_port ==
		     MTK_M4U_TO_PORT(id)))
			return i;
	}
	return ERROR_LARB_PORT_ID;
}

bool report_custom_iommu_fault(
	u32 fault_iova,
	u32 fault_pa,
	u32 fault_id, bool is_vpu)
{
	int idx;
	int port;

	if (is_vpu) {
		m4u_aee_print(mmu_translation_log_format, "VPU",
			      "VPU", fault_iova, fault_pa);
		return true;
	}

	idx = mtk_iommu_get_tf_port_idx(fault_id);
	if (idx >= m4u_data->plat_data->port_nr) {
		pr_err("fail,iova:0x%x, port:0x%x\n",
			fault_iova, fault_id);
		return false;
	}

	port = MTK_M4U_ID(m4u_data->plat_data->port[idx].larb_id,
			  m4u_data->plat_data->port[idx].larb_port);
	if (m4u_data->plat_data->port[idx].enable_tf &&
		m4u_data->m4u_cb[idx].fault_fn)
		m4u_data->m4u_cb[idx].fault_fn(port,
		fault_iova, m4u_data->m4u_cb[idx].fault_data);

	m4u_aee_print(mmu_translation_log_format,
		m4u_data->plat_data->port[idx].name,
		m4u_data->plat_data->port[idx].name, fault_iova,
		fault_pa);

	return true;
}

int mtk_iommu_register_fault_callback(int port,
			       mtk_iommu_fault_callback_t fn,
			       void *cb_data)
{
	int idx = mtk_iommu_port_idx(port);

	if (idx >= m4u_data->plat_data->port_nr) {
		pr_info("%s fail, port=%d\n", __func__, port);
		return -1;
	}
	m4u_data->m4u_cb[idx].fault_fn = fn;
	m4u_data->m4u_cb[idx].fault_data = cb_data;
	return 0;
}
EXPORT_SYMBOL(mtk_iommu_register_fault_callback);

int mtk_iommu_unregister_fault_callback(int port)
{
	int idx = mtk_iommu_port_idx(port);

	if (idx >= m4u_data->plat_data->port_nr) {
		pr_info("%s fail, port=%d\n", __func__, port);
		return -1;
	}
	m4u_data->m4u_cb[idx].fault_fn = NULL;
	m4u_data->m4u_cb[idx].fault_data = NULL;
	return 0;
}
EXPORT_SYMBOL(mtk_iommu_unregister_fault_callback);

static int m4u_debug_set(void *data, u64 val)
{
	pr_info("%s:val=%llu\n", __func__, val);

	switch (val) {
	case 1: /* translation fault test */
	{
		report_custom_iommu_fault(0, 0, 0x500000f, false);

	break;
	}
	case 2: /* dump iova info */
	{
		mtk_iova_dbg_dump(NULL);

	break;
	}
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

DEFINE_SIMPLE_ATTRIBUTE(m4u_debug_fops, m4u_debug_get, m4u_debug_set, "%llu\n");

static int m4u_debug_init(struct mtk_m4u_data *data)
{
	struct dentry *debug_file;

	data->debug_root = debugfs_create_dir("m4u", NULL);

	if (IS_ERR_OR_NULL(data->debug_root))
		pr_err("failed to create debug dir.\n");

	debug_file = debugfs_create_file("debug",
		0644, data->debug_root, NULL, &m4u_debug_fops);

	if (IS_ERR_OR_NULL(debug_file))
		pr_err("failed to create debug files 2.\n");

	return 0;
}

static int mtk_m4u_dbg_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	m4u_data = devm_kzalloc(dev, sizeof(struct mtk_m4u_data), GFP_KERNEL);
	if (!m4u_data)
		return -ENOMEM;

	m4u_data->dev = dev;
	m4u_data->plat_data = of_device_get_match_data(dev);
	m4u_data->m4u_cb = devm_kzalloc(dev, m4u_data->plat_data->port_nr *
		sizeof(struct mtk_iommu_cb), GFP_KERNEL);
	if (!m4u_data->m4u_cb)
		return -ENOMEM;

	m4u_debug_init(m4u_data);
	mutex_init(&iova_list.lock);
	INIT_LIST_HEAD(&iova_list.head);
	return 0;
}

static const struct mtk_m4u_plat_data mt6779_data = {
	.port = iommu_port_mt6779,
	.port_nr = ARRAY_SIZE(iommu_port_mt6779),
};

static const struct of_device_id mtk_m4u_dbg_of_ids[] = {
	{ .compatible = "mediatek,mt6779-m4u-debug", .data = &mt6779_data},
	{}
};

static struct platform_driver mtk_m4u_dbg_drv = {
	.probe	= mtk_m4u_dbg_probe,
	.driver	= {
		.name = "mtk-m4u-debug",
		.of_match_table = of_match_ptr(mtk_m4u_dbg_of_ids),
	}
};

module_platform_driver(mtk_m4u_dbg_drv);
