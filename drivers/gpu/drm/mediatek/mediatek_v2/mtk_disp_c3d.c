// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mtk_disp_c3d.h"

#ifndef DRM_CMDQ_DISABLE
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#else
#include "mtk-cmdq-ext.h"
#endif

#define HW_ENGINE_NUM (2)

#define C3D_3DLUT_SIZE_17BIN (17 * 17 * 17 * 3)
#define C3D_3DLUT_SIZE_9BIN (9 * 9 * 9 * 3)

#define DISP_C3D_SRAM_SIZE_17BIN (17 * 17 * 17)
#define DISP_C3D_SRAM_SIZE_9BIN (9 * 9 * 9)

struct DISP_C3D_REG_17BIN {
	unsigned int lut3d_reg[C3D_3DLUT_SIZE_17BIN];
};

struct DISP_C3D_REG_9BIN {
	unsigned int lut3d_reg[C3D_3DLUT_SIZE_9BIN];
};

static struct DISP_C3D_REG_17BIN g_c3d_reg_17bin;
static struct DISP_C3D_REG_9BIN g_c3d_reg_9bin;
static unsigned int g_c3d_sram_cfg[DISP_C3D_SRAM_SIZE_17BIN] = { 0 };
static unsigned int g_c3d_sram_cfg_9bin[DISP_C3D_SRAM_SIZE_9BIN] = { 0 };
static unsigned int g_c3d_lut1d[HW_ENGINE_NUM][DISP_C3D_1DLUT_SIZE] = {
	{0, 256, 512, 768, 1024, 1280, 1536, 1792,
	2048, 2304, 2560, 2816, 3072, 3328, 3584, 3840,
	4096, 4608, 5120, 5632, 6144, 6656, 7168, 7680,
	8192, 9216, 10240, 11264, 12288, 13312, 14336, 15360},
	{0, 256, 512, 768, 1024, 1280, 1536, 1792,
	2048, 2304, 2560, 2816, 3072, 3328, 3584, 3840,
	4096, 4608, 5120, 5632, 6144, 6656, 7168, 7680,
	8192, 9216, 10240, 11264, 12288, 13312, 14336, 15360}
};

static struct DISP_C3D_LUT c3dIocData;

#define C3D_REG_3(v0, off0, v1, off1, v2, off2) \
	(((v2) << (off2)) |  ((v1) << (off1)) | ((v0) << (off0)))
#define C3D_REG_2(v0, off0, v1, off1) \
	(((v1) << (off1)) | ((v0) << (off0)))

// define spinlock here
static DEFINE_SPINLOCK(g_c3d_clock_lock);
static DEFINE_MUTEX(g_c3d_global_lock);

// define Mutex
static DEFINE_MUTEX(c3d_lut_lock);

// It's a work around for no comp assigned in functions.
static struct mtk_ddp_comp *default_comp;
static struct mtk_ddp_comp *c3d1_default_comp;

#define index_of_c3d(module) ((module == DDP_COMPONENT_C3D0) ? 0 : 1)

static atomic_t g_c3d_is_clock_on[HW_ENGINE_NUM] = {
			ATOMIC_INIT(0), ATOMIC_INIT(0) };
static atomic_t g_c3d_force_relay[HW_ENGINE_NUM] = {
			ATOMIC_INIT(0), ATOMIC_INIT(0) };
//static atomic_t g_c3d_lut_set = ATOMIC_INIT(0);
static atomic_t g_c3d_sram_hw_init[HW_ENGINE_NUM] = {
			ATOMIC_INIT(0), ATOMIC_INIT(0) };
static atomic_t g_c3d_force_sram_apb[HW_ENGINE_NUM] = {
			ATOMIC_INIT(0), ATOMIC_INIT(0) };

static atomic_t g_c3d_get_irq = ATOMIC_INIT(0);
static atomic_t g_c3d_eventctl = ATOMIC_INIT(0);
static DECLARE_WAIT_QUEUE_HEAD(g_c3d_get_irq_wq);

// For GCE Program
struct cmdq_pkt *c3d_sram_pkt;
struct cmdq_pkt *c3d1_sram_pkt;
/* wake lock to prevnet system off */
static struct wakeup_source *c3d_wake_lock;
static bool c3d_wake_locked;

enum C3D_IOCTL_CMD {
	SET_C3DLUT = 0,
	BYPASS_C3D,
};

static bool isDualPQ;
static bool gHasSet1DLut[HW_ENGINE_NUM];

struct mtk_disp_c3d_data {
	bool support_shadow;
	bool need_bypass_shadow;
	int bin_num;
	int c3d_sram_start_addr;
	int c3d_sram_end_addr;
};

struct mtk_disp_c3d {
	struct mtk_ddp_comp ddp_comp;
	struct drm_crtc *crtc;
	const struct mtk_disp_c3d_data *data;
};
static struct mtk_disp_c3d *g_c3d_data;

static bool debug_flow_log;
#define C3DFLOW_LOG(fmt, arg...) do { \
	if (debug_flow_log) \
		pr_notice("[FLOW]%s:" fmt, __func__, ##arg); \
	} while (0)

static bool debug_api_log;
#define C3DAPI_LOG(fmt, arg...) do { \
	if (debug_api_log) \
		pr_notice("[API]%s:" fmt, __func__, ##arg); \
	} while (0)

static void mtk_disp_c3d_write_mask(void __iomem *address, u32 data, u32 mask)
{
	u32 value = data;

	if (mask != ~0) {
		value = readl(address);
		value &= ~mask;
		data &= mask;
		value |= data;
	}
	writel(value, address);
}

static inline struct mtk_disp_c3d *comp_to_c3d(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_c3d, ddp_comp);
}

static void disp_c3d_lock_wake_lock(bool lock)
{
	if (lock) {
		if (!c3d_wake_locked) {
			__pm_stay_awake(c3d_wake_lock);
			c3d_wake_locked = true;
		} else  {
			DDPPR_ERR("%s: try lock twice\n", __func__);
		}
	} else {
		if (c3d_wake_locked) {
			__pm_relax(c3d_wake_lock);
			c3d_wake_locked = false;
		} else {
			DDPPR_ERR("%s: try unlock twice\n", __func__);
		}
	}
}

static int mtk_disp_c3d_create_gce_pkt(struct drm_crtc *crtc,
		struct cmdq_pkt **pkt)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	int index = 0;

	if (!mtk_crtc) {
		DDPPR_ERR("%s:%d, invalid crtc:0x%p\n",
				__func__, __LINE__, crtc);
		return -1;
	}

	index = drm_crtc_index(crtc);
	if (index) {
		DDPPR_ERR("%s:%d, invalid crtc:0x%p, index:%d\n",
				__func__, __LINE__, crtc, index);
		return -1;
	}

	if (*pkt != NULL)
		cmdq_pkt_destroy(*pkt);

	*pkt = cmdq_pkt_create(mtk_crtc->gce_obj.client[CLIENT_CFG]);

	return 0;
}

#define C3D_SRAM_POLL_SLEEP_TIME_US	(10)
#define C3D_SRAM_MAX_POLL_TIME_US	(1000)

static inline bool disp_c3d_reg_poll(struct mtk_ddp_comp *comp,
	unsigned long addr, unsigned int value, unsigned int mask)
{
	bool return_value = false;
	unsigned int reg_value = 0;
	unsigned int polling_time = 0;

	do {
		reg_value = readl(comp->regs + addr);

		if ((reg_value & mask) == value) {
			return_value = true;
			break;
		}

		udelay(C3D_SRAM_POLL_SLEEP_TIME_US);
		polling_time += C3D_SRAM_POLL_SLEEP_TIME_US;
	} while (polling_time < C3D_SRAM_MAX_POLL_TIME_US);

	return return_value;
}

static inline bool disp_c3d_sram_write(struct mtk_ddp_comp *comp,
	unsigned int addr, unsigned int value)
{
	bool return_value = false;

	do {
		writel(addr, comp->regs + C3D_SRAM_RW_IF_0);
		writel(value, comp->regs + C3D_SRAM_RW_IF_1);

		return_value = true;
	} while (0);

	return return_value;
}

static inline bool disp_c3d_sram_read(struct mtk_ddp_comp *comp,
	unsigned int addr, unsigned int *value)
{
	bool return_value = false;

	do {
		writel(addr, comp->regs + C3D_SRAM_RW_IF_2);

		if (disp_c3d_reg_poll(comp, C3D_SRAM_STATUS,
				(0x1 << 17), (0x1 << 17)) != true)
			break;

		*value = readl(comp->regs + C3D_SRAM_RW_IF_3);

		return_value = true;
	} while (0);

	return return_value;
}

static bool disp_c3d_write_sram(struct mtk_ddp_comp *comp, bool reuse)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct cmdq_pkt *cmdq_handle = NULL;

	if (comp->id == DDP_COMPONENT_C3D0)
		cmdq_handle = c3d_sram_pkt;
	else
		cmdq_handle = c3d1_sram_pkt;

	disp_c3d_lock_wake_lock(true);
	cmdq_mbox_enable(mtk_crtc->gce_obj.client[CLIENT_CFG]->chan);
	if (!reuse) {
		if (cmdq_handle != NULL)
			cmdq_pkt_flush(cmdq_handle);
	} else {
		if (cmdq_handle != NULL) {
			cmdq_pkt_refinalize(cmdq_handle);
			cmdq_pkt_flush(cmdq_handle);
		}
	}

	cmdq_mbox_disable(mtk_crtc->gce_obj.client[CLIENT_CFG]->chan);
	disp_c3d_lock_wake_lock(false);

	return true;
}

static void ddp_c3d_sram_write_table(struct mtk_ddp_comp *comp)
{
	mtk_disp_c3d_write_mask(comp->regs + C3D_SRAM_CFG,
		(0 << 6)|(0 << 5)|(1 << 4), (0x7 << 4));

	if (atomic_read(&g_c3d_sram_hw_init[index_of_c3d(comp->id)]) == 0)
		disp_c3d_write_sram(comp, false);
	else
		disp_c3d_write_sram(comp, true);

	atomic_set(&g_c3d_force_sram_apb[index_of_c3d(comp->id)], 0);
}

static void disp_c3d_config_sram(struct mtk_ddp_comp *comp,
		struct cmdq_pkt **handle)
{
	struct mtk_disp_c3d *c3d_data;
	unsigned int *cfg;

	unsigned int sram_offset = 0;
	unsigned int write_value = 0;

	c3d_data = comp_to_c3d(comp);
	cfg = g_c3d_sram_cfg;
	if (c3d_data->data->bin_num == 9)
		cfg = g_c3d_sram_cfg_9bin;
	else if (c3d_data->data->bin_num != 17)
		DDPPR_ERR("%s: %d bin Not support!", __func__, c3d_data->data->bin_num);

	// destroy used pkt and create new one
	mtk_disp_c3d_create_gce_pkt(g_c3d_data->crtc, handle);

	C3DFLOW_LOG("handle: %d\n", ((*handle == NULL) ? 1 : 0));
	if (*handle == NULL)
		return;

	// Write 3D LUT to SRAM
	for (sram_offset = c3d_data->data->c3d_sram_start_addr;
		sram_offset <= c3d_data->data->c3d_sram_end_addr;
			sram_offset += 4) {
		write_value = cfg[sram_offset/4];

		cmdq_pkt_write(*handle, comp->cmdq_base,
			comp->regs_pa + C3D_SRAM_RW_IF_0, sram_offset, ~0);
		cmdq_pkt_write(*handle, comp->cmdq_base,
			comp->regs_pa + C3D_SRAM_RW_IF_1, write_value, ~0);
	}
}

static void disp_c3d_sram_write_init_sram(struct mtk_ddp_comp *comp)
{
	int r_index, g_index, b_index, bin_point;
	int sram_cfg_index;
	unsigned int sram_init_table[17];
	unsigned int sram_init_table_9bin[9];

	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);
	int binNum = c3d_data->data->bin_num;

	pr_notice("%s: binNum: %d", __func__, binNum);

	for (bin_point = 0; bin_point < binNum; bin_point++) {
		if (binNum == 17)
			sram_init_table[bin_point] = c3d_min(1023, 64 * bin_point);
		else if (binNum == 9)
			sram_init_table_9bin[bin_point] = c3d_min(1023, 128 * bin_point);
	}

	for (r_index = 0; r_index < binNum; r_index++) {
		for (g_index = 0; g_index < binNum; g_index++) {
			for (b_index = 0; b_index < binNum; b_index++) {
				sram_cfg_index = r_index * binNum * binNum
							+ g_index * binNum + b_index;
				if (binNum == 17) {
					g_c3d_sram_cfg[sram_cfg_index] = sram_init_table[r_index] |
								(sram_init_table[g_index] << 10) |
								(sram_init_table[b_index] << 20);
				} else if (binNum == 9) {
					g_c3d_sram_cfg_9bin[sram_cfg_index] =
							sram_init_table_9bin[r_index] |
							(sram_init_table_9bin[g_index] << 10) |
							(sram_init_table_9bin[b_index] << 20);
				}
			}
		}
	}

	if (comp->id == DDP_COMPONENT_C3D0)
		disp_c3d_config_sram(comp, &c3d_sram_pkt);
	else
		disp_c3d_config_sram(comp, &c3d1_sram_pkt);
}

static void disp_c3d_sram_init(struct mtk_ddp_comp *comp)
{
	//mutex_lock(&c3d_lut_lock);
	disp_c3d_sram_write_init_sram(comp);
	ddp_c3d_sram_write_table(comp);
	//mutex_unlock(&c3d_lut_lock);
}

void disp_c3d_flip_sram(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
	const char *caller)
{
	u32 sram_apb = 0, sram_int = 0, sram_cfg;

	if (atomic_cmpxchg(&g_c3d_force_sram_apb[index_of_c3d(comp->id)], 0, 1) == 0) {
		sram_apb = 0;
		sram_int = 1;
	} else if (atomic_cmpxchg(&g_c3d_force_sram_apb[index_of_c3d(comp->id)], 1, 0) == 1) {
		sram_apb = 1;
		sram_int = 0;
	} else {
		DDPINFO("[SRAM] Error when get hist_apb in %s", caller);
	}
	sram_cfg = (sram_int << 6) | (sram_apb << 5) | (1 << 4);
	C3DFLOW_LOG("[SRAM] hist_apb(%d) hist_int(%d) 0x%08x in %s",
		sram_apb, sram_int, sram_cfg, caller);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + C3D_SRAM_CFG, sram_cfg, (0x7 << 4));
}

int mtk_drm_ioctl_c3d_get_bin_num(struct drm_device *dev, void *data,
			struct drm_file *file_priv)
{
	int ret = 0;
	struct mtk_drm_private *private = dev->dev_private;
	struct mtk_ddp_comp *comp = private->ddp_comp[DDP_COMPONENT_C3D0];
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);

	int *c3d_bin_num = (int *)data;
	*c3d_bin_num = c3d_data->data->bin_num;

	return ret;
}

static int disp_c3d_wait_irq(void)
{
	int ret = 0;

	if (atomic_read(&g_c3d_get_irq) == 0 || atomic_read(&g_c3d_eventctl) == 0) {
		C3DFLOW_LOG("wait_event_interruptible +++\n");
		ret = wait_event_interruptible(g_c3d_get_irq_wq,
				(atomic_read(&g_c3d_get_irq) == 1) &&
				(atomic_read(&g_c3d_eventctl) == 1));

		if (ret >= 0) {
			C3DFLOW_LOG("wait_event_interruptible ---\n");
			C3DFLOW_LOG("get_irq = 1, wake up, ret = %d\n", ret);
		} else {
			DDPINFO("%s: interrupted unexpected\n", __func__);
		}
	} else {
		C3DFLOW_LOG("get_irq = 0");
	}

	atomic_set(&g_c3d_get_irq, 0);

	return ret;
}

int mtk_drm_ioctl_c3d_get_irq(struct drm_device *dev, void *data,
			struct drm_file *file_priv)
{
	int ret = 0;

	pr_notice("%s\n", __func__);
	ret = disp_c3d_wait_irq();

	return ret;
}

int mtk_drm_ioctl_c3d_eventctl(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	int ret = 0;
	int *enabled = (int *)data;

	struct mtk_drm_private *private = dev->dev_private;
	struct mtk_ddp_comp *comp = private->ddp_comp[DDP_COMPONENT_C3D0];

	C3DFLOW_LOG("%d\n", *enabled);

	atomic_set(&g_c3d_eventctl, *enabled);
	C3DFLOW_LOG("%d\n", atomic_read(&g_c3d_eventctl));

	if (atomic_read(&g_c3d_eventctl) == 1)
		wake_up_interruptible(&g_c3d_get_irq_wq);

	if (*enabled)
		mtk_crtc_check_trigger(comp->mtk_crtc, false, true);

	return ret;
}

int mtk_drm_ioctl_c3d_set_lut(struct drm_device *dev, void *data,
			struct drm_file *file_priv)
{
	int ret = 0;
	struct mtk_drm_private *private = dev->dev_private;
	struct mtk_ddp_comp *comp = private->ddp_comp[DDP_COMPONENT_C3D0];
	struct drm_crtc *crtc = private->crtc[0];

	C3DAPI_LOG("line: %d\n", __LINE__);

	ret = mtk_crtc_user_cmd(crtc, comp, SET_C3DLUT, data);

	mtk_crtc_check_trigger(comp->mtk_crtc, false, true);
	return ret;
}

int mtk_drm_ioctl_bypass_c3d(struct drm_device *dev, void *data,
			struct drm_file *file_priv)
{
	int ret = 0;
	struct mtk_drm_private *private = dev->dev_private;
	struct mtk_ddp_comp *comp = private->ddp_comp[DDP_COMPONENT_C3D0];
	struct drm_crtc *crtc = private->crtc[0];

	C3DAPI_LOG("line: %d\n", __LINE__);

	ret = mtk_crtc_user_cmd(crtc, comp, BYPASS_C3D, data);

	mtk_crtc_check_trigger(comp->mtk_crtc, false, true);
	return ret;
}

static void disp_c3d_update_sram(struct mtk_ddp_comp *comp,
	 bool check_sram)
{
	//unsigned long flags;
	unsigned int read_value;
	int sram_apb = 0, sram_int = 0;

	if (check_sram) {
		read_value = readl(comp->regs + C3D_SRAM_CFG);
		sram_apb = (read_value >> 5) & 0x1;
		sram_int = (read_value >> 6) & 0x1;
		C3DFLOW_LOG("[SRAM] hist_apb(%d) hist_int(%d) 0x%08x in (SOF) compId: %d",
			sram_apb, sram_int, read_value, index_of_c3d(comp->id));
		// after suspend/resume, set FORCE_SRAM_APB = 0, FORCE_SRAM_INT = 0;
		// so need to config C3D_SRAM_CFG on ping-pong mode correctly.
		if ((sram_apb == sram_int) && (sram_int == 0)) {
			mtk_disp_c3d_write_mask(comp->regs + C3D_SRAM_CFG,
				(0 << 6)|(1 << 5)|(1 << 4), (0x7 << 4));
			C3DFLOW_LOG("%s: C3D_SRAM_CFG(0x%08x)\n", __func__,
				readl(comp->regs + C3D_SRAM_CFG));
		}
		if (sram_int != atomic_read(&g_c3d_force_sram_apb[index_of_c3d(comp->id)]))
			pr_notice("dre3: SRAM config %d != %d config", sram_int,
				atomic_read(&g_c3d_force_sram_apb[index_of_c3d(comp->id)]));
	}

	if (comp->id == DDP_COMPONENT_C3D0)
		disp_c3d_config_sram(comp, &c3d_sram_pkt);
	else
		disp_c3d_config_sram(comp, &c3d1_sram_pkt);

	disp_c3d_write_sram(comp, false);
}

void disp_c3d_on_start_of_frame(void)
{
	if (!default_comp)
		return;
	atomic_set(&g_c3d_get_irq, 1);

	if (atomic_read(&g_c3d_eventctl) == 1)
		wake_up_interruptible(&g_c3d_get_irq_wq);
}

void disp_c3d_on_end_of_frame_mutex(void)
{
	if (!default_comp)
		return;
	atomic_set(&g_c3d_get_irq, 0);
}

static int disp_c3d_write_3dlut_to_reg(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, const struct DISP_C3D_LUT *c3d_lut)
{
	int c3dBinNum;
	int i;

	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);

	c3dBinNum = c3d_data->data->bin_num;

	C3DFLOW_LOG("c3dBinNum: %d", c3dBinNum);
	if (c3dBinNum == 17) {
		if (copy_from_user(&g_c3d_reg_17bin,
				C3D_U32_PTR(c3d_lut->lut3d),
				sizeof(g_c3d_reg_17bin)) == 0) {
			C3DFLOW_LOG("%x, %x, %x", g_c3d_reg_17bin.lut3d_reg[14735],
				g_c3d_reg_17bin.lut3d_reg[14737], g_c3d_reg_17bin.lut3d_reg[14738]);
			mutex_lock(&c3d_lut_lock);
			for (i = 0; i < C3D_3DLUT_SIZE_17BIN; i += 3) {
				g_c3d_sram_cfg[i / 3] = g_c3d_reg_17bin.lut3d_reg[i] |
					(g_c3d_reg_17bin.lut3d_reg[i + 1] << 10) |
					(g_c3d_reg_17bin.lut3d_reg[i + 2] << 20);
			}
			C3DFLOW_LOG("%x\n", g_c3d_sram_cfg[4912]);

			disp_c3d_update_sram(comp, true);
			mutex_unlock(&c3d_lut_lock);
		}
	} else if (c3dBinNum == 9) {
		if (copy_from_user(&g_c3d_reg_9bin,
					C3D_U32_PTR(c3d_lut->lut3d),
					sizeof(g_c3d_reg_9bin)) == 0) {
			mutex_lock(&c3d_lut_lock);
			for (i = 0; i < C3D_3DLUT_SIZE_9BIN; i += 3) {
				g_c3d_sram_cfg_9bin[i / 3] = g_c3d_reg_9bin.lut3d_reg[i] |
						(g_c3d_reg_9bin.lut3d_reg[i + 1] << 10) |
						(g_c3d_reg_9bin.lut3d_reg[i + 2] << 20);
			}

			disp_c3d_update_sram(comp, true);
			mutex_unlock(&c3d_lut_lock);
		}
	} else {
		pr_notice("%s, c3d bin num: %d not support", __func__, c3dBinNum);
	}

	return 0;
}

static int disp_c3d_set_1dlut(struct mtk_ddp_comp *comp,
		struct cmdq_pkt *handle, int lock)
{
	unsigned int *lut1d;
	int ret = 0;
	int id = index_of_c3d(comp->id);

	if (lock)
		mutex_lock(&g_c3d_global_lock);

	lut1d = &g_c3d_lut1d[id][0];
	if (lut1d == NULL) {
		pr_notice("%s: table [%d] not initialized, use default config\n", __func__, id);
		ret = -EFAULT;
	} else {
		C3DFLOW_LOG("%x, %x, %x, %x, %x", lut1d[0],
				lut1d[2], lut1d[3], lut1d[5], lut1d[6]);
		gHasSet1DLut[id] = true;

		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + C3D_C1D_000_001,
				C3D_REG_2(lut1d[1], 0, lut1d[0], 16), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + C3D_C1D_002_003,
				C3D_REG_2(lut1d[3], 0, lut1d[2], 16), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + C3D_C1D_004_005,
				C3D_REG_2(lut1d[5], 0, lut1d[4], 16), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + C3D_C1D_006_007,
				C3D_REG_2(lut1d[7], 0, lut1d[6], 16), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + C3D_C1D_008_009,
				C3D_REG_2(lut1d[9], 0, lut1d[8], 16), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + C3D_C1D_010_011,
				C3D_REG_2(lut1d[11], 0, lut1d[10], 16), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + C3D_C1D_012_013,
				C3D_REG_2(lut1d[13], 0, lut1d[12], 16), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + C3D_C1D_014_015,
				C3D_REG_2(lut1d[15], 0, lut1d[14], 16), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + C3D_C1D_016_017,
				C3D_REG_2(lut1d[17], 0, lut1d[16], 16), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + C3D_C1D_018_019,
				C3D_REG_2(lut1d[19], 0, lut1d[18], 16), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + C3D_C1D_020_021,
				C3D_REG_2(lut1d[21], 0, lut1d[20], 16), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + C3D_C1D_022_023,
				C3D_REG_2(lut1d[23], 0, lut1d[22], 16), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + C3D_C1D_024_025,
				C3D_REG_2(lut1d[25], 0, lut1d[24], 16), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + C3D_C1D_026_027,
				C3D_REG_2(lut1d[27], 0, lut1d[26], 16), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + C3D_C1D_028_029,
				C3D_REG_2(lut1d[29], 0, lut1d[28], 16), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + C3D_C1D_030_031,
				C3D_REG_2(lut1d[31], 0, lut1d[30], 16), ~0);
	}

	if (lock)
		mutex_unlock(&g_c3d_global_lock);

	return ret;
}

static int disp_c3d_write_1dlut_to_reg(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, const struct DISP_C3D_LUT *c3d_lut)
{
	unsigned int *c3d_lut1d;
	int ret = 0;
	int id = index_of_c3d(comp->id);

	if (c3d_lut == NULL) {
		ret = -EFAULT;
	} else {
		pr_notice("%s", __func__);
		c3d_lut1d = (unsigned int *) &(c3d_lut->lut1d[0]);
		C3DFLOW_LOG("%x, %x, %x, %x, %x", c3d_lut1d[0],
				c3d_lut1d[2], c3d_lut1d[3], c3d_lut1d[5], c3d_lut1d[6]);

		if (id >= 0 && id < HW_ENGINE_NUM) {
			mutex_lock(&g_c3d_global_lock);
			if (!gHasSet1DLut[id] ||
				memcmp(&g_c3d_lut1d[id][0], c3d_lut1d, sizeof(g_c3d_lut1d[id]))) {
				memcpy(&g_c3d_lut1d[id][0], c3d_lut1d, sizeof(g_c3d_lut1d[id]));
				ret = disp_c3d_set_1dlut(comp, handle, 0);
			}
			mutex_unlock(&g_c3d_global_lock);
		} else {
			pr_notice("%s: invalid ID = %d\n", __func__, comp->id);
			ret = -EFAULT;
		}
	}

	return ret;
}

static int disp_c3d_write_lut_to_reg(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, const struct DISP_C3D_LUT *c3d_lut)
{
	if (atomic_read(&g_c3d_force_relay[index_of_c3d(comp->id)]) == 1) {
		// Set reply mode
		DDPINFO("g_c3d_force_relay\n");
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + C3D_CFG, 0x3, 0x3);
	} else {
		// Disable reply mode
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + C3D_CFG, 0x2, 0x3);
	}

	disp_c3d_write_1dlut_to_reg(comp, handle, c3d_lut);
	disp_c3d_write_3dlut_to_reg(comp, handle, c3d_lut);

	return 0;
}

static int disp_c3d_set_lut(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
		void *data)
{
	int ret = -EFAULT;

	memcpy(&c3dIocData, (struct DISP_C3D_LUT *)data,
				sizeof(struct DISP_C3D_LUT));

	ret = disp_c3d_write_lut_to_reg(comp, handle, &c3dIocData);
	disp_c3d_flip_sram(comp, handle, __func__);
	if (comp->mtk_crtc->is_dual_pipe) {
		struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
		struct drm_crtc *crtc = &mtk_crtc->base;
		struct mtk_drm_private *priv = crtc->dev->dev_private;
		struct mtk_ddp_comp *comp_c3d1 = priv->ddp_comp[DDP_COMPONENT_C3D1];

		ret = disp_c3d_write_lut_to_reg(comp_c3d1, handle, &c3dIocData);
		disp_c3d_flip_sram(comp_c3d1, handle, __func__);
	}

//	atomic_set(&g_c3d_lut_set, 1);

	return ret;
}

static void mtk_disp_c3d_bypass(struct mtk_ddp_comp *comp, int bypass,
	struct cmdq_pkt *handle)
{
	pr_notice("%s, comp_id: %d, bypass: %d\n",
			__func__, index_of_c3d(comp->id), bypass);

	if (bypass == 1) {
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + C3D_CFG, 0x1, 0x1);
		atomic_set(&g_c3d_force_relay[index_of_c3d(comp->id)], 0x1);
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + C3D_CFG, 0x0, 0x1);
		atomic_set(&g_c3d_force_relay[index_of_c3d(comp->id)], 0x0);
	}
}

static int mtk_disp_c3d_user_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
	unsigned int cmd, void *data)
{
	C3DFLOW_LOG("cmd: %d\n", cmd);
	switch (cmd) {
	case SET_C3DLUT:
	{
		if (disp_c3d_set_lut(comp, handle, data) < 0) {
			DDPPR_ERR("SET_PARAM: fail\n");
			return -EFAULT;
		}
	}
	break;
	case BYPASS_C3D:
	{
		unsigned int *value = data;

		mtk_disp_c3d_bypass(comp, *value, handle);
		if (comp->mtk_crtc->is_dual_pipe) {
			struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
			struct drm_crtc *crtc = &mtk_crtc->base;
			struct mtk_drm_private *priv = crtc->dev->dev_private;
			struct mtk_ddp_comp *comp_c3d1 = priv->ddp_comp[DDP_COMPONENT_C3D1];

			mtk_disp_c3d_bypass(comp_c3d1, *value, handle);
		}
	}
	break;
	default:
		DDPPR_ERR("error cmd: %d\n", cmd);
		return -EINVAL;
	}
	return 0;
}

static void mtk_disp_c3d_config(struct mtk_ddp_comp *comp,
	struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	unsigned int width;

	pr_notice("%s, line: %d\n", __func__, __LINE__);

	if (comp->mtk_crtc->is_dual_pipe) {
		isDualPQ = true;
		width = cfg->w/2;
	} else {
		isDualPQ = false;
		width = cfg->w;
	}

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + C3D_SIZE, (width << 16) | cfg->h, ~0);
}


static void mtk_disp_c3d_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	pr_notice("%s, line: %d\n", __func__, __LINE__);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + C3D_EN, 0x1, ~0);

	if (atomic_read(&g_c3d_force_relay[index_of_c3d(comp->id)]) == 1) {
		// Set reply mode
		DDPINFO("g_c3d_force_relay\n");
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + C3D_CFG, 0x3, 0x3);
	} else {
		// Disable reply mode
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + C3D_CFG, 0x2, 0x3);
	}

	disp_c3d_set_1dlut(comp, handle, 0);
}

static void mtk_disp_c3d_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	pr_notice("%s, line: %d\n", __func__, __LINE__);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + C3D_EN, 0x0, ~0);
}

static void mtk_disp_c3d_prepare(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_c3d *priv = dev_get_drvdata(comp->dev);
	unsigned long long time[2] = {0};
	// create cmdq_pkt
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct drm_crtc *crtc = &mtk_crtc->base;

	g_c3d_data->crtc = crtc;
	time[0] = sched_clock();

	mtk_ddp_comp_clk_prepare(comp);
	atomic_set(&g_c3d_is_clock_on[index_of_c3d(comp->id)], 1);

	/* Bypass shadow register and read shadow register */
	if (priv->data->need_bypass_shadow)
		mtk_ddp_write_mask_cpu(comp, C3D_BYPASS_SHADOW,
			C3D_SHADOW_CTL, C3D_BYPASS_SHADOW);

	mutex_lock(&c3d_lut_lock);
	if (atomic_read(&g_c3d_sram_hw_init[index_of_c3d(comp->id)]) == 0) {
		disp_c3d_sram_init(comp);
		atomic_set(&g_c3d_sram_hw_init[index_of_c3d(comp->id)], 1);
	} else
		ddp_c3d_sram_write_table(comp);
	mutex_unlock(&c3d_lut_lock);

	time[1] = sched_clock();
	C3DFLOW_LOG("compID:%d, timediff: %llu\n", comp->id, time[1] - time[0]);
}

static void mtk_disp_c3d_unprepare(struct mtk_ddp_comp *comp)
{
	unsigned long flags;
	gHasSet1DLut[index_of_c3d(comp->id)] = false;

	pr_notice("%s, line: %d\n", __func__, __LINE__);

	spin_lock_irqsave(&g_c3d_clock_lock, flags);
	atomic_set(&g_c3d_is_clock_on[index_of_c3d(comp->id)], 0);
	spin_unlock_irqrestore(&g_c3d_clock_lock, flags);
	mtk_ddp_comp_clk_unprepare(comp);
}

void mtk_disp_c3d_first_cfg(struct mtk_ddp_comp *comp,
	       struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	pr_notice("%s\n", __func__);

	mtk_disp_c3d_config(comp, cfg, handle);
}

static const struct mtk_ddp_comp_funcs mtk_disp_c3d_funcs = {
	.config = mtk_disp_c3d_config,
	.first_cfg = mtk_disp_c3d_first_cfg,
	.start = mtk_disp_c3d_start,
	.stop = mtk_disp_c3d_stop,
	.bypass = mtk_disp_c3d_bypass,
	.user_cmd = mtk_disp_c3d_user_cmd,
	.prepare = mtk_disp_c3d_prepare,
	.unprepare = mtk_disp_c3d_unprepare,
};

static int mtk_disp_c3d_bind(struct device *dev, struct device *master,
			       void *data)
{
	struct mtk_disp_c3d *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	int ret;

	pr_notice("%s+\n", __func__);
	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}
	pr_notice("%s-\n", __func__);
	return 0;
}

static void mtk_disp_c3d_unbind(struct device *dev, struct device *master,
				  void *data)
{
	struct mtk_disp_c3d *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	pr_notice("%s+\n", __func__);
	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
	pr_notice("%s-\n", __func__);
}

static const struct component_ops mtk_disp_c3d_component_ops = {
	.bind	= mtk_disp_c3d_bind,
	.unbind = mtk_disp_c3d_unbind,
};

void mtk_c3d_dump(struct mtk_ddp_comp *comp)
{
	void __iomem  *baddr = comp->regs;

	DDPDUMP("== %s REGS:0x%x ==\n", mtk_dump_comp_str(comp), comp->regs_pa);
	mtk_cust_dump_reg(baddr, 0x0, 0x4, 0x18, 0x8C);
	mtk_cust_dump_reg(baddr, 0x24, 0x30, 0x34, 0x38);
	mtk_cust_dump_reg(baddr, 0x3C, 0x40, 0x44, 0x48);
	mtk_cust_dump_reg(baddr, 0x4C, 0x50, 0x54, 0x58);
	mtk_cust_dump_reg(baddr, 0x5C, 0x60, 0x64, 0x68);
	mtk_cust_dump_reg(baddr, 0x6C, 0x70, 0x74, 0x78);
	mtk_cust_dump_reg(baddr, 0x7C, 0x80, 0x84, 0x88);
}

static int mtk_disp_c3d_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_c3d *priv;
	enum mtk_ddp_comp_id comp_id;
	int ret;

	pr_notice("%s+\n", __func__);

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (priv == NULL)
		return -ENOMEM;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_C3D);
	if ((int)comp_id < 0) {
		DDPPR_ERR("Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	if (comp_id == DDP_COMPONENT_C3D0)
		g_c3d_data = priv;

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_c3d_funcs);
	if (ret != 0) {
		DDPPR_ERR("Failed to initialize component: %d\n", ret);
		return ret;
	}

	if (!default_comp && comp_id == DDP_COMPONENT_C3D0)
		default_comp = &priv->ddp_comp;
	if (!c3d1_default_comp && comp_id == DDP_COMPONENT_C3D1)
		c3d1_default_comp = &priv->ddp_comp;

	priv->data = of_device_get_match_data(dev);
	platform_set_drvdata(pdev, priv);

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &mtk_disp_c3d_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}
	pr_notice("%s-\n", __func__);

	return ret;
}

static int mtk_disp_c3d_remove(struct platform_device *pdev)
{
	struct mtk_disp_c3d *priv = dev_get_drvdata(&pdev->dev);

	pr_notice("%s+\n", __func__);
	component_del(&pdev->dev, &mtk_disp_c3d_component_ops);
	mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	pr_notice("%s-\n", __func__);
	return 0;
}

static const struct mtk_disp_c3d_data mt6983_c3d_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
	.bin_num = 17,
	.c3d_sram_start_addr = 0,
	.c3d_sram_end_addr = 19648,
};

static const struct mtk_disp_c3d_data mt6895_c3d_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
	.bin_num = 17,
	.c3d_sram_start_addr = 0,
	.c3d_sram_end_addr = 19648,
};

static const struct mtk_disp_c3d_data mt6879_c3d_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
	.bin_num = 9,
	.c3d_sram_start_addr = 0,
	.c3d_sram_end_addr = 2912,
};

static const struct of_device_id mtk_disp_c3d_driver_dt_match[] = {
	{ .compatible = "mediatek,mt6983-disp-c3d",
	  .data = &mt6983_c3d_driver_data},
	{ .compatible = "mediatek,mt6895-disp-c3d",
	  .data = &mt6895_c3d_driver_data},
	{ .compatible = "mediatek,mt6879-disp-c3d",
	  .data = &mt6879_c3d_driver_data},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_disp_c3d_driver_dt_match);

struct platform_driver mtk_disp_c3d_driver = {
	.probe = mtk_disp_c3d_probe,
	.remove = mtk_disp_c3d_remove,
	.driver = {
			.name = "mediatek-disp-c3d",
			.owner = THIS_MODULE,
			.of_match_table = mtk_disp_c3d_driver_dt_match,
		},
};

void mtk_disp_c3d_debug(const char *opt)
{
	pr_notice("[C3D debug]: %s\n", opt);
	if (strncmp(opt, "flow_log:", 9) == 0) {
		debug_flow_log = strncmp(opt + 9, "1", 1) == 0;
		pr_notice("[C3D debug] debug_flow_log = %d\n", debug_flow_log);
	} else if (strncmp(opt, "api_log:", 8) == 0) {
		debug_api_log = strncmp(opt + 8, "1", 1) == 0;
		pr_notice("[C3D debug] debug_api_log = %d\n", debug_api_log);
	} else if (strncmp(opt, "debugdump:", 10) == 0) {
		pr_notice("[C3D debug] debug_flow_log = %d\n", debug_flow_log);
		pr_notice("[C3D debug] debug_api_log = %d\n", debug_api_log);
	}
}

void disp_c3d_set_bypass(struct drm_crtc *crtc, int bypass)
{
	int ret;

	ret = mtk_crtc_user_cmd(crtc, default_comp, BYPASS_C3D, &bypass);

	DDPINFO("%s : ret = %d", __func__, ret);
}
