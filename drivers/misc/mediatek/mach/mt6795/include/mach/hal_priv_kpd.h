#ifndef KPD_PRIV_H
#define KPD_PRIV_H

/* Keypad registers */
#ifdef CONFIG_OF
extern void __iomem *kp_base;
#define KP_STA			(kp_base + 0x0000)
#define KP_MEM1			(kp_base + 0x0004)
#define KP_MEM2			(kp_base + 0x0008)
#define KP_MEM3			(kp_base + 0x000c)
#define KP_MEM4			(kp_base + 0x0010)
#define KP_MEM5			(kp_base + 0x0014)
#define KP_DEBOUNCE		(kp_base + 0x0018)
#define KP_SCAN_TIMING		(kp_base + 0x001C)
#define KP_SEL			(kp_base + 0x0020)
#define KP_EN			(kp_base + 0x0024)
#else
#define KP_STA			(KP_BASE + 0x0000)
#define KP_MEM1			(KP_BASE + 0x0004)
#define KP_MEM2			(KP_BASE + 0x0008)
#define KP_MEM3			(KP_BASE + 0x000c)
#define KP_MEM4			(KP_BASE + 0x0010)
#define KP_MEM5			(KP_BASE + 0x0014)
#define KP_DEBOUNCE		(KP_BASE + 0x0018)
#define KP_SCAN_TIMING		(KP_BASE + 0x001C)
#define KP_SEL			(KP_BASE + 0x0020)
#define KP_EN			(KP_BASE + 0x0024)
#endif

#define KPD_DEBOUNCE_MASK	((1U << 14) - 1)

#endif
