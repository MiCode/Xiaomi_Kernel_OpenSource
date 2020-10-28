/*
 * Copyright (C) 2020 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/tracepoint.h>

#define CREATE_TRACE_POINTS
#include <mt-plat/slog.h>

/**
 * Data structures to store tracepoints information
 */
struct tracepoints_table {
	const char *name;
	void *func;
	struct tracepoint *tp;
	bool init;
};

void slog(const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;

	va_start(args, fmt);
	vaf.fmt = fmt;
	vaf.va = &args;
	trace_slog(&vaf);
	va_end(args);
}

static void probe_signal_devliver(void *data, int sig,
	struct siginfo *info, struct k_sigaction *ka)
{
	slog("#$#%s#@#%s#sig:%d", "signal", "devliver", sig);
}

static void __nocfi probe_ccci_event(void *data, char *string, char *sub_string,
	unsigned int sub_type, unsigned int resv)
{
	slog("#$#%s#@#%s#%d:%d", string, sub_string, sub_type, resv);
}

static struct tracepoints_table interests[] = {
	{.name = "signal_deliver", .func = probe_signal_devliver},
	{.name = "ccci_event", .func = probe_ccci_event},
};

#define FOR_EACH_INTEREST(i) \
	for (i = 0; i < sizeof(interests) / \
	     sizeof(struct tracepoints_table); i++)

/**
 * Find the struct tracepoint* associated with a given tracepoint
 * name.
 */
static void lookup_tracepoints(struct tracepoint *tp, void *ignore)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (strcmp(interests[i].name, tp->name) == 0)
			interests[i].tp = tp;
	}
}

static void mtk_slog_deinit(void)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (interests[i].init) {
			tracepoint_probe_unregister(interests[i].tp,
						    interests[i].func, NULL);
		}
	}
}

static void mtk_slog_init_map(void)
{
	int i;

	/* Install the tracepoints */
	for_each_kernel_tracepoint(lookup_tracepoints, NULL);

	FOR_EACH_INTEREST(i) {
		if (interests[i].tp == NULL) {
			pr_info("Error: %s not found\n", interests[i].name);
			/* Unload previously loaded */
			mtk_slog_deinit();
			return;
		}

		tracepoint_probe_register(interests[i].tp, interests[i].func,
					  NULL);
		interests[i].init = true;
	}
}

static int __init mtk_slog_init(void)
{
	mtk_slog_init_map();

	return 0;
}

static void __exit mtk_slog_exit(void)
{
	mtk_slog_deinit();
}

/*
 * TODO: The timing of loadable module is too late to have full
 * list of kernel threads. Need to find out solution.
 */
early_initcall(mtk_slog_init);

MODULE_AUTHOR("MediaTek Inc.");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek System Log");
