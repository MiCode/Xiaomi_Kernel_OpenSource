#include <mach/mt_irq.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <mach/mt_clkmgr.h>

#include "m4u_priv.h"
#include "m4u_hw.h"

#include <linux/of.h>
#include <linux/of_address.h>

static m4u_domain_t gM4uDomain;

extern struct m4u_device *gM4uDev;
static unsigned long gM4UBaseAddr[TOTAL_M4U_NUM];
static unsigned long gLarbBaseAddr[SMI_LARB_NR];
static unsigned long gSMICOMMONBaseAddr;
static unsigned long gPericfgBaseAddr;
static unsigned int gM4UTagCount[]  = {64, 32};

static const char* gM4U_SMILARB[] = {
	"mediatek,SMI_LARB0", "mediatek,SMI_LARB1", "mediatek,SMI_LARB2", "mediatek,SMI_LARB3", "mediatek,SMI_LARB5"};


static M4U_RANGE_DES_T gM4u0_seq[M4U0_SEQ_NR] = {{0}};
static M4U_RANGE_DES_T gM4u1_seq[M4U1_SEQ_NR] = {{0}};
static M4U_RANGE_DES_T *gM4USeq[] = {gM4u0_seq, gM4u1_seq};
static M4U_MAU_STATUS_T gM4u0_mau[MAU_NR_PER_M4U_SLAVE] = {{0}};
static unsigned int gMAU_candidate_id = MAU_NR_PER_M4U_SLAVE - 1;

static DEFINE_MUTEX(gM4u_seq_mutex);

#define TF_PROTECT_BUFFER_SIZE 128L

int gM4U_L2_enable = 1;
int gM4U_4G_DRAM_Mode = 0;


static spinlock_t gM4u_reg_lock;
int gM4u_port_num = M4U_PORT_UNKNOWN;



#define M4U0_PORT_INIT(slave, larb, port)  0,slave,larb,port,(((larb)<<7)|((port)<<2)), 1
#define M4U1_PORT_INIT(port,tf_id)   1,0,5,port,tf_id, 1

m4u_port_t gM4uPort[] = 
{
    { "DISP_OVL0",                  M4U0_PORT_INIT( 1, 0,  0    ),    },
    { "DISP_RDMA0",                 M4U0_PORT_INIT( 1, 0,  1    ),    },
    { "DISP_RDMA1",                 M4U0_PORT_INIT( 1, 0,  2    ),    },
    { "DISP_WDMA0",                 M4U0_PORT_INIT( 1, 0,  3    ),    },
    { "DISP_OVL1",                  M4U0_PORT_INIT( 1, 0,  4    ),    },
    { "DISP_RDMA2",                 M4U0_PORT_INIT( 1, 0,  5    ),    },
    { "DISP_WDMA1",                 M4U0_PORT_INIT( 1, 0,  6    ),    },
    { "DISP_OD_R",                  M4U0_PORT_INIT( 1, 0,  7    ),    },
    { "DISP_OD_W",                  M4U0_PORT_INIT( 1, 0,  8    ),    },
    { "MDP_RDMA0",                  M4U0_PORT_INIT( 1, 0,  9    ),    },
    { "MDP_RDMA1",                  M4U0_PORT_INIT( 1, 0,  10    ),    },
    { "MDP_WDMA",                   M4U0_PORT_INIT( 1, 0,  11    ),    },
    { "MDP_WROT0",                  M4U0_PORT_INIT( 1, 0,  12   ),    },
    { "MDP_WROT1",                  M4U0_PORT_INIT( 1, 0,  13   ),    },
                                  
    { "VDEC_MC",                    M4U0_PORT_INIT( 0, 1,  0    ),    },
    { "VDEC_PP",                    M4U0_PORT_INIT( 0, 1,  1    ),    },
    { "VDEC_UFO",                   M4U0_PORT_INIT( 0, 1,  2    ),    },
    { "VDEC_VLD",                   M4U0_PORT_INIT( 0, 1,  3    ),    },
    { "VDEC_VLD2",                  M4U0_PORT_INIT( 0, 1,  4    ),    },
    { "VDEC_AVC_MV",                M4U0_PORT_INIT( 0, 1,  5    ),    },
    { "VDEC_PRED_RD",               M4U0_PORT_INIT( 0, 1,  6    ),    },
    { "VDEC_PRED_WR",               M4U0_PORT_INIT( 0, 1,  7    ),    },
    { "VDEC_PPWRAP",                M4U0_PORT_INIT( 0, 1,  8    ),    },
                                  
    { "CAM_IMGO",                   M4U0_PORT_INIT( 0, 2,  0    ),    },
    { "CAM_RRZO",                   M4U0_PORT_INIT( 0, 2,  1    ),    },
    { "CAM_AAO",                    M4U0_PORT_INIT( 0, 2,  2    ),    },
    { "CAM_LCSO",                   M4U0_PORT_INIT( 0, 2,  3    ),    },
    { "CAM_ESFKO",                  M4U0_PORT_INIT( 0, 2,  4    ),    },
    { "CAM_IMGO_S",                 M4U0_PORT_INIT( 0, 2,  5    ),    },
    { "CAM_LSCI",                   M4U0_PORT_INIT( 0, 2,  6    ),    },
    { "CAM_LSCI_D",                 M4U0_PORT_INIT( 0, 2,  7    ),    },
    { "CAM_BPCI",                   M4U0_PORT_INIT( 0, 2,  8    ),    },
    { "CAM_BPCI_D",                 M4U0_PORT_INIT( 0, 2,  9    ),    },
    { "CAM_UFDI",                   M4U0_PORT_INIT( 0, 2,  10   ),    },
    { "CAM_IMGI",                   M4U0_PORT_INIT( 0, 2,  11   ),    },
    { "CAM_IMG2O",                  M4U0_PORT_INIT( 0, 2,  12   ),    },
    { "CAM_IMG3O",                  M4U0_PORT_INIT( 0, 2,  13   ),    },
    { "CAM_VIPI",                   M4U0_PORT_INIT( 0, 2,  14   ),    },
    { "CAM_VIP2I",                  M4U0_PORT_INIT( 0, 2,  15   ),    },
    { "CAM_VIP3I",                  M4U0_PORT_INIT( 0, 2,  16   ),    },
    { "CAM_LCEI",                   M4U0_PORT_INIT( 0, 2,  17   ),    },
    { "CAM_RB",                     M4U0_PORT_INIT( 0, 2,  18   ),    },
    { "CAM_RP",                     M4U0_PORT_INIT( 0, 2,  19   ),    },
    { "CAM_WR",                     M4U0_PORT_INIT( 0, 2,  20   ),    },
                                    
    { "VENC_RCPU",                  M4U0_PORT_INIT( 0, 3,  0    ),    },
    { "VENC_REC",                   M4U0_PORT_INIT( 0, 3,  1    ),    },
    { "VENC_BSDMA",                 M4U0_PORT_INIT( 0, 3,  2    ),    },
    { "VENC_SV_COMV",               M4U0_PORT_INIT( 0, 3,  3    ),    },
    { "VENC_RD_COMV",               M4U0_PORT_INIT( 0, 3,  4    ),    },
    { "JPGENC_BSDMA",               M4U0_PORT_INIT( 0, 3,  5    ),    },
    { "REMDC_SDMA",                 M4U0_PORT_INIT( 0, 3,  6    ),    },
    { "REMDC_BSDMA",                M4U0_PORT_INIT( 0, 3,  7    ),    },
    { "JPGENC_RDMA",                M4U0_PORT_INIT( 0, 3,  8    ),    },
    { "JPGENC_SDMA",                M4U0_PORT_INIT( 0, 3,  9    ),    },
    { "JPGDEC_WDMA",                M4U0_PORT_INIT( 0, 3,  10   ),    },
    { "JPGDEC_BSDMA",               M4U0_PORT_INIT( 0, 3,  11   ),    },
    { "VENC_CUR_LUMA",              M4U0_PORT_INIT( 0, 3,  12   ),    },
    { "VENC_CUR_CHROMA",            M4U0_PORT_INIT( 0, 3,  13   ),    },
    { "VENC_REF_LUMA",              M4U0_PORT_INIT( 0, 3,  14   ),    },
    { "VENC_REF_CHROMA",            M4U0_PORT_INIT( 0, 3,  15   ),    },
    { "REMDC_WDMA",                 M4U0_PORT_INIT( 0, 3,  16   ),    },
    { "VENC_NBM_RDMA",              M4U0_PORT_INIT( 0, 3,  17   ),    },
    { "VENC_NBM_WDMA",              M4U0_PORT_INIT( 0, 3,  18   ),    },
                                    
    { "MJC_MV_RD",                  M4U0_PORT_INIT( 0, 4,  0    ),    },
    { "MJC_MV_WR",                  M4U0_PORT_INIT( 0, 4,  1    ),    },
    { "MJC_DMA_RD",                 M4U0_PORT_INIT( 0, 4,  2    ),    },
    { "MJC_DMA_WR",                 M4U0_PORT_INIT( 0, 4,  3    ),    },
                                          
    { "MD",                         M4U1_PORT_INIT( 0 ,  0x10   ),    },
    { "SPM",                        M4U1_PORT_INIT( 1 ,  0x60   ),    },
    { "MD32",                       M4U1_PORT_INIT( 2 ,  0x50   ),    },
    { "PTP_THERM",                  M4U1_PORT_INIT( 3 ,  0x70   ),    },
    { "PWM",                        M4U1_PORT_INIT( 4 ,  0x1    ),    },
    { "MSDC1",                      M4U1_PORT_INIT( 5 ,  0x11   ),    },
    { "MSDC2",                      M4U1_PORT_INIT( 6 ,  0x41   ),    },
    { "SPI0",                       M4U1_PORT_INIT( 7 ,  0x31   ),    },
    { "NFI",                        M4U1_PORT_INIT( 8 ,  0x8    ),    },
    { "AUDIO",                      M4U1_PORT_INIT( 9 ,  0x28   ),    },
    { "MSDC3",                      M4U1_PORT_INIT( 10,  0x48   ),    },
    { "USB0",                       M4U1_PORT_INIT( 11,  0x9    ),    },
    { "MSDC0",                      M4U1_PORT_INIT( 12,  0x0    ),    },
    { "USB1",                       M4U1_PORT_INIT( 13,  0x68   ),    },
    { "AP_DMA",                     M4U1_PORT_INIT( 14,  0x18   ),    },
    { "GCPU",                       M4U1_PORT_INIT( 15,  0x2    ),    },
    { "GCE",                        M4U1_PORT_INIT( 16,  0x3    ),    },
    { "DEBUGTOP0",                  M4U1_PORT_INIT( 17,  0x4    ),    },
    { "DEBUGTOP1",                  M4U1_PORT_INIT( 18,  0xC    ),    },
    { "UNKOWN",                  	M4U1_PORT_INIT( 19,  0xF    ),    },
};



int m4u_invalid_tlb(int m4u_id,int L2_en, int isInvAll, unsigned int mva_start, unsigned int mva_end)
{
    unsigned int reg = 0;
    unsigned long m4u_base = gM4UBaseAddr[m4u_id];

    if(mva_start>=mva_end)
        isInvAll = 1;

    if(!isInvAll) //workaroud: tlb invalid should be 8M align (for some hw) 
    {
        mva_start = round_down(mva_start, SZ_8M);
        mva_end = round_up(mva_end, SZ_8M);
    }
    
    if(L2_en)
        reg = F_MMU_INV_EN_L2;

    reg |= F_MMU_INV_EN_L1;

    M4U_WriteReg32(m4u_base, REG_INVLID_SEL, reg); 
    
    if(isInvAll)
    {
        M4U_WriteReg32(m4u_base, REG_MMU_INVLD, F_MMU_INV_ALL);
    }
    else 
    {
        /*
        unsigned int type_start = m4u_get_pt_type(gPgd_nonsec, mva_start);
        unsigned int type_end = m4u_get_pt_type(gPgd_nonsec, mva_end);
        unsigned int type = max(type_start, type_end);
        unsigned int alignment;
        if(type > MMU_PT_TYPE_SUPERSECTION)
            type = MMU_PT_TYPE_SUPERSECTION;
        alignment = m4u_get_pt_type_size(type) - 1;

        M4U_WriteReg32(m4u_base, REG_MMU_INVLD_SA ,mva_start & (~alignment));
        M4U_WriteReg32(m4u_base, REG_MMU_INVLD_EA, mva_end | alignment);
        M4U_WriteReg32(m4u_base, REG_MMU_INVLD, F_MMU_INV_RANGE);
        */
        
        M4U_WriteReg32(m4u_base, REG_MMU_INVLD_SA ,mva_start);
        M4U_WriteReg32(m4u_base, REG_MMU_INVLD_EA, mva_end);
        M4U_WriteReg32(m4u_base, REG_MMU_INVLD, F_MMU_INV_RANGE);
    }
    
    if(!isInvAll)
    {
        while(!M4U_ReadReg32(m4u_base, REG_MMU_CPE_DONE));
        M4U_WriteReg32(m4u_base, REG_MMU_CPE_DONE, 0);
    }

    return 0;
  
}

static void m4u_invalid_tlb_all(int m4u_id)
{
    m4u_invalid_tlb(m4u_id, gM4U_L2_enable, 1, 0, 0);
}

void m4u_invalid_tlb_by_range(m4u_domain_t *m4u_domain,unsigned int mva_start,unsigned int mva_end)
{
    int i;
    //to-do: should get m4u connected to domain here
    for(i=0; i<TOTAL_M4U_NUM; i++)
        m4u_invalid_tlb(i, gM4U_L2_enable, 0, mva_start, mva_end);
   //m4u_invalid_tlb_all(0);
   //m4u_invalid_tlb_all(1);
}


static int __m4u_dump_rs_info(unsigned int va[], unsigned int pa[], unsigned int st[], unsigned int pte[])
{
    int i;

    M4ULOG_MID("id mva        valid   port-id       pa         pte     larb  w/r  other-status  \n");
    for(i=0; i<MMU_TOTAL_RS_NR; i++)
    {
        M4ULOG_MID("%d: 0x%8x %5d   0x%3x    0x%8x   0x%8x %d     %d  0x%3x\n", i,
            F_MMU_RSx_VA_GET(va[i]), F_MMU_RSx_VA_VALID(va[i]),
            F_MMU_RSx_VA_PID(va[i]), pa[i], pte[i], F_MMU_RSx_ST_LID(st[i]),
            F_MMU_RSx_ST_WRT(st[i]), F_MMU_RSx_ST_OTHER(st[i])
        );
    }

    return 0;
}

static int m4u_dump_rs_info(int m4u_index, int m4u_slave_id)
{
    unsigned long m4u_base = gM4UBaseAddr[m4u_index];
    int i;
    unsigned int va[MMU_TOTAL_RS_NR], pa[MMU_TOTAL_RS_NR], st[MMU_TOTAL_RS_NR], pte[MMU_TOTAL_RS_NR];

    for(i=0; i<MMU_TOTAL_RS_NR; i++)
    {
        va[i] = COM_ReadReg32((m4u_base+REG_MMU_RSx_VA(m4u_slave_id, i)));
        pa[i] = COM_ReadReg32((m4u_base+REG_MMU_RSx_PA(m4u_slave_id, i)));
        st[i] = COM_ReadReg32((m4u_base+REG_MMU_RSx_ST(m4u_slave_id, i)));
        pte[i] = COM_ReadReg32((m4u_base+REG_MMU_RSx_2ND_BASE(m4u_slave_id, i)));		
    }

    M4ULOG_MID("m4u dump RS information index: %d=====>\n", m4u_slave_id);
    __m4u_dump_rs_info(va, pa, st, pte);
    M4ULOG_MID("m4u dump RS information done =====>\n");	
    return 0;
}


static inline void m4u_clear_intr(unsigned int m4u_id)
{
    m4uHw_set_field_by_mask(gM4UBaseAddr[m4u_id], REG_MMU_INT_L2_CONTROL, F_INT_L2_CLR_BIT, F_INT_L2_CLR_BIT);
}
static inline void m4u_enable_intr(unsigned int m4u_id)
{
    M4U_WriteReg32(gM4UBaseAddr[m4u_id], REG_MMU_INT_L2_CONTROL, 0x6f);
    M4U_WriteReg32(gM4UBaseAddr[m4u_id], REG_MMU_INT_MAIN_CONTROL, 0xffffffff);
}
static inline void m4u_disable_intr(unsigned int m4u_id)
{
    M4U_WriteReg32(gM4UBaseAddr[m4u_id], REG_MMU_INT_L2_CONTROL, 0);
    M4U_WriteReg32(gM4UBaseAddr[m4u_id], REG_MMU_INT_MAIN_CONTROL, 0);
}
static inline void m4u_intr_modify_all(unsigned long enable)
{
	int i;
    for(i=0;i<TOTAL_M4U_NUM;i++)
    {
		if(enable)
			m4u_enable_intr(i);
		else
			m4u_disable_intr(i);
    }
}


struct mau_config_info
{
    int m4u_id;
    int m4u_slave_id;
    int mau_set;
    unsigned int start;
    unsigned int end;
    unsigned int port_mask;
    unsigned int larb_mask; 
    unsigned int write_monitor;//:1;
    unsigned int virt;//:1;
    unsigned int io;//:1;
    unsigned int start_bit32;//:1;
    unsigned int end_bit32;//:1;
   
};

/***********************************************************/
/** 
* @param   m4u_id       -- IOMMU main id
* @param   m4u_slave_id -- IOMMU slave id
* @param   mau_set      -- mau set/entry (3 mau set per iommu)
* @param   wr           -- write monitor enable: 0 for read, 1 for write
                            NOTES: cannot monitor read and write using one mau set!!
* @param   vir          -- virtual monitor enable ? (if enable we will monitor mva, or else monitor PA)
* @param   io           -- I/O use mau at input or output of RS. 0 for input, 1 for output
                            input: mau @ RS input, can monitor mva or pa (bypass m4u);
                            output:mau @ RS output, can monitor pa to emi(bypass m4u, or after mva translation)
* @param   bit32        -- enable bit32 monitor? 
* @param   start        -- start address of monitor (can be any address without alignment)
* @param   end          -- end address of monitor (can be any address without alignment)
* @param   port         -- port mask or AXI_ID[4:0] mask
* @param   larb         -- larb[0..7] mask or AXI_ID[7:5] mask
*
* @return 
* @remark 
                        monitor range is [start, end)
* @see     
* @author K Zhang      @date 2013/11/13
************************************************************/
int mau_start_monitor(int m4u_id, int m4u_slave_id, int mau_set,
                      int wr, int vir, int io, int bit32,
                      unsigned int start, unsigned int end, unsigned int port_mask, unsigned int larb_mask)
{
    unsigned long m4u_base = gM4UBaseAddr[m4u_id];
    if(0 == m4u_base)
        return -1;
	
    M4ULOG_HIGH("mau_start_monitor [%d], start=0x%x, end=0x%x, write: %d, port_mask=0x%x, larb_mask=0x%x\n", mau_set, start, end, wr, port_mask, larb_mask); 
	
    M4U_WriteReg32(m4u_base, REG_MMU_MAU_START(m4u_slave_id, mau_set), start);
    M4U_WriteReg32(m4u_base, REG_MMU_MAU_START_BIT32(m4u_slave_id, mau_set), !!(bit32));
    M4U_WriteReg32(m4u_base, REG_MMU_MAU_END(m4u_slave_id, mau_set), end);
    M4U_WriteReg32(m4u_base, REG_MMU_MAU_END_BIT32(m4u_slave_id, mau_set), !!(bit32));

    M4U_WriteReg32(m4u_base, REG_MMU_MAU_PORT_EN(m4u_slave_id, mau_set), port_mask);

    m4uHw_set_field_by_mask(m4u_base, REG_MMU_MAU_LARB_EN(m4u_slave_id),
                            F_MAU_LARB_MSK(mau_set), F_MAU_LARB_VAL(mau_set, larb_mask));

    m4uHw_set_field_by_mask(m4u_base, REG_MMU_MAU_IO(m4u_slave_id),
                            F_MAU_BIT_VAL(1, mau_set), F_MAU_BIT_VAL(io, mau_set));

    m4uHw_set_field_by_mask(m4u_base, REG_MMU_MAU_RW(m4u_slave_id),
                            F_MAU_BIT_VAL(1, mau_set), F_MAU_BIT_VAL(wr, mau_set));

    m4uHw_set_field_by_mask(m4u_base, REG_MMU_MAU_VA(m4u_slave_id),
                            F_MAU_BIT_VAL(1, mau_set), F_MAU_BIT_VAL(vir, mau_set));

    return 0;
}

int config_mau(M4U_MAU_STRUCT mau)
{
    int i;
    int free_id = -1;
    int m4u_id = m4u_port_2_m4u_id(mau.port);
    int larb = m4u_port_2_larb_id(mau.port);
    unsigned int MVAStart = mau.mva;
    unsigned int MVAEnd = mau.mva + mau.size;

    if(0 != m4u_id)
        return -1;
	
    for(i=0; i<MAU_NR_PER_M4U_SLAVE; i++)
    {
        if(0!=gM4u0_mau[i].Enabled)
        {
            if(MVAStart >= gM4u0_mau[i].MVAStart && MVAEnd <= gM4u0_mau[i].MVAEnd) //no overlap
            {
                if(mau.enable == 0)
                {
                    gM4u0_mau[i].Enabled = 0;
            	    mau_start_monitor(0, 0, i, 0, 0, 0, 0, 0, 0, 0, 0);
            	    continue;
                }
            }
        }
        else
        {
            free_id = i;
        }
    }

    if(mau.enable == 0)
    {
        return 0;
    }

    if(free_id == -1)
    {
        if(mau.force == 0)
            return -1;  
        else
        {
            free_id = gMAU_candidate_id;
            if(0 == gMAU_candidate_id)
            {
                gMAU_candidate_id = MAU_NR_PER_M4U_SLAVE -1;
            }
            else
            {
                gMAU_candidate_id--;            
            }
        }
    }	

    gM4u0_mau[free_id].Enabled = 1;
    gM4u0_mau[free_id].MVAStart = MVAStart;
    gM4u0_mau[free_id].MVAEnd = MVAEnd;
    gM4u0_mau[free_id].port = mau.port;

    mau_start_monitor(m4u_id, larb_2_m4u_slave_id(larb), free_id, (int)mau.write, 1, 0, 0, MVAStart, MVAEnd, 1 << m4u_port_2_larb_port(mau.port), 1 << larb);
    return free_id;
}

//notes: you must fill cfg->m4u_id/m4u_slave_id/mau_set before call this func.
int mau_get_config_info(struct mau_config_info * cfg)
{
    int m4u_id = cfg->m4u_id;
    int m4u_slave_id = cfg->m4u_slave_id;
    int mau_set = cfg->mau_set;
    unsigned long m4u_base = gM4UBaseAddr[m4u_id];

    cfg->start = M4U_ReadReg32(m4u_base, REG_MMU_MAU_START(m4u_slave_id, mau_set));
    cfg->end = M4U_ReadReg32(m4u_base, REG_MMU_MAU_END(m4u_slave_id, mau_set));
    cfg->start_bit32 = M4U_ReadReg32(m4u_base, REG_MMU_MAU_START_BIT32(m4u_slave_id, mau_set));
    cfg->end_bit32 = M4U_ReadReg32(m4u_base, REG_MMU_MAU_START_BIT32(m4u_slave_id, mau_set));
    cfg->port_mask = M4U_ReadReg32(m4u_base, REG_MMU_MAU_PORT_EN(m4u_slave_id, mau_set));
    cfg->larb_mask = m4uHw_get_field_by_mask(m4u_base, REG_MMU_MAU_LARB_EN(m4u_slave_id),F_MAU_LARB_MSK(mau_set));

    cfg->io = !!(m4uHw_get_field_by_mask(m4u_base, REG_MMU_MAU_IO(m4u_slave_id), F_MAU_BIT_VAL(1, mau_set)));

    cfg->write_monitor = !!m4uHw_get_field_by_mask(m4u_base, REG_MMU_MAU_RW(m4u_slave_id),F_MAU_BIT_VAL(1, mau_set));

    cfg->virt = !!m4uHw_get_field_by_mask(m4u_base, REG_MMU_MAU_VA(m4u_slave_id),F_MAU_BIT_VAL(1, mau_set));

    return 0;
}


int __mau_dump_status(int m4u_id, int m4u_slave_id, int mau)
{
    unsigned long m4u_base;
    unsigned int status;	
    unsigned int assert_id, assert_addr, assert_b32;
    int larb, port;
    struct mau_config_info mau_cfg;

    m4u_base = gM4UBaseAddr[m4u_id];
    status = M4U_ReadReg32(m4u_base, REG_MMU_MAU_ASSERT_ST(m4u_slave_id));

    if (status & (1 << mau))
    {
        M4ULOG_HIGH("mau_assert in set %d\n", mau);
        assert_id = M4U_ReadReg32(m4u_base, REG_MMU_MAU_ASSERT_ID(m4u_slave_id, mau));
        assert_addr = M4U_ReadReg32(m4u_base, REG_MMU_MAU_ADDR(m4u_slave_id, mau));
        assert_b32 = M4U_ReadReg32(m4u_base, REG_MMU_MAU_ADDR_BIT32(m4u_slave_id, mau));
        larb = F_MMU_MAU_ASSERT_ID_LARB(assert_id);
        port = F_MMU_MAU_ASSERT_ID_PORT(assert_id);
        M4ULOG_HIGH("id=0x%x(%s),addr=0x%x,b32=0x%x\n", assert_id, 
            m4u_get_port_name(larb_port_2_m4u_port(larb, port)),assert_addr, assert_b32);

        M4U_WriteReg32(m4u_base, REG_MMU_MAU_CLR(m4u_slave_id), (1 << mau));
        M4U_WriteReg32(m4u_base, REG_MMU_MAU_CLR(m4u_slave_id), 0);

        mau_cfg.m4u_id = m4u_id;
        mau_cfg.m4u_slave_id = m4u_slave_id;
        mau_cfg.mau_set = mau;
        mau_get_config_info(&mau_cfg);
        M4ULOG_HIGH("mau_cfg: start=0x%x,end=0x%x,virt(%d),io(%d),wr(%d),s_b32(%d),e_b32(%d)\n",
                mau_cfg.start, mau_cfg.end, mau_cfg.virt, mau_cfg.io, 
                mau_cfg.write_monitor, mau_cfg.start_bit32, mau_cfg.end_bit32);
        
    }
    else
    {
        M4ULOG_MID("mau no assert in set %d\n", mau);
    }

    return 0;
}

int mau_dump_status(int m4u_id, int m4u_slave_id)
{
    int i;

    for (i = 0; i < MAU_NR_PER_M4U_SLAVE; i++)
    {
        __mau_dump_status(m4u_id, m4u_slave_id, i);
    }

    return 0;
}

int m4u_dump_reg(int m4u_index, unsigned int start) 
{
    int i;
    M4UINFO("Register Start ======= \n");
    for(i=0;i<4096/8;i+=4)
    {
    	M4UINFO("+0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x \n", start + 8*i,
    	M4U_ReadReg32(gM4UBaseAddr[m4u_index], start + 8*i + 4*0), M4U_ReadReg32(gM4UBaseAddr[m4u_index], start + 8*i + 4*1),
    	M4U_ReadReg32(gM4UBaseAddr[m4u_index], start + 8*i + 4*2), M4U_ReadReg32(gM4UBaseAddr[m4u_index], start + 8*i + 4*3),
    	M4U_ReadReg32(gM4UBaseAddr[m4u_index], start + 8*i + 4*4), M4U_ReadReg32(gM4UBaseAddr[m4u_index], start + 8*i + 4*5),
    	M4U_ReadReg32(gM4UBaseAddr[m4u_index], start + 8*i + 4*6), M4U_ReadReg32(gM4UBaseAddr[m4u_index], start + 8*i + 4*7));
}
    M4UINFO("Register End ========== \n");

    return 0;
}

unsigned int m4u_get_main_descriptor(int m4u_id,int m4u_slave_id, int idx)
{
    unsigned int regValue=0;
    unsigned long m4u_base = gM4UBaseAddr[m4u_id];
    regValue = F_READ_ENTRY_EN \
                | F_READ_ENTRY_MMx_MAIN(m4u_slave_id) \
                | F_READ_ENTRY_MAIN_IDX(idx);
    
    
    M4U_WriteReg32(m4u_base, REG_MMU_READ_ENTRY, regValue);
    while(M4U_ReadReg32(m4u_base, REG_MMU_READ_ENTRY)&F_READ_ENTRY_EN);
    return M4U_ReadReg32(m4u_base, REG_MMU_DES_RDATA);
}
unsigned int m4u_get_main_tag(int m4u_id, int m4u_slave_id, int idx)
{
    unsigned long m4u_base = gM4UBaseAddr[m4u_id];

    return M4U_ReadReg32(m4u_base, REG_MMU_MAIN_TAG(m4u_slave_id, idx));
    
}

void m4u_get_main_tlb(int m4u_id, int m4u_slave_id, int idx, mmu_tlb_t *pTlb)
{
    pTlb->tag = m4u_get_main_tag(m4u_id, m4u_slave_id, idx);
    pTlb->desc = m4u_get_main_descriptor(m4u_id, m4u_slave_id, idx);
}


unsigned int m4u_get_pfh_tlb(int m4u_id, int set, int page, int way, mmu_tlb_t *pTlb)
{
    unsigned int regValue=0;
    unsigned long m4u_base = gM4UBaseAddr[m4u_id];
 
    regValue = F_READ_ENTRY_EN  \
            | F_READ_ENTRY_PFH  \
            | F_READ_ENTRY_PFH_IDX(set)     \
            | F_READ_ENTRY_PFH_PAGE_IDX(page)   \
            | F_READ_ENTRY_PFH_WAY(way);

    M4U_WriteReg32(m4u_base, REG_MMU_READ_ENTRY, regValue);
    while(M4U_ReadReg32(m4u_base, REG_MMU_READ_ENTRY)&F_READ_ENTRY_EN);
    pTlb->desc = M4U_ReadReg32(m4u_base, REG_MMU_DES_RDATA);
    pTlb->tag = M4U_ReadReg32(m4u_base, REG_MMU_PFH_TAG_RDATA);
    
    return 0;
}
unsigned int m4u_get_pfh_tag(int m4u_id, int set, int page, int way)
{
    mmu_tlb_t tlb;
    m4u_get_pfh_tlb(m4u_id, set, page, way, &tlb);
    return tlb.tag;
}

unsigned int m4u_get_pfh_descriptor(int m4u_id, int set, int page, int way)
{
    mmu_tlb_t tlb;
    m4u_get_pfh_tlb(m4u_id, set, page, way, &tlb);
    return tlb.desc;
}


int m4u_dump_main_tlb(int m4u_id, int m4u_slave_id) 
{
    // M4U related
    unsigned int i=0;
    mmu_tlb_t tlb;
    M4ULOG_HIGH("dump main tlb: m4u %d  ====>\n", m4u_id);
    for(i=0;i<gM4UTagCount[m4u_id];i++)
    {
        m4u_get_main_tlb(m4u_id, m4u_slave_id, i, &tlb);
        printk("%d:0x%x:0x%x  ", i, tlb.tag, tlb.desc);
        if((i+1)%8==0)
            printk("===\n");
    }
    
    return 0;
}

int m4u_dump_invalid_main_tlb(int m4u_id, int m4u_slave_id)
{
    unsigned int i=0;
    mmu_tlb_t tlb;
    M4UMSG("dump inv main tlb=>\n");
    for(i=0;i<gM4UTagCount[m4u_id];i++)
    {
        m4u_get_main_tlb(m4u_id, m4u_slave_id, i, &tlb);
		if((tlb.tag&(F_MAIN_TLB_VALID_BIT|F_MAIN_TLB_INV_DES_BIT))
				== (F_MAIN_TLB_VALID_BIT|F_MAIN_TLB_INV_DES_BIT))
		{
			printk("%d:0x%x:0x%x  ", i, tlb.tag, tlb.desc);
		}
    }
	printk("\n");
    
    return 0;
}

static unsigned int imu_pfh_tag_to_va(int mmu,int set,int way,unsigned int tag)    
{
    unsigned int tmp;
    if(tag&F_PFH_TAG_LAYER_BIT)
    {
        return (F_PFH_TAG_VA_GET(mmu,tag)|((set)<<15));
    }
    else
    {
        tmp = F_PFH_TAG_VA_GET(mmu,tag);
        tmp &= F_MMU_PFH_TAG_VA_LAYER0_MSK(mmu);
        tmp |= (set)<<23;
        return tmp;
    }
}


int m4u_dump_pfh_tlb(int m4u_id) 
{
    unsigned int regval;
    unsigned long m4u_base = gM4UBaseAddr[m4u_id];
    int result = 0;
    int set_nr, way_nr, set, way;
    int valid;


    set_nr = MMU_SET_NR(m4u_id);
    way_nr = MMU_WAY_NR;

    printk("dump pfh_tlb: m4u %d  ====> \n", m4u_id);
    
    for(way=0; way<way_nr; way++)
    {
        for(set=0; set<set_nr; set++)
        {
            int page;
            mmu_tlb_t tlb;
            regval = M4U_ReadReg32(m4u_base, REG_MMU_PFH_VLD(m4u_id, set, way));
            valid = !!(regval & F_MMU_PFH_VLD_BIT(set, way));
            m4u_get_pfh_tlb(m4u_id, set, 0, way, &tlb);
            printk("va(0x%x) lay(%d) 16x(%d) sec(%d) pfh(%d) v(%d),set(%d),way(%d), 0x%x:", 
                imu_pfh_tag_to_va(m4u_id, set, way, tlb.tag),
                !!(tlb.tag & F_PFH_TAG_LAYER_BIT),
                !!(tlb.tag & F_PFH_TAG_16X_BIT),
                !!(tlb.tag & F_PFH_TAG_SEC_BIT),
                !!(tlb.tag & F_PFH_TAG_AUTO_PFH),
                valid, 
                set, way,
                tlb.desc); 
                
            for(page=1; page<8; page++)
            {
                m4u_get_pfh_tlb(m4u_id, set, page, way, &tlb);
                printk("0x%x:", tlb.desc);
            }
            printk("\n");
            
        }
    }

    return result;
}


int m4u_get_pfh_tlb_all(int m4u_id, mmu_pfh_tlb_t* pfh_buf)
{
    unsigned int regval;
    unsigned long m4u_base = gM4UBaseAddr[m4u_id];
    int set_nr, way_nr, set, way;
    int valid;
    int pfh_id = 0;

    set_nr = MMU_SET_NR(m4u_id);
    way_nr = MMU_WAY_NR;

    for(way=0; way<way_nr; way++)
    {
        for(set=0; set<set_nr; set++)
        {
            int page;
            mmu_tlb_t tlb;
            regval = M4U_ReadReg32(m4u_base, REG_MMU_PFH_VLD(m4u_id, set, way));
            valid = !!(regval & F_MMU_PFH_VLD_BIT(set, way));
            m4u_get_pfh_tlb(m4u_id, set, 0, way, &tlb);

            pfh_buf[pfh_id].tag = tlb.tag;
            pfh_buf[pfh_id].va = imu_pfh_tag_to_va(m4u_id, set, way, tlb.tag);
            pfh_buf[pfh_id].layer = !!(tlb.tag & F_PFH_TAG_LAYER_BIT);
            pfh_buf[pfh_id].x16 = !!(tlb.tag & F_PFH_TAG_16X_BIT);
            pfh_buf[pfh_id].sec = !!(tlb.tag & F_PFH_TAG_SEC_BIT);
            pfh_buf[pfh_id].pfh = !!(tlb.tag & F_PFH_TAG_AUTO_PFH);
            pfh_buf[pfh_id].set = set;
            pfh_buf[pfh_id].way = way;
            pfh_buf[pfh_id].valid = valid;
            pfh_buf[pfh_id].desc[0] = tlb.desc;
            pfh_buf[pfh_id].page_size = pfh_buf[pfh_id].layer ? MMU_SMALL_PAGE_SIZE : MMU_SECTION_SIZE;

            for(page=1; page<MMU_PAGE_PER_LINE; page++)
            {
                m4u_get_pfh_tlb(m4u_id, set, page, way, &tlb);
                pfh_buf[pfh_id].desc[page] = tlb.desc;
            }
            pfh_id++;
            
        }
    }

    return 0;
}





int m4u_confirm_main_range_invalidated(int m4u_index, int m4u_slave_id, unsigned int MVAStart, unsigned int MVAEnd)
{
    unsigned int i;
    unsigned int regval;

    ///> check Main TLB part
    for(i=0;i<gM4UTagCount[m4u_index];i++)
    {
        regval = m4u_get_main_tag(m4u_index, m4u_slave_id, i);
        
        if(regval & (F_MAIN_TLB_VALID_BIT))
        {
            unsigned int tag_s, tag_e, sa, ea; 
            int layer = regval&F_MAIN_TLB_LAYER_BIT;
            int large = regval&F_MAIN_TLB_16X_BIT;

            tag_s = regval & F_MAIN_TLB_VA_MSK;
            sa = MVAStart & (~(PAGE_SIZE-1));
            ea = MVAEnd | (PAGE_SIZE-1);

            if(layer)
            {   //pte
                if(large)
                {
                    tag_e = tag_s + MMU_LARGE_PAGE_SIZE -1;
                }
                else
                {
                    tag_e = tag_s + PAGE_SIZE - 1;
                }

                if( !((tag_e<sa)||(tag_s>ea)))
                {
                    M4UERR("main: i=%d, idx=0x%x, MVAStart=0x%x, MVAEnd=0x%x, RegValue=0x%x\n",
                        i, m4u_index, MVAStart, MVAEnd, regval);
                    return -1;
                }
                
            }
            else
            {
                if(large)
                {
                    tag_e = tag_s + MMU_SUPERSECTION_SIZE -1;
                }
                else
                {
                    tag_e = tag_s + MMU_SECTION_SIZE - 1;
                }

                if((tag_s>=sa)&&(tag_e<=ea))
                {
                    M4UERR("main: i=%d, idx=0x%x, MVAStart=0x%x, MVAEnd=0x%x, RegValue=0x%x\n",
                        i, m4u_index, MVAStart, MVAEnd, regval);
                    return -1;
                }
            }
            
        }
    }
    return 0;
}

int m4u_confirm_range_invalidated(int m4u_index, unsigned int MVAStart, unsigned int MVAEnd)
{
    unsigned int i = 0;
    unsigned int regval;
    unsigned long m4u_base = gM4UBaseAddr[m4u_index];
    int result = 0;
    int set_nr, way_nr, set, way;

    ///> check Main TLB part
    result = m4u_confirm_main_range_invalidated(m4u_index, 0, MVAStart, MVAEnd);
    if(result < 0)
        return -1;

    if(m4u_index==0)
    {
        result = m4u_confirm_main_range_invalidated(m4u_index, 1, MVAStart, MVAEnd);
        if(result < 0)
            return -1;
    }


    set_nr = MMU_SET_NR(m4u_index);
    way_nr = MMU_WAY_NR;

    
    for(way=0; way<way_nr; way++)
    {
        for(set=0; set<set_nr; set++)
        {
            regval = M4U_ReadReg32(m4u_base, REG_MMU_PFH_VLD(m4u_index, set, way));
            if(regval & F_MMU_PFH_VLD_BIT(set, way))
            {
                unsigned int tag = m4u_get_pfh_tag(m4u_index, set, 0, way);
                unsigned int tag_s, tag_e, sa, ea; 
                int layer = tag&F_PFH_TAG_LAYER_BIT;
                int large = tag&F_PFH_TAG_16X_BIT;

                tag_s = imu_pfh_tag_to_va(m4u_index,set, way, tag);
                
                sa = MVAStart & (~(PAGE_SIZE-1));
                ea = MVAEnd | (PAGE_SIZE-1);

                if(layer)
                {   //pte
                    if(large)
                    {
                        tag_e = tag_s + MMU_LARGE_PAGE_SIZE*8 -1;
                    }
                    else
                    {
                        tag_e = tag_s + PAGE_SIZE*8 - 1;
                    }

                    if( !((tag_e<sa)||(tag_s>ea)))
                    {
                        M4UERR("main: i=%d, idx=0x%x, MVAStart=0x%x, MVAEnd=0x%x, RegValue=0x%x\n",
                            i, m4u_index, MVAStart, MVAEnd, regval);
                        return -1;
                    }
                    
                }
                else
                {
                    if(large)
                    {
                        tag_e = tag_s + MMU_SUPERSECTION_SIZE*8 -1;
                    }
                    else
                    {
                        tag_e = tag_s + MMU_SECTION_SIZE*8 - 1;
                    }

                    //if((tag_s>=sa)&&(tag_e<=ea))
                    if( !((tag_e<sa)||(tag_s>ea)))
                    {
                        M4UERR("main: i=%d, idx=0x%x, MVAStart=0x%x, MVAEnd=0x%x, RegValue=0x%x\n",
                            i, m4u_index, MVAStart, MVAEnd, regval);
                        return -1;
                    }
                }
                
            }
        }
    }

    return result;
}

int m4u_confirm_main_all_invalid(int m4u_index, int m4u_slave_id)
{
    unsigned int i;
    unsigned int regval;

    for(i=0;i<gM4UTagCount[m4u_index];i++)
    {
        regval = m4u_get_main_tag(m4u_index, m4u_slave_id, i);
        
        if(regval & (F_MAIN_TLB_VALID_BIT))
        {
            M4UERR("main: i=%d, idx=0x%x, RegValue=0x%x\n",  i, m4u_index, regval);
            return -1;
        }
    }
    return 0;
}

int m4u_confirm_pfh_all_invalid(int m4u_index)
{
    unsigned int regval;
    unsigned long m4u_base = gM4UBaseAddr[m4u_index];
    int set_nr, way_nr, set, way;
    
    set_nr = MMU_SET_NR(m4u_index);
    way_nr = MMU_WAY_NR;

    for(way=0; way<way_nr; way++)
    {
        for(set=0; set<set_nr; set++)
        {
            regval = M4U_ReadReg32(m4u_base, REG_MMU_PFH_VLD(m4u_index, set, way));
            if(regval & F_MMU_PFH_VLD_BIT(set, way))
            {
                return -1;
            }
        }
    }
    return 0;
}

int m4u_confirm_all_invalidated(int m4u_index)
{
    if(m4u_confirm_main_all_invalid(m4u_index, 0))
        return -1;
    
    if(m4u_index==0)
    {
        if(m4u_confirm_main_all_invalid(m4u_index, 1))
            return -1;
    }

    if(m4u_confirm_pfh_all_invalid(m4u_index))
        return -1;

    return 0;
}





int m4u_power_on(int m4u_index)
{
    return 0;
}

int m4u_power_off(int m4u_index)
{
    return 0;
}


static int m4u_clock_on(void)
{
    enable_clock(MT_CG_INFRA_M4U, "infra_m4u");
//    enable_clock(MT_CG_INFRA_SMI, "infra_smi");
    return 0;
}

/*
static int m4u_clock_off(void)
{
    disable_clock(MT_CG_INFRA_M4U, "infra_m4u");
    disable_clock(MT_CG_INFRA_SMI, "infra_smi");
    return 0;
}
*/

static int larb_clock_on(int larb)
{
    switch (larb)
    {
        case 0:
            enable_clock(MT_CG_DISP0_SMI_LARB0, "m4u_larb0");
        break;
        case 1:
            enable_clock(MT_CG_VDEC0_VDEC, "m4u_larb1");
            enable_clock(MT_CG_VDEC1_LARB, "m4u_larb1");
        break;
        case 2:
            enable_clock(MT_CG_IMAGE_LARB2_SMI, "m4u_larb2");
        break;
        case 3:
            enable_clock(MT_CG_VENC_VENC, "m4u_larb3");
            enable_clock(MT_CG_VENC_LARB, "m4u_larb3");
        break;
        case 4:
            enable_clock(MT_CG_MJC_SMI_LARB, "m4u_larb4");
            enable_clock(MT_CG_MJC_LARB4_AXI_ASIF, "m4u_larb4"); 
        break;

        default:
            M4UMSG("error: unknown larb id  %d, %s\n", larb, __FUNCTION__); 
        break;
    }

    return 0;
}

static int larb_clock_off(int larb)
{
    switch (larb)
    {
        case 0:
            disable_clock(MT_CG_DISP0_SMI_LARB0, "m4u_larb0");
        break;
        case 1:
            disable_clock(MT_CG_VDEC0_VDEC, "m4u_larb1");
            disable_clock(MT_CG_VDEC1_LARB, "m4u_larb1");
        break;
        case 2:
            disable_clock(MT_CG_IMAGE_LARB2_SMI, "m4u_larb2");
        break;
        case 3:
            disable_clock(MT_CG_VENC_VENC, "m4u_larb3");
            disable_clock(MT_CG_VENC_LARB, "m4u_larb3");
        break;
        case 4:
            disable_clock(MT_CG_MJC_SMI_LARB, "m4u_larb4");
            disable_clock(MT_CG_MJC_LARB4_AXI_ASIF, "m4u_larb4"); 
        break;

        default:
            M4UMSG("error: unknown larb id  %d, %s\n", larb, __FUNCTION__); 
        break;
    }
    return 0;
}

static int larb_clock_all_on(void)
{
    int i;
    for( i=0 ; i < SMI_LARB_NR ; i++)
    {
        larb_clock_on(i);
    }
    return 0;
}
static int larb_clock_all_off(void)
{
    int i;
    for( i=0 ; i < SMI_LARB_NR ; i++)
    {
        larb_clock_off(i);
    }
    return 0;
}

static void smi_common_clock_on(void)
{
    enable_clock(MT_CG_DISP0_SMI_COMMON, "smi_common");
    //m4uHw_set_field_by_mask(0, 0xf4000108, 0x1, 0x1);
}

static void smi_common_clock_off(void)
{
    disable_clock(MT_CG_DISP0_SMI_COMMON, "smi_common");
    //m4uHw_set_field_by_mask(0, 0xf4000108, 0x1, 0x0);
}



int m4u_insert_seq_range(M4U_PORT_ID port,unsigned int MVAStart,unsigned int MVAEnd) 
{
    int i, free_id = -1;
    unsigned int m4u_index = m4u_port_2_m4u_id(port);
    unsigned int m4u_slave_id = m4u_port_2_m4u_slave_id(port);
    M4U_RANGE_DES_T *pSeq = gM4USeq[m4u_index] + SEQ_NR_PER_M4U_SLAVE*m4u_slave_id;
	  
    M4ULOG_MID("m4u_insert_seq_range , module:%s, MVAStart:0x%x, MVAEnd:0x%x\n", 
            m4u_get_port_name(port), MVAStart, MVAEnd);

    if(MVAEnd - MVAStart < PAGE_SIZE)
    {
        M4ULOG_MID("too small size, skip to insert! module:%s, MVAStart:0x%x, size:%d\n", 
            m4u_get_port_name(port), MVAStart, MVAEnd - MVAStart +1);    
        return free_id;
    }

//===============================================
    //every seq range has to align to 256K Bytes
    MVAStart &= ~M4U_SEQ_ALIGN_MSK;
    MVAEnd |= M4U_SEQ_ALIGN_MSK;

    mutex_lock(&gM4u_seq_mutex);

//==================================================================    
    // check if the range is overlap with previous ones
        
    for(i=0; i<SEQ_NR_PER_M4U_SLAVE; i++)
    {
        if(1==pSeq[i].Enabled)
        {
            if(MVAEnd<pSeq[i].MVAStart || MVAStart>pSeq[i].MVAEnd) //no overlap
            {
            	  continue;
            }
            else
            {
                M4ULOG_HIGH("insert range overlap!: larb=%d,module=%s\n", 
                     m4u_port_2_larb_id(port), m4u_get_port_name(port));
                M4ULOG_HIGH("warning: insert tlb range is overlapped with previous ranges, current process=%s,!\n",  current->comm);	
                M4ULOG_HIGH("module=%s, mva_start=0x%x, mva_end=0x%x \n", m4u_get_port_name(port), MVAStart, MVAEnd);
                M4ULOG_HIGH("overlapped range id=%d, module=%s, mva_start=0x%x, mva_end=0x%x \n", 
                        i, m4u_get_port_name(pSeq[i].port), pSeq[i].MVAStart, pSeq[i].MVAEnd);
                mutex_unlock(&gM4u_seq_mutex);
                return -1;
            }
        }
        else
        {
            free_id = i;
        }
    }
    
    if(free_id == -1)
    {
        M4ULOG_MID("warning: can not find available range \n");
        mutex_unlock(&gM4u_seq_mutex);
        return -1;  
    }

    ///> record range information in array
    pSeq[free_id].Enabled = 1;
    pSeq[free_id].port = port;
    pSeq[free_id].MVAStart = MVAStart;
    pSeq[free_id].MVAEnd = MVAEnd;

    mutex_unlock(&gM4u_seq_mutex);

    ///> set the range register
    
    MVAStart &= F_SQ_VA_MASK;
    MVAStart |= F_SQ_EN_BIT;
    //align mvaend to 256K
    MVAEnd |= ~F_SQ_VA_MASK;
    
    spin_lock(&gM4u_reg_lock);
    {
        M4U_WriteReg32(gM4UBaseAddr[m4u_index], REG_MMU_SQ_START(m4u_slave_id,free_id), MVAStart);
        M4U_WriteReg32(gM4UBaseAddr[m4u_index], REG_MMU_SQ_END(m4u_slave_id,free_id), MVAEnd);
    }
    spin_unlock(&gM4u_reg_lock);
    
    return free_id;
}  



int m4u_invalid_seq_range_by_id(int port, int seq_id)
{
    int m4u_index = m4u_port_2_m4u_id(port);
    int m4u_slave_id = m4u_port_2_m4u_slave_id(port);
    unsigned long m4u_base = gM4UBaseAddr[m4u_index];
    M4U_RANGE_DES_T *pSeq = gM4USeq[m4u_index] + SEQ_NR_PER_M4U_SLAVE*m4u_slave_id;
    int ret=0;

    mutex_lock(&gM4u_seq_mutex);
    {
        pSeq[seq_id].Enabled = 0;
    }
    mutex_unlock(&gM4u_seq_mutex);
    
    spin_lock(&gM4u_reg_lock); 
    M4U_WriteReg32(m4u_base, REG_MMU_SQ_START(m4u_slave_id,seq_id), 0);
    M4U_WriteReg32(m4u_base, REG_MMU_SQ_END(m4u_slave_id,seq_id), 0);
    spin_unlock(&gM4u_reg_lock);

    return ret;
}

/*
static int m4u_invalid_seq_range_by_mva(int m4u_index, int m4u_slave_id, unsigned int MVAStart, unsigned int MVAEnd)
{
    unsigned int i;
    unsigned int m4u_base = gM4UBaseAddr[m4u_index];
    M4U_RANGE_DES_T *pSeq = gM4USeq[m4u_index] + SEQ_NR_PER_M4U_SLAVE*m4u_slave_id;
    int ret=-1;

    MVAStart &= ~M4U_SEQ_ALIGN_MSK;
    MVAEnd |= M4U_SEQ_ALIGN_MSK;
    
    mutex_lock(&gM4u_seq_mutex);
    for(i=0; i<SEQ_NR_PER_M4U_SLAVE; i++)
    {
        if(pSeq[i].Enabled == 1 &&
            pSeq[i].MVAStart>=MVAStart && 
            pSeq[i].MVAEnd<=MVAEnd)
        {
            pSeq[i].Enabled = 0;
            spin_lock(&gM4u_reg_lock); 
            M4U_WriteReg32(m4u_base, REG_MMU_SQ_START(m4u_slave_id,i), 0);
            M4U_WriteReg32(m4u_base, REG_MMU_SQ_END(m4u_slave_id,i), 0);
            spin_unlock(&gM4u_reg_lock);
            break;
        }
    }
    mutex_unlock(&gM4u_seq_mutex);

    return ret;
}
*/



static int _m4u_config_port(int port, int virt, int sec, int dis, int dir)
{
    int m4u_index = m4u_port_2_m4u_id(port);
    unsigned long m4u_base = gM4UBaseAddr[m4u_index];
    unsigned long larb_base;
    unsigned int larb, larb_port;
    int ret = 0;
    
    if(0 == virt || 1 == sec)    
    {
        M4ULOG_HIGH("config_port:%s,v%d,s%d\n", m4u_get_port_name(port), virt, sec);
    }
    //MMProfileLogEx(M4U_MMP_Events[M4U_MMP_CONFIG_PORT], MMProfileFlagStart, port, virt);

    spin_lock(&gM4u_reg_lock);
    // Direction, one bit for each port, 1:-, 0:+
    m4uHw_set_field_by_mask(m4u_base, REG_MMU_PFH_DIR(port),\
                F_MMU_PFH_DIR(port, 1), F_MMU_PFH_DIR(port, dir));
        
    m4uHw_set_field_by_mask(m4u_base, REG_MMU_PFH_DIST(port),\
                F_MMU_PFH_DIST_MASK(port), F_MMU_PFH_DIST_VAL(port,dis));

    if(m4u_index==0)
    {
        int mmu_en = 0;  
        larb = m4u_port_2_larb_id(port);
        larb_port = m4u_port_2_larb_port(port);
        larb_base = gLarbBaseAddr[larb];
        
        m4uHw_set_field_by_mask(larb_base, SMI_LARB_MMU_EN,\
                F_SMI_MMU_EN(larb_port, 1), F_SMI_MMU_EN(larb_port, !!(virt)));
       
        m4uHw_set_field_by_mask(larb_base, SMI_LARB_SEC_EN,\
                F_SMI_SEC_EN(larb_port, 1), F_SMI_SEC_EN(larb_port, !!(sec)));
        
        //multimedia engines will should set domain as 3. 
        //m4uHw_set_field_by_mask(larb_base, REG_SMI_LARB_DOMN_OF_PORT(larb_port),
        //        F_SMI_DOMN(larb_port, 0x3), F_SMI_DOMN(larb_port, pM4uPort->domain));
        

//debug use
        mmu_en = m4uHw_get_field_by_mask(larb_base, SMI_LARB_MMU_EN, F_SMI_MMU_EN(larb_port, 1));
        if(!!(mmu_en) != virt)
        {
            M4ULOG_HIGH("m4u_config_port error, port=%s, Virtuality=%d, mmu_en=%x (%x, %x)\n",m4u_get_port_name(port), virt, mmu_en,M4U_ReadReg32(larb_base, SMI_LARB_MMU_EN),F_SMI_MMU_EN(larb_port, 1));
        }        
    }
    else
    {
        if(port>M4U_PORT_USB0)
        {
            M4UMSG("warning cannot config virtual for port %d\n", port);
			ret = -1;
            goto unlock_out;
        }
        
        larb_port = m4u_port_2_larb_port(port);

        m4uHw_set_field_by_mask(gPericfgBaseAddr, REG_PERIAXI_BUS_CTL3,\
                F_PERI_MMU_EN(larb_port, 1), F_PERI_MMU_EN(larb_port, !!(virt)));
    }

unlock_out:
    spin_unlock(&gM4u_reg_lock);

    //MMProfileLogEx(M4U_MMP_Events[M4U_MMP_CONFIG_PORT], MMProfileFlagEnd, dis, dir);	

    return ret;

}

static inline void _m4u_port_clock_toggle(int m4u_index, int larb, int on)
{
	unsigned long long start, end;

    //MMProfileLogEx(M4U_MMP_Events[M4U_MMP_TOGGLE_CG], MMProfileFlagStart, larb, on);
    if(m4u_index==0)
    {
		start = sched_clock();
		if(on)
		{
	        smi_common_clock_on();
	        larb_clock_on(larb);
		}
		else
		{
	        larb_clock_off(larb);
	        smi_common_clock_off();
		}
		end = sched_clock();

		if(end-start > 50000000ULL) //unit is ns
		{
			M4ULOG_HIGH("warn: larb%d clock%d time: %lldns\n", larb, on, end-start);
		}
    }
    //MMProfileLogEx(M4U_MMP_Events[M4U_MMP_TOGGLE_CG], MMProfileFlagEnd, 0, 0);	
}

int m4u_config_port(M4U_PORT_STRUCT* pM4uPort) //native
{
    M4U_PORT_ID PortID = (pM4uPort->ePortID);
    int m4u_index = m4u_port_2_m4u_id(PortID);
    int larb = m4u_port_2_larb_id(PortID);
	int ret;
    
	_m4u_port_clock_toggle(m4u_index, larb, 1);
	
	ret = _m4u_config_port(PortID, pM4uPort->Virtuality, 
				pM4uPort->Security, pM4uPort->Distance, pM4uPort->Direction);

	_m4u_port_clock_toggle(m4u_index, larb, 0);

    return 0;
}

void m4u_port_array_init(struct m4u_port_array * port_array)
{
    memset(port_array, 0, sizeof(struct m4u_port_array));
}

int m4u_port_array_add(struct m4u_port_array *port_array, 
	    int port, int m4u_en, int secure)
{
    if(port>=M4U_PORT_NR)
    {
        M4UMSG("error: port_array_add, port=%d, v(%d), s(%d)\n", port, m4u_en, secure);
        return -1;
    }
    port_array->ports[port] = M4U_PORT_ATTR_EN;
    if(m4u_en)
        port_array->ports[port] |= M4U_PORT_ATTR_VIRTUAL;
    if(secure)
        port_array->ports[port] |= M4U_PORT_ATTR_SEC;
    return 0;
}

int m4u_config_port_array(struct m4u_port_array *port_array)
{
	int port, m4u_index, larb;
	int last_larb=-1, last_index = 0;
	int ret=0;

	for(port=0; port<M4U_PORT_NR; port++)
	{
		if(port_array->ports[port] & M4U_PORT_ATTR_EN)
		{
		    m4u_index = m4u_port_2_m4u_id(port);
		    larb = m4u_port_2_larb_id(port);
			if(larb != last_larb)
			{
				if(last_larb != -1)	//clock off last larb
					_m4u_port_clock_toggle(last_index, last_larb, 0);

				last_larb = larb;
				last_index = m4u_index;
				//clock on new larb
				_m4u_port_clock_toggle(m4u_index, larb, 1);
			}

			ret |= _m4u_config_port(port, 
						!!(port_array->ports[port] & M4U_PORT_ATTR_VIRTUAL),
						!!(port_array->ports[port] & M4U_PORT_ATTR_SEC),
						1, 0);
		}
	}
	
	if(last_larb != -1)	//clock off last larb
		_m4u_port_clock_toggle(last_index, last_larb, 0);

	return ret;
}



void m4u_get_perf_counter(int m4u_index, int m4u_slave_id, M4U_PERF_COUNT *pM4U_perf_count)
{
    unsigned long m4u_base = gM4UBaseAddr[m4u_index];
    pM4U_perf_count->transaction_cnt= M4U_ReadReg32(m4u_base, REG_MMU_ACC_CNT(m4u_slave_id));    ///> Transaction access count
    pM4U_perf_count->main_tlb_miss_cnt= M4U_ReadReg32(m4u_base, REG_MMU_MAIN_MSCNT(m4u_slave_id)); ///> Main TLB miss count
    pM4U_perf_count->pfh_tlb_miss_cnt= M4U_ReadReg32(m4u_base, REG_MMU_PF_MSCNT);   ///> Prefetch TLB miss count
    pM4U_perf_count->pfh_cnt = M4U_ReadReg32(m4u_base, REG_MMU_PF_CNT);     ///> Prefetch count
    pM4U_perf_count->rs_perf_cnt = M4U_ReadReg32(m4u_base, REG_MMU_RS_PERF_CNT(m4u_slave_id));
}


int m4u_monitor_start(int m4u_id)
{
    unsigned long m4u_base = gM4UBaseAddr[m4u_id];
    M4UINFO("====m4u_monitor_start: %d======\n", m4u_id);	
    //clear GMC performance counter
    m4uHw_set_field_by_mask(m4u_base, REG_MMU_CTRL_REG, 
                F_MMU_CTRL_MONITOR_CLR(1), F_MMU_CTRL_MONITOR_CLR(1));
    m4uHw_set_field_by_mask(m4u_base, REG_MMU_CTRL_REG, 
                F_MMU_CTRL_MONITOR_CLR(1), F_MMU_CTRL_MONITOR_CLR(0));

    //enable GMC performance monitor
    m4uHw_set_field_by_mask(m4u_base, REG_MMU_CTRL_REG, 
                F_MMU_CTRL_MONITOR_EN(1), F_MMU_CTRL_MONITOR_EN(1));
    return 0;
}

/**
 * @brief ,             
 * @param 
 * @return 
 */
int m4u_monitor_stop(int m4u_id)
{
    M4U_PERF_COUNT cnt;
    int m4u_index = m4u_id;
    unsigned long m4u_base = gM4UBaseAddr[m4u_index];
    
    //disable GMC performance monitor
    m4uHw_set_field_by_mask(m4u_base, REG_MMU_CTRL_REG, 
                F_MMU_CTRL_MONITOR_EN(1), F_MMU_CTRL_MONITOR_EN(0));

    m4u_get_perf_counter(m4u_index, 0, &cnt);
    //read register get the count
    M4ULOG_MID("[M4U%d-%d] total:%u, main miss:%u, pfh miss(walk):%u, auto pfh:%u\n", 
        m4u_id, 0,
        cnt.transaction_cnt, cnt.main_tlb_miss_cnt, cnt.pfh_tlb_miss_cnt,cnt.pfh_cnt);

    return 0;
}

void m4u_print_perf_counter(int m4u_index, int m4u_slave_id, const char *msg)
{
    M4U_PERF_COUNT cnt;
    M4UINFO("====m4u performance count for %s m4u%d_%d======\n", msg, m4u_index, m4u_slave_id);
    m4u_get_perf_counter(m4u_index, m4u_slave_id, &cnt);
    M4UINFO("total trans=%u, main_miss=%u, pfh_miss=%u, pfh_cnt=%u, rs_perf_cnt=%u\n",
        cnt.transaction_cnt, cnt.main_tlb_miss_cnt, cnt.pfh_tlb_miss_cnt, cnt.pfh_cnt, cnt.rs_perf_cnt);
}


#define M4U_REG_BACKUP_SIZE     (270*sizeof(unsigned int))
static unsigned int* pM4URegBackUp = 0;
static unsigned int gM4u_reg_backup_real_size = 0;

#define __M4U_BACKUP(base, reg, back)    do{(back)=M4U_ReadReg32(base, reg);}while(0)
#define __M4U_RESTORE(base, reg, back)   do{M4U_WriteReg32(base, reg, back);}while(0)


int m4u_reg_backup(void)
{
    unsigned int* pReg = pM4URegBackUp;
    unsigned long m4u_base;
    int m4u_id, m4u_slave;
    int seq, mau;
    unsigned int real_size;
    
    for(m4u_id=0; m4u_id<TOTAL_M4U_NUM; m4u_id++)
    {
        m4u_base = gM4UBaseAddr[m4u_id];
        __M4U_BACKUP(m4u_base, REG_MMUg_PT_BASE                    , *(pReg++)   );
        __M4U_BACKUP(m4u_base, REG_MMUg_PT_BASE_SEC                , *(pReg++)   );
        __M4U_BACKUP(m4u_base, REG_MMU_SEC_ABORT_INFO              , *(pReg++)   );
        __M4U_BACKUP(m4u_base, REG_MMU_STANDARD_AXI_MODE           , *(pReg++)   );
        __M4U_BACKUP(m4u_base, REG_MMU_PRIORITY                    , *(pReg++)   );
        __M4U_BACKUP(m4u_base, REG_MMU_DCM_DIS                     , *(pReg++)   );
        __M4U_BACKUP(m4u_base, REG_MMU_WR_LEN                      , *(pReg++)   );
        __M4U_BACKUP(m4u_base, REG_MMU_HW_DEBUG                    , *(pReg++)   );
        __M4U_BACKUP(m4u_base, REG_MMU_NON_BLOCKING_DIS            , *(pReg++)   );
        __M4U_BACKUP(m4u_base, REG_MMU_LEGACY_4KB_MODE             , *(pReg++)   );
        __M4U_BACKUP(m4u_base, REG_MMU_PFH_DIST0                   , *(pReg++)   );
        __M4U_BACKUP(m4u_base, REG_MMU_PFH_DIST1                   , *(pReg++)   );
        __M4U_BACKUP(m4u_base, REG_MMU_PFH_DIST2                   , *(pReg++)   );
        __M4U_BACKUP(m4u_base, REG_MMU_PFH_DIST3                   , *(pReg++)   );
        __M4U_BACKUP(m4u_base, REG_MMU_PFH_DIST4                   , *(pReg++)   );
        __M4U_BACKUP(m4u_base, REG_MMU_PFH_DIST5                   , *(pReg++)   );
        __M4U_BACKUP(m4u_base, REG_MMU_PFH_DIST6                   , *(pReg++)   );
        __M4U_BACKUP(m4u_base, REG_MMU_PFH_DIST7                   , *(pReg++)   );		
        __M4U_BACKUP(m4u_base, REG_MMU_PFH_DIR0                    , *(pReg++)   );
        __M4U_BACKUP(m4u_base, REG_MMU_PFH_DIR1                    , *(pReg++)   );
        __M4U_BACKUP(m4u_base, REG_MMU_CTRL_REG                    , *(pReg++)   );
        __M4U_BACKUP(m4u_base, REG_MMU_IVRP_PADDR                  , *(pReg++)   );
        __M4U_BACKUP(m4u_base, REG_MMU_INT_L2_CONTROL              , *(pReg++)   );
        __M4U_BACKUP(m4u_base, REG_MMU_INT_MAIN_CONTROL            , *(pReg++)   );
         
        for(m4u_slave=0; m4u_slave < M4U_SLAVE_NUM(m4u_id); m4u_slave++)
        {
            for(seq=0; seq<SEQ_NR_PER_M4U_SLAVE; seq++)
            {
                __M4U_BACKUP(m4u_base, REG_MMU_SQ_START(m4u_slave, seq)  ,    *(pReg++)      );
                __M4U_BACKUP(m4u_base, REG_MMU_SQ_END(m4u_slave, seq)    ,    *(pReg++)      );
            }

            for(mau=0; mau<MAU_NR_PER_M4U_SLAVE; mau++)
            {
                __M4U_BACKUP(m4u_base, REG_MMU_MAU_START(m4u_slave,mau)       ,  *(pReg++) );
                __M4U_BACKUP(m4u_base, REG_MMU_MAU_START_BIT32(m4u_slave,mau) ,  *(pReg++) );
                __M4U_BACKUP(m4u_base, REG_MMU_MAU_END(m4u_slave,mau)         ,  *(pReg++) );
                __M4U_BACKUP(m4u_base, REG_MMU_MAU_END_BIT32(m4u_slave,mau)   ,  *(pReg++) );
                __M4U_BACKUP(m4u_base, REG_MMU_MAU_PORT_EN(m4u_slave,mau)     ,  *(pReg++) );
            }
            __M4U_BACKUP(m4u_base, REG_MMU_MAU_LARB_EN(m4u_slave)   ,   *(pReg++)      );
            __M4U_BACKUP(m4u_base, REG_MMU_MAU_IO(m4u_slave)        ,   *(pReg++)      );
            __M4U_BACKUP(m4u_base, REG_MMU_MAU_RW(m4u_slave)        ,   *(pReg++)      );
            __M4U_BACKUP(m4u_base, REG_MMU_MAU_VA(m4u_slave)        ,   *(pReg++)      );
        }
    }

    //check register size (to prevent overflow)
    real_size = (pReg - pM4URegBackUp);
    if(real_size > M4U_REG_BACKUP_SIZE)
    {
        m4u_aee_print("m4u_reg overflow! %d>%d\n", real_size, (int)M4U_REG_BACKUP_SIZE);
    }
    gM4u_reg_backup_real_size = real_size;

    return 0;
}

int m4u_reg_restore(void)
{
    unsigned int* pReg = pM4URegBackUp;
    unsigned long m4u_base;
    int m4u_id, m4u_slave;
    int seq, mau;
    unsigned int real_size;

    for(m4u_id=0; m4u_id<TOTAL_M4U_NUM; m4u_id++)
    {
        m4u_base = gM4UBaseAddr[m4u_id];
        __M4U_RESTORE(m4u_base, REG_MMUg_PT_BASE                    , *(pReg++)   );
        __M4U_RESTORE(m4u_base, REG_MMUg_PT_BASE_SEC                , *(pReg++)   );
        __M4U_RESTORE(m4u_base, REG_MMU_SEC_ABORT_INFO              , *(pReg++)   );
        __M4U_RESTORE(m4u_base, REG_MMU_STANDARD_AXI_MODE           , *(pReg++)   );
        __M4U_RESTORE(m4u_base, REG_MMU_PRIORITY                    , *(pReg++)   );
        __M4U_RESTORE(m4u_base, REG_MMU_DCM_DIS                     , *(pReg++)   );
        __M4U_RESTORE(m4u_base, REG_MMU_WR_LEN                      , *(pReg++)   );
        __M4U_RESTORE(m4u_base, REG_MMU_HW_DEBUG                    , *(pReg++)   );
        __M4U_RESTORE(m4u_base, REG_MMU_NON_BLOCKING_DIS            , *(pReg++)   );
        __M4U_RESTORE(m4u_base, REG_MMU_LEGACY_4KB_MODE             , *(pReg++)   );
        __M4U_RESTORE(m4u_base, REG_MMU_PFH_DIST0                   , *(pReg++)   );
        __M4U_RESTORE(m4u_base, REG_MMU_PFH_DIST1                   , *(pReg++)   );
        __M4U_RESTORE(m4u_base, REG_MMU_PFH_DIST2                   , *(pReg++)   );
        __M4U_RESTORE(m4u_base, REG_MMU_PFH_DIST3                   , *(pReg++)   );
        __M4U_RESTORE(m4u_base, REG_MMU_PFH_DIST4                   , *(pReg++)   );
        __M4U_RESTORE(m4u_base, REG_MMU_PFH_DIST5                   , *(pReg++)   );
        __M4U_RESTORE(m4u_base, REG_MMU_PFH_DIST6                   , *(pReg++)   );
        __M4U_RESTORE(m4u_base, REG_MMU_PFH_DIST7                   , *(pReg++)   );		
        __M4U_RESTORE(m4u_base, REG_MMU_PFH_DIR0                    , *(pReg++)   );
        __M4U_RESTORE(m4u_base, REG_MMU_PFH_DIR1                    , *(pReg++)   );
        __M4U_RESTORE(m4u_base, REG_MMU_CTRL_REG                    , *(pReg++)   );
        __M4U_RESTORE(m4u_base, REG_MMU_IVRP_PADDR                  , *(pReg++)   );
        __M4U_RESTORE(m4u_base, REG_MMU_INT_L2_CONTROL              , *(pReg++)   );
        __M4U_RESTORE(m4u_base, REG_MMU_INT_MAIN_CONTROL            , *(pReg++)   );
         
        for(m4u_slave=0; m4u_slave < M4U_SLAVE_NUM(m4u_id); m4u_slave++)
        {
            for(seq=0; seq<SEQ_NR_PER_M4U_SLAVE; seq++)
            {
                __M4U_RESTORE(m4u_base, REG_MMU_SQ_START(m4u_slave, seq)  ,    *(pReg++)      );
                __M4U_RESTORE(m4u_base, REG_MMU_SQ_END(m4u_slave, seq)    ,    *(pReg++)      );
            }

            for(mau=0; mau<MAU_NR_PER_M4U_SLAVE; mau++)
            {
                __M4U_RESTORE(m4u_base, REG_MMU_MAU_START(m4u_slave,mau)       ,  *(pReg++) );
                __M4U_RESTORE(m4u_base, REG_MMU_MAU_START_BIT32(m4u_slave,mau) ,  *(pReg++) );
                __M4U_RESTORE(m4u_base, REG_MMU_MAU_END(m4u_slave,mau)         ,  *(pReg++) );
                __M4U_RESTORE(m4u_base, REG_MMU_MAU_END_BIT32(m4u_slave,mau)   ,  *(pReg++) );
                __M4U_RESTORE(m4u_base, REG_MMU_MAU_PORT_EN(m4u_slave,mau)     ,  *(pReg++) );
            }
            __M4U_RESTORE(m4u_base, REG_MMU_MAU_LARB_EN(m4u_slave)   ,   *(pReg++)      );
            __M4U_RESTORE(m4u_base, REG_MMU_MAU_IO(m4u_slave)        ,   *(pReg++)      );
            __M4U_RESTORE(m4u_base, REG_MMU_MAU_RW(m4u_slave)        ,   *(pReg++)      );
            __M4U_RESTORE(m4u_base, REG_MMU_MAU_VA(m4u_slave)        ,   *(pReg++)      );
        }

    }

    //check register size (to prevent overflow)
    real_size = (pReg - pM4URegBackUp);
    if(real_size != gM4u_reg_backup_real_size)
    {
        m4u_aee_print("m4u_reg_retore %d!=%d\n", real_size, gM4u_reg_backup_real_size);
    }

    return 0;
}

static unsigned int larb_reg_backup_buf[SMI_LARB_NR][4];

static void larb_backup(struct larb_monitor *h, int larb_idx)
{
    unsigned long larb_base;

    if(larb_idx >= SMI_LARB_NR)
    {
        M4UMSG("error: %s larb_idx = %d\n", __FUNCTION__, larb_idx);
        return;
    }

    larb_base = gLarbBaseAddr[larb_idx];                                     

    __M4U_BACKUP(larb_base, SMI_LARB_MMU_EN, larb_reg_backup_buf[larb_idx][0]);    
    __M4U_BACKUP(larb_base, SMI_LARB_SEC_EN, larb_reg_backup_buf[larb_idx][1]);    
    __M4U_BACKUP(larb_base, SMI_LARB_DOMN_L, larb_reg_backup_buf[larb_idx][2]);    
    __M4U_BACKUP(larb_base, SMI_LARB_DOMN_H, larb_reg_backup_buf[larb_idx][3]);    
                                                                                      
    M4UINFO("larb(%d) backup: 0x%x\n", larb_idx, larb_reg_backup_buf[larb_idx][0]);
                                                            
    return;
}

static void larb_restore(struct larb_monitor *h, int larb_idx)
{
    unsigned long larb_base;

    if(larb_idx >= SMI_LARB_NR)
    {
        M4UMSG("error: %s larb_idx = %d\n", __FUNCTION__, larb_idx);
        return;
    }

    larb_base = gLarbBaseAddr[larb_idx];                                 

    __M4U_RESTORE(larb_base, SMI_LARB_MMU_EN, larb_reg_backup_buf[larb_idx][0]);    
    __M4U_RESTORE(larb_base, SMI_LARB_SEC_EN, larb_reg_backup_buf[larb_idx][1]);    
    __M4U_RESTORE(larb_base, SMI_LARB_DOMN_L, larb_reg_backup_buf[larb_idx][2]);    
    __M4U_RESTORE(larb_base, SMI_LARB_DOMN_H, larb_reg_backup_buf[larb_idx][3]);    
                                                                                                                                   

    M4UINFO("larb(%d) restore: 0x%x (0x%x)\n", larb_idx, M4U_ReadReg32(larb_base, SMI_LARB_MMU_EN), larb_reg_backup_buf[larb_idx][0]);
                               
    return;
}

struct larb_monitor m4u_larb_monitor_handler =
{
    .level = LARB_MONITOR_LEVEL_HIGH,
    .backup = larb_backup,
    .restore = larb_restore	
};

static void m4u_register_larb_monitor(void)
{
    int i;
    for( i=0 ; i < SMI_LARB_NR ; i++)
    {
        larb_clock_on(i);
    }
    register_larb_monitor(&m4u_larb_monitor_handler);
    for( i=0 ; i < SMI_LARB_NR ; i++)
    {
        larb_clock_off(i);
    }
}



void m4u_print_port_status(struct seq_file *seq, int only_print_active)
{
    int port, mmu_en, sec;
    int m4u_index, larb, larb_port;
    unsigned long larb_base;

    M4U_PRINT_LOG_OR_SEQ(seq, "m4u_print_port_status ========>\n");
    
    smi_common_clock_on();
    larb_clock_all_on();

    for(port=0; port<gM4u_port_num; port++)
    {
        m4u_index = m4u_port_2_m4u_id(port);
        if(m4u_index==0)
        {
            larb = m4u_port_2_larb_id(port);
            larb_port = m4u_port_2_larb_port(port);
            larb_base = gLarbBaseAddr[larb];
            
            mmu_en = m4uHw_get_field_by_mask(larb_base, SMI_LARB_MMU_EN, F_SMI_MMU_EN(larb_port, 1));
            sec = m4uHw_get_field_by_mask(larb_base, SMI_LARB_SEC_EN, F_SMI_SEC_EN(larb_port, 1));

        }
        else
        {
            if(port>M4U_PORT_USB0)
            {
                continue;
            }
            
            larb_port = m4u_port_2_larb_port(port);

            mmu_en = m4uHw_get_field_by_mask(gPericfgBaseAddr, REG_PERIAXI_BUS_CTL3,F_PERI_MMU_EN(larb_port, 1));
        }

        if(only_print_active && !mmu_en)
        {
            continue;
        }
        M4U_PRINT_LOG_OR_SEQ(seq, "%s(%d),", m4u_get_port_name(port), !!mmu_en);
    }

    larb_clock_all_off();
    smi_common_clock_off();

    M4U_PRINT_LOG_OR_SEQ(seq, "\n");

}

/*
static int m4u_enable_prefetch(M4U_PORT_ID PortID)
{
    unsigned long m4u_base = gM4UBaseAddr[m4u_port_2_m4u_id(PortID)];
    m4uHw_set_field_by_mask(m4u_base, REG_MMU_CTRL_REG, F_MMU_CTRL_PFH_DIS(1), F_MMU_CTRL_PFH_DIS(0));
    return 0;
}

static int m4u_disable_prefetch(M4U_PORT_ID PortID)
{
    unsigned long m4u_base = gM4UBaseAddr[m4u_port_2_m4u_id(PortID)];
    m4uHw_set_field_by_mask(m4u_base, REG_MMU_CTRL_REG, F_MMU_CTRL_PFH_DIS(1), F_MMU_CTRL_PFH_DIS(1));

    return 0;
}

static int m4u_enable_error_hang(int m4u_id)
{
    unsigned long m4u_base = gM4UBaseAddr[m4u_id];
    m4uHw_set_field_by_mask(m4u_base, REG_MMU_CTRL_REG, F_MMU_CTRL_INT_HANG_en(1), F_MMU_CTRL_INT_HANG_en(1));

    return 0;
}

static int m4u_disable_error_hang(int m4u_id)
{
    unsigned long m4u_base = gM4UBaseAddr[m4u_id];
    m4uHw_set_field_by_mask(m4u_base, REG_MMU_CTRL_REG, F_MMU_CTRL_INT_HANG_en(1), F_MMU_CTRL_INT_HANG_en(0));

    return 0;
}
*/

int m4u_register_reclaim_callback(int port, m4u_reclaim_mva_callback_t *fn, void* data)
{
    if(port > M4U_PORT_UNKNOWN)
    {
        M4UMSG("%s fail, port=%d\n", __FUNCTION__, port);
        return -1;
    }
    gM4uPort[port].reclaim_fn= fn;
    gM4uPort[port].reclaim_data= data;
    return 0;
}
int m4u_unregister_reclaim_callback(int port)
{
    if(port > M4U_PORT_UNKNOWN)
    {
        M4UMSG("%s fail, port=%d\n", __FUNCTION__, port);
        return -1;
    }
    gM4uPort[port].reclaim_fn= NULL;
    gM4uPort[port].reclaim_data= NULL;
    return 0;
}

int m4u_reclaim_notify(int port, unsigned int mva, unsigned int size)
{
    int i;
    for(i=0; i<M4U_PORT_UNKNOWN; i++)
    {
        if(gM4uPort[i].reclaim_fn)
            gM4uPort[i].reclaim_fn(port, mva, size, gM4uPort[i].reclaim_data);
    }
    return 0;
}

int m4u_register_fault_callback(int port, m4u_fault_callback_t *fn, void* data)
{
    if(port > M4U_PORT_UNKNOWN)
    {
        M4UMSG("%s fail, port=%d\n", __FUNCTION__, port);
        return -1;
    }
    gM4uPort[port].fault_fn= fn;
    gM4uPort[port].fault_data= data;
    return 0;
}
int m4u_unregister_fault_callback(int port)
{
    if(port > M4U_PORT_UNKNOWN)
    {
        M4UMSG("%s fail, port=%d\n", __FUNCTION__, port);
        return -1;
    }
    gM4uPort[port].fault_fn= NULL;
    gM4uPort[port].fault_data= NULL;
    return 0;
}

int m4u_enable_tf(int port, bool fgenable)
{
    gM4uPort[port].enable_tf = fgenable;
    return 0;
}

//==============================================================================
static struct timer_list m4u_isr_pause_timer;

static void m4u_isr_restart(unsigned long unused)
{
	M4UMSG("restart m4u irq\n");
	m4u_intr_modify_all(1);
}

static int m4u_isr_pause_timer_init(void)
{
	init_timer(&m4u_isr_pause_timer);
	m4u_isr_pause_timer.function = m4u_isr_restart;
	return 0;
}

static int m4u_isr_pause(int delay)
{
	m4u_intr_modify_all(0);	//disable all intr
	m4u_isr_pause_timer.expires = jiffies + delay*HZ; //delay seconds
	add_timer(&m4u_isr_pause_timer);
	M4UMSG("warning: stop m4u irq for %ds\n", delay);
	return 0;
}

static void m4u_isr_record(void)
{
	static int m4u_isr_cnt=0;
	static unsigned long first_jiffies=0;

	//we allow one irq in 1s, or we will disable them after 5s.
	if(!m4u_isr_cnt || time_after(jiffies, first_jiffies + m4u_isr_cnt*HZ))
	{
		m4u_isr_cnt = 1;
		first_jiffies = jiffies;
	}
	else
	{
		m4u_isr_cnt++;
		if(m4u_isr_cnt >= 5)
		{
			//5 irqs come in 5s, too many !
			//disable irq for a while, to avoid HWT timeout
			m4u_isr_pause(10);
			m4u_isr_cnt=0;
		}
	}
}

#define MMU_INT_REPORT(mmu,mmu_2nd_id,id) M4UMSG("iommu%d_%d " #id "(0x%x) int happens!!\n", mmu,mmu_2nd_id, id)

irqreturn_t MTK_M4U_isr(int irq, void *dev_id)
{
    unsigned long m4u_base;
    unsigned int m4u_index;	
    								  
    if(irq == gM4uDev->irq_num[0])
    {
        m4u_base = gM4UBaseAddr[0];
        m4u_index = 0;
    }
    else if(irq == gM4uDev->irq_num[1])
    {
        m4u_base = gM4UBaseAddr[1];
        m4u_index = 1;
    }
    else
    {
        M4UMSG("MTK_M4U_isr(), Invalid irq number %d\n", irq);
        return -1;                              	 	
    }

    {
        //L2 interrupt
        unsigned int regval = M4U_ReadReg32(m4u_base, REG_MMU_L2_FAULT_ST);
        M4UMSG("m4u L2 interrupt sta=0x%x\n", regval);
        
        if(regval&F_INT_L2_MULTI_HIT_FAULT)
        {
            MMU_INT_REPORT(m4u_index, 0, F_INT_L2_MULTI_HIT_FAULT);
        }
        if(regval&F_INT_L2_TABLE_WALK_FAULT)
        {
            unsigned int fault_va, layer;
            MMU_INT_REPORT(m4u_index, 0, F_INT_L2_TABLE_WALK_FAULT);
            fault_va = M4U_ReadReg32(m4u_base, REG_MMU_TBWALK_FAULT_VA);
            layer = fault_va&1;
            fault_va &= (~1);
            m4u_aee_print("L2 table walk fault: mva=0x%x, layer=%d\n", fault_va, layer);
            
        }
        if(regval&F_INT_L2_PFH_DMA_FIFO_OVERFLOW)
        {
            MMU_INT_REPORT(m4u_index, 0, F_INT_L2_PFH_DMA_FIFO_OVERFLOW);
        }
        if(regval&F_INT_L2_MISS_DMA_FIFO_OVERFLOW)
        {
            MMU_INT_REPORT(m4u_index, 0, F_INT_L2_MISS_DMA_FIFO_OVERFLOW);
        }
        if(regval&F_INT_L2_INVALD_DONE)
        {
            MMU_INT_REPORT(m4u_index, 0, F_INT_L2_INVALD_DONE);
        }
        if(regval&F_INT_L2_PFH_OUT_FIFO_ERROR)
        {
            MMU_INT_REPORT(m4u_index, 0, F_INT_L2_PFH_OUT_FIFO_ERROR);
        }
        if(regval&F_INT_L2_PFH_IN_FIFO_ERROR)
        {
            MMU_INT_REPORT(m4u_index, 0, F_INT_L2_PFH_IN_FIFO_ERROR);
        }
        if(regval&F_INT_L2_MISS_OUT_FIFO_ERROR)
        {
            MMU_INT_REPORT(m4u_index, 0, F_INT_L2_MISS_OUT_FIFO_ERROR);
        }
        if(regval&F_INT_L2_MISS_IN_FIFO_ERR)
        {
            MMU_INT_REPORT(m4u_index, 0, F_INT_L2_MISS_IN_FIFO_ERR);
        }
        
    }
    
    	
    {
        unsigned int IntrSrc = M4U_ReadReg32(m4u_base, REG_MMU_MAIN_FAULT_ST);
        int m4u_slave_id;
        unsigned int regval;
        int layer, write, m4u_port;
        unsigned int fault_mva, fault_pa;
        
        M4UMSG("m4u main interrupt happened: sta=0x%x\n", IntrSrc);

        if(IntrSrc & (F_INT_MMU0_MAIN_MSK | F_INT_MMU0_MAU_MSK))
        {
            m4u_slave_id = 0;
        }
        else if(IntrSrc & (F_INT_MMU1_MAIN_MSK | F_INT_MMU1_MAU_MSK))
        {
            m4u_slave_id = 1;
        }
        else
        {
            M4UMSG("m4u interrupt error: status = 0x%x\n", IntrSrc);
            m4u_clear_intr(m4u_index);
            return 0;
        }

        //read error info from registers
        fault_mva = M4U_ReadReg32(m4u_base, REG_MMU_FAULT_VA(m4u_slave_id));
        layer = !!(fault_mva & F_MMU_FAULT_VA_LAYER_BIT);
        write = !!(fault_mva & F_MMU_FAULT_VA_WRITE_BIT);
        fault_mva &= F_MMU_FAULT_VA_MSK;
        fault_pa = M4U_ReadReg32(m4u_base, REG_MMU_INVLD_PA(m4u_slave_id));
        regval = M4U_ReadReg32(m4u_base, REG_MMU_INT_ID(m4u_slave_id));
        m4u_port = m4u_get_port_by_tf_id(m4u_index, regval);
        
        //dump something quickly
        //m4u_dump_rs_info(m4u_index, m4u_slave_id);
        m4u_dump_invalid_main_tlb(m4u_index, m4u_slave_id);

        if(IntrSrc & F_INT_TRANSLATION_FAULT(m4u_slave_id))
        {
            int bypass_DISP_TF = 0;
            MMU_INT_REPORT(m4u_index, m4u_slave_id, F_INT_TRANSLATION_FAULT(m4u_slave_id));
            M4UMSG("fault: port=%s, mva=0x%x, pa=0x%x, layer=%d, wr=%d, 0x%x\n", 
                m4u_get_port_name(m4u_port), fault_mva, fault_pa, layer, write, regval);
            
            if(M4U_PORT_DISP_OVL0 == m4u_port || M4U_PORT_DISP_OVL1 == m4u_port)
            {
                unsigned int valid_mva = 0;
                unsigned int valid_size = 0;			
                unsigned int valid_mva_end = 0;							
                m4u_query_mva_info(fault_mva-1, 0, &valid_mva, &valid_size);
                if(0 != valid_mva && 0 != valid_size)
                {
                    valid_mva_end = valid_mva+valid_size;
                }
				
                if(0 != valid_mva_end && fault_mva < valid_mva_end+SZ_4K)
                {
                    M4UMSG("bypass disp TF, valid mva=0x%x, size=0x%x, mva_end=0x%x\n", valid_mva, valid_size, valid_mva_end);
                    bypass_DISP_TF = 1;
                }
            }

            if(gM4uPort[m4u_port].enable_tf == 1 && bypass_DISP_TF == 0)
            {
                m4u_dump_pte_nolock(m4u_get_domain_by_port(m4u_port), fault_mva);
    
                m4u_print_port_status(NULL, 1);
    
                M4UMSG("bus sel= 0x%x", M4U_ReadReg32(gSMICOMMONBaseAddr, REG_SMI_BUS_SEL));    
    
                //call user's callback to dump user registers
                if(m4u_port < M4U_PORT_UNKNOWN && gM4uPort[m4u_port].fault_fn)
                    gM4uPort[m4u_port].fault_fn(m4u_port, fault_mva, gM4uPort[m4u_port].fault_data);
    
                m4u_dump_buf_info(NULL);
                //m4u_dump_reg(m4u_index,0);
                m4u_aee_print("\nCRDISPATCH_KEY:M4U_%s\ntranslation fault: port=%s, mva=0x%x, pa=0x%x\n", 
                            m4u_get_port_name(m4u_port), m4u_get_port_name(m4u_port), 
                            fault_mva, fault_pa);
            }
            MMProfileLogEx(M4U_MMP_Events[M4U_MMP_M4U_ERROR], MMProfileFlagPulse, m4u_port, fault_mva);
        }
        if(IntrSrc & F_INT_MAIN_MULTI_HIT_FAULT(m4u_slave_id))
        {
            MMU_INT_REPORT(m4u_index, m4u_slave_id, F_INT_MAIN_MULTI_HIT_FAULT(m4u_slave_id));
        }
        if(IntrSrc & F_INT_INVALID_PHYSICAL_ADDRESS_FAULT(m4u_slave_id))
        {
            if(! (IntrSrc & F_INT_TRANSLATION_FAULT(m4u_slave_id)))
            {
                MMU_INT_REPORT(m4u_index, m4u_slave_id, F_INT_INVALID_PHYSICAL_ADDRESS_FAULT(m4u_slave_id));
                
            }
        }
        if(IntrSrc & F_INT_ENTRY_REPLACEMENT_FAULT(m4u_slave_id))
        {
            MMU_INT_REPORT(m4u_index, m4u_slave_id, F_INT_ENTRY_REPLACEMENT_FAULT(m4u_slave_id));
        }
        if(IntrSrc & F_INT_TLB_MISS_FAULT(m4u_slave_id))
        {
            MMU_INT_REPORT(m4u_index, m4u_slave_id, F_INT_TLB_MISS_FAULT(m4u_slave_id));
        }
        if(IntrSrc & F_INT_MISS_FIFO_ERR(m4u_slave_id))
        {
            MMU_INT_REPORT(m4u_index, m4u_slave_id, F_INT_MISS_FIFO_ERR(m4u_slave_id));
        }		
        if(IntrSrc & F_INT_PFH_FIFO_ERR(m4u_slave_id))
        {
            MMU_INT_REPORT(m4u_index, m4u_slave_id, F_INT_PFH_FIFO_ERR(m4u_slave_id));
        }

        if(IntrSrc & F_INT_MAU(m4u_slave_id, 0))
        {
            MMU_INT_REPORT(m4u_index, m4u_slave_id, F_INT_MAU(m4u_slave_id, 0));

            __mau_dump_status(m4u_index, m4u_slave_id, 0);
        }
        if(IntrSrc & F_INT_MAU(m4u_slave_id, 1))
        {
            MMU_INT_REPORT(m4u_index, m4u_slave_id, F_INT_MAU(m4u_slave_id, 1));
            __mau_dump_status(m4u_index, m4u_slave_id, 1);
        }
        if(IntrSrc & F_INT_MAU(m4u_slave_id, 2))
        {
            MMU_INT_REPORT(m4u_index, m4u_slave_id, F_INT_MAU(m4u_slave_id, 2));
            __mau_dump_status(m4u_index, m4u_slave_id, 2);
        }
        if(IntrSrc & F_INT_MAU(m4u_slave_id, 3))
        {
            MMU_INT_REPORT(m4u_index, m4u_slave_id, F_INT_MAU(m4u_slave_id, 3));
            __mau_dump_status(m4u_index, m4u_slave_id, 3);
        }
        
        m4u_clear_intr(m4u_index);
		m4u_isr_record();
    }
    
    return IRQ_HANDLED;
}


m4u_domain_t * m4u_get_domain_by_port(M4U_PORT_ID port)
{
    return &gM4uDomain;
}

m4u_domain_t * m4u_get_domain_by_id(int id)
{
    return &gM4uDomain;
}

int m4u_get_domain_nr()
{
    return 1;
}



int m4u_reg_init(m4u_domain_t* m4u_domain, unsigned long ProtectPA, int m4u_id)
{
    unsigned int regval;
    int i;

    M4UINFO("m4u_reg_init, ProtectPA = 0x%lx\n", ProtectPA);

    //m4u clock is in infra domain, we never close this clock.
    m4u_clock_on();
//=============================================
// SMI registers
//=============================================
    if(0 == m4u_id)
    {
        struct device_node *node = NULL;	
    
        node = of_find_compatible_node(NULL, NULL,"mediatek,SMI_COMMON");
        gSMICOMMONBaseAddr = (unsigned long)of_iomap(node, 0);

        M4UINFO("gSMICOMMONBaseAddr: 0x%lx\n", gSMICOMMONBaseAddr);   
    }

//=========================================
//larb init
//=========================================
    if(0 == m4u_id)
    {
        struct device_node *node = NULL;    
        for(i=0; i<SMI_LARB_NR; i++)
        {
            node = of_find_compatible_node(NULL, NULL,gM4U_SMILARB[i]);
            if(NULL ==  node)
                M4UINFO("init larb %d error\n", i);				
            else
            {
                gLarbBaseAddr[i] = (unsigned long)of_iomap(node, 0);
                M4UINFO("init larb %d, 0x%lx\n", i, gLarbBaseAddr[i]);
                //set mm engine domain to 3
                larb_clock_on(i);
                M4U_WriteReg32(gLarbBaseAddr[i], SMI_LARB_DOMN_L, 0xffffffff);
                M4U_WriteReg32(gLarbBaseAddr[i], SMI_LARB_DOMN_H, 0xffffffff);
                larb_clock_off(i);
            }
        }
    }
    
//=========================================
//perisys init
//=========================================
    if(1 == m4u_id)
    {
        struct device_node *node = NULL;	
    
        node = of_find_compatible_node(NULL, NULL,"mediatek,PERICFG");
        gPericfgBaseAddr = (unsigned long)of_iomap(node, 0);

        M4UINFO("gPericfgBaseAddr: 0x%lx\n", gPericfgBaseAddr);   
    }
    
//=============================================
// m4u registers
//=============================================
    M4UINFO("m4u hw init id = %d, base address: 0x%lx, pgd_pa: 0x%x\n", m4u_id, gM4UBaseAddr[m4u_id], (unsigned int)m4u_domain->pgd_pa);
    
    {
        M4U_WriteReg32(gM4UBaseAddr[m4u_id], REG_MMUg_PT_BASE, (unsigned int)m4u_domain->pgd_pa);
        M4U_WriteReg32(gM4UBaseAddr[m4u_id], REG_MMUg_PT_BASE_SEC, (unsigned int)m4u_domain->pgd_pa);

        regval = M4U_ReadReg32(gM4UBaseAddr[m4u_id], REG_MMU_CTRL_REG);
        
        if(0 == m4u_id)        
        {// mm_iommu
            regval = regval|F_MMU_CTRL_PFH_DIS(0)         \
                    |F_MMU_CTRL_TLB_WALK_DIS(0)    \
                    |F_MMU_CTRL_MONITOR_EN(0)       \
                    |F_MMU_CTRL_MONITOR_CLR(0)     \
                    |F_MMU_CTRL_REROUTE_PFQ_TO_MQ(1) \
                    |F_MMU_CTRL_TF_PROT_VAL(2)    \
                    |F_MMU_CTRL_INT_HANG_en(0)      \
                    |F_MMU_CTRL_COHERE_EN(1)       ;
        }        
        else
        {// peri_iommu
            regval = regval|F_MMU_CTRL_PFH_DIS(1)         \
                    |F_MMU_CTRL_TLB_WALK_DIS(0)    \
                    |F_MMU_CTRL_MONITOR_EN(0)       \
                    |F_MMU_CTRL_MONITOR_CLR(0)     \
                    |F_MMU_CTRL_REROUTE_PFQ_TO_MQ(0) \
                    |F_MMU_CTRL_TF_PROT_VAL(2)    \
                    |F_MMU_CTRL_INT_HANG_en(0)      \
                    |F_MMU_CTRL_COHERE_EN(1)       ;	    
        }
        
        M4U_WriteReg32(gM4UBaseAddr[m4u_id], REG_MMU_CTRL_REG, regval);

        //enable all interrupts
        m4u_enable_intr(m4u_id);
		
        //set translation fault proctection buffer address
        if(!gM4U_4G_DRAM_Mode)
            M4U_WriteReg32(gM4UBaseAddr[m4u_id], REG_MMU_IVRP_PADDR, (unsigned int)F_MMU_IVRP_PA_SET(ProtectPA));
        else
            M4U_WriteReg32(gM4UBaseAddr[m4u_id], REG_MMU_IVRP_PADDR, (unsigned int)F_MMU_IVRP_4G_DRAM_PA_SET(ProtectPA));

        //enable DCM
        M4U_WriteReg32(gM4UBaseAddr[m4u_id], REG_MMU_DCM_DIS, 0);
        
        m4u_invalid_tlb_all(m4u_id);

    }

    //special settings for mmu0 (multimedia iommu) 
    if(0 == m4u_id)
    {
        unsigned long m4u_base=gM4UBaseAddr[0];
        //2 disable in-order-write
        m4uHw_set_field_by_mask(m4u_base, REG_MMU_CTRL_REG, 
                        F_MMU_CTRL_IN_ORDER_WR(1), F_MMU_CTRL_IN_ORDER_WR(0));
        //3 non-standard AXI mode
        M4U_WriteReg32(m4u_base, REG_MMU_STANDARD_AXI_MODE, 0);
        //4 write command throttling mode
        m4uHw_set_field_by_mask(m4u_base, REG_MMU_WR_LEN, F_BIT_SET(5), 0);
    }

    return 0;
}

int m4u_domain_init(struct m4u_device *m4u_dev, void* priv_reserve)
{
    M4UINFO("m4u_domain_init\n");

    memset(&gM4uDomain, 0, sizeof(gM4uDomain));
    gM4uDomain.pgsize_bitmap = M4U_PGSIZES;
    mutex_init(&gM4uDomain.pgtable_mutex);

    m4u_pgtable_init(m4u_dev, &gM4uDomain);

    m4u_mvaGraph_init(priv_reserve);	

    return 0;
}

int m4u_reset(int m4u_id)
{
    m4u_invalid_tlb_all(m4u_id);
    m4u_clear_intr(m4u_id);

    return 0;
}

int m4u_hw_init(struct m4u_device *m4u_dev, int m4u_id)
{
    unsigned long pProtectVA;
    phys_addr_t ProtectPA;

    gM4U_4G_DRAM_Mode = enable_4G();

    M4UMSG("4G DRAM Mode is: %d\n", gM4U_4G_DRAM_Mode);
    
    gM4UBaseAddr[m4u_id] = m4u_dev->m4u_base[m4u_id];
    
    pProtectVA = (unsigned long) kmalloc(TF_PROTECT_BUFFER_SIZE*2, GFP_KERNEL|__GFP_ZERO);
    if(NULL==(void*)pProtectVA)
    {
        M4UMSG("Physical memory not available.\n");
        return -1;
    }
    pProtectVA = (pProtectVA+(TF_PROTECT_BUFFER_SIZE-1))&(~(TF_PROTECT_BUFFER_SIZE-1));
    ProtectPA = virt_to_phys((void *)pProtectVA);
    if(ProtectPA & (TF_PROTECT_BUFFER_SIZE-1))
    {
        M4UMSG("protect buffer (0x%lx) not align.\n",ProtectPA);
        return -1;
    }
           
    M4UINFO("protect memory va=0x%lx, pa=0x%p.\n", pProtectVA, ProtectPA);
           
    pM4URegBackUp = (unsigned int*)kmalloc(M4U_REG_BACKUP_SIZE, GFP_KERNEL|__GFP_ZERO);
    if(pM4URegBackUp==NULL)
    {
        M4UMSG("Physical memory not available size=%d.\n", M4U_REG_BACKUP_SIZE);
        return -1;
    }    

    spin_lock_init(&gM4u_reg_lock);

    m4u_reg_init(&gM4uDomain,ProtectPA, m4u_id);

    if(0 == m4u_id)
        m4u_register_larb_monitor();
    
    if (request_irq(m4u_dev->irq_num[m4u_id], MTK_M4U_isr, IRQF_TRIGGER_LOW, "m4u", NULL))
    {
        M4UERR("request M4U%d IRQ line failed\n",m4u_id);
        return -ENODEV;
    }
    else
    {
        M4UINFO("request_irq, irq_num=%d\n", m4u_dev->irq_num[m4u_id]);    
    }

	m4u_isr_pause_timer_init();

    m4u_monitor_start(m4u_id);

//    mau_start_monitor(0, 0, 0, 1, 1, 0, 0, 0x0, 0xfffff, 0xffffffff, 0xffffffff);
//    mau_start_monitor(0, 0, 1, 0, 1, 0, 0, 0x0, 0xfffff, 0xffffffff, 0xffffffff);	
//    mau_start_monitor(0, 1, 0, 1, 1, 0, 0, 0x0, 0xfffff, 0xffffffff, 0xffffffff);
//    mau_start_monitor(0, 1, 1, 0, 1, 0, 0, 0x0, 0xfffff, 0xffffffff, 0xffffffff);
	
    // config MDP related port default use M4U
    if(0 == m4u_id)    
    {
        M4U_PORT_STRUCT port;
        port.Direction = 0;
        port.Distance = 1;
        port.domain = 3;
        port.Security = 0;
        port.Virtuality = 1;
        
        port.ePortID = M4U_PORT_MDP_RDMA0;
        m4u_config_port(&port);
        port.ePortID = M4U_PORT_MDP_RDMA1;
        m4u_config_port(&port);
        port.ePortID = M4U_PORT_MDP_WROT0;
        m4u_config_port(&port);
        port.ePortID = M4U_PORT_MDP_WROT1;
        m4u_config_port(&port);
        port.ePortID = M4U_PORT_MDP_WDMA;
        m4u_config_port(&port);
    }

    return 0;
}

int m4u_hw_deinit(struct m4u_device *m4u_dev, int m4u_id)
{

#if 1
    free_irq(m4u_dev->irq_num[m4u_id], NULL);
#else
    free_irq(MM_IOMMU_IRQ_B_ID, NULL);
    free_irq(PERISYS_IOMMU_IRQ_B_ID, NULL);
#endif	
    return 0;
}


int m4u_dump_reg_for_smi_hang_issue(void)
{
    /*NOTES: m4u_monitor_start() must be called before using m4u */
    /*please check m4u_hw_init() to ensure that */

    M4UMSG("====== dump m4u reg start =======>\n");

    if(0 == gM4UBaseAddr[0])	
    {
        M4UMSG("gM4UBaseAddr[0] is NULL\n");
        return 0;
    }
    M4UMSG("0x44 = 0x%x\n", M4U_ReadReg32(gM4UBaseAddr[0], 0x44));
	
    m4u_print_perf_counter(0, 0, "m4u");
    m4u_print_perf_counter(0, 1, "m4u");
    m4u_dump_rs_info(0, 0);
    m4u_dump_rs_info(0, 1);

    return 0;
}



