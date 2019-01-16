#ifndef __MT_IRQ_H
#define __MT_IRQ_H

#include "mt_reg_base.h"

/*
 * Define hadware registers.
 */
//#define GIC_DIST_BASE       (CA9_BASE + 0x1000)
//#define GIC_CPU_BASE        (CA9_BASE + 0x2000)
//#define INT_POL_CTL0        (MCUCFG_BASE + 0x034)

#define GIC_PRIVATE_SIGNALS     (32)
#define NR_GIC_SGI              (16)
#define NR_GIC_PPI              (16)
#define GIC_PPI_OFFSET          (27)
#define MT_NR_PPI               (5)
#define MT_NR_SPI               (233)
#define NR_MT_IRQ_LINE          (GIC_PPI_OFFSET + MT_NR_PPI + MT_NR_SPI)

#define GIC_PPI_GLOBAL_TIMER    (GIC_PPI_OFFSET + 0)
#define GIC_PPI_LEGACY_FIQ      (GIC_PPI_OFFSET + 1)
#define GIC_PPI_PRIVATE_TIMER   (GIC_PPI_OFFSET + 2)
#define GIC_PPI_WATCHDOG_TIMER  (GIC_PPI_OFFSET + 3)
#define GIC_PPI_LEGACY_IRQ      (GIC_PPI_OFFSET + 4)

#define MT_BTIF_IRQ_ID                  (GIC_PRIVATE_SIGNALS + 50)
#define MT_DMA_BTIF_TX_IRQ_ID              (GIC_PRIVATE_SIGNALS + 71)
#define MT_DMA_BTIF_RX_IRQ_ID              (GIC_PRIVATE_SIGNALS + 72)

#if !defined(__ASSEMBLY__)
#define X_DEFINE_IRQ(__name, __num, __pol, __sens)  __name = __num,
enum 
{
#include "x_define_irq.h"
};
#undef X_DEFINE_IRQ

#endif

//FIXME: Marcos Add for name alias (may wrong!!!!!)
#define MT_SPM_IRQ_ID SLEEP_IRQ_BIT0_ID
#define MT_SPM1_IRQ_ID SLEEP_IRQ_BIT1_ID
#define MT_KP_IRQ_ID KP_IRQ_BIT_ID
#define MD_WDT_IRQ_ID WDT_IRQ_BIT_ID
#define MT_CIRQ_IRQ_ID SYS_CIRQ_IRQ_BIT_ID
#define MT_USB0_IRQ_ID USB_MCU_IRQ_BIT1_ID
#define MT_UART4_IRQ_ID UART3_IRQ_BIT_ID
/* assign a random number since it won't be used */

#endif  /*  !__IRQ_H__ */

