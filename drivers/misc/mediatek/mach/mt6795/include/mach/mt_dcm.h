#ifndef _MT_DCM_H
#define _MT_DCM_H

#include "mach/mt_reg_base.h"

#ifdef CONFIG_OF

extern void __iomem  *dcm_USB0_base;
extern void __iomem  *dcm_MSDC0_base;
extern void __iomem  *dcm_MSDC1_base;
extern void __iomem  *dcm_MSDC2_base;
extern void __iomem  *dcm_MSDC3_base;
extern void __iomem  *dcm_PMIC_WRAP_base;
extern void __iomem  *dcm_I2C0_base;
extern void __iomem  *dcm_I2C1_base;
extern void __iomem  *dcm_I2C2_base;
extern void __iomem  *dcm_I2C3_base;
extern void __iomem  *dcm_I2C4_base;
extern void __iomem  *dcm_MJC_CONFIG_base;
extern void __iomem  *dcm_MCUCFG_base;
extern void __iomem  *dcm_TOPCKGEN_base;
extern void __iomem  *dcm_INFRACFG_AO_base;
extern void __iomem  *dcm_M4U_base;
extern void __iomem  *dcm_PERISYS_IOMMU_base;

extern void __iomem  *dcm_PERICFG_base;
extern void __iomem  *dcm_DRAMC0_base;
extern void __iomem  *dcm_DRAMC1_base;
extern void __iomem  *dcm_EMI_base;
extern void __iomem  *dcm_SMI_COMMO_base;


extern void __iomem  *dcm_SMI_LARB0_base;
extern void __iomem  *dcm_SMI_LARB1_base;
extern void __iomem  *dcm_SMI_LARB2_base;
extern void __iomem  *dcm_SMI_LARB3_base;
extern void __iomem  *dcm_SMI_LARB5_base;

extern void __iomem  *dcm_CAM1_base;
extern void __iomem  *dcm_FDVT_base;
extern void __iomem  *dcm_JPGENC_base;
extern void __iomem  *dcm_JPGDEC_base;
extern void __iomem  *dcm_MMSYS_CONFIG_base;
extern void __iomem  *dcm_VENC_base;
extern void __iomem  *dcm_VDEC_GCON_base;

extern void __iomem  *dcm_E3TCM_base;
#endif


#ifdef CONFIG_OF
// APB Module usb2
//USB0@0x11200000 {
#define	USB0_DCM						(dcm_USB0_base+0x700)//0x11200700


// APB Module msdc
//MSDC0@0x11230000 {
//MSDC1@0x11240000 {
//MSDC2@0x11250000 {
//MSDC3@0x11260000 {
#define MSDC0_PATCH_BIT1				(dcm_MSDC0_base + 0x00B4)//0xF1230000
#define MSDC1_PATCH_BIT1				(dcm_MSDC1_base + 0x00B4)//0xF1240000
#define MSDC2_PATCH_BIT1				(dcm_MSDC2_base + 0x00B4)//0xF1250000
#define MSDC3_PATCH_BIT1				(dcm_MSDC3_base + 0x00B4)//0xF1260000


// APB Module pmic_wrap
//PMIC_WRAP@0x1000D000 {
#define PMIC_WRAP_DCM_EN			(dcm_PMIC_WRAP_base+0x144)//0xF000D000

// APB Module i2c
//I2C0@0x11007000 {
//I2C1@0x11008000 {
//I2C2@0x11009000 {
//I2C3@0x11010000 {
//I2C4@0x11011000 {
#define I2C0_I2CREG_HW_CG_EN		(dcm_I2C0_base+0x054)//0xF1007000
#define I2C1_I2CREG_HW_CG_EN		(dcm_I2C1_base+0x054)//0xF1008000
#define I2C2_I2CREG_HW_CG_EN		(dcm_I2C2_base+0x054)//0xF1009000
#define I2C3_I2CREG_HW_CG_EN		(dcm_I2C3_base+0x054)//0xF1010000
#define I2C4_I2CREG_HW_CG_EN		(dcm_I2C4_base+0x054)//0xF1011000


//MJC
// APB Module mjc_config MJC_CONFIG_BASE=0xF7000000
//MJC_CONFIG@0x17000000 {
#define MJC_HW_DCM_DIS        	(dcm_MJC_CONFIG_base+0x0010)//0xF7000010
#define MJC_HW_DCM_DIS_SET    	(dcm_MJC_CONFIG_base+0x0014)//0xF7000014
#define MJC_HW_DCM_DIS_CLR    	(dcm_MJC_CONFIG_base+0x0018)//0xF7000018


//CPUSYS_dcm
//MCUCFG@0x10200000 {
#ifndef CONFIG_ARM64
//MCUCFG_BASE=0xF0200000
#define DCM_MCUSYS_CONFIG		(dcm_MCUCFG_base + 0x001C)//0xF020001C
#define CACHE_CONFIG			(dcm_MCUCFG_base + 0x0100)//0xF0200100
#define ARMPLL_CTL				(dcm_MCUCFG_base + 0x0160)//0xF0200160
#else//MCUCFG_BASE=0xF0200000
#define L2C_SRAM_CTRL			(dcm_MCUCFG_base + 0x0648)//0xF0200648
#define CCI_CLK_CTRL			(dcm_MCUCFG_base + 0x0660)//0xF0200660
#define BUS_FABRIC_DCM_CTRL		(dcm_MCUCFG_base + 0x0668)//0xF0200668
#endif




//AXI bus dcm
//TOPCKGen_dcm,CKSYS_BASE=0xF0000000
//TOPCKGEN@0x10000000 {
#define DCM_CFG                 (dcm_TOPCKGEN_base + 0x0004)//0x10000004


//INFRACFG_AO@0x10001000 {
//CA7 DCM,INFRACFG_AO_BASE=0xF0001000
#define CA7_CKDIV1				(dcm_INFRACFG_AO_base + 0x0008) //0x10001008
#define INFRA_TOPCKGEN_DCMCTL   (dcm_INFRACFG_AO_base + 0x0010) //0x10001010
#define INFRA_TOPCKGEN_DCMDBC   (dcm_INFRACFG_AO_base + 0x0014) //0x10001014


//infra dcm
#define INFRA_GLOBALCON_DCMCTL  (dcm_INFRACFG_AO_base   + 0x0050) //0x10001050
#define INFRA_GLOBALCON_DCMDBC  (dcm_INFRACFG_AO_base   + 0x0054) //0x10001054
#define INFRA_GLOBALCON_DCMFSEL (dcm_INFRACFG_AO_base   + 0x0058) //0x10001058
//M4U@0x10205000 {
#define MM_MMU_DCM_DIS			(dcm_M4U_base           + 0x0050) //0x10205050,M4U_BASE=0xF0205000
//PERISYS_IOMMU@0x10214000 {
#define PERISYS_MMU_DCM_DIS		(dcm_PERISYS_IOMMU_base + 0x0050) //0x10214050,PERISYS_IOMMU_BASE=0xF0214000

//peri dcm
//PERICFG@0x10003000 {
#define PERI_GLOBALCON_DCMCTL        (dcm_PERICFG_base + 0x0050) //0x10003050,PERICFG_BASE=0xF0003000
#define PERI_GLOBALCON_DCMDBC        (dcm_PERICFG_base + 0x0054) //0x10003054
#define PERI_GLOBALCON_DCMFSEL       (dcm_PERICFG_base + 0x0058) //0x10003058

//DRAMC1@0x10011000 {
//DRAMC0@0x10004000 {
#define channel_A_DRAMC_PD_CTRL           (dcm_DRAMC0_base + 0x01DC)//0x100041dc,DRAMC0_BASE=0xF0004000
#define channel_B_DRAMC_PD_CTRL           (dcm_DRAMC1_base + 0x01DC)//0x100111dc,DRAMC1_BASE=0xF0011000



// APB Module smi
//Smi_common dcm
//SMI_COMMON@0x14022000
#define SMI_COMMON_SMI_DCM				(dcm_SMI_COMMO_base+0x300)//0x14022300


// APB Module smi
//Smi_secure dcm
#ifdef CONFIG_ARM64
//SMI_COMMON_AO@0x1000C000 {
#define E3TCM_HW_DCM_DIS   		    (dcm_E3TCM_base+0x010)//0x1000C010
#define E3TCM_HW_DCM_DIS_SET		(dcm_E3TCM_base+0x014)//0x1000C014
#define E3TCM_HW_DCM_DIS_CLR		(dcm_E3TCM_base+0x018)//0x1000C018
#endif


// APB Module smi_larb
//SMI_LARB0@0x14021000 {
#define SMI_LARB0_STA        	(dcm_SMI_LARB0_base + 0x00)//0x14021000
#define SMI_LARB0_CON        	(dcm_SMI_LARB0_base + 0x10)//0x14021010
#define SMI_LARB0_CON_SET       (dcm_SMI_LARB0_base + 0x14)//0x14021014
#define SMI_LARB0_CON_CLR       (dcm_SMI_LARB0_base + 0x18)//0x14021018

//SMI_LARB1@16010000 {
#define SMI_LARB1_STAT        	(dcm_SMI_LARB1_base + 0x00)//0x16010000
#define SMI_LARB1_CON        	(dcm_SMI_LARB1_base + 0x10)//0x16010010
#define SMI_LARB1_CON_SET       (dcm_SMI_LARB1_base + 0x14)//0x16010014
#define SMI_LARB1_CON_CLR       (dcm_SMI_LARB1_base + 0x18)//0x16010018

//SMI_LARB2@0x15001000 {
#define SMI_LARB2_STAT        	(dcm_SMI_LARB2_base + 0x00)//0x15001000
#define SMI_LARB2_CON        	(dcm_SMI_LARB2_base + 0x10)//0x15001010
#define SMI_LARB2_CON_SET       (dcm_SMI_LARB2_base + 0x14)//0x15001014
#define SMI_LARB2_CON_CLR       (dcm_SMI_LARB2_base + 0x18)//0x15001018

//SMI_LARB3@18001000 {
#define SMI_LARB3_STAT        	(dcm_SMI_LARB3_base + 0x00)//0x18001000
#define SMI_LARB3_CON        	(dcm_SMI_LARB3_base + 0x10)//0x18001010
#define SMI_LARB3_CON_SET       (dcm_SMI_LARB3_base + 0x14)//0x18001014
#define SMI_LARB3_CON_CLR       (dcm_SMI_LARB3_base + 0x18)//0x18001018

//SMI_LARB5@0x17002000 {
#define SMI_LARB4_STAT        	(dcm_SMI_LARB5_base + 0x00)//0x17002000
#define SMI_LARB4_CON        	(dcm_SMI_LARB5_base + 0x10)//0x17002010
#define SMI_LARB4_CON_SET       (dcm_SMI_LARB5_base + 0x14)//0x17002014
#define SMI_LARB4_CON_CLR       (dcm_SMI_LARB5_base + 0x18)//0x17002018

// APB Module emi
//EMI@0x10203000 {
#ifdef EMI_CONM //fix redefined build warning
#undef EMI_CONM
#endif
#define EMI_CONM       			(dcm_EMI_base + 0x60)//0x10203060



// APB Module cam1
#define CTL_RAW_DCM_DIS         (dcm_CAM1_base + 0x188) //0xF5004188
#define CTL_RAW_D_DCM_DIS       (dcm_CAM1_base + 0x18C) //0xF500418C
#define CTL_DMA_DCM_DIS         (dcm_CAM1_base + 0x190) //0xF5004190
#define CTL_RGB_DCM_DIS         (dcm_CAM1_base + 0x194) //0xF5004194
#define CTL_YUV_DCM_DIS         (dcm_CAM1_base + 0x198) //0xF5004198
#define CTL_TOP_DCM_DIS         (dcm_CAM1_base + 0x19C) //0xF500419C

// APB Module fdvt
#define FDVT_CTRL         		(dcm_FDVT_base + 0x19C) //0xF500B19C

//JPGENC@18003000 {
#define JPGENC_DCM_CTRL         (dcm_JPGENC_base + 0x300) //0x18003300

//JPGDEC@18004000 {
#define JPGDEC_DCM_CTRL         (dcm_JPGDEC_base + 0x300) //0x18004300


//display sys
//mmsys_dcm
// APB Module mmsys_config
#define MMSYS_HW_DCM_DIS0        (dcm_MMSYS_CONFIG_base + 0x120)//MMSYS_HW_DCM_DIS0,MMSYS_CONFIG_BASE=0xF4000000
#define MMSYS_HW_DCM_DIS_SET0    (dcm_MMSYS_CONFIG_base + 0x124)//MMSYS_HW_DCM_DIS_SET0
#define MMSYS_HW_DCM_DIS_CLR0    (dcm_MMSYS_CONFIG_base + 0x128)//MMSYS_HW_DCM_DIS_CLR0

#define MMSYS_HW_DCM_DIS1        (dcm_MMSYS_CONFIG_base + 0x130)//MMSYS_HW_DCM_DIS1
#define MMSYS_HW_DCM_DIS_SET1    (dcm_MMSYS_CONFIG_base + 0x134)//MMSYS_HW_DCM_DIS_SET1
#define MMSYS_HW_DCM_DIS_CLR1    (dcm_MMSYS_CONFIG_base + 0x138)//MMSYS_HW_DCM_DIS_CLR1

//venc sys

#define VENC_CLK_CG_CTRL         (dcm_VENC_base + 0xFC)//0x180020FC
#define VENC_CLK_DCM_CTRL        (dcm_VENC_base + 0xF4)//0x180020F4


// APB Module vdecsys_config
//VDEC_dcm
#define VDEC_DCM_CON             (dcm_VDEC_GCON_base + 0x18)//0x16000018


#else

// APB Module usb2
#define	USB0_DCM						(USB0_BASE+0x700)//0x11200700


// APB Module msdc
#define MSDC0_PATCH_BIT1				(MSDC0_BASE + 0x00B4)//0xF1230000
#define MSDC1_PATCH_BIT1				(MSDC1_BASE + 0x00B4)//0xF1240000
#define MSDC2_PATCH_BIT1				(MSDC2_BASE + 0x00B4)//0xF1250000
#define MSDC3_PATCH_BIT1				(MSDC3_BASE + 0x00B4)//0xF1260000


// APB Module pmic_wrap
#define PMIC_WRAP_DCM_EN			(PWRAP_BASE+0x144)//0xF000D000

// APB Module i2c
#define I2C0_I2CREG_HW_CG_EN		(I2C0_BASE+0x054)//0xF1007000
#define I2C1_I2CREG_HW_CG_EN		(I2C1_BASE+0x054)//0xF1008000
#define I2C2_I2CREG_HW_CG_EN		(I2C2_BASE+0x054)//0xF1009000
#define I2C3_I2CREG_HW_CG_EN		(I2C3_BASE+0x054)//0xF1010000
#define I2C4_I2CREG_HW_CG_EN		(I2C4_BASE+0x054)//0xF1011000


//MJC
// APB Module mjc_config MJC_CONFIG_BASE=0xF7000000
#define MJC_HW_DCM_DIS        	(MJC_CONFIG_BASE+0x0010)//0xF7000010
#define MJC_HW_DCM_DIS_SET    	(MJC_CONFIG_BASE+0x0014)//0xF7000014
#define MJC_HW_DCM_DIS_CLR    	(MJC_CONFIG_BASE+0x0018)//0xF7000018


//CPUSYS_dcm
#if 1//MCUCFG_BASE=0xF0200000
#define DCM_MCUSYS_CONFIG		(MCUCFG_BASE + 0x001C)//0xF020001C
#define CACHE_CONFIG			(MCUCFG_BASE + 0x0100)//0xF0200100
#define ARMPLL_CTL				(MCUCFG_BASE + 0x0160)//0xF0200160
#endif




//AXI bus dcm
//TOPCKGen_dcm,CKSYS_BASE=0xF0000000
#define DCM_CFG                 (CKSYS_BASE + 0x0004)//0x10000004


//CA7 DCM,INFRACFG_AO_BASE=0xF0001000
#define CA7_CKDIV1				(INFRACFG_AO_BASE + 0x0008) //0x10001008
#define INFRA_TOPCKGEN_DCMCTL   (INFRACFG_AO_BASE + 0x0010) //0x10001010
#define INFRA_TOPCKGEN_DCMDBC   (INFRACFG_AO_BASE + 0x0014) //0x10001014


//infra dcm
#define INFRA_GLOBALCON_DCMCTL  (INFRACFG_AO_BASE   + 0x0050) //0x10001050
#define INFRA_GLOBALCON_DCMDBC  (INFRACFG_AO_BASE   + 0x0054) //0x10001054
#define INFRA_GLOBALCON_DCMFSEL (INFRACFG_AO_BASE   + 0x0058) //0x10001058
#define MM_MMU_DCM_DIS			(M4U_BASE           + 0x0050) //0x10205050,M4U_BASE=0xF0205000
#define PERISYS_MMU_DCM_DIS		(PERISYS_IOMMU_BASE + 0x0050) //0x10214050,PERISYS_IOMMU_BASE=0xF0214000

//peri dcm
#define PERI_GLOBALCON_DCMCTL        (PERICFG_BASE + 0x0050) //0x10003050,PERICFG_BASE=0xF0003000
#define PERI_GLOBALCON_DCMDBC        (PERICFG_BASE + 0x0054) //0x10003054
#define PERI_GLOBALCON_DCMFSEL       (PERICFG_BASE + 0x0058) //0x10003058


#define channel_A_DRAMC_PD_CTRL           (DRAMC0_BASE + 0x01DC)//0x100041dc,DRAMC0_BASE=0xF0004000
#define channel_B_DRAMC_PD_CTRL           (DRAMC1_BASE + 0x01DC)//0x100111dc,DRAMC1_BASE=0xF0011000

//m4u dcm
//#define MMU_DCM					(SMI_MMU_TOP_BASE+0x5f0)

//smi_common dcm
//#define SMI_COMMON_DCM          0x10202300 //HW_DCM API_17

// APB Module smi
//Smi_common dcm
#define SMI_COMMON_SMI_DCM				(SMI_COMMON_BASE+0x300)//0x14022300


// APB Module smi
//Smi_secure dcm


#define SMI_CON						(SMI1_BASE+0x010)//SMI_CON
#define SMI_CON_SET					(SMI1_BASE+0x014)//SMI_CON_SET
#define SMI_CON_CLR					(SMI1_BASE+0x018)//SMI_CON_CLR



// APB Module smi_larb
#define SMI_LARB0_STA        	(SMI_LARB0_BASE + 0x00)//0x14021000
#define SMI_LARB0_CON        	(SMI_LARB0_BASE + 0x10)//0x14021010
#define SMI_LARB0_CON_SET       (SMI_LARB0_BASE + 0x14)//0x14021014
#define SMI_LARB0_CON_CLR       (SMI_LARB0_BASE + 0x18)//0x14021018

#define SMI_LARB1_STAT        	(SMI_LARB1_BASE + 0x00)//0x16010000
#define SMI_LARB1_CON        	(SMI_LARB1_BASE + 0x10)//0x16010010
#define SMI_LARB1_CON_SET       (SMI_LARB1_BASE + 0x14)//0x16010014
#define SMI_LARB1_CON_CLR       (SMI_LARB1_BASE + 0x18)//0x16010018

#define SMI_LARB2_STAT        	(SMI_LARB2_BASE + 0x00)//0x15001000
#define SMI_LARB2_CON        	(SMI_LARB2_BASE + 0x10)//0x15001010
#define SMI_LARB2_CON_SET       (SMI_LARB2_BASE + 0x14)//0x15001014
#define SMI_LARB2_CON_CLR       (SMI_LARB2_BASE + 0x18)//0x15001018

#define SMI_LARB3_STAT        	(SMI_LARB3_BASE + 0x00)//0x18001000
#define SMI_LARB3_CON        	(SMI_LARB3_BASE + 0x10)//0x18001010
#define SMI_LARB3_CON_SET       (SMI_LARB3_BASE + 0x14)//0x18001014
#define SMI_LARB3_CON_CLR       (SMI_LARB3_BASE + 0x18)//0x18001018

#define SMI_LARB4_STAT        	(SMI_LARB4_BASE + 0x00)//0x17002000
#define SMI_LARB4_CON        	(SMI_LARB4_BASE + 0x10)//0x17002010
#define SMI_LARB4_CON_SET       (SMI_LARB4_BASE + 0x14)//0x17002014
#define SMI_LARB4_CON_CLR       (SMI_LARB4_BASE + 0x18)//0x17002018

// APB Module emi
#ifdef EMI_CONM //fix redefined build warning
#undef EMI_CONM
#endif
#define EMI_CONM       			(EMI_BASE + 0x60)//0x10203060



#if 0
//MFG
//MFG_DCM
// APB Module mfg_top
#define MFG_DCM_CON_0            (G3D_CONFIG_BASE + 0x10) //MFG_DCM_CON_0
#endif

/*
#define CAM_CTL_RAW_DCM_DIS         (CAM0_BASE + 0x190)//CAM_CTL_RAW_DCM_DIS
#define CAM_CTL_RGB_DCM_DIS         (CAM0_BASE + 0x194)//CAM_CTL_RGB_DCM_DIS
#define CAM_CTL_YUV_DCM_DIS         (CAM0_BASE + 0x198)//CAM_CTL_YUV_DCM_DIS
#define CAM_CTL_CDP_DCM_DIS         (CAM0_BASE + 0x19C)//CAM_CTL_CDP_DCM_DIS
#define CAM_CTL_DMA_DCM_DIS			(CAM0_BASE + 0x1B0)//CAM_CTL_DMA_DCM_DIS

#define CAM_CTL_RAW_DCM_STATUS     (CAM0_BASE + 0x1A0)//CAM_CTL_RAW_DCM_STATUS
#define CAM_CTL_RGB_DCM_STATUS     (CAM0_BASE + 0x1A4)//CAM_CTL_RGB_DCM_STATUS
#define CAM_CTL_YUV_DCM_STATUS     (CAM0_BASE + 0x1A8)//CAM_CTL_YUV_DCM_STATUS
#define CAM_CTL_CDP_DCM_STATUS     (CAM0_BASE + 0x1AC)//CAM_CTL_CDP_DCM_STATUS
#define CAM_CTL_DMA_DCM_STATUS     (CAM0_BASE + 0x1B4)//CAM_CTL_DMA_DCM_STATUS
*/

// APB Module cam1
//CAM1_BASE= 0xF5004000
#define CTL_RAW_DCM_DIS         (CAM1_BASE + 0x188) //0xF5004188
#define CTL_RAW_D_DCM_DIS       (CAM1_BASE + 0x18C) //0xF500418C
#define CTL_DMA_DCM_DIS         (CAM1_BASE + 0x190) //0xF5004190
#define CTL_RGB_DCM_DIS         (CAM1_BASE + 0x194) //0xF5004194
#define CTL_YUV_DCM_DIS         (CAM1_BASE + 0x198) //0xF5004198
#define CTL_TOP_DCM_DIS         (CAM1_BASE + 0x19C) //0xF500419C

// APB Module fdvt
//FDVT_BASE=0xF500B000
#define FDVT_CTRL         		(FDVT_BASE + 0x19C) //0xF500B19C

//JPGENC_BASE=0xF8003000
#define JPGENC_DCM_CTRL         (JPGENC_BASE + 0x300) //0x18003300

//JPGDEC_BASE=0xF8004000
#define JPGDEC_DCM_CTRL         (JPGDEC_BASE + 0x300) //0x18004300

//#define SMI_ISP_COMMON_DCMCON   0x15003010  	//82 N 89 Y
//#define SMI_ISP_COMMON_DCMSET   0x15003014	//82 N 89 Y
//#define SMI_ISP_COMMON_DCMCLR   0x15003018	//82 N 89 Y

//display sys
//mmsys_dcm
// APB Module mmsys_config
//MMSYS_CONFIG_BASE=0xF4000000
#define MMSYS_HW_DCM_DIS0        (MMSYS_CONFIG_BASE + 0x120)//MMSYS_HW_DCM_DIS0
#define MMSYS_HW_DCM_DIS_SET0    (MMSYS_CONFIG_BASE + 0x124)//MMSYS_HW_DCM_DIS_SET0
#define MMSYS_HW_DCM_DIS_CLR0    (MMSYS_CONFIG_BASE + 0x128)//MMSYS_HW_DCM_DIS_CLR0

#define MMSYS_HW_DCM_DIS1        (MMSYS_CONFIG_BASE + 0x130)//MMSYS_HW_DCM_DIS1
#define MMSYS_HW_DCM_DIS_SET1    (MMSYS_CONFIG_BASE + 0x134)//MMSYS_HW_DCM_DIS_SET1
#define MMSYS_HW_DCM_DIS_CLR1    (MMSYS_CONFIG_BASE + 0x138)//MMSYS_HW_DCM_DIS_CLR1

//venc sys
//VENC_BASE=0xF8002000
#define VENC_CLK_CG_CTRL       (VENC_BASE + 0xFC)//0x180020FC
#define VENC_CLK_DCM_CTRL      (VENC_BASE + 0xF4)//0x180020F4


// APB Module vdecsys_config
//VDEC_dcm
//VDEC_GCON_BASE=0xF6000000
#define VDEC_DCM_CON            (VDEC_GCON_BASE + 0x18)//0x16000018


#endif

#define CPU_DCM                 (1U << 0)
#define IFR_DCM                 (1U << 1)
#define PER_DCM                 (1U << 2)
#define SMI_DCM                 (1U << 3)
#define EMI_DCM                 (1U << 4)
#define DIS_DCM                 (1U << 5)
#define ISP_DCM                 (1U << 6)
#define VDE_DCM                 (1U << 7)
//#define SMILARB_DCM				(1U << 8)
//#define TOPCKGEN_DCM			(1U << 8)
#define MJC_DCM					(1U << 8)
//#define ALL_DCM                 (CPU_DCM|IFR_DCM|PER_DCM|SMI_DCM|MFG_DCM|DIS_DCM|ISP_DCM|VDE_DCM|TOPCKGEN_DCM)
#define ALL_DCM                 (CPU_DCM|IFR_DCM|PER_DCM|SMI_DCM|EMI_DCM|DIS_DCM|ISP_DCM|VDE_DCM|MJC_DCM)
#define NR_DCMS                 (0x9)


//extern void dcm_get_status(unsigned int type);
extern void dcm_enable(unsigned int type);
extern void dcm_disable(unsigned int type);

extern void disable_cpu_dcm(void);
extern void enable_cpu_dcm(void);

extern void bus_dcm_enable(void);
extern void bus_dcm_disable(void);

extern void disable_infra_dcm(void);
extern void restore_infra_dcm(void);

extern void disable_peri_dcm(void);
extern void restore_peri_dcm(void);

extern void mt_dcm_init(void);
extern void dcm_CA7_L2_share_256K_to_external_enable(bool enable);

#endif
