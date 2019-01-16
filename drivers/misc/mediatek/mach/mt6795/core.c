#include <linux/pm.h>
#include <linux/bug.h>
#include <linux/memblock.h>
#include <linux/irqchip.h>
#include <linux/of_platform.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>
#include <asm/mach/map.h>
#include <asm/mach-types.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/smp_scu.h>
#include <asm/page.h>
#include <mach/mt_reg_base.h>
#include <mach/irqs.h>

extern struct smp_operations mt_smp_ops;
extern void __init mt_timer_init(void);
extern void mt_fixup(struct tag *tags, char **cmdline, struct meminfo *mi);
#ifdef CONFIG_OF
extern void __init mt_dt_init_early(void);
#endif
extern void mt_reserve(void);

/* FIXME: need to remove */
extern void arm_machine_restart(char mode, const char *cmd);

void __init mt_init(void)
{

}

static struct map_desc mt_io_desc[] __initdata =
{
//#if defined(CONFIG_MTK_FPGA)
	{
		.virtual = CKSYS_BASE,
		.pfn = __phys_to_pfn(IO_VIRT_TO_PHYS(CKSYS_BASE)),
		.length = SZ_4K * 19,
		.type = MT_DEVICE,
	},
	{
		.virtual = SPM_MD32_BASE,
		.pfn = __phys_to_pfn(IO_VIRT_TO_PHYS(SPM_MD32_BASE)),
		.length = SZ_256K,
		.type = MT_DEVICE,
	},
	{
		.virtual = MCUCFG_BASE,
		.pfn = __phys_to_pfn(IO_VIRT_TO_PHYS(MCUCFG_BASE)),
		.length = SZ_4K * 26,
		.type = MT_DEVICE
	},
	{
		.virtual = CA9_BASE,
		.pfn = __phys_to_pfn(IO_VIRT_TO_PHYS(CA9_BASE)),
		.length = SZ_32K,
		.type = MT_DEVICE
	},

	{
		.virtual = DBGAPB_BASE,
		.pfn = __phys_to_pfn(IO_VIRT_TO_PHYS(DBGAPB_BASE)),
		.length = SZ_1M,
		.type = MT_DEVICE,
	},
	{
		.virtual = CCI400_BASE,
		.pfn = __phys_to_pfn(IO_VIRT_TO_PHYS(CCI400_BASE)),
		.length = SZ_64K,
		.type = MT_DEVICE,
	},
	{
		.virtual = AP_DMA_BASE,
		.pfn = __phys_to_pfn(IO_VIRT_TO_PHYS(AP_DMA_BASE)),
		.length = SZ_4K * 18,
		.type = MT_DEVICE,
	},
	{
		.virtual = USB0_BASE,
		.pfn = __phys_to_pfn(IO_VIRT_TO_PHYS(USB0_BASE)),
		.length = SZ_64K * 10 ,
		.type = MT_DEVICE,
	},
	{
		.virtual = HAN_BASE,
		.pfn = __phys_to_pfn(IO_VIRT_TO_PHYS(HAN_BASE)),
		.length = SZ_16M,
		.type = MT_DEVICE,
		},
	{
		.virtual = MMSYS_CONFIG_BASE,
		.pfn = __phys_to_pfn(IO_VIRT_TO_PHYS(MMSYS_CONFIG_BASE)),
		.length = SZ_4K * 36,
		.type = MT_DEVICE,
	},
	{
		.virtual = IMGSYS_BASE,
		.pfn = __phys_to_pfn(IO_VIRT_TO_PHYS(IMGSYS_BASE)),
		.length = SZ_64K,
		.type = MT_DEVICE,
	},
	{
		.virtual = VDEC_GCON_BASE,
		.pfn = __phys_to_pfn(IO_VIRT_TO_PHYS(VDEC_GCON_BASE)),
		.length = SZ_64K * 3,
		.type = MT_DEVICE,
	},
	{
		.virtual = MJC_CONFIG_BASE,
		.pfn = __phys_to_pfn(IO_VIRT_TO_PHYS(MJC_CONFIG_BASE)),
		.length = SZ_4K * 3,
		.type = MT_DEVICE,
	},
	{
		.virtual = VENC_GCON_BASE,
		.pfn = __phys_to_pfn(IO_VIRT_TO_PHYS(VENC_GCON_BASE)),
		.length = SZ_4K * 5,
		.type = MT_DEVICE,
	},
	{
		/* virtual 0xF9000000, physical 0x00100000 */
		.virtual = SYSRAM_BASE,
		.pfn = __phys_to_pfn(BOOTSRAM_BASE),
		.length = SZ_64K,
		.type = MT_MEMORY_NONCACHED
	},
	{
		.virtual = DEVINFO_BASE,
		.pfn = __phys_to_pfn(0x08000000),
		.length = SZ_64K,
		.type = MT_DEVICE
	},
	/* FIXME: comment out for early porting */
	#if 0
	{
		.virtual = G3D_CONFIG_BASE,
		.pfn = __phys_to_pfn(IO_VIRT_TO_PHYS(G3D_CONFIG_BASE)),
		.length = SZ_128K,
		.type = MT_DEVICE
	},
	{
		.virtual = DISPSYS_BASE,
		.pfn = __phys_to_pfn(IO_VIRT_TO_PHYS(DISPSYS_BASE)),
		.length = SZ_16M,
		.type = MT_DEVICE
	},
	{
		.virtual = IMGSYS_CONFG_BASE,
		.pfn = __phys_to_pfn(IO_VIRT_TO_PHYS(IMGSYS_CONFG_BASE)),
		.length = SZ_16M,
		.type = MT_DEVICE
	},
	{
		.virtual = VDEC_GCON_BASE,
		.pfn = __phys_to_pfn(IO_VIRT_TO_PHYS(VDEC_GCON_BASE)),
		.length = SZ_16M,
		.type = MT_DEVICE
	},
	{
		.virtual = CONN_BTSYS_PKV_BASE,
		.pfn = __phys_to_pfn(IO_VIRT_TO_PHYS(CONN_BTSYS_PKV_BASE)),
		.length = SZ_1M,
		.type = MT_DEVICE
	},
	{
		/* virtual 0xF9000000, physical 0x00100000 */
		.virtual = INTER_SRAM,
		.pfn = __phys_to_pfn(0x00100000),
		.length = SZ_64K,
		.type = MT_MEMORY_NONCACHED
	},
	#endif
//#endif
};

#ifdef CONFIG_OF
static const char *mt_dt_match[] __initdata =
{
    "mediatek,mt6595",
    NULL
};

static const struct of_device_id mt_dt_bus_match[] __initconst =
{
    { .compatible = "simple-bus", },
    {}
};

static void __init mt_dt_init_irq(void)
{
    irqchip_init();
    mt_init_irq();
}

static void __init mt_dt_init(void)
{
    mt_init();
    of_platform_populate(NULL, mt_dt_bus_match, NULL, NULL);
}
#endif

void __init mt_map_io(void)
{
	iotable_init(mt_io_desc, ARRAY_SIZE(mt_io_desc));
}

MACHINE_START(MT6595, "MT6595")
	.atag_offset	= 0x00000100,
	.map_io		= mt_map_io,
	.smp		= smp_ops(mt_smp_ops),
	.init_irq	= mt_init_irq,
	.init_time	= mt_timer_init,
	.init_machine	= mt_init,
	.fixup		= mt_fixup,
	/* FIXME: need to implement the restart function */
	.restart	= arm_machine_restart,
	.reserve	= mt_reserve,
MACHINE_END

#ifdef CONFIG_OF
DT_MACHINE_START(MT6595_DT, "MT6595")
	.map_io		= mt_map_io,
	.smp		= smp_ops(mt_smp_ops),
	.init_early	= mt_dt_init_early,
	.init_irq	= mt_dt_init_irq,
	.init_time	= mt_timer_init,
	.init_machine	= mt_dt_init,
	/* FIXME: need to implement the restart function */
	.restart	= arm_machine_restart,
	.reserve	= mt_reserve,
    .dt_compat  = mt_dt_match,
MACHINE_END
#endif
