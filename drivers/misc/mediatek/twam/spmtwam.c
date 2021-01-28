// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#include "spmtwam.h"


#define SPMTWAM_COMPATIBLE_STRING	"mediatek,spmtwam"
static DEFINE_SPINLOCK(__spmtwam_lock);
static bool g_spmtwam_init;

struct spmtwam_reg_pair {
	char            *name;
	void __iomem    *addr;
};

enum {
	SPM_TWAM_CON = 0,
	SPM_TWAM_WINDOW_LEN,
	SPM_TWAM_IDLE_SEL,
	SPM_IRQ_MASK,
	SPM_IRQ_STA,
	SPM_TWAM_LAST_STA0,
	SPM_TWAM_LAST_STA1,
	SPM_TWAM_LAST_STA2,
	SPM_TWAM_LAST_STA3,
	SPM_TWAM_MAXNUM,
};

static struct spmtwam_reg_pair reg[SPM_TWAM_MAXNUM] = {
	{"spm_twam_con", NULL},
	{"spm_twam_window_len", NULL},
	{"spm_twam_idle_sel", NULL},
	{"spm_irq_mask", NULL},
	{"spm_irq_sta", NULL},
	{"spm_twam_last_sta0", NULL},
	{"spm_twam_last_sta1", NULL},
	{"spm_twam_last_sta2", NULL},
	{"spm_twam_last_sta3", NULL},
};

#define REG(name)                   (reg[name].addr)

/* SPM_TWAM_CON */
#define REG_TWAM_ENABLE_LSB         (1U << 0)   /* 1b */
#define REG_TWAM_SPEED_MODE_EN_LSB  (1U << 1)   /* 1b */
/* SPM_IRQ_STA */
#define TWAM_IRQ_LSB                (1U << 2)   /* 1b */
/* SPM_IRQ_MASK */
#define ISRM_TWAM                   (1U << 2)
#define ISRM_PCM_RETURN             (1U << 3)
#define ISRM_RET_IRQ0               (1U << 8)
#define ISRM_RET_IRQ1               (1U << 9)
#define ISRM_RET_IRQ2               (1U << 10)
#define ISRM_RET_IRQ3               (1U << 11)
#define ISRM_RET_IRQ4               (1U << 12)
#define ISRM_RET_IRQ5               (1U << 13)
#define ISRM_RET_IRQ6               (1U << 14)
#define ISRM_RET_IRQ7               (1U << 15)
#define ISRM_RET_IRQ8               (1U << 16)
#define ISRM_RET_IRQ9               (1U << 17)
#define ISRM_RET_IRQ10              (1U << 18)
#define ISRM_RET_IRQ11              (1U << 19)
#define ISRM_RET_IRQ12              (1U << 20)
#define ISRM_RET_IRQ13              (1U << 21)
#define ISRM_RET_IRQ14              (1U << 22)
#define ISRM_RET_IRQ15              (1U << 23)

#define ISRM_RET_IRQ_AUX    (\
	ISRM_RET_IRQ0 | ISRM_RET_IRQ1 | ISRM_RET_IRQ2 | ISRM_RET_IRQ3 | \
	ISRM_RET_IRQ4 | ISRM_RET_IRQ5 | ISRM_RET_IRQ6 | ISRM_RET_IRQ7 | \
	ISRM_RET_IRQ8 | ISRM_RET_IRQ9 | ISRM_RET_IRQ10 | ISRM_RET_IRQ11 | \
	ISRM_RET_IRQ12 | ISRM_RET_IRQ13 | ISRM_RET_IRQ14 | ISRM_RET_IRQ15)

#define ISRM_ALL_EXC_TWAM           (ISRM_RET_IRQ_AUX | ISRM_PCM_RETURN)
#define ISRM_ALL                    (ISRM_ALL_EXC_TWAM | ISRM_TWAM)
#define ISRS_TWAM                   (1U << 2)
#define ISRC_TWAM                   (ISRS_TWAM)

#define write32(addr, value)        writel(value, addr)
#define read32(addr)                readl(addr)

#define sig(x)                      (cfg->ch[x].signal)
#define id(x)                       (cfg->ch[x].id)
#define montype(x)                  (cfg->ch[x].montype)

static spmtwam_handler_t spmtwam_handler;
int spmtwam_monitor(bool enable, struct spmtwam_cfg *cfg,
	spmtwam_handler_t handler)
{
	unsigned long flags;

	if (g_spmtwam_init == false) {
		pr_info("spmtwam: no such device\n");
		return -ENODEV;
	}

	if (enable) {
		if (cfg == NULL || handler == NULL) {
			pr_info("spmtwam: null parameter(s)\n");
			return -EINVAL;
		}

		if (spmtwam_handler != NULL) {
			pr_info("spmtwam: already enable ?\n");
			return -EAGAIN;
		}

		/* Set default value for high/normal speed mode */
		if (cfg->spmtwam_window_len == 0)
			cfg->spmtwam_window_len = cfg->spmtwam_speed_mode ?
				WINDOW_LEN_SPEED : WINDOW_LEN_NORMAL;

		spin_lock_irqsave(&__spmtwam_lock, flags);
		spmtwam_handler = handler;

		write32(REG(SPM_IRQ_MASK),
			read32(REG(SPM_IRQ_MASK)) & ~ISRM_TWAM);

		/* SPM_TWAM_IDLE_SEL
		 * [6:0]   signal select 0 (sig 2 bits, id 5 bits)
		 * [14:8]  signal select 1
		 * [22:16] signal select 2
		 * [30:24] signal select 3
		 */
		write32(REG(SPM_TWAM_IDLE_SEL),
			((((sig(0) & 0x3) << 5) | (id(0) & 0x1f)) << 0) |
			((((sig(1) & 0x3) << 5) | (id(1) & 0x1f)) << 8) |
			((((sig(2) & 0x3) << 5) | (id(2) & 0x1f)) << 16) |
			((((sig(3) & 0x3) << 5) | (id(3) & 0x1f)) << 24));

		/* SPM_TWAM_CON
		 * [0]     twam enable - 0 disable, 1 enable
		 * [1]     twam speed mode - 0 32k, 1 high speed
		 * [5:4]   monitor type 0
		 * [7:6]   monitor type 1
		 * [9:8]   monitor type 2
		 * [11:10] monitor type 3
		 *         (0 rising, 1 falling, 2 high level, 3 low level)
		 */
		write32(REG(SPM_TWAM_CON),
			REG_TWAM_ENABLE_LSB |
			(cfg->spmtwam_speed_mode ?
			REG_TWAM_SPEED_MODE_EN_LSB : 0) |
			((montype(0) & 0x3) << 4) |
			((montype(1) & 0x3) << 6) |
			((montype(2) & 0x3) << 8) |
			((montype(3) & 0x3) << 10));

		/* SPM_TWAM_WINDOW_LEN */
		write32(REG(SPM_TWAM_WINDOW_LEN), cfg->spmtwam_window_len);

		spin_unlock_irqrestore(&__spmtwam_lock, flags);

		pr_debug("spmtwam: enable TWAM %u/%u, %u/%u, %u/%u, %u/%u (%s)\n",
			sig(0), id(0), sig(1), id(1),
			sig(2), id(2), sig(3), id(3),
			cfg->spmtwam_speed_mode ? "32k" : "high speed");
	} else {
		spin_lock_irqsave(&__spmtwam_lock, flags);
		spmtwam_handler = NULL;

		write32(REG(SPM_TWAM_CON),
			read32(REG(SPM_TWAM_CON)) & ~REG_TWAM_ENABLE_LSB);
		write32(REG(SPM_IRQ_MASK),
			read32(REG(SPM_IRQ_MASK)) | ISRM_TWAM);
		write32(REG(SPM_IRQ_STA), ISRC_TWAM);

		spin_unlock_irqrestore(&__spmtwam_lock, flags);

		pr_debug("spmtwam: disable TWAM\n");
	}

	return 0;
}
EXPORT_SYMBOL(spmtwam_monitor);

static irqreturn_t spm_irq0_handler(int irq, void *dev_id)
{
	u32 isr = 0;
	unsigned long flags;
	struct spmtwam_result r;
	struct spmtwam_cfg *cfg = &r.cfg;
	u32 twam_idle_sel = 0;
	u32 twam_con = 0;

	spin_lock_irqsave(&__spmtwam_lock, flags);
	/* get ISR status */
	isr = read32(REG(SPM_IRQ_STA));
	if (isr & ISRS_TWAM) {
		/* return result */
		r.value[0] = read32(REG(SPM_TWAM_LAST_STA0));
		r.value[1] = read32(REG(SPM_TWAM_LAST_STA1));
		r.value[2] = read32(REG(SPM_TWAM_LAST_STA2));
		r.value[3] = read32(REG(SPM_TWAM_LAST_STA3));
		/* return current configs */
		twam_idle_sel = read32(REG(SPM_TWAM_IDLE_SEL));
		cfg->ch[0].signal = ((twam_idle_sel & 0x00000060) >> 5);
		cfg->ch[1].signal = ((twam_idle_sel & 0x00006000) >> 13);
		cfg->ch[2].signal = ((twam_idle_sel & 0x00600000) >> 21);
		cfg->ch[3].signal = ((twam_idle_sel & 0x60000000) >> 29);
		cfg->ch[0].id = (twam_idle_sel & 0x0000001F);
		cfg->ch[1].id = ((twam_idle_sel & 0x00001F00) >> 8);
		cfg->ch[2].id = ((twam_idle_sel & 0x001F0000) >> 16);
		cfg->ch[3].id = ((twam_idle_sel & 0x1F000000) >> 24);
		twam_con = read32(REG(SPM_TWAM_CON));
		cfg->ch[0].montype = ((twam_con & 0x30) >> 4);
		cfg->ch[1].montype = ((twam_con & 0xc0) >> 6);
		cfg->ch[2].montype = ((twam_con & 0x300) >> 8);
		cfg->ch[3].montype = ((twam_con & 0xc00) >> 10);
		cfg->spmtwam_speed_mode =
			(twam_con & REG_TWAM_SPEED_MODE_EN_LSB) ? true : false;
		cfg->spmtwam_window_len = read32(REG(SPM_TWAM_WINDOW_LEN));
		udelay(40); /* delay 1T @ 32K */
	}
	/* clean ISR status */
	write32(REG(SPM_IRQ_MASK),
		read32(REG(SPM_IRQ_MASK)) | ISRM_ALL_EXC_TWAM);
	write32(REG(SPM_IRQ_STA), isr);

	spin_unlock_irqrestore(&__spmtwam_lock, flags);

	if ((isr & ISRS_TWAM) && spmtwam_handler)
		spmtwam_handler(&r);

	return IRQ_HANDLED;
}

static int spmtwam_probe(struct platform_device *pdev)
{
	int i, ret = 0;
	struct device_node *node;
	void __iomem *base;
	unsigned int irq0;
	unsigned int offset;

	node = of_find_compatible_node(NULL, NULL, SPMTWAM_COMPATIBLE_STRING);
	if (!node) {
		pr_info("failed to get spmtwam node\n");
		return -ENOENT;
	}

	base = of_iomap(node, 0);
	if (!base) {
		pr_info("failed to get spmtwam base\n");
		return -ENOENT;
	}

	irq0 = irq_of_parse_and_map(node, 0);
	if (!irq0) {
		pr_info("failed to get spmtwam irq0\n");
		return -ENOENT;
	}

	for (i = 0; i < sizeof(reg)/sizeof(struct spmtwam_reg_pair); i++) {
		offset = 0;
		if (of_property_read_u32(node, reg[i].name, &offset)) {
			pr_info("failed to parse '%s' in spmtwam dts\n",
				reg[i].name);
			return -ENOENT;
		}
		reg[i].addr = base + offset;
		pr_info("%s 0x%x\n", reg[i].name, offset);
	}

	ret = request_irq(irq0, spm_irq0_handler,
		IRQF_TRIGGER_LOW | IRQF_NO_SUSPEND, "TWAM", NULL);
	if (ret)
		return ret;

	pr_info("spmtwam base %p irq %u\n", base, irq0);

	return ret;
}

static const struct of_device_id spmtwam_of_ids[] = {
	{.compatible = SPMTWAM_COMPATIBLE_STRING,},
	{}
};

static struct platform_driver spmtwam_drv = {
	.probe = spmtwam_probe,
	.driver = {
		.name = "twam",
		.owner = THIS_MODULE,
		.of_match_table = spmtwam_of_ids,
	},
};

static int __init spmtwam_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&spmtwam_drv);
	g_spmtwam_init = (ret == 0);

	return ret;
}

module_init(spmtwam_init);

static void __exit spmtwam_exit(void)
{
	g_spmtwam_init = false;

	return platform_driver_unregister(&spmtwam_drv);
}

module_exit(spmtwam_exit);

MODULE_DESCRIPTION("Mediatek MT67XX spmtwam driver");
MODULE_AUTHOR("JM Lai <jm.lai@mediatek.com>");
MODULE_LICENSE("GPL");
