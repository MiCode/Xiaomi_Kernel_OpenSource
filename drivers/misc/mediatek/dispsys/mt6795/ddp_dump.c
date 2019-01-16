#define LOG_TAG "dump"

#include "ddp_reg.h"
#include "ddp_log.h"
#include "ddp_dump.h"
#include "ddp_ovl.h"
#include "ddp_wdma.h"
#include "ddp_rdma.h"

static char* ddp_signal_0(int bit)
{
    switch(bit)
    {
        case 31:  return "gamma0__gamma_mout";
        case 30:  return "path1_sel__path1_sout";
        case 29:  return "split0__path1_sel2";
        case 28:  return "split0__ufoe_sel1";
        case 27:  return "ufoe_sel__ufoe0";
        case 26:  return "path0_sout0__ufoe_sel0";
        case 25:  return "path0_sout1__split0";
        case 24:  return "aal_sel__aal0";
        case 23:  return "color0_sout1__merge0";
        case 22:  return "color0_sout0__aal_sel0";
        case 21:  return "color1_sout0__gamma0";
        case 20:  return "color1__color1_sout";
        case 19:  return "color1_sout1__merge0";
        case 18:  return "merge0__aal_sel1";
        case 17:  return "rdma2__rdma2_sout";
        case 16:  return "rdma2_sout1__dpi_sel2";
        case 15:  return "rdma2_sout0__dsi1_sel2";
        case 14:  return "dpi_sel__dpi0";
        case 13:  return "split1__dsi1_sel0";
        case 12:  return "dsi0_sel__dsi0";
        case 11:  return "ufoe0__ufoe_mout";
        case 10:  return "aal0__od";
        case 9 :  return "path1_sout0__dsi0_sel2";
        case 8 :  return "dsi1_sel__dsi1";
        case 7 :  return "ovl1_mout0__color1_sel1";
        case 6 :  return "color1_sel__color1";
        case 5 :  return "ovl1__ovl1_mout";
        case 4 :  return "ovl1_mout1__wdma1_sel0";
        case 3 :  return "color0_sel__color0";
        case 2 :  return "color0__color0_sout";
        case 1 :  return "ovl0__ovl0_mout";
        case 0 :  return "ovl0_mout1__wdma0_sel0";
        default:
            DDPERR("ddp_signal_0, unknown bit=%d \n", bit);
            return "unknown";
    }
}

static char* ddp_signal_1(int bit)
{
    switch(bit)
    {
        case 23:  return "od__od_mout";
        case 22:  return "ufoe_mout3__wdma0_sel2";
        case 21:  return "rdma1_sout0__path1_sel0";
        case 20:  return "gamma_mout1__path1_sel1";
        case 19:  return "gamma_mout0__rdma1";
        case 18:  return "wdma1_sel__wdma1";
        case 17:  return "gamma_mout2__wdma1_sel1";
        case 16:  return "rdma1_sout1__color1_sel0";
        case 15:  return "rdma1__rdma1_sout";
        case 14:  return "od_mout2__wdma0_sel1";
        case 13:  return "wdma0_sel__wdma0";
        case 12:  return "path0_sel__path0_sout";
        case 11:  return "rdma0_sout0__path0_sel0";
        case 10:  return "od_mout0__rdma0";
        case 9 :  return "od_mout1__path0_sel1";
        case 8 :  return "rdma0_sout1__color0_sel0";
        case 7 :  return "ovl0_mout0__color0_sel1";
        case 6 :  return "rdma0__rdma0_sout";
        case 5 :  return "path1_sout1__dsi1_sel1";
        case 4 :  return "path1_sout2__dpi_sel1";
        case 3 :  return "ufoe_mout2__dpi_sel0";
        case 2 :  return "ufoe_mout1__split1";
        case 1 :  return "ufoe_mout0__dsi0_sel0";
        case 0 :  return "split1__dsi0_sel1";
        default:
            DDPERR("ddp_signal_1, unknown bit=%d \n", bit);
            return "unknown";
    }
}

static char* ddp_get_mutex_module_name(unsigned int bit)
{
    switch(bit)
    {
       case 11: return "ovl0";
       case 12: return "ovl1";
       case 13: return "rdma0";
       case 14: return "rdma1";
       case 15: return "rdma2";
       case 16: return "wdma0";
       case 17: return "wdma1";
       case 18: return "color0";
       case 19: return "color1";
       case 20: return "aal";
       case 21: return "gamma";
       case 22: return "ufoe";
       case 23: return "pwm0";
       case 24: return "pwm1";
       case 25: return "od";
       default: return "mutex-unknown";
    }
}

char* ddp_mutex_sof_to_string(unsigned int sof)
{
   switch(sof)
   {
       case 0: return "single";
       case 1: return "dsi0_vdo";
       case 2: return "dsi1_vdo";
       case 3: return "dpi";
       default:
           DDPDUMP("ddp_mutex_sof_to_string, unknown sof=%u\n", sof);
           return "unknown";
   }
   return "unknown";
}

static char* ddp_clock_0(int bit)
{
    switch(bit)
    {
        case 0:   return "smi_common ";
        case 1:   return "smi_larb0 ";
        case 14:  return "fake_eng ";
        case 15:  return "mutex_32k ";
        case 16 : return "ovl0 ";
        case 17 : return "ovl1 ";
        case 18 : return "rdma0 ";
        case 19 : return "rdma1 ";
        case 20 : return "rdma2 ";
        case 21 : return "wdma0 ";
        case 22 : return "wdma1 ";
        case 23 : return "color0 ";
        case 24 : return "color1 ";
        case 25 : return "aal ";
        case 26 : return "gamma ";
        case 27 : return "ufoe ";
        case 28 : return "split0 ";
        case 29 : return "split1 ";
        case 30 : return "merge ";
        case 31 : return "od ";
        default : return " ";
    }
}

static char* ddp_clock_1(int bit)
{
    switch(bit)
    {
        case 0: return "pwm0_mm ";
        case 1: return "pwm0_26m ";
        case 2: return "pwm1_mm ";
        case 3: return "pwm1_26m ";
        case 4: return "dsi0_eng ";
        case 5: return "dsi0_dig ";
        case 6: return "dsi1_eng ";
        case 7: return "dsi1_dig ";
        case 8: return "dpi_pixel ";
        case 9: return "dpi_eng ";
        default : return " ";
    }
}

static void  mutex_dump_reg(void)
{
    DDPDUMP("==DISP MUTEX REGS==\n");
    DDPDUMP("MUTEX:0x000=0x%08x,0x004=0x%08x,0x020=0x%08x,0x028=0x%08x\n",
                      DISP_REG_GET(DISP_REG_CONFIG_MUTEX_INTEN),
                      DISP_REG_GET(DISP_REG_CONFIG_MUTEX_INTSTA),
                      DISP_REG_GET(DISP_REG_CONFIG_MUTEX0_EN),
                      DISP_REG_GET(DISP_REG_CONFIG_MUTEX0_RST));

    DDPDUMP("MUTEX:0x02c=0x%08x,0x030=0x%08x,0x040=0x%08x,0x048=0x%08x\n",
                      DISP_REG_GET(DISP_REG_CONFIG_MUTEX0_MOD),
                      DISP_REG_GET(DISP_REG_CONFIG_MUTEX0_SOF),
                      DISP_REG_GET(DISP_REG_CONFIG_MUTEX1_EN),
                      DISP_REG_GET(DISP_REG_CONFIG_MUTEX1_RST));

    DDPDUMP("MUTEX:0x04c=0x%08x,0x050=0x%08x,0x060=0x%08x,0x068=0x%08x\n",
                      DISP_REG_GET(DISP_REG_CONFIG_MUTEX1_MOD),
                      DISP_REG_GET(DISP_REG_CONFIG_MUTEX1_SOF),
                      DISP_REG_GET(DISP_REG_CONFIG_MUTEX2_EN),
                      DISP_REG_GET(DISP_REG_CONFIG_MUTEX2_RST));

    DDPDUMP("MUTEX:0x06c=0x%08x,0x070=0x%08x,0x080=0x%08x,0x088=0x%08x\n",
                      DISP_REG_GET(DISP_REG_CONFIG_MUTEX2_MOD),
                      DISP_REG_GET(DISP_REG_CONFIG_MUTEX2_SOF),
                      DISP_REG_GET(DISP_REG_CONFIG_MUTEX3_EN),
                      DISP_REG_GET(DISP_REG_CONFIG_MUTEX3_RST));

    DDPDUMP("MUTEX:0x08c=0x%08x,0x090=0x%08x,0x0a0=0x%08x,0x0a8=0x%08x\n",
                       DISP_REG_GET(DISP_REG_CONFIG_MUTEX3_MOD),
                       DISP_REG_GET(DISP_REG_CONFIG_MUTEX3_SOF),
                       DISP_REG_GET(DISP_REG_CONFIG_MUTEX4_EN),
                       DISP_REG_GET(DISP_REG_CONFIG_MUTEX4_RST));

    DDPDUMP("MUTEX:0x0ac=0x%08x,0x0b0=0x%08x,0x0c0=0x%08x,0x0c8=0x%08x\n",
                       DISP_REG_GET(DISP_REG_CONFIG_MUTEX4_MOD),
                       DISP_REG_GET(DISP_REG_CONFIG_MUTEX4_SOF),
                       DISP_REG_GET(DISP_REG_CONFIG_MUTEX5_EN),
                       DISP_REG_GET(DISP_REG_CONFIG_MUTEX5_RST));
    DDPDUMP("MUTEX:0x0cc=0x%08x,0x0d0=0x%08x,0x200=0x%08x\n",
                       DISP_REG_GET(DISP_REG_CONFIG_MUTEX5_MOD),
                       DISP_REG_GET(DISP_REG_CONFIG_MUTEX5_SOF),
                       DISP_REG_GET(DISP_REG_CONFIG_DEBUG_OUT_SEL));

    return ;
}

static void mutex_dump_analysis(void)
{
    int i=0;
    int j=0;
    char mutex_module[512]={'\0'};
    char * p = NULL;
    int len = 0;
    DDPDUMP("==DISP Mutex Analysis==\n");
    for (i = 0; i < 5; i++)
    {
        p = mutex_module;
        len = 0;
        if( DISP_REG_GET(DISP_REG_CONFIG_MUTEX_MOD(i))!=0 &&
           ((DISP_REG_GET(DISP_REG_CONFIG_MUTEX_EN(i)+0x20*i)==1 &&
            DISP_REG_GET(DISP_REG_CONFIG_MUTEX_SOF(i)+0x20*i)!=SOF_SINGLE ) ||
            DISP_REG_GET(DISP_REG_CONFIG_MUTEX_SOF(i)+0x20*i)==SOF_SINGLE))
        {
          len = sprintf(p,"MUTEX%d :mode=%s,module=(",
            i, ddp_mutex_sof_to_string( DISP_REG_GET(DISP_REG_CONFIG_MUTEX_SOF(i))));
          p += len;
          for(j=11;j<=25;j++)
          {
              if((DISP_REG_GET(DISP_REG_CONFIG_MUTEX_MOD(i))>>j)&0x1)
              {
                 len = sprintf(p,"%s,", ddp_get_mutex_module_name(j));
                 p += len;
              }
          }
          DDPDUMP("%s)\n",mutex_module);
        }
    }
    return;
}

static void  mmsys_config_dump_reg(void)
{
    DDPDUMP("==DISP MMSYS_CONFIG REGS==\n");
    DDPDUMP("MMSYS:0x000=0x%08x,0x004=0x%08x,0x040=0x%08x,0x044=0x%08x\n",
                     DISP_REG_GET(DISP_REG_CONFIG_MMSYS_INTEN),
                     DISP_REG_GET(DISP_REG_CONFIG_MMSYS_INTSTA),
                     DISP_REG_GET(DISP_REG_CONFIG_DISP_OVL0_MOUT_EN),
                     DISP_REG_GET(DISP_REG_CONFIG_DISP_OVL1_MOUT_EN));

    DDPDUMP("MMSYS:0x048=0x%08x,0x04C=0x%08x,0x050=0x%08x,0x054=0x%08x\n",
                     DISP_REG_GET(DISP_REG_CONFIG_DISP_OD_MOUT_EN),
                     DISP_REG_GET(DISP_REG_CONFIG_DISP_GAMMA_MOUT_EN),
                     DISP_REG_GET(DISP_REG_CONFIG_DISP_UFOE_MOUT_EN),
                     DISP_REG_GET(DISP_REG_CONFIG_MMSYS_MOUT_RST));

    DDPDUMP("MMSYS:0x084=0x%08x,0x088=0x%08x,0x08C=0x%08x,0x090=0x%08x\n",
                     DISP_REG_GET(DISP_REG_CONFIG_DISP_COLOR0_SEL_IN),
                     DISP_REG_GET(DISP_REG_CONFIG_DISP_COLOR1_SEL_IN),
                     DISP_REG_GET(DISP_REG_CONFIG_DISP_AAL_SEL_IN),
                     DISP_REG_GET(DISP_REG_CONFIG_DISP_PATH0_SEL_IN));

    DDPDUMP("MMSYS:0x094=0x%08x,0x098=0x%08x,0x09C=0x%08x,0x0A0=0x%08x\n",
                     DISP_REG_GET(DISP_REG_CONFIG_DISP_PATH1_SEL_IN),
                     DISP_REG_GET(DISP_REG_CONFIG_DISP_MODULE_WDMA0_SEL_IN),
                     DISP_REG_GET(DISP_REG_CONFIG_DISP_WDMA1_SEL_IN),
                     DISP_REG_GET(DISP_REG_CONFIG_DISP_UFOE_SEL_IN));

    DDPDUMP("MMSYS:0x0A4=0x%08x,0x0A8=0x%08x,0x0AC=0x%08x,0x0B0=0x%08x\n",
                     DISP_REG_GET(DISP_REG_CONFIG_DSI0_SEL_IN),
                     DISP_REG_GET(DISP_REG_CONFIG_DSI1_SEL_IN),
                     DISP_REG_GET(DISP_REG_CONFIG_DPI_SEL_IN),
                     DISP_REG_GET(DISP_REG_CONFIG_DISP_RDMA0_SOUT_SEL_IN));

    DDPDUMP("MMSYS:0x0B4=0x%08x,0x0B8=0x%08x,0x0BC=0x%08x,0x0C0=0x%08x\n",
                     DISP_REG_GET(DISP_REG_CONFIG_DISP_RDMA1_SOUT_SEL_IN),
                     DISP_REG_GET(DISP_REG_CONFIG_DISP_RDMA2_SOUT_SEL_IN),
                     DISP_REG_GET(DISP_REG_CONFIG_DISP_COLOR0_SOUT_SEL_IN),
                     DISP_REG_GET(DISP_REG_CONFIG_DISP_COLOR1_SOUT_SEL_IN));

    DDPDUMP("MMSYS:0x0C4=0x%08x,0x0C8=0x%08x,0x0F0=0x%08x,0x100=0x%08x\n",
                     DISP_REG_GET(DISP_REG_CONFIG_DISP_PATH0_SOUT_SEL_IN),
                     DISP_REG_GET(DISP_REG_CONFIG_DISP_PATH1_SOUT_SEL_IN),
                     DISP_REG_GET(DISP_REG_CONFIG_MMSYS_MISC),
                     DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON0));

    DDPDUMP("MMSYS:0x110=0x%08x,0x8d0=0x%08x,0x8b0=0x%08x,0x8b4=0x%08x\n",
                     DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON1),
                     DISP_REG_GET(DISP_REG_CONFIG_SMI_LARB0_GREQ),
                     DISP_REG_GET(DISPSYS_CONFIG_BASE+0x8b0),
                     DISP_REG_GET(DISPSYS_CONFIG_BASE+0x8b4));

    DDPDUMP("MMSYS:0x8b8=0x%08x,0x8bc=0x%08x,0x8c0=0x%08x,0x8c4=0x%08x\n",
                      DISP_REG_GET(DISPSYS_CONFIG_BASE+0x8b8),
                      DISP_REG_GET(DISPSYS_CONFIG_BASE+0x8bc),
                      DISP_REG_GET(DISPSYS_CONFIG_BASE+0x8c0),
                      DISP_REG_GET(DISPSYS_CONFIG_BASE+0x8c4));

    DDPDUMP("MMSYS:0x8c8=0x%08x,0x8cc=0x%08x,0x8d4=0x%08x\n",
                      DISP_REG_GET(DISPSYS_CONFIG_BASE+0x8c8),
                      DISP_REG_GET(DISPSYS_CONFIG_BASE+0x8cc),
                      DISP_REG_GET(DISPSYS_CONFIG_BASE+0x8d4));

    return ;
}


/* ------ clock:
Before power on mmsys:
CLK_CFG_0_CLR (address is 0x10000048) = 0x80000000 (bit 31).
Before using DISP_PWM0 or DISP_PWM1:
CLK_CFG_1_CLR(address is 0x10000058)=0x80 (bit 7).
Before using DPI pixel clock:
CLK_CFG_6_CLR(address is 0x100000A8)=0x80 (bit 7).

Only need to enable the corresponding bits of MMSYS_CG_CON0 and MMSYS_CG_CON1 for the modules:
smi_common, larb0, mdp_crop, fake_eng, mutex_32k, pwm0, pwm1, dsi0, dsi1, dpi.
Other bits could keep 1. Suggest to keep smi_common and larb0 always clock on.

--------valid & ready
example:
ovl0 -> ovl0_mout_ready=1 means engines after ovl_mout are ready for receiving data
        ovl0_mout_ready=0 means ovl0_mout can not receive data, maybe ovl0_mout or after engines config error
ovl0 -> ovl0_mout_valid=1 means engines before ovl0_mout is OK,
        ovl0_mout_valid=0 means ovl can not transfer data to ovl0_mout, means ovl0 or before engines are not ready.
*/

static void  mmsys_config_dump_analysis(void)
{
    unsigned int i = 0;
    unsigned int reg = 0;
    char clock_on[512]={'\0'};
    char * pos = NULL;
    int len = 0;
    unsigned int valid0 = DISP_REG_GET(DISPSYS_CONFIG_BASE + 0x8b0);
    unsigned int valid1 = DISP_REG_GET(DISPSYS_CONFIG_BASE + 0x8b4);
    unsigned int ready0 = DISP_REG_GET(DISPSYS_CONFIG_BASE + 0x8b8);
    unsigned int ready1 = DISP_REG_GET(DISPSYS_CONFIG_BASE + 0x8bc);
    DDPDUMP("==DISP MMSYS_CONFIG ANALYSIS==\n");
#if 0
	DDPDUMP("mmsys clock=0x%x \n", DISP_REG_GET(DISP_REG_CLK_CFG_0_MM_CLK));
    if((DISP_REG_GET(DISP_REG_CLK_CFG_0_MM_CLK)>>31)&0x1)
    {
        DDPERR("mmsys clock abnormal!!\n");
    }
    DDPDUMP("PLL clock=0x%x\n", DISP_REG_GET(DISP_REG_VENCPLL_CON0));
    if(!(DISP_REG_GET(DISP_REG_VENCPLL_CON0)&0x1))
    {
        DDPERR("PLL clock abnormal!!\n");
    }
    reg = DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON0);

    for(i=0;i<2;i++)
    {
        if((reg&(1<<i))==0)
           len += scnprintf(clock_on+len, sizeof(clock_on) - len, "%s ", ddp_clock_0(i));
    }
    for(i=14;i<32;i++)
    {
        if((reg&(1<<i))==0)
           len += scnprintf(clock_on+len, sizeof(clock_on) - len, "%s ", ddp_clock_0(i));
    }
    reg = DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON1);
    for(i=0;i<9;i++)
    {
        if((reg&(1<<i))==0)
            len += scnprintf(clock_on+len, sizeof(clock_on) - len, "%s ", ddp_clock_1(i));
    }
    DDPDUMP("clock on modules:%s\n",clock_on);
    DDPDUMP("clock1 setting:0x%x,0x%x\n",
        DISP_REG_GET(DISP_REG_CONFIG_C09),DISP_REG_GET(DISP_REG_CONFIG_C10));
#endif

    DDPDUMP("valid0=0x%x, valid1=0x%x, ready0=0x%x, ready1=0x%x\n",
        valid0,valid1,ready0,ready1);
    for (i = 0; i < 32; i++)
    {
        pos = clock_on;
        if ((valid0 & (1 << i)) == 0)
        {
            len = sprintf(pos,"%-26s:Not Valid,",ddp_signal_0(i));
        }
        else
        {
            len = sprintf(pos,"%-26s:Valid,",ddp_signal_0(i));
        }
        pos+=len;
        if ((ready0 & (1 << i)) == 0)
        {
            len=sprintf(pos,"Not Ready");
        }
        else
        {
            len=sprintf(pos,"Ready");
        }
        DDPDUMP("%s\n",clock_on);
    }
    for (i = 0; i < 24; i++)
    {
        pos = clock_on;
        if ((valid1 & (1 << i)) == 0)
        {
            len = sprintf(pos,"%-26s:Not Valid,",ddp_signal_1(i));
        }
        else
        {
            len = sprintf(pos,"%-26s:    Valid,",ddp_signal_1(i));
        }
        pos+=len;
        if ((ready1 & (1 << i)) == 0)
        {
			len=sprintf(pos,"Not Ready");
        }
        else
        {
            len=sprintf(pos,"    Ready");
        }
        DDPDUMP("%s\n",clock_on);
    }
}

static void gamma_dump_reg(void)
{
    DDPDUMP("==DISP GAMMA REGS==\n");
    DDPDUMP("GAMMA:0x000=0x%08x,0x004=0x%08x,0x008=0x%08x,0x00c=0x%08x\n",
                      DISP_REG_GET(DISP_REG_GAMMA_EN),
                      DISP_REG_GET(DISP_REG_GAMMA_RESET),
                      DISP_REG_GET(DISP_REG_GAMMA_INTEN),
                      DISP_REG_GET(DISP_REG_GAMMA_INTSTA));

    DDPDUMP("GAMMA:0x010=0x%08x,0x020=0x%08x,0x024=0x%08x,0x028=0x%08x\n",
                      DISP_REG_GET(DISP_REG_GAMMA_STATUS),
                      DISP_REG_GET(DISP_REG_GAMMA_CFG),
                      DISP_REG_GET(DISP_REG_GAMMA_INPUT_COUNT),
                      DISP_REG_GET(DISP_REG_GAMMA_OUTPUT_COUNT));

    DDPDUMP("GAMMA:0x02c=0x%08x,0x030=0x%08x,0x0c0=0x%08x,0x800=0x%08x\n",
                      DISP_REG_GET(DISP_REG_GAMMA_CHKSUM),
                      DISP_REG_GET(DISP_REG_GAMMA_SIZE),
                      DISP_REG_GET(DISP_REG_GAMMA_DUMMY_REG),
                      DISP_REG_GET(DISP_REG_GAMMA_LUT));
    return ;
}

static void gamma_dump_analysis(void)
{
    DDPDUMP("==DISP GAMMA ANALYSIS==\n");
    DDPDUMP("gamma: en=%d, w=%d, h=%d, in_p=%d, in_l=%d, out_p=%d, out_l=%d\n",
        DISP_REG_GET(DISP_REG_GAMMA_EN),
        (DISP_REG_GET(DISP_REG_GAMMA_SIZE)>>16)&0x1fff,
        DISP_REG_GET(DISP_REG_GAMMA_SIZE)&0x1fff,
        DISP_REG_GET(DISP_REG_GAMMA_INPUT_COUNT)&0x1fff,
        (DISP_REG_GET(DISP_REG_GAMMA_INPUT_COUNT)>>16)&0x1fff,
        DISP_REG_GET(DISP_REG_GAMMA_OUTPUT_COUNT)&0x1fff,
        (DISP_REG_GET(DISP_REG_GAMMA_OUTPUT_COUNT)>>16)&0x1fff
    );
    return;
}

static void merge_dump_reg(void)
{
    DDPDUMP("==DISP MERGE REGS==\n");
    DDPDUMP("MERGE:0x000=0x%08x,0x004=0x%08x,0x008=0x%08x\n",
                      DISP_REG_GET(DISP_REG_MERGE_ENABLE),
                      DISP_REG_GET(DISP_REG_MERGE_SW_RESET),
                      DISP_REG_GET(DISP_REG_MERGE_DEBUG));
    return ;
}

static void merge_dump_analysis(void)
{
    DDPDUMP("==DISP MERGE ANALYSIS==\n");
    DDPDUMP("merge: en=%d, debug=0x%x \n",
        DISP_REG_GET(DISP_REG_MERGE_ENABLE),
        DISP_REG_GET(DISP_REG_MERGE_DEBUG));
    return ;
}

static void split_dump_reg(DISP_MODULE_ENUM module)
{
    int index =0;
    if(DISP_MODULE_SPLIT0==module)
    {
        index = 0;
    }
    else if(DISP_MODULE_SPLIT1==module)
    {
        index = 1;
    }
    DDPDUMP("==DISP SPLIT%d REGS==\n", index);
    DDPDUMP("SPLIT:0x000=0x%08x,0x004=0x%08x,0x008=0x%08x\n",
		DISP_REG_GET(DISP_REG_SPLIT_ENABLE + DISP_SPLIT_INDEX_OFFSET* index),
		DISP_REG_GET(DISP_REG_SPLIT_SW_RESET + DISP_SPLIT_INDEX_OFFSET * index),
		DISP_REG_GET(DISP_REG_SPLIT_DEBUG + DISP_SPLIT_INDEX_OFFSET * index));
    return ;
}

static void split_dump_analysis(DISP_MODULE_ENUM module)
{
    int index =0;
    if(DISP_MODULE_SPLIT0==module)
    {
        index = 0;
    }
    else if(DISP_MODULE_SPLIT1==module)
    {
        index = 1;
    }
    DDPDUMP("==DISP SPLIT%d ANALYSIS==\n", index);
    DDPDUMP("split%d, en=%d, debug=0x%x\n",
        index,
		DISP_REG_GET(DISP_REG_SPLIT_ENABLE + DISP_SPLIT_INDEX_OFFSET * index),
		DISP_REG_GET(DISP_REG_SPLIT_DEBUG + DISP_SPLIT_INDEX_OFFSET * index)
    );
    return ;
}

static void color_dump_reg(DISP_MODULE_ENUM module)
{
    int index =0;
	if (DISP_MODULE_COLOR0 == module) {
        index = 0;
	} else if (DISP_MODULE_COLOR1 == module) {
        index = 1;
    }
    DDPDUMP("==DISP COLOR%d REGS==\n", index);
    DDPDUMP("COLOR:0x400=0x%08x,0x404=0x%08x,0x408=0x%08x,0xc00=0x%08x\n",
		DISP_REG_GET(DISP_COLOR_CFG_MAIN + DISP_COLOR_INDEX_OFFSET * index),
		DISP_REG_GET(DISP_COLOR_PXL_MAIN + DISP_COLOR_INDEX_OFFSET* index),
		DISP_REG_GET(DISP_COLOR_LNE_MAIN + DISP_COLOR_INDEX_OFFSET * index),
		DISP_REG_GET(DISP_COLOR_START + DISP_COLOR_INDEX_OFFSET * index));

    DDPDUMP("COLOR:0xc50=0x%08x,0xc54=0x%08x\n",
		DISP_REG_GET(DISP_COLOR_INTERNAL_IP_WIDTH + DISP_COLOR_INDEX_OFFSET * index),
		DISP_REG_GET(DISP_COLOR_INTERNAL_IP_HEIGHT + DISP_COLOR_INDEX_OFFSET * index));

    return ;
}

static void color_dump_analysis(DISP_MODULE_ENUM module)
{
    int index =0;
	if (DISP_MODULE_COLOR0 == module) {
        index = 0;
	} else if (DISP_MODULE_COLOR1 == module) {
        index = 1;
    }
    DDPDUMP("==DISP COLOR%d ANALYSIS==\n", index);
    DDPDUMP("color%d: bypass=%d, w=%d, h=%d\n",
        index,
		(DISP_REG_GET(DISP_COLOR_CFG_MAIN + DISP_COLOR_INDEX_OFFSET * index) >> 7) & 0x1,
		DISP_REG_GET(DISP_COLOR_INTERNAL_IP_WIDTH + DISP_COLOR_INDEX_OFFSET * index),
		DISP_REG_GET(DISP_COLOR_INTERNAL_IP_HEIGHT + DISP_COLOR_INDEX_OFFSET * index)
    );

    return ;
}

static void aal_dump_reg(void)
{
    DDPDUMP("==DISP AAL REGS==\n");
    DDPDUMP("AAL:0x000=0x%08x,0x004=0x%08x,0x008=0x%08x,0x00c=0x%08x\n",
                      DISP_REG_GET(DISP_AAL_EN),
                      DISP_REG_GET(DISP_AAL_RST),
                      DISP_REG_GET(DISP_AAL_INTEN),
                      DISP_REG_GET(DISP_AAL_INTSTA));

    DDPDUMP("AAL:0x010=0x%08x,0x020=0x%08x,0x024=0x%08x,0x028=0x%08x\n",
                      DISP_REG_GET(DISP_AAL_STATUS),
                      DISP_REG_GET(DISP_AAL_CFG),
                      DISP_REG_GET(DISP_AAL_IPUT),
                      DISP_REG_GET(DISP_AAL_OPUT));

    DDPDUMP("AAL:0x030=0x%08x,0x20c=0x%08x,0x214=0x%08x,0x20c=0x%08x\n",
                      DISP_REG_GET(DISP_AAL_SIZE),
                      DISP_REG_GET(DISP_AAL_CABC_00),
                      DISP_REG_GET(DISP_AAL_CABC_02),
                      DISP_REG_GET(DISP_AAL_STATUS_00));

    DDPDUMP("AAL:0x210=0x%08x,0x2a0=0x%08x,0x2a4=0x%08x,0x354=0x%08x\n",
                      DISP_REG_GET(DISP_AAL_STATUS_00 + 0x4),
                      DISP_REG_GET(DISP_AAL_STATUS_32 - 0x4),
                      DISP_REG_GET(DISP_AAL_STATUS_32),
                      DISP_REG_GET(DISP_AAL_DRE_GAIN_FILTER_00));

    DDPDUMP("AAL:0x3b0=0x%x\n", DISP_REG_GET(DISP_AAL_DRE_MAPPING_00));
    return ;
}

static void aal_dump_analysis(void)
{
    DDPDUMP("==DISP AAL ANALYSIS==\n");
    DDPDUMP("aal: en=%d, relay=%d, w=%d, h=%d\n",
        DISP_REG_GET(DISP_AAL_EN)&0x1,
        DISP_REG_GET(DISP_AAL_CFG)&0x01,
        (DISP_REG_GET(DISP_AAL_SIZE)>>16)&0x1fff,
        DISP_REG_GET(DISP_AAL_SIZE)&0x1fff
    );
}

static void pwm_dump_reg(DISP_MODULE_ENUM module)
{
    int index = 0;
	unsigned long reg_base = 0;
    if (module == DISP_MODULE_PWM0) {
         index = 0;
         reg_base = DISPSYS_PWM0_BASE;
     } else {
         index = 1;
         reg_base = DISPSYS_PWM1_BASE;
     }
     DDPDUMP("==DISP PWM%d REGS==\n", index);
     DDPDUMP("PWM:0x000=0x%08x,0x010=0x%08x,0x014=0x%08x,0x028=0x%08x\n",
                      DISP_REG_GET(reg_base + DISP_PWM_EN_OFF),
                      DISP_REG_GET(reg_base + DISP_PWM_CON_0_OFF),
                      DISP_REG_GET(reg_base + DISP_PWM_CON_1_OFF),
                      DISP_REG_GET(reg_base + 0x28));
    return ;
}

static void pwm_dump_analysis(DISP_MODULE_ENUM module)
{
    int index = 0;
	unsigned long reg_base = 0;
    if (module == DISP_MODULE_PWM0) {
         index = 0;
         reg_base = DISPSYS_PWM0_BASE;
     } else {
         index = 1;
         reg_base = DISPSYS_PWM1_BASE;
     }
     DDPDUMP("==DISP PWM%d ANALYSIS==\n", index);
 	// will KE, please fix it later
     //DDPDUMP("pwm clock=%d \n", (DISP_REG_GET(DISP_REG_CLK_CFG_1_CLR)>>7)&0x1);

    return;
}

static void od_dump_reg(void)
{
    DDPDUMP("==DISP OD REGS==\n");
    DDPDUMP("OD:0x000=0x%08x,0x004=0x%08x,0x008=0x%08x,0x00c=0x%08x\n",
                      DISP_REG_GET(DISP_REG_OD_EN),
                      DISP_REG_GET(DISP_REG_OD_RESET),
                      DISP_REG_GET(DISP_REG_OD_INTEN),
                      DISP_REG_GET(DISP_REG_OD_INTSTA));

    DDPDUMP("OD:0x010=0x%08x,0x020=0x%08x,0x024=0x%08x,0x028=0x%08x\n",
                      DISP_REG_GET(DISP_REG_OD_STATUS),
                      DISP_REG_GET(DISP_REG_OD_CFG),
                      DISP_REG_GET(DISP_REG_OD_INPUT_COUNT),
                      DISP_REG_GET(DISP_REG_OD_OUTPUT_COUNT));

    DDPDUMP("OD:0x02c=0x%08x,0x030=0x%08x,0x040=0x%08x,0x044=0x%08x\n",
                      DISP_REG_GET(DISP_REG_OD_CHKSUM),
                      DISP_REG_GET(DISP_REG_OD_SIZE),
                      DISP_REG_GET(DISP_REG_OD_HSYNC_WIDTH),
                      DISP_REG_GET(DISP_REG_OD_VSYNC_WIDTH));

    DDPDUMP("OD:0x048=0x%08x,0x0C0=0x%08x\n",
                      DISP_REG_GET(DISP_REG_OD_MISC),
                      DISP_REG_GET(DISP_REG_OD_DUMMY_REG));

    DDPDUMP("OD:0x684=0x%08x,0x688=0x%08x,0x68c=0x%08x,0x690=0x%08x\n",
                      DISP_REG_GET(DISPSYS_OD_BASE + 0x684),
                      DISP_REG_GET(DISPSYS_OD_BASE + 0x688),
                      DISP_REG_GET(DISPSYS_OD_BASE + 0x68c),
                      DISP_REG_GET(DISPSYS_OD_BASE + 0x690));

    DDPDUMP("OD:0x694=0x%08x,0x698=0x%08x,0x700=0x%08x,0x704=0x%08x\n",
                      DISP_REG_GET(DISPSYS_OD_BASE + 0x694),
                      DISP_REG_GET(DISPSYS_OD_BASE + 0x698),
                      DISP_REG_GET(DISPSYS_OD_BASE + 0x700),
                      DISP_REG_GET(DISPSYS_OD_BASE + 0x704));

    DDPDUMP("OD:0x708=0x%08x,0x778=0x%08x,0x78c=0x%08x,0x790=0x%08x\n",
                      DISP_REG_GET(DISPSYS_OD_BASE + 0x708),
                      DISP_REG_GET(DISPSYS_OD_BASE + 0x778),
                      DISP_REG_GET(DISPSYS_OD_BASE + 0x78c),
                      DISP_REG_GET(DISPSYS_OD_BASE + 0x790));
                      
    DDPDUMP("OD:0x7a0=0x%08x,0x7dc=0x%08x,0x7e8=0x%08x\n",
                      DISP_REG_GET(DISPSYS_OD_BASE + 0x7a0),
                      DISP_REG_GET(DISPSYS_OD_BASE + 0x7dc),
                      DISP_REG_GET(DISPSYS_OD_BASE + 0x7e8));
    return;
}

static void od_dump_analysis(void)
{
    DDPDUMP("==DISP OD ANALYSIS==\n");
	unsigned int cfg_val = DISP_REG_GET(DISP_REG_OD_CFG);
	unsigned int status = DISP_REG_GET(DISP_REG_OD_STATUS);
    DDPDUMP("od: w=%d, h=%d, en%d, in(%d,%d), out(%d,%d)\n"
			"od: relay%d,core_en%d,dither%d,checksum%d\n",
        (DISP_REG_GET(DISP_REG_OD_SIZE)>>16)&0xffff,
        DISP_REG_GET(DISP_REG_OD_SIZE)&0xffff,
        DISP_REG_GET(DISP_REG_OD_EN),
        DISP_REG_GET_FIELD(OD_INPUT_COUNT_FLD_INP_PIX_CNT, DISP_REG_OD_INPUT_COUNT),
        DISP_REG_GET_FIELD(OD_INPUT_COUNT_FLD_INP_LINE_CNT, DISP_REG_OD_INPUT_COUNT),
        DISP_REG_GET_FIELD(OD_OUTPUT_COUNT_FLD_OUTP_PIX_CNT, DISP_REG_OD_OUTPUT_COUNT),
        DISP_REG_GET_FIELD(OD_OUTPUT_COUNT_FLD_OUTP_LINE_CNT, DISP_REG_OD_OUTPUT_COUNT),
        !!cfg_val&(1<<0), !!cfg_val&(1<<1), 
        !!cfg_val&(1<<2), !!cfg_val&(1<<4));
	
    DDPDUMP("in(valid%d,ready%d) out(valid%d,ready%d)\n",
        !!status&(1<<3), !!status&(1<<2),
        !!status&(1<<1), !!status&(1<<0));

    return;
}

static void ufoe_dump_reg(void)
{
    DDPDUMP("==DISP UFOE REGS==\n");
    DDPDUMP("(0x000)UFOE_START =0x%x\n", DISP_REG_GET(DISP_REG_UFO_START));
    return;
}

static void ufoe_dump_analysis(void)
{
    DDPDUMP("==DISP UFOE ANALYSIS==\n");
    DDPDUMP("ufoe: bypass=%d,0x%08x\n",
        DISP_REG_GET(DISP_REG_UFO_START)==0x4,
        DISP_REG_GET(DISP_REG_UFO_START));
    return;
}

static void dsi_dump_reg(DISP_MODULE_ENUM module)
{
    int i =0;
    if(DISP_MODULE_DSI0==module)
    {
        DDPDUMP("==DISP DSI0 REGS==\n");
        for (i = 0; i < 20*16; i += 16)
        {
            printk("DSI0+%04x : 0x%08x  0x%08x  0x%08x  0x%08x\n", i,
				DISP_REG_GET(DDP_REG_BASE_DSI0 + i), DISP_REG_GET(DDP_REG_BASE_DSI0 + i + 0x4),
				DISP_REG_GET(DDP_REG_BASE_DSI0 + i + 0x8), DISP_REG_GET(DDP_REG_BASE_DSI0 + i + 0xc));
        }
    }
    else if(DISP_MODULE_DSI1==module)
    {
        DDPDUMP("==DISP DSI1 REGS==\n");
        for (i = 0; i < 20*16; i += 16)
        {
            printk("DSI1+%04x : 0x%08x  0x%08x  0x%08x  0x%08x\n", i, 
				DISP_REG_GET(DDP_REG_BASE_DSI1 + i), DISP_REG_GET(DDP_REG_BASE_DSI1 + i + 0x4),
				DISP_REG_GET(DDP_REG_BASE_DSI1 + i + 0x8), DISP_REG_GET(DDP_REG_BASE_DSI1 + i + 0xc));
        }
    }
    else 
    {
        DDPDUMP("==DISP DSIDUAL REGS==\n");
        for (i = 0; i < 20*16; i += 16)
        {
            printk("DSI0+%04x : 0x%08x  0x%08x  0x%08x  0x%08x\n", i,
				DISP_REG_GET(DDP_REG_BASE_DSI0 + i), DISP_REG_GET(DDP_REG_BASE_DSI0 + i + 0x4),
				DISP_REG_GET(DDP_REG_BASE_DSI0 + i + 0x8), DISP_REG_GET(DDP_REG_BASE_DSI0 + i + 0xc));
        }
        for (i = 0; i < 20*16; i += 16)
        {
            printk("DSI1+%04x : 0x%08x  0x%08x  0x%08x  0x%08x\n", i,
				DISP_REG_GET(DDP_REG_BASE_DSI1 + i), DISP_REG_GET(DDP_REG_BASE_DSI1 + i + 0x4),
				DISP_REG_GET(DDP_REG_BASE_DSI1 + i + 0x8), DISP_REG_GET(DDP_REG_BASE_DSI1 + i + 0xc));
        }
    }
    return ;
}

static void dpi_dump_analysis(void)
{
    DDPDUMP("==DISP DPI ANALYSIS==\n");
    DDPDUMP("DPI clock=0x%x \n", DISP_REG_GET(DISP_REG_CLK_CFG_6_DPI));
    if((DISP_REG_GET(DISP_REG_VENCPLL_CON0)>>7)&0x1)
    {
        DDPDUMP("DPI clock abnormal!!\n");
    }
    DDPDUMP("DPI  clock_clear=%d \n", (DISP_REG_GET(DISP_REG_CLK_CFG_6_CLR)>>7)&0x1);
    return;
}

int ddp_dump_reg(DISP_MODULE_ENUM module)
{
    switch(module)
    {
        case DISP_MODULE_WDMA0:
        case DISP_MODULE_WDMA1:
            wdma_dump_reg(module);
            break;
        case DISP_MODULE_RDMA0:
        case DISP_MODULE_RDMA1:
        case DISP_MODULE_RDMA2:
            rdma_dump_reg(module);
            break;
        case DISP_MODULE_OVL0:
        case DISP_MODULE_OVL1:
            ovl_dump_reg(module);
            break;
        case DISP_MODULE_GAMMA:
            gamma_dump_reg();
            break;
        case DISP_MODULE_CONFIG:
            mmsys_config_dump_reg();
            break;
        case DISP_MODULE_MUTEX:
            mutex_dump_reg();
            break;
        case DISP_MODULE_MERGE:
            merge_dump_reg();
            break;
        case DISP_MODULE_SPLIT0:
        case DISP_MODULE_SPLIT1:
            split_dump_reg(module);
            break;
        case DISP_MODULE_COLOR0:
        case DISP_MODULE_COLOR1:
            color_dump_reg(module);
            break;
        case DISP_MODULE_AAL:
            aal_dump_reg();
            break;
        case DISP_MODULE_PWM0:
        case DISP_MODULE_PWM1:
            pwm_dump_reg(module);
            break;
        case DISP_MODULE_UFOE:
            ufoe_dump_reg();
            break;
        case DISP_MODULE_OD:
            od_dump_reg();
            break;
        case DISP_MODULE_DSI0:
        case DISP_MODULE_DSI1:
            dsi_dump_reg(module);
            break;
        case DISP_MODULE_DPI:
            break;
        default:
            DDPDUMP("DDP error, dump_reg unknow module=%d\n", module);
    }
    return 0;
}
int ddp_dump_analysis(DISP_MODULE_ENUM module)
{
    switch(module)
    {
        case DISP_MODULE_WDMA0:
        case DISP_MODULE_WDMA1:
            wdma_dump_analysis(module);
            break;
        case DISP_MODULE_RDMA0:
        case DISP_MODULE_RDMA1:
        case DISP_MODULE_RDMA2:
            rdma_dump_analysis(module);
            break;
        case DISP_MODULE_OVL0:
        case DISP_MODULE_OVL1:
            ovl_dump_analysis(module);
            break;
        case DISP_MODULE_GAMMA:
            gamma_dump_analysis();
            break;
        case DISP_MODULE_CONFIG:
            mmsys_config_dump_analysis();
            break;
        case DISP_MODULE_MUTEX:
            mutex_dump_analysis();
            break;
        case DISP_MODULE_MERGE:
            merge_dump_analysis();
            break;
        case DISP_MODULE_SPLIT0:
        case DISP_MODULE_SPLIT1:
            split_dump_analysis(module);
            break;
        case DISP_MODULE_COLOR0:
        case DISP_MODULE_COLOR1:
            color_dump_analysis(module);
            break;
        case DISP_MODULE_AAL:
            aal_dump_analysis();
            break;
        case DISP_MODULE_UFOE:
            ufoe_dump_analysis();
            break;
        case DISP_MODULE_OD:
            od_dump_analysis();
            break;
        case DISP_MODULE_PWM0:
        case DISP_MODULE_PWM1:
            pwm_dump_analysis(module);
            break;
        case DISP_MODULE_DSI0:
        case DISP_MODULE_DSI1:
	    case DISP_MODULE_DSIDUAL:
		    break;
        case DISP_MODULE_DPI:
            dpi_dump_analysis();
            break;
        default:
            DDPDUMP("DDP error, dump_analysis unknow module=%d \n", module);
    }
    return 0;
}
