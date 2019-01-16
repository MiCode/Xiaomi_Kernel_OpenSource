#ifndef __MCU_H__
#define __MCU_H__

/*
 * Define hardware registers.
 */
#define MCU_BIU_CON             (MCU_BIU_BASE + 0x0)
#define MCU_BIU_PMCR            (MCU_BIU_BASE + 0x14)
#define MCU_BIU_CCR             (MCU_BIU_BASE + 0x40)
#define MCU_BIU_CCR_CON         (MCU_BIU_BASE + 0x44)
#define MCU_BIU_CCR_OVFL        (MCU_BIU_BASE + 0x48)
#define MCU_BIU_EVENT0_SEL      (MCU_BIU_BASE + 0x50)
#define MCU_BIU_EVENT0_CNT      (MCU_BIU_BASE + 0x54)
#define MCU_BIU_EVENT0_CON      (MCU_BIU_BASE + 0x58)
#define MCU_BIU_EVENT0_OVFL     (MCU_BIU_BASE + 0x5C)
#define MCU_BIU_EVENT1_SEL      (MCU_BIU_BASE + 0x60)
#define MCU_BIU_EVENT1_CNT      (MCU_BIU_BASE + 0x64)
#define MCU_BIU_EVENT1_CON      (MCU_BIU_BASE + 0x68)
#define MCU_BIU_EVENT1_OVFL     (MCU_BIU_BASE + 0x6C)



/*
 * Define constants.
 */


/*
 * Define function prototypes.
 */
#endif  /*!__MCU_H__ */
