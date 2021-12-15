/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/string.h>
#include <linux/time.h>
#include <linux/wait.h>
#if IS_ENABLED(CONFIG_DEBUG_FS)
#include <linux/debugfs.h>
#endif

#if IS_ENABLED(CONFIG_PROC_FS)
#include <linux/proc_fs.h>
#endif
#include <linux/sched/clock.h>
#include <linux/of_address.h>
#include <drm/drmP.h>
#include <drm/drm_mipi_dsi.h>
#include "mtk_dump.h"
#include "mtk_debug.h"
#include "mtk_drm_crtc.h"
#include "mtk_drm_mmp.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_assert.h"
#include "mtk_drm_helper.h"
#include "mtk_layering_rule.h"
#include "mtk_drm_lowpower.h"
#if defined(CONFIG_MTK_IOMMU_V2)
#include "mt_iommu.h"
#include "mtk_iommu_ext.h"
#endif
#include "mtk_drm_gem.h"
#include "mtk_drm_fb.h"
#include "mtk_drm_fbdev.h"
#include "mtk_disp_aal.h"
#ifdef CONFIG_MTK_HDMI_SUPPORT
#include "mtk_dp_debug.h"
#endif
#include "mtk_drm_arr.h"
#include "mtk_drm_graphics_base.h"

#define DISP_REG_CONFIG_MMSYS_CG_SET(idx) (0x104 + 0x10 * (idx))
#define DISP_REG_CONFIG_MMSYS_CG_CLR(idx) (0x108 + 0x10 * (idx))
#define DISP_REG_CONFIG_DISP_FAKE_ENG_EN(idx) (0x200 + 0x20 * (idx))
#define DISP_REG_CONFIG_DISP_FAKE_ENG_RST(idx) (0x204 + 0x20 * (idx))
#define DISP_REG_CONFIG_DISP_FAKE_ENG_CON0(idx) (0x208 + 0x20 * (idx))
#define DISP_REG_CONFIG_DISP_FAKE_ENG_CON1(idx) (0x20c + 0x20 * (idx))
#define DISP_REG_CONFIG_DISP_FAKE_ENG_RD_ADDR(idx) (0x210 + 0x20 * (idx))
#define DISP_REG_CONFIG_DISP_FAKE_ENG_WR_ADDR(idx) (0x214 + 0x20 * (idx))
#define DISP_REG_CONFIG_DISP_FAKE_ENG_STATE(idx) (0x218 + 0x20 * (idx))
#define DISP_REG_CONFIG_RDMA_SHARE_SRAM_CON (0x654)
#define	DISP_RDMA_FAKE_SMI_SEL(idx) (BIT(4 + idx))
#define SMI_LARB_VC_PRI_MODE (0x020)
#define SMI_LARB_NON_SEC_CON(port) (0x380 + 4 * (port))
#define GET_M4U_PORT 0x1F
#define MTK_CWB_NO_EFFECT_HRT_MAX_WIDTH 128

#if IS_ENABLED(CONFIG_DEBUG_FS)
static struct dentry *mtkfb_dbgfs;
#endif

#if IS_ENABLED(CONFIG_PROC_FS)
static struct proc_dir_entry *mtkfb_procfs;
static struct proc_dir_entry *disp_lowpower_proc;
static struct proc_dir_entry *mtkfb_debug_procfs;
#endif
static struct drm_device *drm_dev;
bool g_mobile_log;
bool g_fence_log;
bool g_irq_log;
bool g_detail_log;
bool g_trace_log;
unsigned int mipi_volt;
unsigned int disp_met_en;
static unsigned int m_old_pq_persist_property[32];
unsigned int m_new_pq_persist_property[32];

int gCaptureOVLEn;
int gCaptureWDMAEn;
int gCapturePriLayerDownX = 10;
int gCapturePriLayerDownY = 10;
u64 vfp_backup;

int hwc_pid;
static atomic_t lfr_dbg;
static atomic_t lfr_params;

static struct completion cwb_cmp;

struct logger_buffer {
	char **buffer_ptr;
	unsigned int len;
	unsigned int id;
	const unsigned int cnt;
	const unsigned int size;
};

static DEFINE_SPINLOCK(dprec_logger_spinlock);

static char **err_buffer;
static char **fence_buffer;
static char **dbg_buffer;
static char **dump_buffer;
static char **status_buffer;
static struct logger_buffer dprec_logger_buffer[DPREC_LOGGER_PR_NUM] = {
	{0, 0, 0, ERROR_BUFFER_COUNT, LOGGER_BUFFER_SIZE},
	{0, 0, 0, FENCE_BUFFER_COUNT, LOGGER_BUFFER_SIZE},
	{0, 0, 0, DEBUG_BUFFER_COUNT, LOGGER_BUFFER_SIZE},
	{0, 0, 0, DUMP_BUFFER_COUNT, LOGGER_BUFFER_SIZE},
	{0, 0, 0, STATUS_BUFFER_COUNT, LOGGER_BUFFER_SIZE},
};
static bool is_buffer_init;
static char *debug_buffer;

static int draw_RGBA8888_buffer(char *va, int w, int h,
		       char r, char g, char b, char a)
{
	int i, j;
	int Bpp =  mtk_get_format_bpp(DRM_FORMAT_RGBA8888);

	for (i = 0; i < h; i++)
		for (j = 0; j < w; j++) {
			int x = j * Bpp + i * w * Bpp;

			va[x++] = a;
			va[x++] = b;
			va[x++] = g;
			va[x++] = r;
		}

	return 0;
}

static int prepare_fake_layer_buffer(struct drm_crtc *crtc)
{
	unsigned int i;
	size_t size;
	struct mtk_drm_gem_obj *mtk_gem;
	struct drm_mode_fb_cmd2 mode = { 0 };
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_fake_layer *fake_layer = &mtk_crtc->fake_layer;

	if (fake_layer->init)
		return 0;

	mode.width = crtc->state->adjusted_mode.hdisplay;
	mode.height = crtc->state->adjusted_mode.vdisplay;
	mode.pixel_format = DRM_FORMAT_RGBA8888;
	mode.pitches[0] = mode.width
			* mtk_get_format_bpp(mode.pixel_format);
	size = mode.width * mode.height
		* mtk_get_format_bpp(mode.pixel_format);

	for (i = 0; i < PRIMARY_OVL_PHY_LAYER_NR; i++) {
		mtk_gem = mtk_drm_gem_create(crtc->dev, size, true);
		draw_RGBA8888_buffer(mtk_gem->kvaddr, mode.width, mode.height,
			(!((i + 0) % 3)) * 255 / (i / 3 + 1),
			(!((i + 1) % 3)) * 255 / (i / 3 + 1),
			(!((i + 2) % 3)) * 255 / (i / 3 + 1), 100);
		fake_layer->fake_layer_buf[i] =
			mtk_drm_framebuffer_create(crtc->dev, &mode,
						&mtk_gem->base);
	}
	fake_layer->init = true;
	DDPMSG("%s init done\n", __func__);

	return 0;
}

static unsigned long long get_current_time_us(void)
{
	unsigned long long time = sched_clock();
	struct timeval t;

	/* return do_div(time,1000); */
	return time;

	do_gettimeofday(&t);
	return (t.tv_sec & 0xFFF) * 1000000 + t.tv_usec;
}

static char *_logger_pr_type_spy(enum DPREC_LOGGER_PR_TYPE type)
{
	switch (type) {
	case DPREC_LOGGER_ERROR:
		return "error";
	case DPREC_LOGGER_FENCE:
		return "fence";
	case DPREC_LOGGER_DEBUG:
		return "dbg";
	case DPREC_LOGGER_DUMP:
		return "dump";
	case DPREC_LOGGER_STATUS:
		return "status";
	default:
		return "unknown";
	}
}

static void init_log_buffer(void)
{
	int i, buf_size, buf_idx;
	char *temp_buf;

	if (is_buffer_init)
		return;

	/*1. Allocate Error, Fence, Debug and Dump log buffer slot*/
	err_buffer = kzalloc(sizeof(char *) * ERROR_BUFFER_COUNT, GFP_KERNEL);
	if (!err_buffer)
		goto err;
	fence_buffer = kzalloc(sizeof(char *) * FENCE_BUFFER_COUNT, GFP_KERNEL);
	if (!fence_buffer)
		goto err;
	dbg_buffer = kzalloc(sizeof(char *) * DEBUG_BUFFER_COUNT, GFP_KERNEL);
	if (!dbg_buffer)
		goto err;
	dump_buffer = kzalloc(sizeof(char *) * DUMP_BUFFER_COUNT, GFP_KERNEL);
	if (!dump_buffer)
		goto err;
	status_buffer = kzalloc(sizeof(char *) * DUMP_BUFFER_COUNT, GFP_KERNEL);
	if (!status_buffer)
		goto err;

	/*2. Allocate log ring buffer.*/
	buf_size = sizeof(char) * (DEBUG_BUFFER_SIZE - 4096);
	temp_buf = kzalloc(buf_size, GFP_KERNEL);
	if (!temp_buf)
		goto err;

	/*3. Dispatch log ring buffer to each buffer slot*/
	buf_idx = 0;
	for (i = 0; i < ERROR_BUFFER_COUNT; i++) {
		err_buffer[i] = (temp_buf + buf_idx * LOGGER_BUFFER_SIZE);
		buf_idx++;
	}
	dprec_logger_buffer[0].buffer_ptr = err_buffer;

	for (i = 0; i < FENCE_BUFFER_COUNT; i++) {
		fence_buffer[i] = (temp_buf + buf_idx * LOGGER_BUFFER_SIZE);
		buf_idx++;
	}
	dprec_logger_buffer[1].buffer_ptr = fence_buffer;

	for (i = 0; i < DEBUG_BUFFER_COUNT; i++) {
		dbg_buffer[i] = (temp_buf + buf_idx * LOGGER_BUFFER_SIZE);
		buf_idx++;
	}
	dprec_logger_buffer[2].buffer_ptr = dbg_buffer;

	for (i = 0; i < DUMP_BUFFER_COUNT; i++) {
		dump_buffer[i] = (temp_buf + buf_idx * LOGGER_BUFFER_SIZE);
		buf_idx++;
	}
	dprec_logger_buffer[3].buffer_ptr = dump_buffer;

	for (i = 0; i < STATUS_BUFFER_COUNT; i++) {
		status_buffer[i] = (temp_buf + buf_idx * LOGGER_BUFFER_SIZE);
		buf_idx++;
	}
	dprec_logger_buffer[4].buffer_ptr = status_buffer;

	is_buffer_init = true;
	DDPINFO("[DISP]%s success\n", __func__);
	return;
err:
	DDPPR_ERR("[DISP]%s: log buffer allocation fail\n", __func__);
}

int mtk_dprec_logger_pr(unsigned int type, char *fmt, ...)
{
	int n = 0;
	unsigned long flags = 0;
	uint64_t time = get_current_time_us();
	unsigned long rem_nsec;
	char **buf_arr;
	char *buf = NULL;
	int len = 0;

	if (type >= DPREC_LOGGER_PR_NUM)
		return -1;

	if (!is_buffer_init)
		return -1;

	spin_lock_irqsave(&dprec_logger_spinlock, flags);
	if (dprec_logger_buffer[type].len < 128) {
		dprec_logger_buffer[type].id++;
		dprec_logger_buffer[type].id = dprec_logger_buffer[type].id %
					       dprec_logger_buffer[type].cnt;
		dprec_logger_buffer[type].len = dprec_logger_buffer[type].size;
	}
	buf_arr = dprec_logger_buffer[type].buffer_ptr;
	buf = buf_arr[dprec_logger_buffer[type].id] +
	      dprec_logger_buffer[type].size - dprec_logger_buffer[type].len;
	len = dprec_logger_buffer[type].len;

	if (buf) {
		va_list args;

		rem_nsec = do_div(time, 1000000000);
		n += snprintf(buf + n, len - n, "[%5lu.%06lu]",
			      (unsigned long)time, rem_nsec / 1000);

		va_start(args, fmt);
		n += vscnprintf(buf + n, len - n, fmt, args);
		va_end(args);
	}

	dprec_logger_buffer[type].len -= n;
	spin_unlock_irqrestore(&dprec_logger_spinlock, flags);

	return n;
}

int mtk_dprec_logger_get_buf(enum DPREC_LOGGER_PR_TYPE type, char *stringbuf,
			     int len)
{
	int n = 0;
	int i;
	char **buf_arr;
	int c;

	if (type >= DPREC_LOGGER_PR_NUM || type < 0 || len < 0) {
		DDPPR_ERR("%s invalid DPREC_LOGGER_PR_TYPE\n", __func__);
		return 0;
	}

	if (!is_buffer_init)
		return 0;

	c = dprec_logger_buffer[type].id;
	buf_arr = dprec_logger_buffer[type].buffer_ptr;

	for (i = 0; i < dprec_logger_buffer[type].cnt; i++) {
		c++;
		c %= dprec_logger_buffer[type].cnt;
		n += scnprintf(stringbuf + n, len - n,
			       "dprec log buffer[%s][%d]\n",
			       _logger_pr_type_spy(type), c);
		n += scnprintf(stringbuf + n, len - n, "%s\n", buf_arr[c]);
	}

	return n;
}

extern int mtk_drm_setbacklight(struct drm_crtc *crtc, unsigned int level);
int mtkfb_set_backlight_level(unsigned int level)
{
	struct drm_crtc *crtc;

	/* this debug cmd only for crtc0 */
	crtc = list_first_entry(&(drm_dev)->mode_config.crtc_list,
				typeof(*crtc), head);
	if (!crtc) {
		DDPPR_ERR("find crtc fail\n");
		return 0;
	}
	mtk_drm_setbacklight(crtc, level);

	return 0;
}
EXPORT_SYMBOL(mtkfb_set_backlight_level);

int mtkfb_set_aod_backlight_level(unsigned int level)
{
	struct drm_crtc *crtc;

	/* this debug cmd only for crtc0 */
	crtc = list_first_entry(&(drm_dev)->mode_config.crtc_list,
				typeof(*crtc), head);
	if (!crtc) {
		pr_info("find crtc fail\n");
		return 0;
	}
	mtk_drm_aod_setbacklight(crtc, level);

	return 0;
}
EXPORT_SYMBOL(mtkfb_set_aod_backlight_level);

void mtk_disp_mipi_ccci_callback(unsigned int en, unsigned int usrdata)
{
	struct drm_crtc *crtc;

	crtc = list_first_entry(&(drm_dev)->mode_config.crtc_list,
				typeof(*crtc), head);

	if (!crtc) {
		DDPPR_ERR("find crtc fail\n");
		return;
	}
	mtk_crtc_mipi_freq_switch(crtc, en, usrdata);

	return;
}
EXPORT_SYMBOL(mtk_disp_mipi_ccci_callback);

void mtk_disp_osc_ccci_callback(unsigned int en, unsigned int usrdata)
{
	struct drm_crtc *crtc;

	crtc = list_first_entry(&(drm_dev)->mode_config.crtc_list,
				typeof(*crtc), head);

	if (!crtc) {
		DDPPR_ERR("find crtc fail\n");
		return;
	}
	mtk_crtc_osc_freq_switch(crtc, en, usrdata);

	return;
}
EXPORT_SYMBOL(mtk_disp_osc_ccci_callback);

void display_enter_tui(void)
{
	struct drm_crtc *crtc;

	crtc = list_first_entry(&(drm_dev)->mode_config.crtc_list,
				typeof(*crtc), head);

	if (!crtc) {
		DDPPR_ERR("find crtc fail\n");
		return;
	}
	mtk_crtc_enter_tui(crtc);
}
EXPORT_SYMBOL(display_enter_tui);


void display_exit_tui(void)
{
	struct drm_crtc *crtc;

	crtc = list_first_entry(&(drm_dev)->mode_config.crtc_list,
				typeof(*crtc), head);

	if (!crtc) {
		DDPPR_ERR("find crtc fail\n");
		return;
	}
	mtk_crtc_exit_tui(crtc);
}
EXPORT_SYMBOL(display_exit_tui);

static int debug_get_info(unsigned char *stringbuf, int buf_len)
{
	int n = 0;
	struct mtk_drm_private *private;

	if (!drm_dev) {
		DDPPR_ERR("%s:%d, drm_dev is NULL\n",
			__func__, __LINE__);
		return -1;
	}
	if (!drm_dev->dev_private) {
		DDPPR_ERR("%s:%d, drm_dev->dev_private is NULL\n",
			__func__, __LINE__);
		return -1;
	}

	private = drm_dev->dev_private;
#if 0
	DISPFUNC();

	n += mtkfb_get_debug_state(stringbuf + n, buf_len - n);

	n += primary_display_get_debug_state(stringbuf + n, buf_len - n);

	n += disp_sync_get_debug_info(stringbuf + n, buf_len - n);

	n += dprec_logger_get_result_string_all(stringbuf + n, buf_len - n);

	n += disp_helper_get_option_list(stringbuf + n, buf_len - n);
#endif
	n += mtk_drm_primary_display_get_debug_state(private, stringbuf + n,
		buf_len - n);

	n += mtk_drm_dump_wk_lock(private, stringbuf + n,
		buf_len - n);

	n += mtk_drm_helper_get_opt_list(private->helper_opt, stringbuf + n,
					 buf_len - n);

	n += mtk_dprec_logger_get_buf(DPREC_LOGGER_ERROR, stringbuf + n,
				      buf_len - n);

	n += mtk_dprec_logger_get_buf(DPREC_LOGGER_FENCE, stringbuf + n,
				      buf_len - n);

	n += mtk_dprec_logger_get_buf(DPREC_LOGGER_DUMP, stringbuf + n,
				      buf_len - n);

	n += mtk_dprec_logger_get_buf(DPREC_LOGGER_DEBUG, stringbuf + n,
				      buf_len - n);

	n += mtk_dprec_logger_get_buf(DPREC_LOGGER_STATUS, stringbuf + n,
				      buf_len - n);

	stringbuf[n++] = 0;
	return n;
}

static void mtk_fake_engine_iommu_enable(struct drm_device *dev,
		unsigned int idx)
{
	int port, ret;
	unsigned int value;
	struct device_node *larb_node;
	void __iomem *baddr;
	struct mtk_drm_private *priv = dev->dev_private;

	/* get larb reg */
	larb_node = of_parse_phandle(priv->mmsys_dev->of_node,
				"fake-engine", idx * 2);
	if (!larb_node) {
		DDPPR_ERR("Cannot find larb node\n");
		return;
	}
	baddr = of_iomap(larb_node, 0);
	of_node_put(larb_node);

	/* get port num */
	ret = of_property_read_u32_index(priv->mmsys_dev->of_node,
				"fake-engine", idx * 2 + 1, &port);
	if (ret < 0) {
		DDPPR_ERR("Node %s cannot find fake-engine data!\n",
			priv->mmsys_dev->of_node->full_name);
		return;
	}
	port &= GET_M4U_PORT;

	value = readl(baddr + SMI_LARB_NON_SEC_CON(port));
	value = (value & ~0x1) | (0x1 & 0x1);
	writel_relaxed(value, baddr + SMI_LARB_NON_SEC_CON(port));
}

static void mtk_fake_engine_share_port_config(struct drm_crtc *crtc,
						unsigned int idx, bool en)
{
	unsigned int value;
	struct device_node *larb_node;
	static void __iomem **baddr;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	const struct mtk_fake_eng_data *fake_eng_data =
						priv->data->fake_eng_data;
	int i;

	if (!baddr) {
		baddr = devm_kmalloc_array(crtc->dev->dev,
				fake_eng_data->fake_eng_num,
				sizeof(void __iomem *),
				GFP_KERNEL);
		if (!baddr) {
			DDPPR_ERR("%s: devm_kmalloc_array failed\n", __func__);
			return;
		}
		for (i = 0; i < fake_eng_data->fake_eng_num; i++) {
			larb_node = of_parse_phandle(priv->mmsys_dev->of_node,
				"fake-engine", i * 2);
			if (!larb_node) {
				DDPPR_ERR("Cannot find larb node\n");
				return;
			}
			baddr[i] = of_iomap(larb_node, 0);
			of_node_put(larb_node);
		}
	}

	if (en) {
		value = readl(baddr[idx] + SMI_LARB_VC_PRI_MODE);
		value = (value & ~0x3) | (0x0 & 0x3);
		writel_relaxed(value, baddr + SMI_LARB_VC_PRI_MODE);

		value = readl(mtk_crtc->config_regs +
				DISP_REG_CONFIG_RDMA_SHARE_SRAM_CON);
		value |= DISP_RDMA_FAKE_SMI_SEL(idx);
		writel_relaxed(value, mtk_crtc->config_regs +
				DISP_REG_CONFIG_RDMA_SHARE_SRAM_CON);
	} else {
		value = readl(baddr[idx] + SMI_LARB_VC_PRI_MODE);
		value = (value & ~0x3) | (0x1 & 0x3);
		writel_relaxed(value, baddr + SMI_LARB_VC_PRI_MODE);

		value = readl(mtk_crtc->config_regs +
				DISP_REG_CONFIG_RDMA_SHARE_SRAM_CON);
		value &= ~(DISP_RDMA_FAKE_SMI_SEL(idx));
		writel_relaxed(value, mtk_crtc->config_regs +
				DISP_REG_CONFIG_RDMA_SHARE_SRAM_CON);
	}
}

void fake_engine(struct drm_crtc *crtc, unsigned int idx, unsigned int en,
		unsigned int wr_en, unsigned int rd_en, unsigned int wr_pat1,
		unsigned int wr_pat2, unsigned int latency,
		unsigned int preultra_cnt,
		unsigned int ultra_cnt)
{
	int burst = 7;
	int test_len = 255;
	int loop = 1;
	int preultra_en = 0;
	int ultra_en = 0;
	int dis_wr = !wr_en;
	int dis_rd = !rd_en;
	int delay_cnt = 0;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	const struct mtk_fake_eng_data *fake_eng_data;
	const struct mtk_fake_eng_reg *fake_eng;
	static struct mtk_drm_gem_obj **gem;
	int i;

	fake_eng_data = priv->data->fake_eng_data;
	if (!fake_eng_data) {
		DDPPR_ERR("this platform not support any fake engine\n");
		return;
	}

	if (idx > fake_eng_data->fake_eng_num - 1) {
		DDPPR_ERR("this platform not support fake engine %d\n", idx);
		return;
	}

	fake_eng = &fake_eng_data->fake_eng_reg[idx];

	if (preultra_cnt > 0) {
		preultra_en = 1;
		preultra_cnt--;
	}

	if (ultra_cnt > 0) {
		ultra_en = 1;
		ultra_cnt--;
	}

	if (en) {
		if (!gem) {
			gem = devm_kmalloc_array(crtc->dev->dev,
					fake_eng_data->fake_eng_num,
					sizeof(struct mtk_drm_gem_obj *),
					GFP_KERNEL);
			if (!gem) {
				DDPPR_ERR("%s: devm_kmalloc_array failed\n", __func__);
				return;
			}
			for (i = 0; i < fake_eng_data->fake_eng_num; i++) {
				gem[i] = mtk_drm_gem_create(crtc->dev,
							1024*1024, true);
				mtk_fake_engine_iommu_enable(crtc->dev, i);
				DDPMSG("fake_engine_%d va=0x%08lx, pa=0x%08x\n",
					i, (unsigned long)gem[i]->kvaddr,
					(unsigned int)gem[i]->dma_addr);
			}
		}

		if (fake_eng->share_port)
			mtk_fake_engine_share_port_config(crtc, idx, en);

		writel_relaxed(BIT(fake_eng->CG_bit), mtk_crtc->config_regs +
			DISP_REG_CONFIG_MMSYS_CG_CLR(fake_eng->CG_idx));

		writel_relaxed((unsigned int)gem[idx]->dma_addr,
			mtk_crtc->config_regs +
			DISP_REG_CONFIG_DISP_FAKE_ENG_RD_ADDR(idx));
		writel_relaxed((unsigned int)gem[idx]->dma_addr + 4096,
			mtk_crtc->config_regs +
			DISP_REG_CONFIG_DISP_FAKE_ENG_WR_ADDR(idx));
		writel_relaxed((wr_pat1 << 24) | (loop << 22) | test_len,
			mtk_crtc->config_regs +
			DISP_REG_CONFIG_DISP_FAKE_ENG_CON0(idx));
		writel_relaxed((ultra_en << 23) | (ultra_cnt << 20) |
			(preultra_en << 19) | (preultra_cnt << 16) |
			(burst << 12) | (dis_wr << 11) | (dis_rd << 10) |
			latency, mtk_crtc->config_regs +
			DISP_REG_CONFIG_DISP_FAKE_ENG_CON1(idx));

		writel_relaxed(1, mtk_crtc->config_regs +
			DISP_REG_CONFIG_DISP_FAKE_ENG_RST(idx));
		writel_relaxed(0, mtk_crtc->config_regs +
			DISP_REG_CONFIG_DISP_FAKE_ENG_RST(idx));
		writel_relaxed(0x3, mtk_crtc->config_regs +
			DISP_REG_CONFIG_DISP_FAKE_ENG_EN(idx));

		if (wr_pat2 != wr_pat1)
			writel_relaxed((wr_pat2 << 24) | (loop << 22) |
				test_len,
				mtk_crtc->config_regs +
				DISP_REG_CONFIG_DISP_FAKE_ENG_CON0(idx));

		DDPMSG("fake_engine_%d enable\n", idx);
	} else {
		writel_relaxed(0x1, mtk_crtc->config_regs +
			DISP_REG_CONFIG_DISP_FAKE_ENG_EN(idx));

		while ((readl(mtk_crtc->config_regs +
				DISP_REG_CONFIG_DISP_FAKE_ENG_STATE(idx))
				& 0x1) == 0x1) {
			delay_cnt++;
			udelay(1);
			if (delay_cnt > 1000) {
				DDPPR_ERR("Wait fake_engine_%d idle timeout\n",
					idx);
				break;
			}
		}

		writel_relaxed(0x0, mtk_crtc->config_regs +
			DISP_REG_CONFIG_DISP_FAKE_ENG_EN(idx));

		writel_relaxed(BIT(fake_eng->CG_bit), mtk_crtc->config_regs +
			DISP_REG_CONFIG_MMSYS_CG_SET(fake_eng->CG_idx));

		if (fake_eng->share_port)
			mtk_fake_engine_share_port_config(crtc, idx, en);

		DDPMSG("fake_engine_%d disable\n", idx);
	}
}

void dump_fake_engine(void __iomem *config_regs)
{
	DDPDUMP("=================Dump Fake_engine================\n");
		mtk_serial_dump_reg(config_regs, 0x100, 1);
		mtk_serial_dump_reg(config_regs, 0x110, 1);
		mtk_serial_dump_reg(config_regs, 0x200, 4);
		mtk_serial_dump_reg(config_regs, 0x210, 3);
		mtk_serial_dump_reg(config_regs, 0x220, 4);
		mtk_serial_dump_reg(config_regs, 0x230, 3);
}

static void mtk_ddic_send_cb(struct cmdq_cb_data data)
{
	struct mtk_cmdq_cb_data *cb_data = data.data;

	cmdq_pkt_destroy(cb_data->cmdq_handle);
	kfree(cb_data);
	CRTC_MMP_MARK(0, ddic_send_cmd, 1, 1);
}

int mtk_ddic_dsi_send_cmd(struct mtk_ddic_dsi_msg *cmd_msg,
			bool blocking)
{
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_drm_private *private;
	struct mtk_ddp_comp *output_comp;
	struct cmdq_pkt *cmdq_handle;
	bool is_frame_mode;
	struct mtk_cmdq_cb_data *cb_data;
	int index = 0;
	int ret = 0;

	DDPMSG("%s +\n", __func__);

	/* This cmd only for crtc0 */
	crtc = list_first_entry(&(drm_dev)->mode_config.crtc_list,
			typeof(*crtc), head);
	index = drm_crtc_index(crtc);

	CRTC_MMP_EVENT_START(index, ddic_send_cmd, (unsigned long)crtc,
				blocking);

	if (!crtc) {
		DDPPR_ERR("find crtc fail\n");
		CRTC_MMP_EVENT_END(index, ddic_send_cmd, 0, 0);
		return -EINVAL;
	}

	private = crtc->dev->dev_private;
	mtk_crtc = to_mtk_crtc(crtc);

	mutex_lock(&private->commit.lock);
	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);

	if (!mtk_crtc->enabled) {
		DDPMSG("crtc%d disable skip %s\n",
			drm_crtc_index(&mtk_crtc->base), __func__);
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		mutex_unlock(&private->commit.lock);
		CRTC_MMP_EVENT_END(index, ddic_send_cmd, 0, 1);
		return -EINVAL;
	} else if (mtk_crtc->ddp_mode == DDP_NO_USE) {
		DDPMSG("skip %s, ddp_mode: NO_USE\n",
			__func__);
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		mutex_unlock(&private->commit.lock);
		CRTC_MMP_EVENT_END(index, ddic_send_cmd, 0, 2);
		return -EINVAL;
	}

	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (unlikely(!output_comp)) {
		DDPPR_ERR("%s:invalid output comp\n", __func__);
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		mutex_unlock(&private->commit.lock);
		CRTC_MMP_EVENT_END(index, ddic_send_cmd, 0, 3);
		return -EINVAL;
	}

	is_frame_mode = mtk_crtc_is_frame_trigger_mode(&mtk_crtc->base);

	CRTC_MMP_MARK(index, ddic_send_cmd, 1, 0);

	/* Kick idle */
	mtk_drm_idlemgr_kick(__func__, crtc, 0);

	CRTC_MMP_MARK(index, ddic_send_cmd, 2, 0);

	mtk_crtc_pkt_create(&cmdq_handle, crtc,
			mtk_crtc->gce_obj.client[CLIENT_DSI_CFG]);

	if (mtk_crtc_with_sub_path(crtc, mtk_crtc->ddp_mode))
		mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle,
			DDP_SECOND_PATH, 0);
	else
		mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle,
			DDP_FIRST_PATH, 0);

	if (is_frame_mode) {
		cmdq_pkt_clear_event(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_STREAM_BLOCK]);
		cmdq_pkt_wfe(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_CABC_EOF]);
		cmdq_pkt_clear_event(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_STREAM_DIRTY]);
	}

	/* DSI_SEND_DDIC_CMD */
	if (output_comp)
		ret = mtk_ddp_comp_io_cmd(output_comp, cmdq_handle,
		DSI_SEND_DDIC_CMD, cmd_msg);

	if (is_frame_mode) {
		cmdq_pkt_set_event(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_STREAM_DIRTY]);
		cmdq_pkt_set_event(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_CABC_EOF]);
		cmdq_pkt_set_event(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_STREAM_BLOCK]);
	}

	if (blocking) {
		cmdq_pkt_flush(cmdq_handle);
		cmdq_pkt_destroy(cmdq_handle);
	} else {
		cb_data = kmalloc(sizeof(*cb_data), GFP_KERNEL);
		if (!cb_data) {
			DDPPR_ERR("%s:cb data creation failed\n", __func__);
			DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
			mutex_unlock(&private->commit.lock);
			CRTC_MMP_EVENT_END(index, ddic_send_cmd, 0, 4);
			return -EINVAL;
		}

		cb_data->cmdq_handle = cmdq_handle;
		cmdq_pkt_flush_threaded(cmdq_handle, mtk_ddic_send_cb, cb_data);
	}
	DDPMSG("%s -\n", __func__);
	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
	mutex_unlock(&private->commit.lock);
	CRTC_MMP_EVENT_END(index, ddic_send_cmd, (unsigned long)crtc,
			blocking);

	return ret;
}

int mtk_ddic_dsi_read_cmd(struct mtk_ddic_dsi_msg *cmd_msg)
{
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_drm_private *private;
	struct mtk_ddp_comp *output_comp;
	int index = 0;
	int ret = 0;

	DDPMSG("%s +\n", __func__);

	/* This cmd only for crtc0 */
	crtc = list_first_entry(&(drm_dev)->mode_config.crtc_list,
			typeof(*crtc), head);
	index = drm_crtc_index(crtc);

	CRTC_MMP_EVENT_START(index, ddic_read_cmd, (unsigned long)crtc, 0);

	if (!crtc) {
		DDPPR_ERR("find crtc fail\n");
		CRTC_MMP_EVENT_END(index, ddic_read_cmd, 0, 0);
		return -EINVAL;
	}

	private = crtc->dev->dev_private;
	mtk_crtc = to_mtk_crtc(crtc);

	mutex_lock(&private->commit.lock);
	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);

	if (!mtk_crtc->enabled) {
		DDPMSG("crtc%d disable skip %s\n",
			drm_crtc_index(&mtk_crtc->base), __func__);
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		mutex_unlock(&private->commit.lock);
		CRTC_MMP_EVENT_END(index, ddic_read_cmd, 0, 1);
		return -EINVAL;
	} else if (mtk_crtc->ddp_mode == DDP_NO_USE) {
		DDPMSG("skip %s, ddp_mode: NO_USE\n",
			__func__);
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		mutex_unlock(&private->commit.lock);
		CRTC_MMP_EVENT_END(index, ddic_read_cmd, 0, 2);
		return -EINVAL;
	}

	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (unlikely(!output_comp)) {
		DDPPR_ERR("%s:invalid output comp\n", __func__);
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		mutex_unlock(&private->commit.lock);
		CRTC_MMP_EVENT_END(index, ddic_read_cmd, 0, 3);
		return -EINVAL;
	}

	CRTC_MMP_MARK(index, ddic_read_cmd, 1, 0);

	/* Kick idle */
	mtk_drm_idlemgr_kick(__func__, crtc, 0);

	CRTC_MMP_MARK(index, ddic_read_cmd, 2, 0);

	/* DSI_READ_DDIC_CMD */
	if (output_comp)
		ret = mtk_ddp_comp_io_cmd(output_comp, NULL, DSI_READ_DDIC_CMD,
				cmd_msg);

	CRTC_MMP_MARK(index, ddic_read_cmd, 3, 0);

	DDPMSG("%s -\n", __func__);
	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
	mutex_unlock(&private->commit.lock);
	CRTC_MMP_EVENT_END(index, ddic_read_cmd, (unsigned long)crtc, 4);

	return ret;
}

void ddic_dsi_send_cmd_test(unsigned int case_num)
{
	unsigned int i = 0, j = 0;
	int ret;
	struct mtk_ddic_dsi_msg *cmd_msg =
		vmalloc(sizeof(struct mtk_ddic_dsi_msg));
	u8 tx[10] = {0};
	u8 tx_1[10] = {0};

	DDPMSG("%s start case_num:%d\n", __func__, case_num);

	if (!cmd_msg) {
		DDPPR_ERR("cmd msg is NULL\n");
		return;
	}
	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));

	switch (case_num) {
	case 1:
	{
		/* Send 0x34 */
		cmd_msg->channel = 0;
		cmd_msg->flags = 0;
		cmd_msg->tx_cmd_num = 1;
		cmd_msg->type[0] = 0x05;
		tx[0] = 0x34;
		cmd_msg->tx_buf[0] = tx;
		cmd_msg->tx_len[0] = 1;

		break;
	}
	case 2:
	{
		/* Send 0x35:0x00 */
		cmd_msg->channel = 0;
		cmd_msg->flags = 0;
		cmd_msg->tx_cmd_num = 1;
		cmd_msg->type[0] = 0x15;
		tx[0] = 0x35;
		tx[1] = 0x00;
		cmd_msg->tx_buf[0] = tx;
		cmd_msg->tx_len[0] = 2;

		break;
	}
	case 3:
	{
		/* Send 0x28 */
		cmd_msg->channel = 0;
		cmd_msg->flags |= MIPI_DSI_MSG_USE_LPM;
		cmd_msg->tx_cmd_num = 1;
		cmd_msg->type[0] = 0x05;
		tx[0] = 0x28;
		cmd_msg->tx_buf[0] = tx;
		cmd_msg->tx_len[0] = 1;

		break;
	}
	case 4:
	{
		/* Send 0x29 */
		cmd_msg->channel = 0;
		cmd_msg->flags |= MIPI_DSI_MSG_USE_LPM;
		cmd_msg->tx_cmd_num = 1;
		cmd_msg->type[0] = 0x05;
		tx[0] = 0x29;
		cmd_msg->tx_buf[0] = tx;
		cmd_msg->tx_len[0] = 1;

		break;
	}
	case 5:
	{
		/* Multiple cmd UT case */
		cmd_msg->channel = 0;
		cmd_msg->flags = 0;
		/*	cmd_msg->flags |= MIPI_DSI_MSG_USE_LPM; */
		cmd_msg->tx_cmd_num = 2;

		/* Send 0x34 */
		cmd_msg->type[0] = 0x05;
		tx[0] = 0x34;
		cmd_msg->tx_buf[0] = tx;
		cmd_msg->tx_len[0] = 1;

		/* Send 0x28 */
		cmd_msg->type[1] = 0x05;
		tx_1[0] = 0x28;
		cmd_msg->tx_buf[1] = tx_1;
		cmd_msg->tx_len[1] = 1;

		break;
	}
	case 6:
	{
		/* Multiple cmd UT case */
		cmd_msg->channel = 0;
		cmd_msg->flags = 0;
		/*	cmd_msg->flags |= MIPI_DSI_MSG_USE_LPM; */
		cmd_msg->tx_cmd_num = 2;

		/* Send 0x35 */
		cmd_msg->type[0] = 0x15;
		tx[0] = 0x35;
		tx[1] = 0x00;
		cmd_msg->tx_buf[0] = tx;
		cmd_msg->tx_len[0] = 2;

		/* Send 0x29 */
		cmd_msg->type[1] = 0x05;
		tx_1[0] = 0x29;
		cmd_msg->tx_buf[1] = tx_1;
		cmd_msg->tx_len[1] = 1;

		break;
	}
	default:
		DDPMSG("%s no this test case:%d\n", __func__, case_num);
		break;
	}

	DDPMSG("send lcm tx_cmd_num:%d\n", (int)cmd_msg->tx_cmd_num);
	for (i = 0; i < (int)cmd_msg->tx_cmd_num; i++) {
		DDPMSG("send lcm tx_len[%d]=%d\n",
			i, (int)cmd_msg->tx_len[i]);
		for (j = 0; j < (int)cmd_msg->tx_len[i]; j++) {
			DDPMSG(
				"send lcm type[%d]=0x%x, tx_buf[%d]--byte:%d,val:0x%x\n",
				i, cmd_msg->type[i], i, j,
				*(char *)(cmd_msg->tx_buf[i] + j));
		}
	}

	ret = mtk_ddic_dsi_send_cmd(cmd_msg, true);
	if (ret != 0) {
		DDPPR_ERR("mtk_ddic_dsi_send_cmd error\n");
		goto  done;
	}
done:
	vfree(cmd_msg);

	DDPMSG("%s end -\n", __func__);
}

void ddic_dsi_read_cmd_test(unsigned int case_num)
{
	unsigned int i = 0, j = 0;
	unsigned int ret_dlen = 0;
	int ret;
	struct mtk_ddic_dsi_msg *cmd_msg =
		vmalloc(sizeof(struct mtk_ddic_dsi_msg));
	u8 tx[10] = {0};
	u8 tx_1[10] = {0};
	u8 tx_2[10] = {0};
	u8 tx_3[10] = {0};

	DDPMSG("%s start case_num:%d\n", __func__, case_num);

	if (!cmd_msg) {
		DDPPR_ERR("cmd msg is NULL\n");
		return;
	}
	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));

	switch (case_num) {
	case 1:
	{
		/* Read 0x0A = 0x1C */
		cmd_msg->channel = 0;
		cmd_msg->tx_cmd_num = 1;
		cmd_msg->type[0] = 0x06;
		tx[0] = 0x0A;
		cmd_msg->tx_buf[0] = tx;
		cmd_msg->tx_len[0] = 1;

		cmd_msg->rx_cmd_num = 1;
		cmd_msg->rx_buf[0] = kmalloc(4 * sizeof(unsigned char),
			GFP_ATOMIC);
		memset(cmd_msg->rx_buf[0], 0, 4);
		cmd_msg->rx_len[0] = 1;

		break;
	}
	case 1001:
	{
		/* Read 0x0A = 0x1C */
		cmd_msg->channel = 0;
		cmd_msg->tx_cmd_num = 20;
		cmd_msg->rx_cmd_num = 20;
		tx[0] = 0x0A;

		for (i = 0; i < 20; i++) {
			cmd_msg->type[i] = 0x06;
			cmd_msg->tx_buf[i] = tx;
			cmd_msg->tx_len[i] = 1;

			cmd_msg->rx_buf[i] = kmalloc(4 * sizeof(unsigned char),
				GFP_ATOMIC);
			memset(cmd_msg->rx_buf[i], 0, 4);
			cmd_msg->rx_len[i] = 1;
		}
		break;
	}
	case 2:
	{
		/* Read 0xe8 = 0x00,0x01,0x23,0x00 */
		cmd_msg->channel = 0;
		cmd_msg->tx_cmd_num = 1;
		cmd_msg->type[0] = 0x06;
		tx[0] = 0xe8;
		cmd_msg->tx_buf[0] = tx;
		cmd_msg->tx_len[0] = 1;

		cmd_msg->rx_cmd_num = 1;
		cmd_msg->rx_buf[0] = kmalloc(8 * sizeof(unsigned char),
			GFP_ATOMIC);
		memset(cmd_msg->rx_buf[0], 0, 4);
		cmd_msg->rx_len[0] = 4;

		break;
	}
	case 1002:
	{
		/* Read 0xe8 = 0x00,0x01,0x23,0x00 */
		cmd_msg->channel = 0;
		cmd_msg->tx_cmd_num = 20;
		cmd_msg->rx_cmd_num = 20;
		tx[0] = 0xe8;

		for (i = 0; i < 20; i++) {
			cmd_msg->type[i] = 0x06;
			cmd_msg->tx_buf[i] = tx;
			cmd_msg->tx_len[i] = 1;

			cmd_msg->rx_buf[i] = kmalloc(8 * sizeof(unsigned char),
				GFP_ATOMIC);
			memset(cmd_msg->rx_buf[i], 0, 4);
			cmd_msg->rx_len[i] = 4;
		}
		break;
	}
	case 3:
	{
/*
 * Read 0xb6 =
 *	0x30,0x6b,0x00,0x06,0x03,0x0A,0x13,0x1A,0x6C,0x18
 */
		cmd_msg->channel = 0;
		cmd_msg->tx_cmd_num = 1;
		cmd_msg->type[0] = 0x06;
		tx[0] = 0xb6;
		cmd_msg->tx_buf[0] = tx;
		cmd_msg->tx_len[0] = 1;

		cmd_msg->rx_cmd_num = 1;
		cmd_msg->rx_buf[0] = kmalloc(20 * sizeof(unsigned char),
			GFP_ATOMIC);
		memset(cmd_msg->rx_buf[0], 0, 20);
		cmd_msg->rx_len[0] = 10;

		break;
	}
	case 4:
	{
		/* Read 0x0e = 0x80 */
		cmd_msg->channel = 0;
		cmd_msg->tx_cmd_num = 1;
		cmd_msg->type[0] = 0x06;
		tx[0] = 0x0e;
		cmd_msg->tx_buf[0] = tx;
		cmd_msg->tx_len[0] = 1;

		cmd_msg->rx_cmd_num = 1;
		cmd_msg->rx_buf[0] = kmalloc(4 * sizeof(unsigned char),
			GFP_ATOMIC);
		memset(cmd_msg->rx_buf[0], 0, 4);
		cmd_msg->rx_len[0] = 1;

		break;
	}
	case 5:
	{
		/* multiple cmd Read*/
		/*0x0A = 0x1C;*/
		cmd_msg->channel = 0;
		cmd_msg->tx_cmd_num = 2;
		cmd_msg->rx_cmd_num = 2;

		cmd_msg->type[0] = 0x06;
		tx[0] = 0x0A;
		cmd_msg->tx_buf[0] = tx;
		cmd_msg->tx_len[0] = 1;

		cmd_msg->rx_buf[0] = kmalloc(4 * sizeof(unsigned char),
			GFP_ATOMIC);
		memset(cmd_msg->rx_buf[0], 0, 4);
		cmd_msg->rx_len[0] = 1;

		/*0x0e = 0x80 */
		cmd_msg->type[1] = 0x06;
		tx_1[0] = 0x0e;
		cmd_msg->tx_buf[1] = tx_1;
		cmd_msg->tx_len[1] = 1;

		cmd_msg->rx_buf[1] = kmalloc(4 * sizeof(unsigned char),
			GFP_ATOMIC);
		memset(cmd_msg->rx_buf[1], 0, 4);
		cmd_msg->rx_len[1] = 1;
		break;
	}
	case 6:
	{
		/* multiple cmd Read*/
		/*0x0A = 0x1C; */
		cmd_msg->channel = 0;
		cmd_msg->tx_cmd_num = 3;
		cmd_msg->rx_cmd_num = 3;

		cmd_msg->type[0] = 0x06;
		tx[0] = 0x0A;
		cmd_msg->tx_buf[0] = tx;
		cmd_msg->tx_len[0] = 1;
		cmd_msg->rx_buf[0] = kmalloc(4 * sizeof(unsigned char),
			GFP_ATOMIC);
		memset(cmd_msg->rx_buf[0], 0, 4);
		cmd_msg->rx_len[0] = 1;

		/*0x0e = 0x80 */
		cmd_msg->type[1] = 0x06;
		tx_1[0] = 0x0e;
		cmd_msg->tx_buf[1] = tx_1;
		cmd_msg->tx_len[1] = 1;
		cmd_msg->rx_buf[1] = kmalloc(4 * sizeof(unsigned char),
			GFP_ATOMIC);
		memset(cmd_msg->rx_buf[1], 0, 4);
		cmd_msg->rx_len[1] = 1;

		/* Read 0xe8 = 0x00,0x01,0x23,0x00 */
		cmd_msg->type[2] = 0x06;
		tx_2[0] = 0xe8;
		cmd_msg->tx_buf[2] = tx_2;
		cmd_msg->tx_len[2] = 1;
		cmd_msg->rx_buf[2] = kmalloc(8 * sizeof(unsigned char),
			GFP_ATOMIC);
		memset(cmd_msg->rx_buf[2], 0, 4);
		cmd_msg->rx_len[2] = 4;
		break;
	}
	case 7:
	{
		/* multiple cmd Read*/
		/*0x0A = 0x1C; */
		cmd_msg->channel = 0;
		cmd_msg->tx_cmd_num = 4;
		cmd_msg->rx_cmd_num = 4;

		cmd_msg->type[0] = 0x06;
		tx[0] = 0x0A;
		cmd_msg->tx_buf[0] = tx;
		cmd_msg->tx_len[0] = 1;
		cmd_msg->rx_buf[0] = kmalloc(4 * sizeof(unsigned char),
			GFP_ATOMIC);
		memset(cmd_msg->rx_buf[0], 0, 4);
		cmd_msg->rx_len[0] = 1;

		/*0x0e = 0x80 */
		cmd_msg->type[1] = 0x06;
		tx_1[0] = 0x0e;
		cmd_msg->tx_buf[1] = tx_1;
		cmd_msg->tx_len[1] = 1;
		cmd_msg->rx_buf[1] = kmalloc(4 * sizeof(unsigned char),
			GFP_ATOMIC);
		memset(cmd_msg->rx_buf[1], 0, 4);
		cmd_msg->rx_len[1] = 1;

		/* Read 0xe8 = 0x00,0x01,0x23,0x00 */
		cmd_msg->type[2] = 0x06;
		tx_2[0] = 0xe8;
		cmd_msg->tx_buf[2] = tx_2;
		cmd_msg->tx_len[2] = 1;
		cmd_msg->rx_buf[2] = kmalloc(8 * sizeof(unsigned char),
			GFP_ATOMIC);
		memset(cmd_msg->rx_buf[2], 0, 4);
		cmd_msg->rx_len[2] = 4;

		/*
		 * Read 0xb6 =
		 *	0x30,0x6b,0x00,0x06,0x03,0x0A,0x13,0x1A,0x6C,0x18
		 */
		cmd_msg->type[3] = 0x06;
		tx_3[0] = 0xb6;
		cmd_msg->tx_buf[3] = tx_3;
		cmd_msg->tx_len[3] = 1;
		cmd_msg->rx_buf[3] = kmalloc(20 * sizeof(unsigned char),
			GFP_ATOMIC);
		memset(cmd_msg->rx_buf[3], 0, 20);
		cmd_msg->rx_len[3] = 10;
		break;
	}
	default:
		DDPMSG("%s no this test case:%d\n", __func__, case_num);
		break;
	}

	ret = mtk_ddic_dsi_read_cmd(cmd_msg);
	if (ret != 0) {
		DDPPR_ERR("%s error\n", __func__);
		goto  done;
	}

	for (i = 0; i < cmd_msg->rx_cmd_num; i++) {
		ret_dlen = cmd_msg->rx_len[i];
		DDPMSG("read lcm addr:0x%x--dlen:%d--cmd_idx:%d\n",
			*(char *)(cmd_msg->tx_buf[i]), ret_dlen, i);
		for (j = 0; j < ret_dlen; j++) {
			DDPMSG("read lcm addr:0x%x--byte:%d,val:0x%x\n",
				*(char *)(cmd_msg->tx_buf[i]), j,
				*(char *)(cmd_msg->rx_buf[i] + j));
		}
	}

done:
	for (i = 0; i < cmd_msg->rx_cmd_num; i++)
		kfree(cmd_msg->rx_buf[i]);
	vfree(cmd_msg);

	DDPMSG("%s end -\n", __func__);
}

int mtk_dprec_mmp_dump_ovl_layer(struct mtk_plane_state *plane_state)
{
	if (gCaptureOVLEn) {
		mtk_drm_mmp_ovl_layer(plane_state, gCapturePriLayerDownX,
			gCapturePriLayerDownY);
		return 0;
	}
	DDPINFO("%s, gCapturePriLayerEnable is %d\n",
		__func__, gCaptureOVLEn);
	return -1;
}

int mtk_dprec_mmp_dump_cwb_buffer(struct drm_crtc *crtc,
		void *buffer, unsigned int buf_idx)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	if (gCaptureWDMAEn && mtk_crtc->cwb_info) {
		mtk_drm_mmp_cwb_buffer(crtc, mtk_crtc->cwb_info,
					buffer, buf_idx);
		return 0;
	}
	DDPDBG("%s, gCaptureWDMAEn is %d\n",
		__func__, gCaptureWDMAEn);
	return -1;
}

static void user_copy_done_function(void *buffer,
	enum CWB_BUFFER_TYPE type)
{
	DDPMSG("[capture] I get buffer:0x%x, type:%d\n",
			buffer, type);
	complete(&cwb_cmp);
}

static const struct mtk_cwb_funcs user_cwb_funcs = {
	.copy_done = user_copy_done_function,
};

static void mtk_drm_cwb_info_init(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	int crtc_idx = drm_crtc_index(&mtk_crtc->base);
	struct mtk_drm_private *priv = mtk_crtc->base.dev->dev_private;
	struct mtk_cwb_info *cwb_info = mtk_crtc->cwb_info;
	struct drm_mode_fb_cmd2 mode = {0};
	struct mtk_drm_gem_obj *mtk_gem;

	if (!cwb_info) {
		DDPPR_ERR("%s: cwb_info not found\n", __func__);
		return;
	} else if (!cwb_info->enable)
		return;

	cwb_info->count = 0;

	cwb_info->src_roi.width =
				crtc->state->adjusted_mode.hdisplay;
	cwb_info->src_roi.height =
				crtc->state->adjusted_mode.vdisplay;

	cwb_info->scn = WDMA_WRITE_BACK;
	if (crtc_idx == 0)
		cwb_info->comp = priv->ddp_comp[DDP_COMPONENT_WDMA0];

	if (!cwb_info->buffer[0].dst_roi.width ||
		!cwb_info->buffer[0].dst_roi.height) {
		mtk_rect_make(&cwb_info->buffer[0].dst_roi, 0, 0,
			MTK_CWB_NO_EFFECT_HRT_MAX_WIDTH,
			MTK_CWB_NO_EFFECT_HRT_MAX_WIDTH);
		mtk_rect_make(&cwb_info->buffer[1].dst_roi, 0, 0,
			MTK_CWB_NO_EFFECT_HRT_MAX_WIDTH,
			MTK_CWB_NO_EFFECT_HRT_MAX_WIDTH);
	}

	/*alloc && config two fb*/
	if (!cwb_info->buffer[0].fb) {
		mode.width = MTK_CWB_NO_EFFECT_HRT_MAX_WIDTH;
		mode.height = cwb_info->src_roi.height;
		mode.pixel_format = DRM_FORMAT_RGB888;
		mode.pitches[0] = mode.width * 3;

		mtk_gem = mtk_drm_gem_create(
			crtc->dev, mode.width * mode.height * 3, true);
		cwb_info->buffer[0].addr_mva = (u32)mtk_gem->dma_addr;
		cwb_info->buffer[0].addr_va = (u64)mtk_gem->kvaddr;

		cwb_info->buffer[0].fb  =
			mtk_drm_framebuffer_create(
			crtc->dev, &mode, &mtk_gem->base);
		DDPMSG("[capture] b[0].addr_mva:0x%x, addr_va:0x%llx\n",
				cwb_info->buffer[0].addr_mva,
				cwb_info->buffer[0].addr_va);

		mtk_gem = mtk_drm_gem_create(
			crtc->dev, mode.width * mode.height * 3, true);
		cwb_info->buffer[1].addr_mva = (u32)mtk_gem->dma_addr;
		cwb_info->buffer[1].addr_va = (u64)mtk_gem->kvaddr;

		cwb_info->buffer[1].fb  =
			mtk_drm_framebuffer_create(
			crtc->dev, &mode, &mtk_gem->base);
		DDPMSG("[capture] b[1].addr_mva:0x%x, addr_va:0x%llx\n",
				cwb_info->buffer[1].addr_mva,
				cwb_info->buffer[1].addr_va);
	}

	DDPMSG("[capture] enable capture, roi:(%d,%d,%d,%d)\n",
		cwb_info->buffer[0].dst_roi.x,
		cwb_info->buffer[0].dst_roi.y,
		cwb_info->buffer[0].dst_roi.width,
		cwb_info->buffer[0].dst_roi.height);
}

bool mtk_drm_cwb_enable(int en,
			const struct mtk_cwb_funcs *funcs,
			enum CWB_BUFFER_TYPE type)
{
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_cwb_info *cwb_info;

	crtc = list_first_entry(&(drm_dev)->mode_config.crtc_list,
				typeof(*crtc), head);
	if (!crtc) {
		DDPPR_ERR("find crtc fail\n");
		return false;
	}
	mtk_crtc = to_mtk_crtc(crtc);

	if (!mtk_crtc->cwb_info) {
		mtk_crtc->cwb_info = kzalloc(sizeof(struct mtk_cwb_info),
			GFP_KERNEL);
		DDPMSG("%s: need allocate memory\n", __func__);
	}
	if (!mtk_crtc->cwb_info) {
		DDPPR_ERR("%s: allocate memory fail\n", __func__);
		return false;
	}

	cwb_info = mtk_crtc->cwb_info;
	if (cwb_info->enable == en) {
		DDPMSG("[capture] en:%d already effective\n", en);
		return true;
	}
	cwb_info->funcs = funcs;
	cwb_info->type = type;

	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);
	cwb_info->enable = en;
	if (en)
		mtk_drm_cwb_info_init(crtc);
	else
		DDPMSG("[capture] disable capture");
	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

	return true;
}

bool mtk_drm_set_cwb_roi(struct mtk_rect rect)
{
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_cwb_info *cwb_info;
	struct mtk_drm_gem_obj *mtk_gem;
	struct drm_mode_fb_cmd2 mode = {0};

	crtc = list_first_entry(&(drm_dev)->mode_config.crtc_list,
				typeof(*crtc), head);
	if (!crtc) {
		DDPPR_ERR("find crtc fail\n");
		return false;
	}

	mtk_crtc = to_mtk_crtc(crtc);
	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);
	if (!mtk_crtc->cwb_info) {
		mtk_crtc->cwb_info = kzalloc(sizeof(struct mtk_cwb_info),
			GFP_KERNEL);
			DDPMSG("%s: need allocate memory\n", __func__);
	}
	if (!mtk_crtc->cwb_info) {
		DDPPR_ERR("%s: allocate memory fail\n", __func__);
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		return false;
	}
	cwb_info = mtk_crtc->cwb_info;
	cwb_info->src_roi.width =
				crtc->state->adjusted_mode.hdisplay;
	cwb_info->src_roi.height =
				crtc->state->adjusted_mode.vdisplay;

	if (rect.x >= cwb_info->src_roi.width ||
		rect.y >= cwb_info->src_roi.height ||
		!rect.width || !rect.height) {
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		return false;
	}

	if (rect.width > MTK_CWB_NO_EFFECT_HRT_MAX_WIDTH)
		rect.width = MTK_CWB_NO_EFFECT_HRT_MAX_WIDTH;

	if (rect.x + rect.width > cwb_info->src_roi.width)
		rect.width = cwb_info->src_roi.width - rect.x;
	if (rect.y + rect.height > cwb_info->src_roi.height)
		rect.height = cwb_info->src_roi.height - rect.y;

	if (!cwb_info->buffer[0].fb) {
		mode.width = MTK_CWB_NO_EFFECT_HRT_MAX_WIDTH;
		mode.height = cwb_info->src_roi.height;
		mode.pixel_format = DRM_FORMAT_RGB888;
		mode.pitches[0] = mode.width * 3;

		mtk_gem = mtk_drm_gem_create(
			crtc->dev, mode.width * mode.height * 3, true);
		cwb_info->buffer[0].addr_mva = (u32)mtk_gem->dma_addr;
		cwb_info->buffer[0].addr_va = (u64)mtk_gem->kvaddr;

		cwb_info->buffer[0].fb  =
			mtk_drm_framebuffer_create(
			crtc->dev, &mode, &mtk_gem->base);
		DDPMSG("[capture] b[0].addr_mva:0x%x, addr_va:0x%llx\n",
				cwb_info->buffer[0].addr_mva,
				cwb_info->buffer[0].addr_va);

		mtk_gem = mtk_drm_gem_create(
			crtc->dev, mode.width * mode.height * 3, true);
		cwb_info->buffer[1].addr_mva = (u32)mtk_gem->dma_addr;
		cwb_info->buffer[1].addr_va = (u64)mtk_gem->kvaddr;

		cwb_info->buffer[1].fb  =
			mtk_drm_framebuffer_create(
			crtc->dev, &mode, &mtk_gem->base);
		DDPMSG("[capture] b[1].addr_mva:0x%x, addr_va:0x%llx\n",
				cwb_info->buffer[1].addr_mva,
				cwb_info->buffer[1].addr_va);
	}

	/* update roi */
	mtk_rect_make(&cwb_info->buffer[0].dst_roi,
		rect.x, rect.y, rect.width, rect.height);
	mtk_rect_make(&cwb_info->buffer[1].dst_roi,
		rect.x, rect.y, rect.width, rect.height);

	DDPMSG("[capture] change roi:(%d,%d,%d,%d)\n",
		cwb_info->buffer[0].dst_roi.x,
		cwb_info->buffer[0].dst_roi.y,
		cwb_info->buffer[0].dst_roi.width,
		cwb_info->buffer[0].dst_roi.height);

	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
	return true;

}

void mtk_drm_cwb_backup_copy_size(void)
{
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_cwb_info *cwb_info;
	struct mtk_ddp_comp *comp;

	crtc = list_first_entry(&(drm_dev)->mode_config.crtc_list,
				typeof(*crtc), head);
	if (!crtc) {
		DDPPR_ERR("find crtc fail\n");
		return;
	}
	mtk_crtc = to_mtk_crtc(crtc);
	cwb_info = mtk_crtc->cwb_info;

	if (!cwb_info)
		return;

	if (!cwb_info->comp) {
		DDPPR_ERR("[capture] cwb enable, but has not comp\n");
		return;
	}

	comp = cwb_info->comp;
	mtk_ddp_comp_io_cmd(comp, NULL, WDMA_READ_DST_SIZE, cwb_info);
}

bool mtk_drm_set_cwb_user_buf(void *user_buffer, enum CWB_BUFFER_TYPE type)
{
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_cwb_info *cwb_info;

	/* this debug cmd only for crtc0 */
	crtc = list_first_entry(&(drm_dev)->mode_config.crtc_list,
				typeof(*crtc), head);
	if (!crtc) {
		DDPPR_ERR("find crtc fail\n");
		return false;
	}
	mtk_crtc = to_mtk_crtc(crtc);
	cwb_info = mtk_crtc->cwb_info;

	if (!cwb_info)
		return false;

	DDP_MUTEX_LOCK(&mtk_crtc->cwb_lock, __func__, __LINE__);
	cwb_info->type = type;
	cwb_info->user_buffer = user_buffer;
	DDP_MUTEX_UNLOCK(&mtk_crtc->cwb_lock, __func__, __LINE__);
	DDPMSG("[capture] User set buffer:0x%x, type:%d\n",
			user_buffer, type);

	return true;
}

int mtk_drm_ioctl_pq_get_persist_property(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	int i, ret = 0;
	unsigned int pq_persist_property[32];
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_crtc *crtc = private->crtc[0];

	memset(pq_persist_property, 0, sizeof(pq_persist_property));
	memcpy(pq_persist_property, (unsigned int *)data, sizeof(pq_persist_property));

	for (i = 0; i < DISP_PQ_PROPERTY_MAX; i++) {
		m_old_pq_persist_property[i] = m_new_pq_persist_property[i];
		m_new_pq_persist_property[i] = pq_persist_property[i];
	}

	DDPFUNC("+");

	if (m_old_pq_persist_property[DISP_PQ_COLOR_BYPASS] !=
		m_new_pq_persist_property[DISP_PQ_COLOR_BYPASS])
		disp_color_set_bypass(crtc, m_new_pq_persist_property[DISP_PQ_COLOR_BYPASS]);

	if (m_old_pq_persist_property[DISP_PQ_CCORR_BYPASS] !=
		m_new_pq_persist_property[DISP_PQ_CCORR_BYPASS])
		disp_ccorr_set_bypass(crtc, m_new_pq_persist_property[DISP_PQ_CCORR_BYPASS]);

	if (m_old_pq_persist_property[DISP_PQ_GAMMA_BYPASS] !=
		m_new_pq_persist_property[DISP_PQ_GAMMA_BYPASS])
		disp_gamma_set_bypass(crtc, m_new_pq_persist_property[DISP_PQ_GAMMA_BYPASS]);

	if (m_old_pq_persist_property[DISP_PQ_DITHER_BYPASS] !=
		m_new_pq_persist_property[DISP_PQ_DITHER_BYPASS])
		disp_dither_set_bypass(crtc, m_new_pq_persist_property[DISP_PQ_DITHER_BYPASS]);

	if (m_old_pq_persist_property[DISP_PQ_AAL_BYPASS] !=
		m_new_pq_persist_property[DISP_PQ_AAL_BYPASS])
		disp_aal_set_bypass(crtc, m_new_pq_persist_property[DISP_PQ_AAL_BYPASS]);

	DDPFUNC("-");

	return ret;
}

static void process_dbg_opt(const char *opt)
{
	DDPINFO("display_debug cmd %s\n", opt);

	if (strncmp(opt, "helper", 6) == 0) {
		/*ex: echo helper:DISP_OPT_BYPASS_OVL,0 > /d/mtkfb */
		char option[100] = "";
		char *tmp;
		int value, i;
		enum MTK_DRM_HELPER_OPT helper_opt;
		struct mtk_drm_private *priv = drm_dev->dev_private;
		int ret;

		tmp = (char *)(opt + 7);
		for (i = 0; i < 100; i++) {
			if (tmp[i] != ',' && tmp[i] != ' ')
				option[i] = tmp[i];
			else
				break;
		}
		tmp += i + 1;
		ret = sscanf(tmp, "%d\n", &value);
		if (ret != 1) {
			DDPPR_ERR("error to parse cmd %s: %s %s ret=%d\n", opt,
				  option, tmp, ret);
			return;
		}

		DDPMSG("will set option %s to %d\n", option, value);
		mtk_drm_helper_set_opt_by_name(priv->helper_opt, option, value);
		helper_opt =
			mtk_drm_helper_name_to_opt(priv->helper_opt, option);
		mtk_update_layering_opt_by_disp_opt(helper_opt, value);
	} else if (strncmp(opt, "mobile:", 7) == 0) {
		if (strncmp(opt + 7, "on", 2) == 0)
			g_mobile_log = 1;
		else if (strncmp(opt + 7, "off", 3) == 0)
			g_mobile_log = 0;
	} else if (strncmp(opt, "fence:", 6) == 0) {
		if (strncmp(opt + 6, "on", 2) == 0)
			g_fence_log = 1;
		else if (strncmp(opt + 6, "off", 3) == 0)
			g_fence_log = 0;
	} else if (strncmp(opt, "irq:", 4) == 0) {
		if (strncmp(opt + 4, "on", 2) == 0)
			g_irq_log = 1;
		else if (strncmp(opt + 4, "off", 3) == 0)
			g_irq_log = 0;
	} else if (strncmp(opt, "detail:", 7) == 0) {
		if (strncmp(opt + 7, "on", 2) == 0)
			g_detail_log = 1;
		else if (strncmp(opt + 7, "off", 3) == 0)
			g_detail_log = 0;
	} else if (strncmp(opt, "trace:", 6) == 0) {
		if (strncmp(opt + 6, "on", 2) == 0)
			g_trace_log = 1;
		else if (strncmp(opt + 6, "off", 3) == 0)
			g_trace_log = 0;
	} else if (strncmp(opt, "diagnose", 8) == 0) {
		struct drm_crtc *crtc;
		struct mtk_drm_crtc *mtk_crtc;

		drm_for_each_crtc(crtc, drm_dev) {
			if (!crtc) {
				DDPPR_ERR("find crtc fail\n");
				continue;
			}

			mtk_crtc = to_mtk_crtc(crtc);
			if (!crtc->enabled
				|| mtk_crtc->ddp_mode == DDP_NO_USE)
				continue;

			mtk_drm_crtc_analysis(crtc);
			mtk_drm_crtc_dump(crtc);
		}
	} else if (strncmp(opt, "repaint", 7) == 0) {
		drm_trigger_repaint(DRM_REPAINT_FOR_IDLE, drm_dev);
	} else if (strncmp(opt, "dalprintf", 9) == 0) {
		struct drm_crtc *crtc;

		crtc = list_first_entry(&(drm_dev)->mode_config.crtc_list,
					typeof(*crtc), head);

		if (!crtc) {
			DDPPR_ERR("find crtc fail\n");
			return;
		}

		DAL_Printf("DAL printf\n");
	} else if (strncmp(opt, "dalclean", 8) == 0) {
		struct drm_crtc *crtc;

		crtc = list_first_entry(&(drm_dev)->mode_config.crtc_list,
					typeof(*crtc), head);

		if (!crtc) {
			DDPPR_ERR("find crtc fail\n");
			return;
		}

		DAL_Clean();
	} else if (strncmp(opt, "ata_check", 9) == 0) {
		struct drm_crtc *crtc;

		crtc = list_first_entry(&(drm_dev)->mode_config.crtc_list,
					typeof(*crtc), head);

		if (!crtc) {
			DDPMSG("find crtc fail\n");
			return;
		}

		mtk_crtc_lcm_ATA(crtc);
	} else if (strncmp(opt, "path_switch:", 11) == 0) {
		struct drm_crtc *crtc;
		int path_sel, ret;

		crtc = list_first_entry(&(drm_dev)->mode_config.crtc_list,
					typeof(*crtc), head);

		if (!crtc) {
			DDPPR_ERR("find crtc fail\n");
			return;
		}
		ret = sscanf(opt, "path_switch:%d\n", &path_sel);
		mtk_crtc_path_switch(crtc, path_sel, 1);
	} else if (strncmp(opt, "enable_idlemgr:", 15) == 0) {
		char *p = (char *)opt + 15;
		unsigned int flg = 0;
		struct drm_crtc *crtc;
		int ret;

		ret = kstrtouint(p, 0, &flg);
		if (ret) {
			DDPPR_ERR("%d error to parse cmd %s\n", __LINE__, opt);
			return;
		}

		/* this debug cmd only for crtc0 */
		crtc = list_first_entry(&(drm_dev)->mode_config.crtc_list,
					typeof(*crtc), head);
		if (!crtc) {
			DDPPR_ERR("find crtc fail\n");
			return;
		}

		mtk_drm_set_idlemgr(crtc, flg, 1);
	} else if (strncmp(opt, "idle_wait:", 10) == 0) {
		unsigned long long idle_check_interval = 0;
		struct drm_crtc *crtc;
		int ret;

		ret = sscanf(opt, "idle_wait:%llu\n", &idle_check_interval);
		if (ret != 1) {
			DDPPR_ERR("%d error to parse cmd %s\n", __LINE__, opt);
			return;
		}

		/* this debug cmd only for crtc0 */
		crtc = list_first_entry(&(drm_dev)->mode_config.crtc_list,
					typeof(*crtc), head);
		if (!crtc) {
			DDPPR_ERR("find crtc fail\n");
			return;
		}

		idle_check_interval = max(idle_check_interval, 17ULL);
		mtk_drm_set_idle_check_interval(crtc, idle_check_interval);
		DDPMSG("change idle interval to %llu ms\n",
		       idle_check_interval);
	} else if (strncmp(opt, "hrt_bw", 6) == 0) {
		DDPINFO("HRT test+\n");
#ifdef MTK_FB_MMDVFS_SUPPORT
		mtk_disp_hrt_bw_dbg();
#endif
		DDPINFO("HRT test-\n");
	} else if (strncmp(opt, "lcm0_reset", 10) == 0) {
		struct mtk_ddp_comp *comp;
		struct drm_crtc *crtc;
		struct mtk_drm_crtc *mtk_crtc;
		int enable;

		/* this debug cmd only for crtc0 */
		crtc = list_first_entry(&(drm_dev)->mode_config.crtc_list,
					typeof(*crtc), head);
		if (!crtc) {
			DDPPR_ERR("find crtc fail\n");
			return;
		}

		mtk_crtc = to_mtk_crtc(crtc);
		comp = mtk_ddp_comp_request_output(mtk_crtc);
		if (!comp || !comp->funcs || !comp->funcs->io_cmd) {
			DDPINFO("cannot find output component\n");
			return;
		}
		DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);
		DDPMSG("lcm0_reset tset\n");
		enable = 1;
		comp->funcs->io_cmd(comp, NULL, LCM_RESET, &enable);
		msleep(20);
		enable = 0;
		comp->funcs->io_cmd(comp, NULL, LCM_RESET, &enable);
		msleep(20);
		enable = 1;
		comp->funcs->io_cmd(comp, NULL, LCM_RESET, &enable);
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
	} else if (strncmp(opt, "backlight:", 10) == 0) {
		unsigned int level;
		int ret;

		ret = sscanf(opt, "backlight:%u\n", &level);
		if (ret != 1) {
			DDPPR_ERR("%d error to parse cmd %s\n",
				__LINE__, opt);
			return;
		}

		mtkfb_set_backlight_level(level);
	} else if (!strncmp(opt, "aod_bl:", 7)) {
		unsigned int level;
		int ret;

		ret = sscanf(opt, "aod_bl:%u\n", &level);
		if (ret != 1) {
			pr_info("%d fail to parse cmd %s\n",
				__LINE__, opt);
			return;
		}

		mtkfb_set_aod_backlight_level(level);
	} else if (strncmp(opt, "dump_fake_engine", 16) == 0) {
		struct drm_crtc *crtc;
		struct mtk_drm_crtc *mtk_crtc;

		crtc = list_first_entry(&(drm_dev)->mode_config.crtc_list,
				typeof(*crtc), head);
		if (!crtc) {
			DDPPR_ERR("find crtc fail\n");
			return;
		}

		mtk_crtc = to_mtk_crtc(crtc);
		dump_fake_engine(mtk_crtc->config_regs);
	} else if (!strncmp(opt, "fake_engine:", 12)) {
		unsigned int en, idx, wr_en, rd_en, wr_pat1, wr_pat2, latency,
				preultra_cnt, ultra_cnt;
		struct drm_crtc *crtc;
		int ret = 0;

		ret = sscanf(opt, "fake_engine:%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
				&idx, &en, &wr_en, &rd_en, &wr_pat1, &wr_pat2,
				&latency, &preultra_cnt, &ultra_cnt);

		if (ret != 9) {
			DDPPR_ERR("%d error to parse cmd %s\n",
				__LINE__, opt);
			return;
		}

		crtc = list_first_entry(&(drm_dev)->mode_config.crtc_list,
				typeof(*crtc), head);
		if (!crtc) {
			DDPPR_ERR("find crtc fail\n");
			return;
		}

		mtk_drm_idlemgr_kick(__func__, crtc, 1);
		mtk_drm_set_idlemgr(crtc, 0, 1);
		fake_engine(crtc, idx, en, wr_en, rd_en, wr_pat1, wr_pat2,
			latency, preultra_cnt, ultra_cnt);
	} else if (strncmp(opt, "checkt", 6) == 0) { /* check trigger */
		struct drm_crtc *crtc;
		struct mtk_drm_crtc *mtk_crtc;

		/* this debug cmd only for crtc0 */
		crtc = list_first_entry(&(drm_dev)->mode_config.crtc_list,
				typeof(*crtc), head);

		if (!crtc) {
			DDPPR_ERR("find crtc fail\n");
			return;
		}

		mtk_crtc = to_mtk_crtc(crtc);
		mtk_crtc_check_trigger(mtk_crtc, false, true);
	} else if (strncmp(opt, "checkd", 6) == 0) { /* check trigger delay */
		struct drm_crtc *crtc;
		struct mtk_drm_crtc *mtk_crtc;

		/* this debug cmd only for crtc0 */
		crtc = list_first_entry(&(drm_dev)->mode_config.crtc_list,
				typeof(*crtc), head);
		if (!crtc) {
			DDPPR_ERR("find crtc fail\n");
			return;
		}

		mtk_crtc = to_mtk_crtc(crtc);
		mtk_crtc_check_trigger(mtk_crtc, true, true);
	} else if (!strncmp(opt, "fake_layer:", 11)) {
		unsigned int mask;
		struct drm_crtc *crtc;
		struct mtk_drm_crtc *mtk_crtc;
		int ret = 0;

		ret = sscanf(opt, "fake_layer:0x%x\n", &mask);
		if (ret != 1) {
			DDPPR_ERR("%d error to parse cmd %s\n",
				__LINE__, opt);
			return;
		}

		crtc = list_first_entry(&(drm_dev)->mode_config.crtc_list,
				typeof(*crtc), head);
		if (!crtc) {
			DDPPR_ERR("find crtc fail\n");
			return;
		}

		mtk_drm_idlemgr_kick(__func__, crtc, 1);
		mtk_drm_set_idlemgr(crtc, 0, 1);

		prepare_fake_layer_buffer(crtc);

		mtk_crtc = to_mtk_crtc(crtc);
		if (!mask && mtk_crtc->fake_layer.fake_layer_mask)
			mtk_crtc->fake_layer.first_dis = true;
		mtk_crtc->fake_layer.fake_layer_mask = mask;

		DDPINFO("fake_layer:0x%x enable\n", mask);
	} else if (!strncmp(opt, "mipi_ccci:", 10)) {
		unsigned int en, ret;

		ret = sscanf(opt, "mipi_ccci:%d\n", &en);
		if (ret != 1) {
			DDPPR_ERR("%d error to parse cmd %s\n",
				__LINE__, opt);
			return;
		}

		DDPINFO("mipi_ccci:%d\n", en);
		mtk_disp_mipi_ccci_callback(en, 0);
	} else if (strncmp(opt, "aal:", 4) == 0) {
		disp_aal_debug(opt + 4);
	} else if (strncmp(opt, "aee:", 4) == 0) {
		DDPAEE("trigger aee dump of mmproile\n");
	} else if (strncmp(opt, "send_ddic_test:", 15) == 0) {
		unsigned int case_num, ret;

		ret = sscanf(opt, "send_ddic_test:%d\n", &case_num);
		if (ret != 1) {
			DDPPR_ERR("%d error to parse cmd %s\n",
				__LINE__, opt);
			return;
		}

		DDPMSG("send_ddic_test:%d\n", case_num);

		ddic_dsi_send_cmd_test(case_num);
	} else if (strncmp(opt, "read_ddic_test:", 15) == 0) {
		unsigned int case_num, ret;

		ret = sscanf(opt, "read_ddic_test:%d\n", &case_num);
		if (ret != 1) {
			DDPPR_ERR("%d error to parse cmd %s\n",
				__LINE__, opt);
			return;
		}

		DDPMSG("read_ddic_test:%d\n", case_num);

		ddic_dsi_read_cmd_test(case_num);
	} else if (!strncmp(opt, "chg_mipi:", 9)) {
		int ret;
		unsigned int rate;
		struct drm_crtc *crtc;

		ret = sscanf(opt, "chg_mipi:%u\n", &rate);
		if (ret != 1) {
			DDPPR_ERR("%d error to parse cmd %s\n",
				__LINE__, opt);
			return;
		}
		DDPMSG("chg_mipi:%u  1\n", rate);

		crtc = list_first_entry(&(drm_dev)->mode_config.crtc_list,
						typeof(*crtc), head);
		if (!crtc) {
			DDPPR_ERR("find crtc fail\n");
			return;
		}
		DDPMSG("chg_mipi:%u  2\n", rate);

		mtk_mipi_clk_change(crtc, rate);
	} else if (strncmp(opt, "mipi_volt:", 10) == 0) {
		char *p = (char *)opt + 10;
		int ret;

		ret = kstrtouint(p, 0, &mipi_volt);
		if (ret) {
			DDPMSG("%d error to parse cmd %s\n", __LINE__, opt);
			return;
		}

		DDPMSG("mipi_volt change :%d\n",
		       mipi_volt);
	} else if (strncmp(opt, "dump_layer:", 11) == 0) {
		int ret;
		unsigned int dump_en;
		unsigned int downSampleX, downSampleY;

		DDPMSG("get dump\n");
		ret = sscanf(opt, "dump_layer:%d,%d,%d\n", &dump_en,
			     &downSampleX, &downSampleY);
		if (ret != 3) {
			DDPMSG("error to parse cmd\n");
			return;
		}

		if (downSampleX)
			gCapturePriLayerDownX = downSampleX;
		if (downSampleY)
			gCapturePriLayerDownY = downSampleY;
		gCaptureOVLEn = dump_en;
	} else if (strncmp(opt, "dump_user_buffer:", 17) == 0) {
		int ret;
		unsigned int dump_en;

		DDPMSG("get dump\n");
		ret = sscanf(opt, "dump_user_buffer:%d\n", &dump_en);
		if (ret != 1) {
			DDPMSG("error to parse cmd\n");
			return;
		}
		gCaptureWDMAEn = dump_en;
#ifdef CONFIG_MTK_HDMI_SUPPORT
	} else if (strncmp(opt, "dptx:", 5) == 0) {
		mtk_dp_debug(opt + 5);
	} else if (strncmp(opt, "dpintf_dump:", 12) == 0) {
		struct mtk_ddp_comp *comp;
		struct drm_crtc *crtc;
		struct mtk_drm_crtc *mtk_crtc;

		drm_for_each_crtc(crtc, drm_dev) {
			if (!crtc) {
				DDPPR_ERR("find crtc fail\n");
				continue;
			}
			DDPINFO("------find crtc------");
			mtk_crtc = to_mtk_crtc(crtc);
			if (!crtc->enabled
				|| mtk_crtc->ddp_mode == DDP_NO_USE)
				continue;

			mtk_crtc = to_mtk_crtc(crtc);
			comp = mtk_ddp_comp_request_output(mtk_crtc);
			mtk_dp_intf_dump(comp);
		}
#endif
	} else if (strncmp(opt, "pan_disp_test:", 13) == 0) {
		int frame_num, bpp, ret;

		ret = sscanf(opt, "pan_disp_test:%d,%d\n", &frame_num, &bpp);
		if (ret != 2) {
			DDPMSG("%d error to parse cmd %s\n", __LINE__, opt);
			return;
		}

		pan_display_test(frame_num, bpp);
	} else if (strncmp(opt, "arr4_enable", 11) == 0) {
		struct mtk_ddp_comp *comp;
		struct drm_crtc *crtc;
		struct mtk_drm_crtc *mtk_crtc;
		int lfr_enable = 1;

		/* this debug cmd only for crtc0 */
		crtc = list_first_entry(&(drm_dev)->mode_config.crtc_list,
					typeof(*crtc), head);
		if (!crtc) {
			DDPPR_ERR("find crtc fail\n");
			return;
		}

		mtk_crtc = to_mtk_crtc(crtc);
		comp = mtk_ddp_comp_request_output(mtk_crtc);
		if (comp && comp->funcs && comp->funcs->io_cmd)
			comp->funcs->io_cmd(comp, NULL, DSI_LFR_SET, &lfr_enable);
	} else if (strncmp(opt, "LFR_update", 10) == 0) {
		struct mtk_ddp_comp *comp;
		struct drm_crtc *crtc;
		struct mtk_drm_crtc *mtk_crtc;

		/* this debug cmd only for crtc0 */
		crtc = list_first_entry(&(drm_dev)->mode_config.crtc_list,
					typeof(*crtc), head);
		if (!crtc) {
			DDPPR_ERR("find crtc fail\n");
			return;
		}

		mtk_crtc = to_mtk_crtc(crtc);
		comp = mtk_ddp_comp_request_output(mtk_crtc);
		if (comp && comp->funcs && comp->funcs->io_cmd)
			comp->funcs->io_cmd(comp, NULL, DSI_LFR_UPDATE, NULL);
	} else if (strncmp(opt, "LFR_status_check", 16) == 0) {
		//unsigned int data = mtk_dbg_get_LFR_value();
		struct mtk_ddp_comp *comp;
		struct drm_crtc *crtc;
		struct mtk_drm_crtc *mtk_crtc;

		/* this debug cmd only for crtc0 */
		crtc = list_first_entry(&(drm_dev)->mode_config.crtc_list,
					typeof(*crtc), head);
		if (!crtc) {
			DDPPR_ERR("find crtc fail\n");
			return;
		}

		mtk_crtc = to_mtk_crtc(crtc);
		comp = mtk_ddp_comp_request_output(mtk_crtc);
		if (comp && comp->funcs && comp->funcs->io_cmd)
			comp->funcs->io_cmd(comp, NULL, DSI_LFR_STATUS_CHECK, NULL);
	} else if (strncmp(opt, "tui:", 4) == 0) {
		unsigned int en, ret;

		ret = sscanf(opt, "tui:%d\n", &en);
		if (ret != 1) {
			DDPPR_ERR("%d error to parse cmd %s\n",
				__LINE__, opt);
			return;
		}

		if (en)
			display_enter_tui();
		else
			display_exit_tui();
	} else if (strncmp(opt, "cwb_en:", 7) == 0) {
		unsigned int ret, enable;

		/* this debug cmd only for crtc0 */
		ret = sscanf(opt, "cwb_en:%d\n", &enable);
		if (ret != 1) {
			DDPMSG("error to parse cmd\n");
			return;
		}

		mtk_drm_cwb_enable(enable, &user_cwb_funcs, IMAGE_ONLY);
	} else if (strncmp(opt, "cwb_roi:", 8) == 0) {
		unsigned int ret, offset_x, offset_y, clip_w, clip_h;
		struct mtk_rect rect;

		/* this debug cmd only for crtc0 */
		ret = sscanf(opt, "cwb_roi:%d,%d,%d,%d\n", &offset_x,
			     &offset_y, &clip_w, &clip_h);
		if (ret != 4) {
			DDPMSG("error to parse cmd\n");
			return;
		}
		rect.x = offset_x;
		rect.y = offset_y;
		rect.width = clip_w;
		rect.height = clip_h;

		mtk_drm_set_cwb_roi(rect);
	} else if (strncmp(opt, "cwb:", 4) == 0) {
		unsigned int ret, enable, offset_x, offset_y;
		unsigned int clip_w, clip_h;
		struct mtk_rect rect;

		/* this debug cmd only for crtc0 */
		ret = sscanf(opt, "cwb:%d,%d,%d,%d,%d\n", &enable,
				&offset_x, &offset_y,
				&clip_w, &clip_h);
		if (ret != 5) {
			DDPMSG("error to parse cmd\n");
			return;
		}
		rect.x = offset_x;
		rect.y = offset_y;
		rect.width = clip_w;
		rect.height = clip_h;

		mtk_drm_set_cwb_roi(rect);
		mtk_drm_cwb_enable(enable, &user_cwb_funcs, IMAGE_ONLY);
	} else if (strncmp(opt, "cwb_get_buffer", 14) == 0) {
		u8 *user_buffer;
		struct drm_crtc *crtc;
		struct mtk_drm_crtc *mtk_crtc;
		struct mtk_cwb_info *cwb_info;
		int width, height, size, ret;

		/* this debug cmd only for crtc0 */
		crtc = list_first_entry(&(drm_dev)->mode_config.crtc_list,
					typeof(*crtc), head);
		if (!crtc) {
			DDPPR_ERR("find crtc fail\n");
			return;
		}

		mtk_crtc = to_mtk_crtc(crtc);
		cwb_info = mtk_crtc->cwb_info;
		if (!cwb_info)
			return;

		DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);
		width = MTK_CWB_NO_EFFECT_HRT_MAX_WIDTH;
		height = cwb_info->src_roi.height;
		size = sizeof(u8) * width * height * 3;
		user_buffer = kzalloc(size, GFP_KERNEL);
		mtk_drm_set_cwb_user_buf((void *)user_buffer, IMAGE_ONLY);
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		DDPMSG("[capture] wait frame complete\n");
		ret = wait_for_completion_interruptible_timeout(&cwb_cmp,
			msecs_to_jiffies(3000));
		if (ret > 0)
			DDPMSG("[capture] frame complete done\n");
		else {
			DDPMSG("[capture] wait frame timeout(3s)\n");
			DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);
			mtk_drm_set_cwb_user_buf((void *)NULL, IMAGE_ONLY);
			DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		}
		kfree(user_buffer);
		reinit_completion(&cwb_cmp);
	}  else if (strncmp(opt, "drm:", 4) == 0) {
		disp_drm_debug(opt + 4);
	}  else if (strncmp(opt, "fake_wcg", 8) == 0) {
		unsigned int fake_hdr_en = 0;
		struct drm_crtc *crtc;
		struct mtk_panel_params *params = NULL;
		int ret;

		ret = sscanf(opt, "fake_wcg:%u\n", &fake_hdr_en);
		if (ret != 1) {
			DDPPR_ERR("%d error to parse cmd %s\n", __LINE__, opt);
			return;
		}

		/* this debug cmd only for crtc0 */
		crtc = list_first_entry(&(drm_dev)->mode_config.crtc_list,
					typeof(*crtc), head);
		if (!crtc) {
			DDPPR_ERR("find crtc fail\n");
			return;
		}

		params = mtk_drm_get_lcm_ext_params(crtc);
		if (!params) {
			DDPPR_ERR("[Fake HDR] find lcm ext fail\n");
			return;
		}

		params->lcm_color_mode = (fake_hdr_en) ?
			MTK_DRM_COLOR_MODE_DISPLAY_P3 : MTK_DRM_COLOR_MODE_NATIVE;
		DDPINFO("set panel color_mode to %d\n", params->lcm_color_mode);
	}

}

static void process_dbg_cmd(char *cmd)
{
	char *tok;

	DDPINFO("[mtkfb_dbg] %s\n", cmd);

	while ((tok = strsep(&cmd, " ")) != NULL)
		process_dbg_opt(tok);
}

static int debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;

	return 0;
}
static ssize_t debug_read(struct file *file, char __user *ubuf, size_t count,
			  loff_t *ppos)
{
	int debug_bufmax;
	static int n;

	if (*ppos != 0 || !is_buffer_init)
		goto out;

	if (!debug_buffer) {
		debug_buffer = vmalloc(sizeof(char) * DEBUG_BUFFER_SIZE);
		if (!debug_buffer)
			return -ENOMEM;

		memset(debug_buffer, 0, sizeof(char) * DEBUG_BUFFER_SIZE);
	}

	debug_bufmax = DEBUG_BUFFER_SIZE - 1;
	n = debug_get_info(debug_buffer, debug_bufmax);

out:
	if (n < 0)
		return -EINVAL;

	return simple_read_from_buffer(ubuf, count, ppos, debug_buffer, n);
}

static ssize_t debug_write(struct file *file, const char __user *ubuf,
			   size_t count, loff_t *ppos)
{
	const int debug_bufmax = 512 - 1;
	size_t ret;
	char cmd_buffer[512];

	ret = count;

	if (count > debug_bufmax)
		count = debug_bufmax;

	if (copy_from_user(&cmd_buffer, ubuf, count))
		return -EFAULT;

	cmd_buffer[count] = 0;

	process_dbg_cmd(cmd_buffer);

	return ret;
}

static const struct file_operations debug_fops = {
	.read = debug_read, .write = debug_write, .open = debug_open,
};

static int idletime_set(void *data, u64 val)
{
	struct drm_crtc *crtc;
	u64 ret = 0;

	if (val < 33)
		val = 33;
	if (val > 1000000)
		val = 1000000;

	crtc = list_first_entry(&(drm_dev)->mode_config.crtc_list,
				typeof(*crtc), head);
	if (!crtc) {
		DDPPR_ERR("find crtc fail\n");
		return -ENODEV;
	}
	ret = mtk_drm_set_idle_check_interval(crtc, val);
	if (ret == 0)
		return -ENODEV;

	return 0;
}

static int idletime_get(void *data, u64 *val)
{
	struct drm_crtc *crtc;

	crtc = list_first_entry(&(drm_dev)->mode_config.crtc_list,
				typeof(*crtc), head);
	if (!crtc) {
		DDPPR_ERR("find crtc fail\n");
		return -ENODEV;
	}
	*val = mtk_drm_get_idle_check_interval(crtc);
	if (*val == 0)
		return -ENODEV;

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(idletime_fops, idletime_get, idletime_set, "%llu\n");

int disp_met_set(void *data, u64 val)
{
	/*1 enable  ; 0 disable*/
	disp_met_en = val;
	return 0;
}

static int disp_met_get(void *data, u64 *val)
{
	*val = disp_met_en;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(disp_met_fops, disp_met_get, disp_met_set, "%llu\n");
int disp_lfr_dbg_set(void *data, u64 val)
{
	atomic_set(&lfr_dbg, val);
	return 0;
}

static int disp_lfr_dbg_get(void *data, u64 *val)
{
	*val = atomic_read(&lfr_dbg);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(disp_lfr_dbg_fops, disp_lfr_dbg_get,
	disp_lfr_dbg_set, "%llu\n");

int disp_lfr_params_set(void *data, u64 val)
{
	atomic_set(&lfr_params, val);
	return 0;
}

static int disp_lfr_params_get(void *data, u64 *val)
{
	*val = atomic_read(&lfr_params);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(disp_lfr_params_fops, disp_lfr_params_get,
	disp_lfr_params_set, "%llu\n");

unsigned int mtk_dbg_get_lfr_mode_value(void)
{
	unsigned int lfr_mode = (atomic_read(&lfr_params) & 0x03);

	lfr_mode = lfr_mode & 0x03;
	return lfr_mode;
}
unsigned int mtk_dbg_get_lfr_type_value(void)
{
	unsigned int lfr_type = (atomic_read(&lfr_params));

	lfr_type = (lfr_type & 0x0C) >> 2;
	return lfr_type;
}
unsigned int mtk_dbg_get_lfr_enable_value(void)
{
	unsigned int lfr_enable = atomic_read(&lfr_params);

	lfr_enable = (lfr_enable & 0x10) >> 4;
	return lfr_enable;
}
unsigned int mtk_dbg_get_lfr_update_value(void)
{
	unsigned int lfr_update = atomic_read(&lfr_params);

	lfr_update = (lfr_update & 0x20) >> 5;
	return lfr_update;
}
unsigned int mtk_dbg_get_lfr_vse_dis_value(void)
{
	unsigned int lfr_vse_dis = atomic_read(&lfr_params);

	lfr_vse_dis = (lfr_vse_dis & 0x40) >> 6;
	return lfr_vse_dis;
}
unsigned int mtk_dbg_get_lfr_skip_num_value(void)
{
	unsigned int lfr_skip_num = atomic_read(&lfr_params);

	lfr_skip_num = (lfr_skip_num & 0x3F00) >> 8;
	return lfr_skip_num;
}

unsigned int mtk_dbg_get_lfr_dbg_value(void)
{
	return atomic_read(&lfr_dbg);
}

static void backup_vfp_for_lp_cust(u64 vfp)
{
		vfp_backup = vfp;
}

static u64 get_backup_vfp(void)
{
	return vfp_backup;
}

static int idlevfp_set(void *data, u64 val)
{
	if (val > 4095)
		val = 4095;

	backup_vfp_for_lp_cust((unsigned int)val);
	return 0;
}

static int idlevfp_get(void *data, u64 *val)
{
	*val = (u64)get_backup_vfp();
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(idlevfp_fops, idlevfp_get, idlevfp_set, "%llu\n");

void disp_dbg_probe(void)
{
#if IS_ENABLED(CONFIG_DEBUG_FS)
	struct dentry *d_folder;
	struct dentry *d_file;

	mtkfb_dbgfs = debugfs_create_file("mtkfb", S_IFREG | 0444, NULL,
					  NULL, &debug_fops);

	d_folder = debugfs_create_dir("displowpower", NULL);
	if (d_folder) {
		d_file = debugfs_create_file("idletime", S_IFREG | 0644,
					     d_folder, NULL, &idletime_fops);
	}

	d_folder = debugfs_create_dir("mtkfb_debug", NULL);
	if (d_folder) {
		d_file = debugfs_create_file("disp_met", S_IFREG | 0644,
					     d_folder, NULL, &disp_met_fops);
	}
	if (d_folder) {
		d_file = debugfs_create_file("disp_lfr_dbg",
			S_IFREG | 0644,	d_folder, NULL, &disp_lfr_dbg_fops);
		d_file = debugfs_create_file("disp_lfr_params",
			S_IFREG | 0644,	d_folder, NULL, &disp_lfr_params_fops);
	}
	init_log_buffer();

	drm_mmp_init();
#endif

#if IS_ENABLED(CONFIG_PROC_FS)
	mtkfb_procfs = proc_create("mtkfb", S_IFREG | 0440,
				   NULL,
				   &debug_fops);
	if (!mtkfb_procfs) {
		pr_info("[%s %d]failed to create mtkfb in /proc/disp_ddp\n",
			__func__, __LINE__);
		goto out;
	}

	disp_lowpower_proc = proc_mkdir("displowpower", NULL);
	if (!disp_lowpower_proc) {
		pr_info("[%s %d]failed to create dir: /proc/displowpower\n",
			__func__, __LINE__);
		goto out;
	}

	if (!proc_create("idletime", S_IFREG | 0440,
			 disp_lowpower_proc, &idletime_fops)) {
		pr_info("[%s %d]failed to create idletime in /proc/displowpower\n",
			__func__, __LINE__);
		goto out;
	}

	if (!proc_create("idlevfp", S_IFREG | 0440,
		disp_lowpower_proc, &idlevfp_fops)) {
		pr_info("[%s %d]failed to create idlevfp in /proc/displowpower\n",
			__func__, __LINE__);
		goto out;
	}

	mtkfb_debug_procfs = proc_mkdir("mtkfb_debug", NULL);
	if (!mtkfb_debug_procfs) {
		pr_info("[%s %d]failed to create dir: /proc/mtkfb_debug\n",
			__func__, __LINE__);
		goto out;
	}
	if (!proc_create("disp_met", S_IFREG | 0440,
		mtkfb_debug_procfs, &disp_met_fops)) {
		pr_info("[%s %d]failed to create idlevfp in /proc/mtkfb_debug/disp_met\n",
			__func__, __LINE__);
		goto out;
	}

	if (!proc_create("disp_lfr_dbg", S_IFREG | 0440,
		mtkfb_debug_procfs, &disp_lfr_dbg_fops)) {
		pr_info("[%s %d]failed to create idlevfp in /proc/mtkfb_debug/disp_lfr_dbg\n",
			__func__, __LINE__);
		goto out;
	}
	if (!proc_create("disp_lfr_params", S_IFREG | 0444,
		mtkfb_debug_procfs, &disp_lfr_params_fops)) {
		pr_info("[%s %d]failed to create idlevfp in /proc/mtkfb_debug/disp_lfr_params\n",
			__func__, __LINE__);
		goto out;
	}
#endif

#if IS_ENABLED(CONFIG_MTK_HDMI_SUPPORT)
	mtk_dp_debugfs_init();
#endif

out:
	return;
}

void disp_dbg_init(struct drm_device *dev)
{
	drm_dev = dev;
	init_completion(&cwb_cmp);
}

void disp_dbg_deinit(void)
{
	if (debug_buffer)
		vfree(debug_buffer);
#if IS_ENABLED(CONFIG_DEBUG_FS)
	debugfs_remove(mtkfb_dbgfs);
#endif

#if IS_ENABLED(CONFIG_PROC_FS)
	if (mtkfb_procfs) {
		proc_remove(mtkfb_procfs);
		mtkfb_procfs = NULL;
	}
	if (disp_lowpower_proc) {
		proc_remove(disp_lowpower_proc);
		disp_lowpower_proc = NULL;
	}
#endif
#if IS_ENABLED(CONFIG_MTK_HDMI_SUPPORT)
	mtk_dp_debugfs_deinit();
#endif
}

void get_disp_dbg_buffer(unsigned long *addr, unsigned long *size,
	unsigned long *start)
{
	init_log_buffer();
	if (is_buffer_init) {
		*addr = (unsigned long)err_buffer[0];
		*size = (DEBUG_BUFFER_SIZE - 4096);
		*start = 0;
	} else {
		*addr = 0;
		*size = 0;
		*start = 0;
	}
}

int mtk_disp_ioctl_debug_log_switch(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	unsigned int switch_log = *(unsigned int *)data;

	DDPMSG("%d:%s():switch_log=%d\n", __LINE__, __func__, switch_log);
	if (switch_log == MTK_DRM_MOBILE_LOG)
		g_mobile_log = 1;
	else if (switch_log == MTK_DRM_DETAIL_LOG)
		g_detail_log = 1;
	else if (switch_log == MTK_DRM_FENCE_LOG)
		g_fence_log = 1;
	else if (switch_log == MTK_DRM_IRQ_LOG)
		g_irq_log = 1;
	return 0;
}

