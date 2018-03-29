
#ifndef __TEEI_SMC_ID_H__
#define __TEEI_SMC_ID_H__


/* SMC Identifiers for non-secure world functions */
#define CALL_TRUSTZONE_API  0x1
#if defined(CONFIG_S5PV310_BOARD) || defined(CONFIG_MVV4412_BOARD)
/* Based arch/arm/mach-exynos/include/mach/smc.h */
#define SMC_CMD_INIT		(-1)
#define SMC_CMD_INFO		(-2)
/* For Power Management */
#define SMC_CMD_SLEEP		(-3)
#define SMC_CMD_CPU1BOOT	(-4)
#define SMC_CMD_CPU0AFTR	(-5)
/* For CP15 Access */
#define SMC_CMD_C15RESUME	(-11)
/* For L2 Cache Access */
#define SMC_CMD_L2X0CTRL	(-21)
#define SMC_CMD_L2X0SETUP1	(-22)
#define SMC_CMD_L2X0SETUP2	(-23)
#define SMC_CMD_L2X0INVALL	(-24)
#define SMC_CMD_L2X0DEBUG	(-25)
#define SMC_CMD_L2X0FLUSHALL (-26)
#define SMC_CMD_L2X0CLEANALL (-27)
#define SMC_CMD_L2X0FLUSHRANGE (-28)
/* For Framebuffer */
#define SMC_CMD_INIT_SECURE_WINDOW (-29)

#define SMC_CP15_REG			(-102)
#define SMC_CP15_AUX_CTRL		0x1
#define SMC_CP15_L2_PREFETCH	0x2
#define SMC_CACHE_CTRL			0x3
#endif

#ifdef CONFIG_ZYNQ7_BOARD
#define SMC_CMD_CPU1BOOT       (-4)
#define SMC_CMD_SECURE_READ		(-30)
#define SMC_CMD_SECURE_WRITE	(-31)
#endif

#endif /* __TEEI_SMC_ID__ */
