/* 2MB large area for motherboard's peripherals static mapping */
#define V2M_PERIPH 0xf8000000

/* Tile's peripherals static mappings should start here */
#define V2T_PERIPH 0xf8200000

void vexpress_dt_smp_map_io(void);

#define AMBA_DEVICE(name,busid,base,plat)	\
struct amba_device name##_device = {		\
	.dev		= {			\
		.coherent_dma_mask = ~0UL,	\
		.init_name = busid,		\
		.platform_data = plat,		\
	},					\
	.res		= {			\
		.start	= base,			\
		.end	= base + SZ_4K - 1,	\
		.flags	= IORESOURCE_MEM,	\
	},					\
	.dma_mask	= ~0UL,			\
	.irq		= IRQ_##base,		\
	/* .dma		= DMA_##base,*/		\
}

struct arm_soc_smp_init_ops;
struct arm_soc_smp_ops;

extern struct arm_soc_smp_init_ops	vexpress_soc_smp_init_ops;
extern struct arm_soc_smp_ops		vexpress_soc_smp_ops;

extern void vexpress_cpu_die(unsigned int cpu);
