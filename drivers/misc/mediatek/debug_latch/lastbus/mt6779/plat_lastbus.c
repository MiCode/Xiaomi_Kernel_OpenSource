// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include "./../lastbus.h"
#include "plat_lastbus.h"
#include <linux/seqlock.h>
#include <linux/irqflags.h>

static DEFINE_SPINLOCK(lastbus_spin_lock);

int infra_set_event(const struct plt_cfg_bus_latch *self, const char *buf)
{
	int i;
	char *arg;
	char *p = (char *)buf;
	unsigned long input;
	unsigned int settings[PLAT_NUM_INFRA_EVENT_REG];

	for (i = 0; i < self->num_infra_event_reg; i++) {
		arg = strsep(&p, " ");
		if (arg == NULL)
			return -EINVAL;
		if (kstrtoul(arg, 16, &input) != 0) {
			pr_info("%s:%d: kstrtoul fail for %s\n",
				 __func__, __LINE__, p);
			return -1;
		}
		settings[i] = (unsigned int)input;
	}

	writel(settings[0],
		self->infra_base + self->infrasys_offsets.bus_infra_mask_l);
	writel(settings[1],
		self->infra_base + self->infrasys_offsets.bus_infra_mask_h);
	return 0;
}

int infra_get_event(const struct plt_cfg_bus_latch *self, char *buf)
{
	int ret;

	ret = snprintf(buf, PAGE_SIZE,
			"INFRA_MASKL = 0x%x, INFRA_MASKH = 0x%x\n",
		readl(self->infra_base +
			self->infrasys_offsets.bus_infra_mask_l),
		readl(self->infra_base +
			self->infrasys_offsets.bus_infra_mask_h));

	return ret;
}


int infra_set_timeout(const struct plt_cfg_bus_latch *self, const char *buf)
{
	int i;
	char *arg;
	char *p = (char *)buf;
	unsigned long input;
	unsigned int settings[4];
	unsigned int infra_timeout;
	unsigned int peri_timeout;


	for (i = 0; i < 4; i++) {
		arg = strsep(&p, " ");
		if (arg == NULL)
			return -EINVAL;
		if (kstrtoul(arg, 16, &input) != 0) {
			pr_info("%s:%d: kstrtoul fail for %s\n",
				 __func__, __LINE__, p);
			return -1;
		}
		settings[i] = (unsigned int)input;
	}

	infra_timeout = readl(self->infra_base +
		self->infrasys_offsets.bus_infra_ctrl) & ~(0xffff);
	infra_timeout = infra_timeout | settings[2];
	writel(infra_timeout, self->infra_base +
		self->infrasys_offsets.bus_infra_ctrl);

	peri_timeout = readl(self->peri_base +
		self->perisys_offsets.bus_peri_r0) & ~0xffff;
	peri_timeout = peri_timeout | settings[3];
	writel(peri_timeout, self->peri_base +
		self->perisys_offsets.bus_peri_r0);

	return 0;
}

int infra_get_timeout(const struct plt_cfg_bus_latch *self, char *buf)
{
	return snprintf(buf, PAGE_SIZE,
		"infra_config=0x%x, peri_config=0x%x\n",
		readl(self->infra_base +
			self->infrasys_offsets.bus_infra_ctrl),
		readl(self->peri_base +
			self->perisys_offsets.bus_peri_r0));
}

int peri_set_event(const struct plt_cfg_bus_latch *self, const char *buf)
{
	char *p = (char *)buf;
	unsigned long input;

	if (kstrtoul(p, 16, &input) != 0) {
		pr_info("%s:%d: kstrtoul fail for %s\n",
			 __func__, __LINE__, p);
		return -1;
	}

	writel(input, self->peri_base +
		self->perisys_offsets.bus_peri_r1);

	return 0;
}

int peri_get_event(const struct plt_cfg_bus_latch *self, char *buf)
{
	int ret;

	ret = snprintf(buf, PAGE_SIZE, "PERI_MASK = 0x%x\n",
		readl(self->peri_base +
			self->perisys_offsets.bus_peri_r1));

	return ret;

}

int infra_dump(const struct plt_cfg_bus_latch *self, char *buf, int *wp)
{
	unsigned int i;
	void __iomem *infra_base = self->infra_base;
	unsigned int ctrl;

	if (self->num_infrasys_mon != 0) {
		ctrl =  readl(infra_base +
			self->infrasys_offsets.bus_infra_ctrl);
		if (ctrl & 0xFF000000) {
			*wp += snprintf(buf + *wp, LATCH_BUF_LENGTH - *wp,
				"[LAST BUS] INFRASYS TIMEOUT:\n");
			*wp += snprintf(buf + *wp, LATCH_BUF_LENGTH - *wp,
				"[LAST BUS] INFRASYS_CTRL = %04x\n", ctrl);
			*wp += snprintf(buf + *wp, LATCH_BUF_LENGTH - *wp,
					"[LAST BUS] INFRASYS_MASKL = %04x\n",
					readl(infra_base +
					self->infrasys_offsets.bus_infra_ctrl));

			*wp += snprintf(buf + *wp, LATCH_BUF_LENGTH - *wp,
				"[LAST BUS] INFRASYS_MASKH = %04x\n",
				readl(infra_base +
				self->infrasys_offsets.bus_infra_ctrl));

			for (i = 0; i <= self->num_infrasys_mon-1; ++i)
				*wp += snprintf(buf + *wp,
				LATCH_BUF_LENGTH - *wp,
				"INFRA SNAPSHOT%d = %04x\n",
				i, readl(infra_base +
				self->infrasys_offsets.bus_infra_snapshot
				+ 4*i));
		} else {
			*wp += snprintf(buf + *wp,
				LATCH_BUF_LENGTH - *wp,
				"[LAST BUS] INFRASYS NO TIMEOUT:\n");
		}
	}


	return 0;

}

static unsigned long preisys_dump_offset[] = {
	0x508, /* PERIBUS_DBG0 */
	0x50c, /* PERIBUS_DBG1 */
	0x510, /* PERIBUS_DBG2 */
	0x514, /* PERIBUS_DBG3 */
	0x518, /* PERIBUS_DBG4 */
	0x51c, /* PERIBUS_DBG5 */
	0x520, /* PERIBUS_DBG6 */
	0x524, /* PERIBUS_DBG7 */
	0x528, /* PERIBUS_DBG8 */
	0x52c, /* PERIBUS_DBG9 */
	0x530, /* PERIBUS_DBG10 */
	0x534, /* PERIBUS_DBG11 */
	0x538, /* PERIBUS_DBG12 */
	0x53c, /* PERIBUS_DBG13 */
	0x580, /* PERIBUS_DBG14 */
	0x584, /* PERIBUS_DBG15 */
	0x588, /* PERIBUS_DBG16 */
	0x58c, /* PERIBUS_DBG17 */
	0x590, /* PERIBUS_DBG18 */
	0x594, /* PERIBUS_DBG19 */
	0x598, /* PERIBUS_DBG20 */
};

int peri_dump(const struct plt_cfg_bus_latch *self, char *buf, int *wp)
{
	unsigned int i;
	void __iomem *peri_base = self->peri_base;
	unsigned long dump_size =
		sizeof(preisys_dump_offset) / sizeof(unsigned long);
	unsigned __iomem *reg;
	unsigned int data;

	*wp += snprintf(buf + *wp, LATCH_BUF_LENGTH - *wp,
			"[LAST BUS] PERISYS BUS DUMP:\n");

	for (i = 0; i < dump_size; ++i) {
		reg = peri_base + preisys_dump_offset[i];
		data = readl(reg);
		*wp += snprintf(buf + *wp, LATCH_BUF_LENGTH - *wp,
				"PERIBUS_DBG%d(%p) = %04x\n",
				i, reg, data);
	}

	return 0;
}

struct plt_cfg_bus_latch cfg_bus_latch = {
	.supported = 1,
	.num_perisys_mon = 21,
	.num_infrasys_mon = 48,
	.num_infra_event_reg = PLAT_NUM_INFRA_EVENT_REG,
	.num_peri_event_reg = PLAT_NUM_PERI_EVENT_REG,
	.perisys_offsets = {
		.bus_peri_r0 = 0x0500,
		.bus_peri_r1 = 0x0504,
		.bus_peri_mon = 0x0508,
	},
	.infrasys_offsets = {
		.bus_infra_ctrl = 0x0d04,
		.bus_infra_mask_l = 0x0d00,
		.bus_infra_mask_h = 0x0df0,
		.bus_infra_snapshot = 0x0d08,
	},
	.perisys_ops = {
		.dump = peri_dump,
		.set_event = peri_set_event,
		.get_event = peri_get_event,
	},
	.infrasys_ops = {
		.dump = infra_dump,
		.set_event = infra_set_event,
		.get_event = infra_get_event,
		.set_timeout = infra_set_timeout,
		.get_timeout = infra_get_timeout,
	},
};

struct plt_cfg_bus_latch *lb = &cfg_bus_latch;

int infra_timeout_dump(void)
{
	unsigned int i;
	void __iomem *infra_base = lb->infra_base;

	pr_info("[LAST BUS] PERISYS BUS & INFRA BUS DUMP:\n");

	/* infrabus_dbg_in_0 */
	pr_info("%08x\n", readl(infra_base +
		lb->infrasys_offsets.bus_infra_mask_l));

	/* infrabus_dbg_mask_2 */
	pr_info("%08x\n", readl(infra_base +
		lb->infrasys_offsets.bus_infra_mask_h));

	/* infrabus_dbg_in_1 */
	pr_info("%08x\n", readl(infra_base +
		lb->infrasys_offsets.bus_infra_ctrl));

	if (lb->num_infrasys_mon != 0) {
		for (i = 0; i <= lb->num_infrasys_mon-1; ++i)
			pr_info("%08x\n",
			readl(infra_base +
			lb->infrasys_offsets.bus_infra_snapshot
			+ 4*i));
	}

	return 0;
}

int peri_timeout_dump(void)
{
	unsigned int i;
	void __iomem *peri_base = lb->peri_base;
	unsigned long dump_size =
		sizeof(preisys_dump_offset) / sizeof(unsigned long);
	unsigned __iomem *reg;
	unsigned int data;

	/* peribus_dbg_in_0 */
	pr_info("%08x\n", readl(peri_base +
		lb->perisys_offsets.bus_peri_r0));

	/* peribus_dbg_in_1 */
	pr_info("%08x\n", readl(peri_base +
		lb->perisys_offsets.bus_peri_r1));

	for (i = 0; i < dump_size; ++i) {
		reg = peri_base + preisys_dump_offset[i];
		data = readl(reg);
		pr_info("%08x\n", data);
	}

	return 0;
}

int is_infra_timeout(void)
{
	int ctrl = 0;

	if (!lb->infra_base) {
		pr_info("%s:%d: not ready\n", __func__, __LINE__);

		return 0;
	}

	ctrl = readl(lb->infra_base +
		lb->infrasys_offsets.bus_infra_ctrl);

	return ctrl & 0x1;
}

int is_peri_timeout(void)
{
	int ctrl = 0;

	if (!lb->peri_base) {
		pr_info("%s:%d: not ready\n", __func__, __LINE__);

		return 0;
	}

	ctrl = readl(lb->peri_base +
		lb->perisys_offsets.bus_peri_r1);

	return ctrl & 0x1;
}

void reset_infra(void)
{
	writel(0x8, lb->infra_base + lb->infrasys_offsets.bus_infra_ctrl);
	writel(0xffff000c,
		lb->infra_base + lb->infrasys_offsets.bus_infra_ctrl);
}

void reset_peri(void)
{
	writel(0x0, lb->peri_base + lb->perisys_offsets.bus_peri_r0);
	writel(0x8, lb->peri_base + lb->perisys_offsets.bus_peri_r1);

	writel(0x3fff, lb->peri_base + lb->perisys_offsets.bus_peri_r0);
	writel(0xc, lb->peri_base + lb->perisys_offsets.bus_peri_r1);
}

int lastbus_timeout_dump(void)
{
	int infra = 0, peri = 0;
	unsigned long flags;

	spin_lock_irqsave(&lastbus_spin_lock, flags);

	if (!lb->peri_base || !lb->infra_base) {
		pr_info("%s:%d: not ready\n", __func__, __LINE__);

		spin_unlock_irqrestore(&lastbus_spin_lock, flags);

		return -1;
	}

	infra = is_infra_timeout();
	peri = is_peri_timeout();

	if (infra | peri) {
		infra_timeout_dump();
		peri_timeout_dump();
		reset_infra();
		reset_peri();
	}

	spin_unlock_irqrestore(&lastbus_spin_lock, flags);

	return infra | peri;
}

static int __init plt_lastbus_init(void)
{
	lastbus_setup(&cfg_bus_latch);
	return 0;
}

late_initcall(plt_lastbus_init);
