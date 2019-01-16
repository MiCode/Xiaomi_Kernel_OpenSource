#ifndef _MT6595_M4U_REG_H__
#define _MT6595_M4U_REG_H__


#define M4U_BASE0                0xf0205000
#define M4U_BASE1                0xf0214000

#define LARB0_BASE 	0xf4021000
#define LARB1_BASE 	0xf6010000
#define LARB2_BASE 	0xf5001000
#define LARB3_BASE 	0xf8001000
#define LARB4_BASE 	0xf7002000

//smi_common register is in mmsys domain
//needs to call smi_common_clock_on/off when access these registers
#define SMI_COMMON_EXT_BASE 0xf4022000

//=================================================
//common macro definitions
#define F_VAL(val,msb,lsb) (((val)&((1<<(msb-lsb+1))-1))<<lsb)
#define F_MSK(msb, lsb)     F_VAL(0xffffffff, msb, lsb)
#define F_BIT_SET(bit)          (1<<(bit))
#define F_BIT_VAL(val,bit)  ((!!(val))<<(bit))
#define F_MSK_SHIFT(regval,msb,lsb) (((regval)&F_MSK(msb,lsb))>>lsb)


//=====================================================
//M4U register definition
//=====================================================

#define REG_MMUg_PT_BASE           (0x0)
 #define F_MMUg_PT_VA_MSK        0xffff0000
#define REG_MMUg_PT_BASE_SEC         (0x4)
 #define F_MMUg_PT_VA_MSK_SEC        0xffff0000


#define REG_MMU_PROG_EN                 0x10
    #define F_MMU0_PROG_EN               1
    #define F_MMU1_PROG_EN               2
#define REG_MMU_PROG_VA                 0x14
    #define F_PROG_VA_LOCK_BIT             (1<<11)
    #define F_PROG_VA_LAYER_BIT            F_BIT_SET(9)
    #define F_PROG_VA_SIZE16X_BIT          F_BIT_SET(8)
    #define F_PROG_VA_SECURE_BIT           (1<<7)
    #define F_PROG_VA_MASK            0xfffff000

#define REG_MMU_PROG_DSC                0x18

#define REG_MMU_INVLD          	 (0x20)    
    #define F_MMU_INV_ALL         	 0x2   
    #define F_MMU_INV_RANGE       	 0x1   

#define REG_MMU_INVLD_SA        	 (0x24)     
#define REG_MMU_INVLD_EA          (0x28)    


#define REG_MMU_INVLD_SEC           (0x2c)
     #define F_MMU_INV_SEC_ALL          0x2 
     #define F_MMU_INV_SEC_RANGE        0x1 
                                     
#define REG_MMU_INVLD_SA_SEC        (0x30)	    
#define REG_MMU_INVLD_EA_SEC        (0x34)

#define REG_INVLID_SEL           	 (0x38)
 #define F_MMU_INV_EN_L1    	 (1<<0)
 #define F_MMU_INV_EN_L2    	 (1<<1)     


#define REG_INVLID_SEL_SEC          (0x3c)
 #define F_MMU_INV_SEC_EN_L1    	 (1<<0)
 #define F_MMU_INV_SEC_EN_L2    	 (1<<1)     
 #define F_MMU_INV_SEC_INV_DONE     (1<<2)
 #define F_MMU_INV_SEC_INV_INT_CLR   (1<<3)
 #define F_MMU_INV_SEC_INV_INT_EN    (1<<4)
 #define F_MMU_INV_SEC_DBG         (1<<5)


#define REG_MMU_SEC_ABORT_INFO      (0x40)
#define REG_MMU_STANDARD_AXI_MODE   (0x48)

#define REG_MMU_PRIORITY        (0x4c)
#define REG_MMU_DCM_DIS         (0x50)
#define REG_MMU_WR_LEN          (0x54)
#define REG_MMU_HW_DEBUG        (0x58)
    #define F_MMU_HW_DBG_L2_SCAN_ALL    F_BIT_SET(1)
    #define F_MMU_HW_DBG_PFQ_BRDCST     F_BIT_SET(0)

#define REG_MMU_NON_BLOCKING_DIS    0x5C
    #define F_MMU_NON_BLOCK_DISABLE_BIT     1
    #define F_MMU_NON_BLOCK_HALF_ENTRY_BIT  2

#define REG_MMU_LEGACY_4KB_MODE     (0x60)

#define REG_MMU_PFH_DIST0           0x80
#define REG_MMU_PFH_DIST1           0x84
#define REG_MMU_PFH_DIST2           0x88
#define REG_MMU_PFH_DIST3           0x8c
#define REG_MMU_PFH_DIST4           0x90
#define REG_MMU_PFH_DIST5           0x94
#define REG_MMU_PFH_DIST6           0x98
#define REG_MMU_PFH_DIST7           0x9c

#define REG_MMU_PFH_DIST(port)      (0x80+(((port)>>3)<<2))
    #define F_MMU_PFH_DIST_VAL(port,val) ((val&0xf)<<(((port)&0x7)<<2))
    #define F_MMU_PFH_DIST_MASK(port)     F_MMU_PFH_DIST_VAL((port), 0xf)

#define REG_MMU_PFH_DIR0         0xF0
#define REG_MMU_PFH_DIR1         0xF4
#define REG_MMU_PFH_DIR(port)   (((port)<32) ? REG_MMU_PFH_DIR0: REG_MMU_PFH_DIR1)
#define F_MMU_PFH_DIR(port,val) ((!!(val))<<((port)&0x1f))


#define REG_MMU_READ_ENTRY       0x100
    #define F_READ_ENTRY_EN                 F_BIT_SET(31)
    #define F_READ_ENTRY_MM1_MAIN           F_BIT_SET(26)
    #define F_READ_ENTRY_MM0_MAIN           F_BIT_SET(25)
    #define F_READ_ENTRY_MMx_MAIN(id)       F_BIT_SET(25+id)
    #define F_READ_ENTRY_PFH                F_BIT_SET(24)
    #define F_READ_ENTRY_MAIN_IDX(idx)      F_VAL(idx,21,16)
    #define F_READ_ENTRY_PFH_IDX(idx)       F_VAL(idx,11,5)
    //#define F_READ_ENTRY_PFH_HI_LO(high)    F_VAL(high, 4,4)
    //#define F_READ_ENTRY_PFH_PAGE(page)     F_VAL(page, 3,2)
    #define F_READ_ENTRY_PFH_PAGE_IDX(idx)    F_VAL(idx, 4, 2)
    #define F_READ_ENTRY_PFH_WAY(way)       F_VAL(way, 1,0)

#define REG_MMU_DES_RDATA        0x104

#define REG_MMU_PFH_TAG_RDATA    0x108
    #define F_PFH_TAG_VA_GET(mmu,tag)    ((mmu==0)?F_MMU0_PFH_TAG_VA_GET(tag): F_MMU1_PFH_TAG_VA_GET(tag))
    #define F_MMU0_PFH_TAG_VA_GET(tag)    (F_MSK_SHIFT(tag, 13, 4)<<(MMU_SET_MSB_OFFSET(0)+1))
    #define F_MMU1_PFH_TAG_VA_GET(tag)    (F_MSK_SHIFT(tag, 14, 4)<<(MMU_SET_MSB_OFFSET(1)+1))	
    #define F_MMU_PFH_TAG_VA_LAYER0_MSK(mmu)  ((mmu=0)?F_MSK(31, 30):F_MSK(31, 29))
    #define F_PFH_TAG_LAYER_BIT         F_BIT_SET(3)
    #define F_PFH_TAG_16X_BIT           F_BIT_SET(2) //this bit is always 0 -- cost down.
    #define F_PFH_TAG_SEC_BIT           F_BIT_SET(1)
    #define F_PFH_TAG_AUTO_PFH          F_BIT_SET(0)
    
    
// tag releated macro
    #define MMU0_SET_ORDER         7
    #define MMU1_SET_ORDER         6
    #define MMU_SET_ORDER(mmu)      ((mmu==0) ? MMU0_SET_ORDER : MMU1_SET_ORDER)
    #define MMU_SET_NR(mmu)    (1<<MMU_SET_ORDER(mmu))
    #define MMU_SET_LSB_OFFSET               15
    #define MMU_SET_MSB_OFFSET(mmu)         (MMU_SET_LSB_OFFSET+MMU_SET_ORDER(mmu)-1)
    #define MMU_PFH_VA_TO_SET(mmu,va)     F_MSK_SHIFT(va, MMU_SET_MSB_OFFSET(mmu), MMU_SET_LSB_OFFSET)

    #define MMU_PAGE_PER_LINE      8
    #define MMU_WAY_NR  4
    #define MMU_PFH_TOTAL_LINE(mmu) (MMU_SET_NR(mmu)*MMU_WAY_NR)
    
    
#define REG_MMU_CTRL_REG         0x110
    #define F_MMU_CTRL_PFH_DIS(dis)         F_BIT_VAL(dis, 0)
    #define F_MMU_CTRL_TLB_WALK_DIS(dis)    F_BIT_VAL(dis, 1)
    #define F_MMU_CTRL_MONITOR_EN(en)       F_BIT_VAL(en, 2)
    #define F_MMU_CTRL_MONITOR_CLR(clr)       F_BIT_VAL(clr, 3)
    #define F_MMU_CTRL_REROUTE_PFQ_TO_MQ(en)           F_BIT_VAL(en, 4)
    #define F_MMU_CTRL_TF_PROT_VAL(prot)    F_VAL(prot, 6, 5)
    #define F_MMU_CTRL_TF_PROT_MSK           F_MSK(6,5)
    #define F_MMU_CTRL_INT_HANG_en(en)       F_BIT_VAL(en, 7)
    #define F_MMU_CTRL_COHERE_EN(en)        F_BIT_VAL(en, 8)
    #define F_MMU_CTRL_IN_ORDER_WR(en)      F_BIT_VAL(en, 9)
    #define F_MMU_CTRL_MAIN_TLB_SHARE_ALL(en)   F_BIT_VAL(en, 10)
    

#define REG_MMU_IVRP_PADDR       0x114
    #define F_MMU_IVRP_PA_SET(PA)       (PA>>1)
    #define F_MMU_IVRP_4G_DRAM_PA_SET(PA)    ((PA>>1)|(1<<31))
    
#define REG_MMU_INT_L2_CONTROL      0x120
    #define F_INT_L2_CLR_BIT (1<<12)
    #define F_INT_L2_MULTI_HIT_FAULT                   F_BIT_SET(0)
    #define F_INT_L2_TABLE_WALK_FAULT                  F_BIT_SET(1)
    #define F_INT_L2_PFH_DMA_FIFO_OVERFLOW             F_BIT_SET(2)
    #define F_INT_L2_MISS_DMA_FIFO_OVERFLOW            F_BIT_SET(3)
    #define F_INT_L2_INVALD_DONE                       F_BIT_SET(4)
    #define F_INT_L2_PFH_FIFO_ERROR                    F_BIT_SET(5)
    #define F_INT_L2_MISS_FIFO_ERR                     F_BIT_SET(6)

#define REG_MMU_INT_MAIN_CONTROL    0x124
    #define F_INT_TRANSLATION_FAULT(MMU)                 F_BIT_SET(0+(MMU)*7)
    #define F_INT_MAIN_MULTI_HIT_FAULT(MMU)              F_BIT_SET(1+(MMU)*7)
    #define F_INT_INVALID_PHYSICAL_ADDRESS_FAULT(MMU)    F_BIT_SET(2+(MMU)*7)
    #define F_INT_ENTRY_REPLACEMENT_FAULT(MMU)           F_BIT_SET(3+(MMU)*7)
    #define F_INT_TLB_MISS_FAULT(MMU)                    F_BIT_SET(4+(MMU)*7)
    #define F_INT_MISS_FIFO_ERR(MMU)                     F_BIT_SET(5+(MMU)*7)
    #define F_INT_PFH_FIFO_ERR(MMU)                      F_BIT_SET(6+(MMU)*7)

    #define F_INT_MAU(mmu, set)     F_BIT_SET(14+(set)+(mmu<<2))     //(14+(set)+(mmu*4))

    #define F_INT_MMU0_MAIN_MSK          F_MSK(6, 0)
    #define F_INT_MMU1_MAIN_MSK          F_MSK(13, 7)
    #define F_INT_MMU0_MAU_MSK           F_MSK(17, 14)
    #define F_INT_MMU1_MAU_MSK           F_MSK(21, 18)
    
#define REG_MMU_CPE_DONE_SEC    0x128
#define REG_MMU_CPE_DONE        0x12C

#define REG_MMU_L2_FAULT_ST         0x130
    #define F_INT_L2_PFH_OUT_FIFO_ERROR                   F_BIT_SET(5)
    #define F_INT_L2_PFH_IN_FIFO_ERROR                    F_BIT_SET(6)
    #define F_INT_L2_MISS_OUT_FIFO_ERROR             F_BIT_SET(7)
    #define F_INT_L2_MISS_IN_FIFO_ERR                     F_BIT_SET(8)
#define REG_MMU_MAIN_FAULT_ST       0x134

#define REG_MMU_TBWALK_FAULT_VA         0x138
    #define F_MMU_TBWALK_FAULT_VA_MSK   F_MSK(31, 12)
    #define F_MMU_TBWALK_FAULT_LAYER(regval) F_MSK_SHIFT(regval, 0, 0)
    
#define REG_MMU_FAULT_VA(mmu)         (0x13c+((mmu)<<3))
    #define F_MMU_FAULT_VA_MSK        F_MSK(31, 12)
    #define F_MMU_FAULT_VA_WRITE_BIT    F_BIT_SET(1)
    #define F_MMU_FAULT_VA_LAYER_BIT    F_BIT_SET(0)
    
#define REG_MMU_INVLD_PA(mmu)         (0x140+((mmu)<<3))
#define REG_MMU_INT_ID(mmu)             (0x150+((mmu)<<2))
    #define F_MMU0_INT_ID_TF_MSK        (~0x3) //only for MM iommu.

#define REG_MMU_PF_MSCNT            0x160
#define REG_MMU_PF_CNT              0x164
#define REG_MMU_ACC_CNT(mmu)        (0x168+(((mmu)<<3)|((mmu)<<2)))  //(0x168+((mmu)*12)
#define REG_MMU_MAIN_MSCNT(mmu)     (0x16c+(((mmu)<<3)|((mmu)<<2)))
#define REG_MMU_RS_PERF_CNT(mmu)    (0x170+(((mmu)<<3)|((mmu)<<2)))

#define MMU01_SQ_OFFSET (0x600-0x300)
#define REG_MMU_SQ_START(mmu,x)             (0x300+((x)<<3)+((mmu)*MMU01_SQ_OFFSET))
    #define F_SQ_VA_MASK                F_MSK(31, 18)
    #define F_SQ_EN_BIT                 (1<<17)
    //#define F_SQ_MULTI_ENTRY_VAL(x)     (((x)&0xf)<<13)
#define REG_MMU_SQ_END(mmu, x)               (0x304+((x)<<3)+((mmu)*MMU01_SQ_OFFSET))


#define MMU_TOTAL_RS_NR         8
#define REG_MMU_RSx_VA(mmu,x)      (0x380+((x)<<4)+((mmu)*MMU01_SQ_OFFSET))
    #define F_MMU_RSx_VA_GET(regval)    ((regval)&F_MSK(31, 12))
    #define F_MMU_RSx_VA_VALID(regval)  F_MSK_SHIFT(regval, 11, 11)
    #define F_MMU_RSx_VA_PID(regval)    F_MSK_SHIFT(regval, 9, 0)
    
#define REG_MMU_RSx_PA(mmu,x)      (0x384+((x)<<4)+((mmu)*MMU01_SQ_OFFSET))
    #define F_MMU_RSx_PA_GET(regval)    ((regval)&F_MSK(31, 12))

#define REG_MMU_RSx_2ND_BASE(mmu,x) (0x388+((x)<<4)+((mmu)*MMU01_SQ_OFFSET))

#define REG_MMU_RSx_ST(mmu,x)      (0x38c+((x)<<4)+((mmu)*MMU01_SQ_OFFSET))
    #define F_MMU_RSx_ST_LID(regval)    F_MSK_SHIFT(regval, 22, 20)
    #define F_MMU_RSx_ST_WRT(regval)    F_MSK_SHIFT(regval, 12, 12)
    #define F_MMU_RSx_ST_OTHER(regval)  F_MSK_SHIFT(regval, 8, 0)

#define REG_MMU_MAIN_TAG(mmu,x)       (0x500+((x)<<2)+((mmu)*MMU01_SQ_OFFSET))
    #define F_MAIN_TLB_VA_MSK           F_MSK(31, 12)
    #define F_MAIN_TLB_LOCK_BIT         (1<<11)
    #define F_MAIN_TLB_VALID_BIT        (1<<10)
    #define F_MAIN_TLB_LAYER_BIT        F_BIT_SET(9)
    #define F_MAIN_TLB_16X_BIT          F_BIT_SET(8)
    #define F_MAIN_TLB_SEC_BIT          F_BIT_SET(7)
    #define F_MAIN_TLB_INV_DES_BIT      (1<<6)
    #define F_MAIN_TLB_SQ_EN_BIT        (1<<5)
    #define F_MAIN_TLB_SQ_INDEX_MSK     F_MSK(4,1)
    #define F_MAIN_TLB_SQ_INDEX_GET(regval)     F_MSK_SHIFT(regval, 4, 1)


#define REG_MMU_MAU_START(mmu,mau)              (0x900+((mau)*0x20)+((mmu)*0xa0))
#define REG_MMU_MAU_START_BIT32(mmu,mau)        (0x904+((mau)*0x20)+((mmu)*0xa0))
#define REG_MMU_MAU_END(mmu,mau)                (0x908+((mau)*0x20)+((mmu)*0xa0))
#define REG_MMU_MAU_END_BIT32(mmu,mau)          (0x90C+((mau)*0x20)+((mmu)*0xa0))
#define REG_MMU_MAU_PORT_EN(mmu,mau)            (0x910+((mau)*0x20)+((mmu)*0xa0))
#define REG_MMU_MAU_ASSERT_ID(mmu,mau)          (0x914+((mau)*0x20)+((mmu)*0xa0))
    #define F_MMU_MAU_ASSERT_ID_LARB(regval)    F_MSK_SHIFT(regval, 7, 5)
    #define F_MMU_MAU_ASSERT_ID_PORT(regval)    F_MSK_SHIFT(regval, 4, 0)

#define REG_MMU_MAU_ADDR(mmu,mau)               (0x918+((mau)*0x20)+((mmu)*0xa0))
#define REG_MMU_MAU_ADDR_BIT32(mmu,mau)         (0x91C+((mau)*0x20)+((mmu)*0xa0))

#define REG_MMU_MAU_LARB_EN(mmu)                (0x980+((mmu)*0xa0))
    #define F_MAU_LARB_VAL(mau,larb)         ((larb)<<(mau*8))
    #define F_MAU_LARB_MSK(mau)             (0xff<<(mau*8))
#define REG_MMU_MAU_CLR(mmu)                (0x984+((mmu)*0xa0))
#define REG_MMU_MAU_IO(mmu)                (0x988+((mmu)*0xa0))
    #define F_MAU_BIT_VAL(val, mau)     F_BIT_VAL(val, mau)
#define REG_MMU_MAU_RW(mmu)                (0x98c+((mmu)*0xa0))
#define REG_MMU_MAU_VA(mmu)                (0x990+((mmu)*0xa0))
#define REG_MMU_MAU_ASSERT_ST(mmu)                (0x994+((mmu)*0xa0))

#define REG_MMU_PFH_VLD_0   (0x180)
#define REG_MMU_PFH_VLD(mmu, set, way)     (REG_MMU_PFH_VLD_0+(((set)>>5)<<2)+((way)<<((mmu==0)?(MMU0_SET_ORDER - 3):(MMU1_SET_ORDER - 3)))) //+((set/32)*4)+(way*16)
    #define F_MMU_PFH_VLD_BIT(set, way)      F_BIT_SET((set)&0x1f)       // set%32



//================================================================
// SMI larb
//================================================================

#define SMI_LARB_MMU_EN                 (0xf00	)
    #define F_SMI_MMU_EN(port, en)       ((en)<<((port)))
#define SMI_LARB_SEC_EN                (0xf04	)
    #define F_SMI_SEC_EN(port, en)       ((en)<<((port)))
#define SMI_LARB_DOMN_L                 (0xf08	)
#define SMI_LARB_DOMN_H                 (0xf0c	)
#define REG_SMI_LARB_DOMN_OF_PORT(port)     (((port)>15) ? 0xf0c : 0xf08)
    #define F_SMI_DOMN(port, domain)        (((domain)&0x3)<<((((port)>15) ? (port-16) : port)<<1))


/* ===============================================================
 * 					  SMI COMMON
 * =============================================================== */
#define REG_SMI_BUS_SEL	                (0x220)
    #define F_SMI_BUS_SEL_larb0(mmu_idx)     F_VAL(mmu_idx, 1, 0)
    #define F_SMI_BUS_SEL_larb1(mmu_idx)     F_VAL(mmu_idx, 3, 2)
    #define F_SMI_BUS_SEL_larb2(mmu_idx)     F_VAL(mmu_idx, 5, 4)
    #define F_SMI_BUS_SEL_larb3(mmu_idx)     F_VAL(mmu_idx, 7, 6)
    #define F_SMI_BUS_SEL_larb4(mmu_idx)     F_VAL(mmu_idx, 9, 8)

//=========================================================================
//        peripheral system 
//=========================================================================
#define REG_PERIAXI_BUS_CTL3   (0x208)
    #define F_PERI_MMU_EN(port, en)       ((en)<<((port)))



#include <mach/sync_write.h>


static inline unsigned int COM_ReadReg32(unsigned long addr) 
{
    return ioread32((void*)addr);
}
static inline void COM_WriteReg32(unsigned long addr, unsigned int Val)
{          
    mt_reg_sync_writel(Val, (void*)addr);    
}

static inline unsigned int M4U_ReadReg32(unsigned long M4uBase, unsigned int Offset) 
{
    unsigned int val;
    val = COM_ReadReg32((M4uBase+Offset));
    return val;
}
static inline void M4U_WriteReg32(unsigned long M4uBase, unsigned int Offset, unsigned int Val) 
{                   
    COM_WriteReg32((M4uBase+Offset), Val);    
}


static inline void m4uHw_set_field_by_mask(unsigned long M4UBase, unsigned int reg,
                                      unsigned long mask, unsigned int val)
{
    unsigned int regval;
    regval = M4U_ReadReg32(M4UBase, reg);
    regval = (regval & (~mask))|val;
    M4U_WriteReg32(M4UBase, reg, regval);
}
static inline unsigned int m4uHw_get_field_by_mask(unsigned long M4UBase, unsigned int reg,
                                      unsigned int mask)
{
    return M4U_ReadReg32(M4UBase, reg)&mask;
}




#endif 

