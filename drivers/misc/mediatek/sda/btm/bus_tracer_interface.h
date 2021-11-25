/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __BUS_TRACER_H__
#define __BUS_TRACER_H__

#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/compiler.h>

struct bus_tracer_plt;

struct watchpoint_filter {
	unsigned int addr_h;
	unsigned int addr;
	unsigned int mask;
	unsigned char enabled;
};

struct bypass_filter {
	unsigned int addr;
	unsigned int mask;
	unsigned char enabled;
};

struct id_filter {
	unsigned int id;
	unsigned char enabled;
};

struct rw_filter {
	unsigned char read;
	unsigned char write;
};

struct bus_tracer_filter {
	struct watchpoint_filter watchpoint;
	struct bypass_filter bypass;
	struct id_filter *idf;
	struct rw_filter rwf;
};

struct bus_tracer_plt_operations {
	/* if platform needs special settings before */
	int (*start)(struct bus_tracer_plt *plt);
	/* dump for tracer */
	int (*dump)(struct bus_tracer_plt *plt, char *buf, int len);
	/* enable the bus_tracer functionality */
	int (*enable)(struct bus_tracer_plt *plt,
		unsigned char force_enable, unsigned int tracer_id);
	/* disable the bus_tracer functionality */
	int (*disable)(struct bus_tracer_plt *plt);
	/* pause/resume recording */
	int (*set_recording)(struct bus_tracer_plt *plt, unsigned char pause);
	/* setup watchpoint filter */
	int (*set_watchpoint_filter)(struct bus_tracer_plt *plt,
		struct watchpoint_filter f, unsigned int tracer_id);
	/* setup bypass filter */
	int (*set_bypass_filter)(struct bus_tracer_plt *plt,
		struct bypass_filter f, unsigned int tracer_id);
	/* setup id filter */
	int (*set_id_filter)(struct bus_tracer_plt *plt,
		struct id_filter f, unsigned int tracer_id,
		unsigned int idf_id);
	/* setup rw filter */
	int (*set_rw_filter)(struct bus_tracer_plt *plt,
		struct rw_filter f, unsigned int tracer_id);
	/* dump current setting of tracers */
	int (*dump_setting)(struct bus_tracer_plt *plt, char *buf, int len);
	/* if you want to do anything more than bus_tracer_probe() */
	int (*probe)(struct bus_tracer_plt *plt, struct platform_device *pdev);
	/* if you want to do anything more than bus_tracer_remove() */
	int (*remove)(struct bus_tracer_plt *plt, struct platform_device *pdev);
	/* if you want to do anything more than bus_tracer_suspend() */
	int (*suspend)(struct bus_tracer_plt *plt, struct platform_device *pdev,
		pm_message_t state);
	/* if you want to do anything more than bus_tracer_resume() */
	int (*resume)(struct bus_tracer_plt *plt, struct platform_device *pdev);
};

struct tracer {
	void __iomem *base;
	unsigned char enabled;
	unsigned char recording;
	unsigned char at_id;
	struct bus_tracer_filter filter;
};

struct bus_tracer_plt {
	unsigned int min_buf_len;
	unsigned int num_tracer;
	unsigned int err_flag;
	struct bus_tracer_plt_operations *ops;
	struct tracer *tracer;
	void __iomem *dem_base;
	void __iomem *dbgao_base;
	void __iomem *funnel_base;
	void __iomem *etb_base;
	void __iomem *dfdsoc_base;
	void __iomem *lastbus_base;
};

struct bus_tracer {
	struct platform_driver plt_drv;
	struct bus_tracer_plt *cur_plt;
};

/* for platform register their specific bus_tracer behaviors
 * (chip or various versions of bus_tracer)
 */
int bus_tracer_register(struct bus_tracer_plt *plt);

#endif /* end of __BUS_TRACER_H__ */
