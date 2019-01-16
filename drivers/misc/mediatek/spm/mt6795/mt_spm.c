#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/smp.h>
#include <linux/delay.h>
#include <linux/atomic.h>

#include <mach/mt_spm_sleep.h>
#include <mach/mt_spm_idle.h>
#include <mach/mt_boot.h>
#include <mach/irqs.h>
#include <mach/wd_api.h>

#include "mt_spm_internal.h"

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif

#ifdef CONFIG_OF
void __iomem *spm_base;

void __iomem *spm_i2c0_base;
void __iomem *spm_i2c1_base;
void __iomem *spm_i2c2_base;
void __iomem *spm_i2c3_base;
void __iomem *spm_mcucfg;
void __iomem *spm_md32_base;

u32 spm_irq_0 = 195;
u32 spm_irq_1 = 196;
u32 spm_irq_2 = 197;
u32 spm_irq_3 = 198;
u32 spm_irq_4 = 199;
u32 spm_irq_5 = 200;
u32 spm_irq_6 = 201;
u32 spm_irq_7 = 202;
#endif





/**************************************
 * Config and Parameter
 **************************************/
#define SPM_MD_DDR_EN_OUT	0


/**************************************
 * Define and Declare
 **************************************/
struct spm_irq_desc {
	unsigned int irq;
	irq_handler_t handler;
};

static twam_handler_t spm_twam_handler;


/**************************************
 * Init and IRQ Function
 **************************************/
static irqreturn_t spm_irq0_handler(int irq, void *dev_id)
{
	u32 isr;
	unsigned long flags;
	struct twam_sig twamsig;

	spin_lock_irqsave(&__spm_lock, flags);
	/* get ISR status */
	isr = spm_read(SPM_SLEEP_ISR_STATUS);
	if (isr & ISRS_TWAM) {
		twamsig.sig0 = spm_read(SPM_SLEEP_TWAM_STATUS0);
		twamsig.sig1 = spm_read(SPM_SLEEP_TWAM_STATUS1);
		twamsig.sig2 = spm_read(SPM_SLEEP_TWAM_STATUS2);
		twamsig.sig3 = spm_read(SPM_SLEEP_TWAM_STATUS3);
	}

	/* clean ISR status */
	spm_write(SPM_SLEEP_ISR_MASK, spm_read(SPM_SLEEP_ISR_MASK) | ISRM_ALL_EXC_TWAM);
	spm_write(SPM_SLEEP_ISR_STATUS, isr);
	if (isr & ISRS_TWAM)
		udelay(100);	/* need 3T TWAM clock (32K/26M) */
	spm_write(SPM_PCM_SW_INT_CLEAR, PCM_SW_INT0);
	spin_unlock_irqrestore(&__spm_lock, flags);

	if ((isr & ISRS_TWAM) && spm_twam_handler)
		spm_twam_handler(&twamsig);

	if (isr & (ISRS_SW_INT0 | ISRS_PCM_RETURN))
		spm_err("IRQ0 HANDLER SHOULD NOT BE EXECUTED (0x%x)\n", isr);

	return IRQ_HANDLED;
}

static irqreturn_t spm_irq_aux_handler(u32 irq_id)
{
	u32 isr;
	unsigned long flags;

	spin_lock_irqsave(&__spm_lock, flags);
	isr = spm_read(SPM_SLEEP_ISR_STATUS);
	spm_write(SPM_PCM_SW_INT_CLEAR, (1U << irq_id));
	spin_unlock_irqrestore(&__spm_lock, flags);

	spm_err("IRQ%u HANDLER SHOULD NOT BE EXECUTED (0x%x)\n", irq_id, isr);

	return IRQ_HANDLED;
}

static irqreturn_t spm_irq1_handler(int irq, void *dev_id)
{
	return spm_irq_aux_handler(1);
}

static irqreturn_t spm_irq2_handler(int irq, void *dev_id)
{
	return spm_irq_aux_handler(2);
}

static irqreturn_t spm_irq3_handler(int irq, void *dev_id)
{
	return spm_irq_aux_handler(3);
}

static irqreturn_t spm_irq4_handler(int irq, void *dev_id)
{
	return spm_irq_aux_handler(4);
}

static irqreturn_t spm_irq5_handler(int irq, void *dev_id)
{
	return spm_irq_aux_handler(5);
}

static irqreturn_t spm_irq6_handler(int irq, void *dev_id)
{
	return spm_irq_aux_handler(6);
}

static irqreturn_t spm_irq7_handler(int irq, void *dev_id)
{
	return spm_irq_aux_handler(7);
}

static int spm_irq_register(void)
{
	int i, err, r = 0;
#ifdef CONFIG_OF
struct spm_irq_desc irqdesc[] = {
                { .irq = 0, .handler = spm_irq0_handler, },
                { .irq = 0, .handler = spm_irq1_handler, },
                { .irq = 0, .handler = spm_irq2_handler, },
                { .irq = 0, .handler = spm_irq3_handler, },
                { .irq = 0, .handler = spm_irq4_handler, },
                { .irq = 0, .handler = spm_irq5_handler, },
                { .irq = 0, .handler = spm_irq6_handler, },
                { .irq = 0, .handler = spm_irq7_handler, }
        };

    irqdesc[0].irq = SPM_IRQ0_ID;
    irqdesc[1].irq = SPM_IRQ1_ID;
    irqdesc[2].irq = SPM_IRQ2_ID;
    irqdesc[3].irq = SPM_IRQ3_ID;
    irqdesc[4].irq = SPM_IRQ4_ID;
    irqdesc[5].irq = SPM_IRQ5_ID;
    irqdesc[6].irq = SPM_IRQ6_ID;
    irqdesc[7].irq = SPM_IRQ7_ID;
#else
	struct spm_irq_desc irqdesc[] = {
		{ .irq = SPM_IRQ0_ID, .handler = spm_irq0_handler, },
		{ .irq = SPM_IRQ1_ID, .handler = spm_irq1_handler, },
		{ .irq = SPM_IRQ2_ID, .handler = spm_irq2_handler, },
		{ .irq = SPM_IRQ3_ID, .handler = spm_irq3_handler, },
		{ .irq = SPM_IRQ4_ID, .handler = spm_irq4_handler, },
		{ .irq = SPM_IRQ5_ID, .handler = spm_irq5_handler, },
		{ .irq = SPM_IRQ6_ID, .handler = spm_irq6_handler, },
		{ .irq = SPM_IRQ7_ID, .handler = spm_irq7_handler, }
	};
#endif
	for (i = 0; i < ARRAY_SIZE(irqdesc); i++) {
		err = request_irq(irqdesc[i].irq, irqdesc[i].handler,
				  IRQF_TRIGGER_LOW | IRQF_NO_SUSPEND,
				  "SPM", NULL);
		if (err) {
			spm_err("FAILED TO REQUEST IRQ%d (%d)\n", i, err);
			r = -EPERM;
		}

		/* assign each SPM IRQ to each CPU */
		mt_gic_cfg_irq2cpu(irqdesc[i].irq, 0, 0);
		mt_gic_cfg_irq2cpu(irqdesc[i].irq, i % num_possible_cpus(), 1);
	}

	return r;
}

static void spm_register_init(void)
{
	unsigned long flags;
#ifdef CONFIG_OF
    struct device_node *node;

    node = of_find_compatible_node(NULL, NULL, "mediatek,SLEEP");
    if (!node) {
        spm_err("find SLEEP node failed\n");
    }
    spm_base = of_iomap(node, 0);
    if (!spm_base)
        spm_err("base spm_base failed\n");

    spm_irq_0 = irq_of_parse_and_map(node, 0);
    if (!spm_irq_0) {
                spm_err("get spm_irq_0 failed\n");
        }
    spm_irq_1 = irq_of_parse_and_map(node, 1);
    if (!spm_irq_1) {
                spm_err("get spm_irq_1 failed\n");
        }
    spm_irq_2 = irq_of_parse_and_map(node, 2);
    if (!spm_irq_2) {
                spm_err("get spm_irq_2 failed\n");
        }
    spm_irq_3 = irq_of_parse_and_map(node, 3);
    if (!spm_irq_3) {
                spm_err("get spm_irq_3 failed\n");
        }
    spm_irq_4 = irq_of_parse_and_map(node, 4);
    if (!spm_irq_4) {
                spm_err("get spm_irq_4 failed\n");
        }
    spm_irq_5 = irq_of_parse_and_map(node, 5);
    if (!spm_irq_5) {
                spm_err("get spm_irq_5 failed\n");
        }
    spm_irq_6 = irq_of_parse_and_map(node, 6);
    if (!spm_irq_6) {
                spm_err("get spm_irq_6 failed\n");
        }
    spm_irq_7 = irq_of_parse_and_map(node, 7);
    if (!spm_irq_7) {
                spm_err("get spm_irq_7 failed\n");
        }
    node = of_find_compatible_node(NULL, NULL, "mediatek,I2C0");
    if (!node) {
        spm_err("find I2C0 node failed\n");
    }
    spm_i2c0_base = of_iomap(node, 0);
    if (!spm_i2c0_base)
        spm_err("base i2c0_base failed\n");

    node = of_find_compatible_node(NULL, NULL, "mediatek,I2C1");
    if (!node) {
        spm_err("find I2C1 node failed\n");
    }
    spm_i2c1_base = of_iomap(node, 0);
    if (!spm_i2c1_base)
        spm_err("base i2c1_base failed\n");

    node = of_find_compatible_node(NULL, NULL, "mediatek,I2C2");
    if (!node) {
        spm_err("find I2C2 node failed\n");
    }
    spm_i2c2_base = of_iomap(node, 0);
    if (!spm_i2c2_base)
        spm_err("base i2c2_base failed\n");

    node = of_find_compatible_node(NULL, NULL, "mediatek,I2C3");
    if (!node) {
        spm_err("find I2C3 node failed\n");
    }
    spm_i2c3_base = of_iomap(node, 0);
    if (!spm_i2c3_base)
        spm_err("base i2c3_base failed\n");
    //mcucfg        
    node = of_find_compatible_node(NULL, NULL, "mediatek,MCUCFG");
    if (!node) {
        spm_err("[MCUCFG] find node failed\n");
    }
    spm_mcucfg = of_iomap(node, 0);
    if (!spm_mcucfg)
        spm_err("[MCUCFG] base failed\n");

    //MD32 base
   node = of_find_compatible_node(NULL, NULL, "mediatek,SCP_SRAM");
    if(!node)
        spm_err("[MD32] Can't find node: mediatek,SCP_SRAM\n");
    spm_md32_base = of_iomap(node, 0);
    if (!spm_md32_base)
        spm_err("[MD32] Unable to ioremap mregisters\n");

    spm_err("spm_base = %p, i2c0_base = %p, i2c1_base = %p, i2c2_base = %p, i2c3_base = %p\n", spm_base, spm_i2c0_base, spm_i2c1_base, spm_i2c2_base, spm_i2c3_base);
    spm_err("spm_irq_0 = %d, spm_irq_1 = %d, spm_irq_2 = %d, spm_irq_3 = %d\n", spm_irq_0, spm_irq_1, spm_irq_2, spm_irq_3);
    spm_err("spm_irq_4 = %d, spm_irq_5 = %d, spm_irq_6 = %d, spm_irq_7 = %d\n", spm_irq_4, spm_irq_5, spm_irq_6, spm_irq_7);
    spm_err("spm_mcucfg = %p, spm_md32_base = %p\n", spm_mcucfg, spm_md32_base);
#endif

	spin_lock_irqsave(&__spm_lock, flags);
	/* enable register control */
	spm_write(SPM_POWERON_CONFIG_SET, SPM_REGWR_CFG_KEY | SPM_REGWR_EN);

	/* init power control register */
	spm_write(SPM_POWER_ON_VAL0, 0);
	spm_write(SPM_POWER_ON_VAL1, POWER_ON_VAL1_DEF);
	spm_write(SPM_PCM_PWR_IO_EN, 0);

	/* reset PCM */
	spm_write(SPM_PCM_CON0, CON0_CFG_KEY | CON0_PCM_SW_RESET);
	spm_write(SPM_PCM_CON0, CON0_CFG_KEY);
	BUG_ON(spm_read(SPM_PCM_FSM_STA) != PCM_FSM_STA_DEF);	/* PCM reset failed */

	/* init PCM control register */
	spm_write(SPM_PCM_CON0, CON0_CFG_KEY | CON0_IM_SLEEP_DVS);
	spm_write(SPM_PCM_CON1, CON1_CFG_KEY | CON1_EVENT_LOCK_EN |
				CON1_SPM_SRAM_ISO_B | CON1_SPM_SRAM_SLP_B |
				CON1_MIF_APBEN);
	spm_write(SPM_PCM_IM_PTR, 0);
	spm_write(SPM_PCM_IM_LEN, 0);

	/*
	 * SRCLKENA0: POWER_ON_VAL1 (PWR_IO_EN[7]=0) or
	 *            E1: r7|SRCLKENAI0|SRCLKENAI1|MD1_SRCLKENA (PWR_IO_EN[7]=1)
	 *            E2: r7|SRCLKENAI0 (PWR_IO_EN[7]=1)
	 * CLKSQ0_OFF: POWER_ON_VAL0 (PWR_IO_EN[0]=0) or r0 (PWR_IO_EN[0]=1)
	 * SRCLKENA1: MD2_SRCLKENA
	 * CLKSQ1_OFF: !MD2_SRCLKENA
	 */
	spm_write(SPM_CLK_CON, (mt_get_chip_sw_ver() == CHIP_SW_VER_01 ? CC_SYSCLK0_EN_1 : 0) |
			       CC_SYSCLK0_EN_0 |
			       CC_SYSCLK1_EN_1 | CC_SYSCLK1_EN_0 |
			       CC_CLKSQ1_SEL |
			       CC_CXO32K_RM_EN_MD2 | CC_CXO32K_RM_EN_MD1 |
			       CC_MD32_DCM_EN);
	spm_write(SPM_PCM_SRC_REQ, SR_CCIF_TO_AP_MASK_B | SR_CCIF_TO_MD_MASK_B);

	/* clean wakeup event raw status */
	spm_write(SPM_SLEEP_WAKEUP_EVENT_MASK, ~0);

	/* clean ISR status */
	spm_write(SPM_SLEEP_ISR_MASK, ISRM_ALL);
	spm_write(SPM_SLEEP_ISR_STATUS, ISRC_ALL);
	spm_write(SPM_PCM_SW_INT_CLEAR, PCM_SW_INT_ALL);

	/* switch spm_md32_irq source to MD32 wakeup source */
	spm_write(SPM_SLEEP_MD32_WAKEUP_EVENT_MASK, ~0);
	spm_write(SPM_PCM_MD32_IRQ, PCM_MD32_IRQ_SEL);

	/* output md_ddr_en if needed for debug */
#if SPM_MD_DDR_EN_OUT
	__spm_dbgout_md_ddr_en(true);
#endif
	spin_unlock_irqrestore(&__spm_lock, flags);
}

int spm_module_init(void)
{
	int r = 0;
	struct wd_api *wd_api;

	spm_register_init();

	if (spm_irq_register() != 0)
		r = -EPERM;

	if (spm_fs_init() != 0)
		r = -EPERM;

	get_wd_api(&wd_api);
	if (wd_api->wd_spmwdt_mode_config) {
		wd_api->wd_spmwdt_mode_config(WD_REQ_EN, WD_REQ_RST_MODE);
	} else {
		spm_err("FAILED TO GET WD API\n");
		r = -ENODEV;
	}

	spm_suspend_init();
	spm_dpidle_init();
	spm_sodi_init();
	spm_mcdi_init();
	spm_talking_init();

	return r;
}


/**************************************
 * PLL Request API
 **************************************/
void spm_mainpll_on_request(const char *drv_name)
{
	int req;
	req = atomic_inc_return(&__spm_mainpll_req);
	spm_debug("%s request MAINPLL on (%d)\n", drv_name, req);
}
EXPORT_SYMBOL(spm_mainpll_on_request);

void spm_mainpll_on_unrequest(const char *drv_name)
{
	int req;
	req = atomic_dec_return(&__spm_mainpll_req);
	spm_debug("%s unrequest MAINPLL on (%d)\n", drv_name, req);
}
EXPORT_SYMBOL(spm_mainpll_on_unrequest);


/**************************************
 * TWAM Control API
 **************************************/
void spm_twam_register_handler(twam_handler_t handler)
{
	spm_twam_handler = handler;
}
EXPORT_SYMBOL(spm_twam_register_handler);

void spm_twam_enable_monitor(const struct twam_sig *twamsig, bool speed_mode)
{
	u32 sig0 = 0, sig1 = 0, sig2 = 0, sig3 = 0;
	unsigned long flags;

	if (twamsig) {
		sig0 = twamsig->sig0 & 0x1f;
		sig1 = twamsig->sig1 & 0x1f;
		sig2 = twamsig->sig2 & 0x1f;
		sig3 = twamsig->sig3 & 0x1f;
	}

	spin_lock_irqsave(&__spm_lock, flags);
	spm_write(SPM_SLEEP_ISR_MASK, spm_read(SPM_SLEEP_ISR_MASK) & ~ISRM_TWAM);
	spm_write(SPM_SLEEP_TWAM_CON, (sig3 << 27) |
				      (sig2 << 22) |
				      (sig1 << 17) |
				      (sig0 << 12) |
				      (speed_mode ? TWAM_CON_SPEED_EN : 0) |
				      TWAM_CON_EN);
	spin_unlock_irqrestore(&__spm_lock, flags);

	spm_debug("enable TWAM for signal %u, %u, %u, %u (%u)\n",
		  sig0, sig1, sig2, sig3, speed_mode);
}
EXPORT_SYMBOL(spm_twam_enable_monitor);

void spm_twam_disable_monitor(void)
{
	unsigned long flags;

	spin_lock_irqsave(&__spm_lock, flags);
	spm_write(SPM_SLEEP_TWAM_CON, spm_read(SPM_SLEEP_TWAM_CON) & ~TWAM_CON_EN);
	spm_write(SPM_SLEEP_ISR_MASK, spm_read(SPM_SLEEP_ISR_MASK) | ISRM_TWAM);
	spm_write(SPM_SLEEP_ISR_STATUS, ISRC_TWAM);
	spin_unlock_irqrestore(&__spm_lock, flags);

	spm_debug("disable TWAM\n");
}
EXPORT_SYMBOL(spm_twam_disable_monitor);

MODULE_DESCRIPTION("SPM Driver v0.6");
