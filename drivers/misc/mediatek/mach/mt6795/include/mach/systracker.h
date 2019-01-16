#ifndef _SYSTRAKCER_H
#define _SYSTRAKCER_H

#define BUS_DBG_CON                 (BUS_DBG_BASE + 0x0000)
#define BUS_DBG_TIMER_CON           (BUS_DBG_BASE + 0x0004)
#define BUS_DBG_TIMER               (BUS_DBG_BASE + 0x0008)
#define BUS_DBG_WP                  (BUS_DBG_BASE + 0x000C)
#define BUS_DBG_WP_MASK             (BUS_DBG_BASE + 0x0010)
#define BUS_DBG_MON                 (BUS_DBG_BASE + 0x0014)
#define BUS_DBG_AR_TRACK_L(__n)     (BUS_DBG_BASE + 0x0100 + 8 * (__n))
#define BUS_DBG_AR_TRACK_H(__n)     (BUS_DBG_BASE + 0x0104 + 8 * (__n))
#define BUS_DBG_AR_TRANS_TID(__n)   (BUS_DBG_BASE + 0x0180 + 4 * (__n))
#define BUS_DBG_AW_TRACK_L(__n)     (BUS_DBG_BASE + 0x0200 + 8 * (__n))
#define BUS_DBG_AW_TRACK_H(__n)     (BUS_DBG_BASE + 0x0204 + 8 * (__n))
#define BUS_DBG_AW_TRANS_TID(__n)   (BUS_DBG_BASE + 0x0280 + 4 * (__n))


#define BUS_DBG_BUS_MHZ             (266)
#define BUS_DBG_NUM_TRACKER         (8)
#define BUS_DBG_CON_DEFAULT_VAL     (0x00006036)

#define BUS_DBG_CON_BUS_DBG_EN      (0x00000001)
#define BUS_DBG_CON_TIMEOUT_EN      (0x00000002)
#define BUS_DBG_CON_SLV_ERR_EN      (0x00000004)
#define BUS_DBG_CON_WP_EN           (0x00000008)
#define BUS_DBG_CON_IRQ_AR_EN       (0x00000010)
#define BUS_DBG_CON_IRQ_AW_EN       (0x00000020)
#define BUS_DBG_CON_SW_RST_DN       (0x00000040)
#define BUS_DBG_CON_IRQ_CLR         (0x00000080)
#define BUS_DBG_CON_IRQ_AR_STA      (0x00000100)
#define BUS_DBG_CON_IRQ_AW_STA      (0x00000200)
#define BUS_DBG_CON_IRQ_WP_STA      (0x00000400)
#define BUS_DBG_CON_WDT_RST_EN      (0x00001000)
#define BUS_DBG_CON_HALT_ON_EN      (0x00002000)
#define BUS_DBG_CON_BUS_OT_EN       (0x00004000)
#define BUS_DBG_CON_SW_RST          (0x00010000)

#define BUS_DBG_CON_IRQ_EN          (BUS_DBG_CON_IRQ_AR_EN | BUS_DBG_CON_IRQ_AW_EN)

extern void enable_systracker(void);
extern void disable_systracker(void);

static inline unsigned int extract_n2mbits(unsigned int input, int n, int m)
{
/*
 * 1. ~0 = 1111 1111 1111 1111 1111 1111 1111 1111
 * 2. ~0 << (m - n + 1) = 1111 1111 1111 1111 1100 0000 0000 0000
 * // assuming we are extracting 14 bits, the +1 is added for inclusive selection
 * 3. ~(~0 << (m - n + 1)) = 0000 0000 0000 0000 0011 1111 1111 1111
 */
        int mask;
        if (n > m) {
            n = n + m;
            m = n - m;
            n = n - m;
        }
        mask = ~(~0 << (m - n + 1));
        return ((input >> n) & mask);
}

#endif
