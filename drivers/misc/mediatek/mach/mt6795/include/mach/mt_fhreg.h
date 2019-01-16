#ifndef __MT_FHREG_H__
#define __MT_FHREG_H__

#include <linux/bitops.h>
#include <mach/mt_reg_base.h>


/*--------------------------------------------------------------------------*/
/* Common Macro                                                             */
/*--------------------------------------------------------------------------*/
#ifdef CONFIG_OF

#ifdef CONFIG_ARM64 
#define REG_ADDR(x)                 ((unsigned long)g_fhctl_base   + (x))
#define REG_APMIX_ADDR(x)           ((unsigned long)g_apmixed_base + (x))
#define REG_DDRPHY_ADDR(x)          ((unsigned long)g_ddrphy_base  + (x))
#else
#define REG_ADDR(x)                 ((unsigned int)g_fhctl_base   + (x))
#define REG_APMIX_ADDR(x)           ((unsigned int)g_apmixed_base + (x))
#define REG_DDRPHY_ADDR(x)          ((unsigned int)g_ddrphy_base  + (x))
#endif

#else
#define REG_ADDR(x)                 (FHCTL_BASE + (x))
#define REG_APMIX_ADDR(x)           (APMIXED_BASE + (x))
#define REG_DDRPHY_ADDR(x)          (DDRPHY_BASE  + (x))   
#endif

#define REG_FHCTL_HP_EN     REG_ADDR(0x0000)
#define REG_FHCTL_CLK_CON   REG_ADDR(0x0004)
#define REG_FHCTL_RST_CON   REG_ADDR(0x0008)
#define REG_FHCTL_SLOPE0    REG_ADDR(0x000C)
#define REG_FHCTL_SLOPE1    REG_ADDR(0x0010)
#define REG_FHCTL_DSSC_CFG  REG_ADDR(0x0014)

#define REG_FHCTL_DSSC0_CON REG_ADDR(0x0018)
#define REG_FHCTL_DSSC1_CON REG_ADDR(0x001C)
#define REG_FHCTL_DSSC2_CON REG_ADDR(0x0020)
#define REG_FHCTL_DSSC3_CON REG_ADDR(0x0024)
#define REG_FHCTL_DSSC4_CON REG_ADDR(0x0028)
#define REG_FHCTL_DSSC5_CON REG_ADDR(0x002C)
#define REG_FHCTL_DSSC6_CON REG_ADDR(0x0030)
#define REG_FHCTL_DSSC7_CON REG_ADDR(0x0034)

#define REG_FHCTL0_CFG      REG_ADDR(0x0038)
#define REG_FHCTL0_UPDNLMT  REG_ADDR(0x003C)
#define REG_FHCTL0_DDS      REG_ADDR(0x0040)
#define REG_FHCTL0_DVFS     REG_ADDR(0x0044)
#define REG_FHCTL0_MON      REG_ADDR(0x0048)

#define REG_FHCTL1_CFG      REG_ADDR(0x004C)
#define REG_FHCTL1_UPDNLMT  REG_ADDR(0x0050)
#define REG_FHCTL1_DDS      REG_ADDR(0x0054)
#define REG_FHCTL1_DVFS     REG_ADDR(0x0058)
#define REG_FHCTL1_MON      REG_ADDR(0x005C)

#define REG_FHCTL2_CFG      REG_ADDR(0x0060)
#define REG_FHCTL2_UPDNLMT  REG_ADDR(0x0064)
#define REG_FHCTL2_DDS      REG_ADDR(0x0068)
#define REG_FHCTL2_DVFS     REG_ADDR(0x006C)
#define REG_FHCTL2_MON      REG_ADDR(0x0070)

#define REG_FHCTL3_CFG      REG_ADDR(0x0074)
#define REG_FHCTL3_UPDNLMT  REG_ADDR(0x0078)
#define REG_FHCTL3_DDS      REG_ADDR(0x007C)
#define REG_FHCTL3_DVFS     REG_ADDR(0x0080)
#define REG_FHCTL3_MON      REG_ADDR(0x0084)

#define REG_FHCTL4_CFG      REG_ADDR(0x0088)
#define REG_FHCTL4_UPDNLMT  REG_ADDR(0x008C)
#define REG_FHCTL4_DDS      REG_ADDR(0x0090)
#define REG_FHCTL4_DVFS     REG_ADDR(0x0094)
#define REG_FHCTL4_MON      REG_ADDR(0x0098)

#define REG_FHCTL5_CFG      REG_ADDR(0x009C)
#define REG_FHCTL5_UPDNLMT  REG_ADDR(0x00A0)
#define REG_FHCTL5_DDS      REG_ADDR(0x00A4)
#define REG_FHCTL5_DVFS     REG_ADDR(0x00A8)
#define REG_FHCTL5_MON      REG_ADDR(0x00AC)

#define REG_FHCTL6_CFG      REG_ADDR(0x00B0)
#define REG_FHCTL6_UPDNLMT  REG_ADDR(0x00B4)
#define REG_FHCTL6_DDS      REG_ADDR(0x00B8)
#define REG_FHCTL6_DVFS     REG_ADDR(0x00BC)
#define REG_FHCTL6_MON      REG_ADDR(0x00C0)

#define REG_FHCTL7_CFG      REG_ADDR(0x00C4)
#define REG_FHCTL7_UPDNLMT  REG_ADDR(0x00C8)
#define REG_FHCTL7_DDS      REG_ADDR(0x00CC)
#define REG_FHCTL7_DVFS     REG_ADDR(0x00D0)
#define REG_FHCTL7_MON      REG_ADDR(0x00D4)

#define REG_FHCTL8_CFG      REG_ADDR(0x00D8)
#define REG_FHCTL8_UPDNLMT  REG_ADDR(0x00DC)
#define REG_FHCTL8_DDS      REG_ADDR(0x00E0)
#define REG_FHCTL8_DVFS     REG_ADDR(0x00E4)
#define REG_FHCTL8_MON      REG_ADDR(0x00E8)

//****************************************************
// APMIXED CON0/CON1 Register 
//****************************************************

/*CON0, PLL enable status */
#define REG_ARMCA7PLL_CON0    REG_APMIX_ADDR(0x0210)
#define REG_ARMCA15PLL_CON0   REG_APMIX_ADDR(0x0200)
#define REG_MAINPLL_CON0      REG_APMIX_ADDR(0x0220)
#define REG_MPLL_CON0         REG_APMIX_ADDR(0x0280)
#define REG_MSDCPLL_CON0      REG_APMIX_ADDR(0x0250)
#define REG_MMPLL_CON0        REG_APMIX_ADDR(0x0240)
#define REG_VENCPLL_CON0      REG_APMIX_ADDR(0x0260)
#define REG_TVDPLL_CON0       REG_APMIX_ADDR(0x0270)
#define REG_VCODECPLL_CON0    REG_APMIX_ADDR(0x0290)

/*CON1, DDS value */
#define REG_ARMCA7PLL_CON1    REG_APMIX_ADDR(0x0214)
#define REG_ARMCA15PLL_CON1   REG_APMIX_ADDR(0x0204)
#define REG_MAINPLL_CON1      REG_APMIX_ADDR(0x0224)
#define REG_MPLL_CON1         REG_APMIX_ADDR(0x0284)
#define REG_MSDCPLL_CON1      REG_APMIX_ADDR(0x0254)
#define REG_MMPLL_CON1        REG_APMIX_ADDR(0x0244)
#define REG_VENCPLL_CON1      REG_APMIX_ADDR(0x0264)
#define REG_TVDPLL_CON1       REG_APMIX_ADDR(0x0274)
#define REG_VCODECPLL_CON1    REG_APMIX_ADDR(0x0294)
//****************************************************

static inline unsigned int uffs(unsigned int x)
{
    unsigned int r = 1;

    if (!x)
        return 0;
    if (!(x & 0xffff)) {
        x >>= 16;
        r += 16;
    }
    if (!(x & 0xff)) {
        x >>= 8;
        r += 8;
    }
    if (!(x & 0xf)) {
        x >>= 4;
        r += 4;
    }
    if (!(x & 3)) {
        x >>= 2;
        r += 2;
    }
    if (!(x & 1)) {
        x >>= 1;
        r += 1;
    }
    return r;
}
#define fh_read8(reg)           readb(reg)
#define fh_read16(reg)          readw(reg)
#define fh_read32(reg)          readl((void __iomem *)reg)
#define fh_write8(reg,val)      mt_reg_sync_writeb((val),(reg))
#define fh_write16(reg,val)     mt_reg_sync_writew((val),(reg))
#define fh_write32(reg,val)     mt_reg_sync_writel((val),(reg))

//#define fh_set_bits(reg,bs)     ((*(volatile u32*)(reg)) |= (u32)(bs))
//#define fh_clr_bits(reg,bs)     ((*(volatile u32*)(reg)) &= ~((u32)(bs)))

#define fh_set_field(reg,field,val) \
    do {	\
        volatile unsigned int tv = fh_read32(reg);	\
        tv &= ~(field); \
        tv |= ((val) << (uffs((unsigned int)field) - 1)); \
        fh_write32(reg,tv); \
    } while(0)

#define fh_get_field(reg,field,val) \
    do {	\
        volatile unsigned int tv = fh_read32(reg);	\
        val = ((tv & (field)) >> (uffs((unsigned int)field) - 1)); \
    } while(0)

#endif //#ifndef __MT_FHREG_H__







































