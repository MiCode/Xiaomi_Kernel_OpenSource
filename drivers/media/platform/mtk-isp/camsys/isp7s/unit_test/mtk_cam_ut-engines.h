/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CAM_UT_ENGINES_H
#define __MTK_CAM_UT_ENGINES_H

#include <linux/kfifo.h>
#include "mtk_cam_ut-event.h"
#include "mtk_cam_ut.h"

struct engine_ops {
	/* test mdl, optional */
	int (*set_size)(struct device *dev,
			int width, int height,
			int pixmode_lg2,
			int pattern,
			int seninf_idx,
			int tg_idx);

	int (*initialize)(struct device *dev, void *ext_params);
	int (*reset)(struct device *dev);
	int (*s_stream)(struct device *dev, int on);
	int (*apply_cq)(struct device *dev,
		dma_addr_t cq_addr, unsigned int cq_size, unsigned int cq_offset,
		unsigned int sub_cq_size,
		unsigned int sub_cq_offset);
	int (*dev_config)(struct device *dev,
		struct mtkcam_ipi_input_param *cfg_in_param);
};

#define CALL_ENGINE_OPS(dev, ops, ...) \
	((dev && dev->ops) ? dev.ops(dev, ##__VA_ARGS__) : -EINVAL)

enum RAW_STREAMON_TYPE {
	STREAM_FROM_TG,
	STREAM_FROM_RAWI_R2,
	STREAM_FROM_RAWI_R5,
	STREAM_FROM_RAWI_R6,
};

struct mtk_ut_raw_initial_params {
	int subsample;
	int streamon_type;
	int hardware_scenario;
};

struct mtk_ut_raw_device {
	struct device *dev;
	unsigned int id;
	void __iomem *base;
	void __iomem *base_inner;
	void __iomem *yuv_base;
	unsigned int num_clks;
	struct clk **clks;

	struct ut_event_source event_src;
	struct engine_ops ops;
	struct mtk_cam_ut *ut;

	unsigned int	fifo_size;
	struct kfifo	msgfifo;

	int is_subsample;
	int is_initial_cq;
	int cq_done_mask; /* [0]: main, [1]: sub */
	int hardware_scenario;

};

#define CALL_RAW_OPS(dev, op, ...) \
{\
	struct mtk_ut_raw_device *drvdata = dev_get_drvdata(dev);\
	((dev && drvdata->ops.op) ? drvdata->ops.op(dev, ##__VA_ARGS__) : \
	 -EINVAL);\
}

struct ut_yuv_status {
	/* yuv INT 1/2/4/5 */
	u32 irq;
	u32 wdma;
	u32 drop;
	u32 ofl;
};

struct ut_yuv_debug_csr {
	/* DMAO */
	u32 yuvo_r1_addr;
	u32 yuvo_r3_addr;

    /* FBC */
	u32 fbc_yuvo_r1ctl2;
	u32 fbc_yuvo_r3ctl2;
};

struct mtk_ut_yuv_device {
	struct device *dev;
	unsigned int id;
	void __iomem *base;
	unsigned int num_clks;
	struct clk **clks;

	struct engine_ops ops;
};

#define CALL_YUV_OPS(dev, op, ...) \
{\
	struct mtk_ut_yuv_device *drvdata = dev_get_drvdata(dev);\
	((dev && drvdata->ops.op) ? drvdata->ops.op(dev, ##__VA_ARGS__) : \
	 -EINVAL);\
}

struct mtk_ut_camsv_device {
	struct device *dev;
	unsigned int id;
	void __iomem *base;
	void __iomem *base_inner;
	unsigned int num_clks;
	struct clk **clks;
	unsigned int exp_order;
	unsigned int exp_num;
	unsigned int hw_cap;
	unsigned int cammux_id;

	struct ut_event_source event_src;
	struct engine_ops ops;
};

#define CALL_CAMSV_OPS(dev, op, ...) \
{\
	struct mtk_ut_camsv_device *camsv = dev_get_drvdata(dev);\
	((dev && camsv->ops.op) ? camsv->ops.op(dev, ##__VA_ARGS__) : -EINVAL);\
}

struct mtk_ut_seninf_device {
	struct device *dev;
	void __iomem *base;

	unsigned int num_clks;
	struct clk **clks;

	struct engine_ops ops;
};

#define CALL_SENINF_OPS(dev, op, ...) \
{\
	struct mtk_ut_seninf_device *seninf = dev_get_drvdata(dev);\
	((dev && seninf->ops.op) ? seninf->ops.op(dev, ##__VA_ARGS__) : -EINVAL);\
}

extern struct platform_driver mtk_ut_raw_driver;
extern struct platform_driver mtk_ut_yuv_driver;
extern struct platform_driver mtk_ut_camsv_driver;
extern struct platform_driver mtk_ut_seninf_driver;
#define WITH_LARB_DRIVER 1
extern struct platform_driver mtk_ut_larb_driver;

#endif /* __MTK_CAM_UT_ENGINES_H */
