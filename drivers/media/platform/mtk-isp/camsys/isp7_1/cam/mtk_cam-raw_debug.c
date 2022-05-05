// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include <linux/device.h>
#include <linux/io.h>

#include "mtk_cam-regs.h"
#include "mtk_cam-raw_debug.h"
#include "mtk_cam-trace.h"
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <aee.h>
#endif

#define ADD_FBC_DMA(name)	 #name
static const char * const fbc_r1_list[] = {
	ADD_FBC_DMA(IMGO_R1),
	ADD_FBC_DMA(FHO_R1),
	ADD_FBC_DMA(AAHO_R1),
	ADD_FBC_DMA(PDO_R1),
	ADD_FBC_DMA(AAO_R1),
	ADD_FBC_DMA(TSFSO_R1),
	ADD_FBC_DMA(LTMSO_R1),
	ADD_FBC_DMA(AFO_R1),
	ADD_FBC_DMA(FLKO_R1),
	ADD_FBC_DMA(UFEO_R1),
	ADD_FBC_DMA(TSFSO_R2),
};

static const char * const fbc_r2_list[] = {
	ADD_FBC_DMA(YUVO_R1),
	ADD_FBC_DMA(YUVBO_R1),
	ADD_FBC_DMA(YUVCO_R1),
	ADD_FBC_DMA(YUVDO_R1),
	ADD_FBC_DMA(YUVO_R3),
	ADD_FBC_DMA(YUVBO_R3),
	ADD_FBC_DMA(YUVCO_R3),
	ADD_FBC_DMA(YUVDO_R3),
	ADD_FBC_DMA(YUVO_R2),
	ADD_FBC_DMA(YUVBO_R2),
	ADD_FBC_DMA(YUVO_R4),
	ADD_FBC_DMA(YUVBO_R4),
	ADD_FBC_DMA(RZH1N2TO_R1),
	ADD_FBC_DMA(RZH1N2TBO_R1),
	ADD_FBC_DMA(RZH1N2TO_R2),
	ADD_FBC_DMA(RZH1N2TO_R3),
	ADD_FBC_DMA(RZH1N2TBO_R3),
	ADD_FBC_DMA(DRZS4NO_R1),
	ADD_FBC_DMA(DRZS4NO_R2),
	ADD_FBC_DMA(DRZS4NO_R3),
	ADD_FBC_DMA(TNCSO_R1),
	ADD_FBC_DMA(TNCSYO_R1),
	ADD_FBC_DMA(TNCSBO_R1),
	ADD_FBC_DMA(TNCSHO_R1),
	ADD_FBC_DMA(ACTSO_R1),
	ADD_FBC_DMA(YUVO_R5),
	ADD_FBC_DMA(YUVBO_R5),
};

#define LOGGER_PREFIX_SIZE 16
#define LOGGER_BUFSIZE 128
struct buffered_logger {
	struct device *dev;
	void (*log_handler)(struct buffered_logger *log);

	char prefix[LOGGER_PREFIX_SIZE];
	char buf[LOGGER_BUFSIZE + 1];
	int size;
};

#define _INIT_LOGGER(logger, _dev, _hdl)	\
({						\
	(logger)->dev = _dev;			\
	(logger)->log_handler = _hdl;		\
	(logger)->prefix[0] = '\0';		\
	(logger)->buf[LOGGER_BUFSIZE] = '\0';	\
	(logger)->size = 0;			\
})

#define INIT_LOGGER_ALWAYS(logger, dev)		\
	_INIT_LOGGER(logger, dev, mtk_cam_log_handle_info)
#define INIT_LOGGER_LIMITED(logger, dev)	\
	_INIT_LOGGER(logger, dev, mtk_cam_log_handle_limited)

#define INIT_LOGGER_FTRACE_FBC(logger, dev)		\
	_INIT_LOGGER(logger, dev, mtk_cam_log_handle_trace_fbc)
#define INIT_LOGGER_FTRACE_HW_IRQ(logger, dev)		\
	_INIT_LOGGER(logger, dev, mtk_cam_log_handle_trace_hw_irq)

static __printf(2, 3)
void mtk_cam_log_set_prefix(struct buffered_logger *log, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vscnprintf(log->prefix, LOGGER_PREFIX_SIZE, fmt, args);
	va_end(args);
}

static void mtk_cam_log_handle_limited(struct buffered_logger *log)
{
	dev_info_ratelimited(log->dev, "%s: %.*s\n",
			     log->prefix, log->size, log->buf);
	log->size = 0;
}

static void mtk_cam_log_handle_info(struct buffered_logger *log)
{
	dev_info(log->dev, "%s: %.*s\n",
		 log->prefix, log->size, log->buf);
	log->size = 0;
}

static void mtk_cam_log_handle_trace_fbc(struct buffered_logger *log)
{
	MTK_CAM_TRACE(FBC, "%s: %s: %.*s",
		 dev_name(log->dev), log->prefix, log->size, log->buf);
	log->size = 0;
}

static void mtk_cam_log_handle_trace_hw_irq(struct buffered_logger *log)
{
	MTK_CAM_TRACE(HW_IRQ, "%s: %s: %.*s",
		 dev_name(log->dev), log->prefix, log->size, log->buf);
	log->size = 0;
}

static void mtk_cam_log_flush(struct buffered_logger *log)
{
	log->log_handler(log);
}

static __printf(2, 3)
void mtk_cam_log_push(struct buffered_logger *log, const char *fmt, ...)
{
	va_list args;
	int len;

	va_start(args, fmt);
	len = vscnprintf(log->buf + log->size, LOGGER_BUFSIZE - log->size + 1,
			 fmt, args);
	va_end(args);

	if (len + log->size < LOGGER_BUFSIZE) {
		log->size += len;
		return;
	}

	mtk_cam_log_flush(log);

	va_start(args, fmt);
	len = vscnprintf(log->buf + log->size, LOGGER_BUFSIZE - log->size + 1,
			 fmt, args);
	va_end(args);

	log->size += len;

	if (len == LOGGER_BUFSIZE)
		dev_info(log->dev, "log buffer size not enough: %d\n",
			 LOGGER_BUFSIZE);
}

void debug_dma_fbc(struct device *dev,
			  void __iomem *base, void __iomem *yuvbase)
{
	u32 fbc_r1_ctl[ARRAY_SIZE(fbc_r1_list)];
	u32 fbc_r2_ctl[ARRAY_SIZE(fbc_r2_list)];
	u32 fbc_r1_ctl2[ARRAY_SIZE(fbc_r1_list)];
	u32 fbc_r2_ctl2[ARRAY_SIZE(fbc_r2_list)];
	u32 i;

	for (i = 0; i < ARRAY_SIZE(fbc_r1_list); i++) {
		fbc_r1_ctl[i] = readl_relaxed(base + REG_FBC_CTL1(FBC_R1A_BASE, i));
		fbc_r1_ctl2[i] = readl_relaxed(base + REG_FBC_CTL2(FBC_R1A_BASE, i));
		dev_info(dev, "%s:RAW FBC:%s: ctl1:0x%08x, ctl2:0x%08x\n", __func__,
			fbc_r1_list[i], fbc_r1_ctl[i], fbc_r1_ctl2[i]);
	}
	for (i = 0; i < ARRAY_SIZE(fbc_r2_list); i++) {
		fbc_r2_ctl[i] = readl_relaxed(yuvbase + REG_FBC_CTL1(FBC_R2A_BASE, i));
		fbc_r2_ctl2[i] = readl_relaxed(yuvbase + REG_FBC_CTL2(FBC_R2A_BASE, i));
		dev_info(dev, "%s:YUV FBC:%s: ctl1:0x%08x, ctl2:0x%08x\n", __func__,
			fbc_r2_list[i], fbc_r2_ctl[i], fbc_r2_ctl2[i]);
	}
}

void mtk_cam_raw_dump_fbc(struct device *dev,
			  void __iomem *base, void __iomem *yuvbase)
{
	struct buffered_logger log;
	int fbc_r1_ctl2[ARRAY_SIZE(fbc_r1_list)];
	int fbc_r2_ctl2[ARRAY_SIZE(fbc_r2_list)];
	int i;

	for (i = 0; i < ARRAY_SIZE(fbc_r1_list); i++)
		fbc_r1_ctl2[i] = readl(base + REG_FBC_CTL2(FBC_R1A_BASE, i));

	for (i = 0; i < ARRAY_SIZE(fbc_r2_list); i++)
		fbc_r2_ctl2[i] = readl(yuvbase + REG_FBC_CTL2(FBC_R2A_BASE, i));

	if (MTK_CAM_TRACE_ENABLED(FBC))
		INIT_LOGGER_FTRACE_FBC(&log, dev);
	else
		INIT_LOGGER_ALWAYS(&log, dev);

	mtk_cam_log_set_prefix(&log, "%s", "RAW FBC");
	for (i = 0; i < ARRAY_SIZE(fbc_r1_list); i++)
		if (fbc_r1_ctl2[i] & 0xffffff) /* if has been used */
			mtk_cam_log_push(&log, " %s: 0x%08x",
					 fbc_r1_list[i], fbc_r1_ctl2[i]);
	mtk_cam_log_flush(&log);

	mtk_cam_log_set_prefix(&log, "%s", "YUV FBC");
	for (i = 0; i < ARRAY_SIZE(fbc_r2_list); i++)
		if (fbc_r2_ctl2[i] & 0xffffff) /* if has been used */
			mtk_cam_log_push(&log, " %s: 0x%08x",
					 fbc_r2_list[i], fbc_r2_ctl2[i]);
	mtk_cam_log_flush(&log);
}

struct reg_to_dump {
	const char *name;
	unsigned int reg;
};

#define ADD_DMA(name)	{ #name, REG_ ## name ## _BASE + DMA_OFFSET_ERR_STAT }
static const struct reg_to_dump raw_dma_list[] = {
	ADD_DMA(IMGO_R1),
	ADD_DMA(UFEO_R1),
	ADD_DMA(PDO_R1),
	ADD_DMA(FLKO_R1),
	ADD_DMA(TSFSO_R1),
	ADD_DMA(TSFSO_R2),
	ADD_DMA(AAO_R1),
	ADD_DMA(AAHO_R1),
	ADD_DMA(AFO_R1),
	ADD_DMA(LTMSO_R1),
	ADD_DMA(FHO_R1),
	/* ADD_DMA(BPCO_R1), */

	ADD_DMA(RAWI_R2),
	ADD_DMA(UFDI_R2),
	ADD_DMA(BPCI_R1),
	ADD_DMA(LSCI_R1),
	ADD_DMA(AAI_R1),
	ADD_DMA(PDI_R1),
	ADD_DMA(BPCI_R2),
	ADD_DMA(RAWI_R3),
	ADD_DMA(UFDI_R3),
	ADD_DMA(BPCI_R3),
	/* ADD_DMA(RAWI_R4), */
	/* ADD_DMA(BPCI_R4), */
	ADD_DMA(RAWI_R5),
	ADD_DMA(RAWI_R6),
	ADD_DMA(CACI_R1),
};

static const struct reg_to_dump yuv_dma_list[] = {
	ADD_DMA(ACTSO_R1),
	//ADD_DMA(TNCSO_R1), /* not supported in 7.1 */
	//ADD_DMA(TNCSBO_R1), /* not supported in 7.1 */
	//ADD_DMA(TNCSHO_R1), /* not supported in 7.1 */
	ADD_DMA(TNCSYO_R1),
	ADD_DMA(DRZS4NO_R1),
	ADD_DMA(DRZS4NO_R2),
	ADD_DMA(DRZS4NO_R3),
	ADD_DMA(RZH1N2TO_R1),
	ADD_DMA(RZH1N2TBO_R1),
	ADD_DMA(RZH1N2TO_R2),
	ADD_DMA(RZH1N2TO_R3),
	ADD_DMA(RZH1N2TBO_R3),
	ADD_DMA(YUVO_R1),
	ADD_DMA(YUVBO_R1),
	ADD_DMA(YUVCO_R1),
	ADD_DMA(YUVDO_R1),
	ADD_DMA(YUVO_R2),
	ADD_DMA(YUVBO_R2),
	ADD_DMA(YUVO_R3),
	ADD_DMA(YUVBO_R3),
	ADD_DMA(YUVCO_R3),
	ADD_DMA(YUVDO_R3),
	ADD_DMA(YUVO_R4),
	ADD_DMA(YUVBO_R4),
	ADD_DMA(YUVO_R5),
	ADD_DMA(YUVBO_R5),
};

static void mtk_cam_dump_dma_err_st(struct device *dev, void __iomem *base,
				    const char *prefix,
				    const struct reg_to_dump *from,
				    const struct reg_to_dump *to)
{
	struct buffered_logger log;
	int err_found = 0;
	int err_st;

	if (MTK_CAM_TRACE_ENABLED(HW_IRQ))
		INIT_LOGGER_FTRACE_HW_IRQ(&log, dev);
	else
		INIT_LOGGER_LIMITED(&log, dev);

	mtk_cam_log_set_prefix(&log, "%s", prefix);
	while (from < to) {
		err_st = readl_relaxed(base + from->reg);

		/* [15:0] ERR_STAT */
		if (err_st & 0xffff) {
			mtk_cam_log_push(&log, " %s: 0x%08x",
					 from->name, err_st);
			err_found = 1;
		}
		from++;
	}

	if (err_found)
		mtk_cam_log_flush(&log);
}

void mtk_cam_raw_dump_dma_err_st(struct device *dev, void __iomem *base)
{
	mtk_cam_dump_dma_err_st(dev, base, "RAW DMA_ERR",
				raw_dma_list,
				raw_dma_list + ARRAY_SIZE(raw_dma_list));
}

void mtk_cam_yuv_dump_dma_err_st(struct device *dev, void __iomem *base)
{
	mtk_cam_dump_dma_err_st(dev, base, "YUV DMA_ERR",
				yuv_dma_list,
				yuv_dma_list + ARRAY_SIZE(yuv_dma_list));
}

void mtk_cam_dump_req_rdy_status(struct device *dev,
				 void __iomem *base, void __iomem *yuvbase)
{
	dev_dbg_ratelimited(dev,
			    "REQ RAW/2/3 DMA/2:%08x/%08x/%08x/%08x/%08x\n",
			    readl_relaxed(base + REG_CTL_RAW_MOD_REQ_STAT),
			    readl_relaxed(base + REG_CTL_RAW_MOD2_REQ_STAT),
			    readl_relaxed(base + REG_CTL_RAW_MOD3_REQ_STAT),
			    readl_relaxed(base + REG_CTL_RAW_MOD5_REQ_STAT),
			    readl_relaxed(base + REG_CTL_RAW_MOD6_REQ_STAT));
	dev_dbg_ratelimited(dev,
			    "RDY RAW/2/3 DMA/2:%08x/%08x/%08x/%08x/%08x\n",
			    readl_relaxed(base + REG_CTL_RAW_MOD_RDY_STAT),
			    readl_relaxed(base + REG_CTL_RAW_MOD2_RDY_STAT),
			    readl_relaxed(base + REG_CTL_RAW_MOD3_RDY_STAT),
			    readl_relaxed(base + REG_CTL_RAW_MOD5_RDY_STAT),
			    readl_relaxed(base + REG_CTL_RAW_MOD6_RDY_STAT));
	dev_dbg_ratelimited(dev,
			    "REQ YUV/2/3/4 WDMA:%08x/%08x/%08x/%08x/%08x\n",
			    readl_relaxed(yuvbase + REG_CTL_RAW_MOD_REQ_STAT),
			    readl_relaxed(yuvbase + REG_CTL_RAW_MOD2_REQ_STAT),
			    readl_relaxed(yuvbase + REG_CTL_RAW_MOD3_REQ_STAT),
			    readl_relaxed(yuvbase + REG_CTL_RAW_MOD4_REQ_STAT),
			    readl_relaxed(yuvbase + REG_CTL_RAW_MOD5_REQ_STAT));
	dev_dbg_ratelimited(dev,
			    "RDY YUV/2/3/4 WDMA:%08x/%08x/%08x/%08x/%08x\n",
			    readl_relaxed(yuvbase + REG_CTL_RAW_MOD_RDY_STAT),
			    readl_relaxed(yuvbase + REG_CTL_RAW_MOD2_RDY_STAT),
			    readl_relaxed(yuvbase + REG_CTL_RAW_MOD3_RDY_STAT),
			    readl_relaxed(yuvbase + REG_CTL_RAW_MOD4_RDY_STAT),
			    readl_relaxed(yuvbase + REG_CTL_RAW_MOD5_RDY_STAT));
}

void mtk_cam_dump_dma_debug(struct device *dev,
			    void __iomem *dmatop_base,
			    const char *dma_name,
			    struct dma_debug_item *items, int n)
{
#define MAX_DEBUG_SIZE (32)

	void __iomem *dbg_sel = dmatop_base + 0x70;
	void __iomem *dbg_port = dmatop_base + 0x74;
	int i = 0;
	unsigned int vals[MAX_DEBUG_SIZE];

	if (n >= MAX_DEBUG_SIZE) {
		dev_info(dev, "%s: should enlarge array size for n(%d)\n",
			__func__, n);
		return;
	}

	for (i = 0; i < n; i++) {
		writel(items[i].debug_sel, dbg_sel);
		if (readl(dbg_sel) != items[i].debug_sel)
			dev_info(dev, "failed to write dbg_sel %08x\n",
				items[i].debug_sel);

		vals[i] = readl(dbg_port);
	};


	if (MTK_CAM_TRACE_ENABLED(HW_IRQ)) {
		MTK_CAM_TRACE(HW_IRQ, "%s: %s", dev_name(dev), dma_name);
		for (i = 0; i < n; i++)
			MTK_CAM_TRACE(HW_IRQ, "%s: %08x: %08x [%s]",
				dev_name(dev),
				items[i].debug_sel, vals[i], items[i].msg);
	} else {
		dev_info(dev, "%s: %s\n", __func__, dma_name);
		for (i = 0; i < n; i++)
			dev_info(dev, "%08x: %08x [%s]\n",
				 items[i].debug_sel, vals[i], items[i].msg);
	}
}

void mtk_cam_sw_reset_check(struct device *dev,
			    void __iomem *dmatop_base,
			    struct dma_debug_item *items, int n)
{
#define MAX_DEBUG_SIZE (32)

	void __iomem *dbg_sel = dmatop_base + 0x70;
	void __iomem *dbg_port = dmatop_base + 0x74;
	int i = 0;
	unsigned int vals[MAX_DEBUG_SIZE];
	bool bPrint = false;

	if (n >= MAX_DEBUG_SIZE) {
		dev_info(dev, "%s: should enlarge array size for n(%d)\n",
			__func__, n);
		return;
	}

	for (i = 0; i < n; i++) {
		writel(items[i].debug_sel, dbg_sel);
		if (readl(dbg_sel) != items[i].debug_sel)
			dev_info(dev, "failed to write dbg_sel %08x\n",
				items[i].debug_sel);

		vals[i] = readl(dbg_port);
		if ((vals[i] >> 16) != (vals[i] & 0xffff))
			bPrint = true;
	};

	if (bPrint) {
		dev_info(dev, "%s:, n = %d", __func__, n);
		for (i = 0; i < n; i++)
			dev_info(dev, "%08x: %08x [%s]\n",
				items[i].debug_sel, vals[i], items[i].msg);

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
		aee_kernel_exception_api(
				__FILE__, __LINE__, DB_OPT_DEFAULT,
				"Camsys: SW reset fail", "SW reset fail");
#endif
	}
}

void mtk_cam_set_topdebug_rdyreq(struct device *dev,
				 void __iomem *base, void __iomem *yuvbase,
				 u32 event)
{
	u32 val = event << 16 | 0xa << 12;

	writel(val, base + REG_CTL_DBG_SET);
	writel(event, base + REG_CTL_DBG_SET2);
	writel(val, yuvbase + REG_CTL_DBG_SET);
	dev_info(dev, "set CAMCTL_DBG_SET2/CAMCTL_DBG_SET (RAW/YUV) 0x%08x/0x%08x\n",
		event, val);
}

void mtk_cam_dump_topdebug_rdyreq(struct device *dev,
				  void __iomem *base, void __iomem *yuvbase)
{
	static const u32 debug_sel[] = {
		/* req group 1~7 */
		0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6,
		/* rdy group 1~7 */
		0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE,
		/* latched_events */
		0xF,
	};
	void __iomem *dbg_set;
	void __iomem *dbg_port;
	u32 set;
	int i;

	/* CAMCTL_DBG_SET
	 *   CAMCTL_SNAPSHOT_SEL  [23:16]
	 *   CAMCTL_DEBUG_TOP_SEL [15:12]
	 *   CAMCTL_DEBUG_SEL     [11: 8]
	 *   CAMCTL_DEBUG_MOD_SEL [ 7: 0]
	 */

	/* CAMCTL_DEBUG_MOD_SEL
	 *   0: tg_scq_cnt_sub_pass1_done
	 *   1: rdyreq_dbg_data
	 */

	dbg_set = base + REG_CTL_DBG_SET;
	dbg_port = base + REG_CTL_DBG_PORT;

	set = (readl(dbg_set) & 0xfff000) | 0x1;
	for (i = 0; i < ARRAY_SIZE(debug_sel); i++) {
		writel(set | debug_sel[i] << 8, dbg_set);
		dev_info(dev, "RAW debug_set 0x%08x port 0x%08x\n",
			 readl(dbg_set), readl(dbg_port));
	}

	dbg_set = yuvbase + REG_CTL_DBG_SET;
	dbg_port = yuvbase + REG_CTL_DBG_PORT;

	set = (readl(dbg_set) & 0xfff000) | 0x1;
	for (i = 0; i < ARRAY_SIZE(debug_sel); i++) {
		writel(set | debug_sel[i] << 8, dbg_set);
		dev_info(dev, "YUV debug_set 0x%08x port 0x%08x\n",
			 readl(dbg_set), readl(dbg_port));
	}
}

