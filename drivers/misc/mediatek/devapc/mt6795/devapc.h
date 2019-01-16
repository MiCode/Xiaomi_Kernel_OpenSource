#define MOD_NO_IN_1_DEVAPC                  16
//#define DEVAPC_MODULE_MAX_NUM               32  
#define DEVAPC_TAG                          "DEVAPC"
#define MAX_TIMEOUT                         100
 
 
// device apc attribute
/* typedef enum
 {
     E_L0=0,
     E_L1,
     E_L2,
     E_L3,
     E_MAX_APC_ATTR
 }APC_ATTR;*/
 
 // device apc index 
/*
 typedef enum
 {
     E_DEVAPC0=0,
     E_DEVAPC1,  
     E_DEVAPC2,
     E_DEVAPC3,
     E_DEVAPC4,
     E_MAX_DEVAPC
 }DEVAPC_NUM;
 */
 // domain index 
/* typedef enum
 {
     E_DOMAIN_0 = 0,
     E_DOMAIN_1 ,
     E_DOMAIN_2 , 
     E_DOMAIN_3 ,
     E_MAX
 }E_MASK_DOM;*/
 
 
 typedef struct {
     const char      *device_name;
     bool            forbidden;
 } DEVICE_INFO;
 
  
  static DEVICE_INFO D_APC0_Devices[] = {
    {"INFRA_AO_TOP_LEVEL_CLOCK_GENERATOR",      FALSE},//
    {"INFRA_AO_INFRASYS_CONFIG_REGS",           FALSE},//
    {"INFRA_AO_FHCTL",                          TRUE},
    {"INFRA_AO_PERISYS_CONFIG_REGS",            TRUE},
    {"INFRA_AO_DRAM_CONTROLLER",                TRUE},
    {"INFRA_AO_GPIO_CONTROLLER",                FALSE},//
    {"INFRA_AO_TOP_LEVEL_SLP_MANAGER",          TRUE},
    {"INFRA_AO_TOP_LEVEL_RESET_GENERATOR",      FALSE},//
    {"INFRA_AO_GPT",                            TRUE},
    {"INFRA_AO_RSVD",                           TRUE},
    {"INFRA_AO_SEJ",                            TRUE},
    {"INFRA_AO_APMCU_EINT_CONTROLLER",          TRUE},
    {"INFRA_AO_SMI_CONTROL_REG1",               TRUE},
    {"INFRA_AO_PMIC_WRAP_CONTROL_REG",          FALSE},//
    {"INFRA_AO_DEVICE_APC_AO",                  TRUE},
    {"INFRA_AO_DDRPHY_CONTROL_REG",             TRUE},
    {"INFRA_AO_KPAD_CONTROL_REG",		TRUE},
    {"INFRA_AO_DRAM_B_CONTROLLER",		TRUE},
    {"INFRA_AO_DDRPHY_B_CONTROL_REG",		TRUE},
    {"INFRA_AO_SPM_MD32",			TRUE},
    {"INFRASYS_MCUSYS_CONFIG_REG",              TRUE},
    {"INFRASYS_CONTROL_REG",                    TRUE},
    {"INFRASYS_BOOTROM/SRAM",                   TRUE},
    {"INFRASYS_EMI_BUS_INTERFACE",              FALSE},//
    {"INFRASYS_SYSTEM_CIRQ",                    TRUE},
    {"INFRASYS_MM_IOMMU_CONFIGURATION",         FALSE},//
    {"INFRASYS_EFUSEC",                         FALSE},//
    {"INFRASYS_DEVICE_APC_MONITOR",             TRUE},
    {"INFRASYS_MCU_BIU_CONFIGURATION",          TRUE},
    {"INFRASYS_AP_MIXED_CONTROL_REG",           TRUE},
    {"INFRASYS_CA7_AP_CCIF",                    FALSE},//
    {"INFRASYS_CA7_MD_CCIF",                    FALSE},//
    {"INFRASYS_GPIO1_CONTROLLER",               TRUE},
    {"INFRASYS_MBIST_CONTROL_REG",              TRUE},
    {"INFRASYS_DRAMC_NAO_REGION_REG",           TRUE},
    {"INFRASYS_TRNG",                           TRUE},
    {"INFRASYS_GCPU",				TRUE},
    {"INFRASYS_GCPU_NS",			TRUE},
    {"INFRASYS_GCE",				TRUE},
    {"INFRASYS_DRAM_B_NAO_CONTROLLER",		TRUE},
    {"INFRASYS_PERISYS_IOMMU",			FALSE},//
    {"INFRA_AO_ANA_MIPI_DSI0",                  TRUE},
    {"INFRA_AO_ANA_MIPI_DSI1",                  TRUE},
    {"INFRA_AO_ANA_MIPI_CSI0",                  TRUE},
    {"INFRA_AO_ANA_MIPI_CSI1",                  TRUE},
    {"INFRA_AO_ANA_MIPI_CSI2",                  TRUE},
    {"DEGBUG CORESIGHT",                        FALSE},//
    {"DMA",                                     TRUE},
    {"AUXADC",                                  FALSE},//
    {"UART0",                                   TRUE},
    {"UART1",                                   TRUE},
    {"UART2",                                   TRUE},
    {"UART3",                                   TRUE},
    {"PWM",                                     TRUE},
    {"I2C0",                                    TRUE},
    {"I2C1",                                    TRUE},
    {"I2C2",                                    TRUE},
    {"SPI0",                                    TRUE},
    {"PTP",                                     TRUE},
    {"Irda",                                    TRUE},
    {"NFI",                                     TRUE},
    {"NFI_ECC",                                 TRUE},
    {"NLI_ARBITER",                             TRUE},
    {"I2C3",					TRUE},
    {"I2C4",					TRUE},	
    {"USB2.0",                                  TRUE},
    {"USBSIF",                                  TRUE},
    {"AUDIO",                                   FALSE},//
    {"MSDC0",                                   TRUE},
    {"MSDC1",                                   TRUE},
    {"MSDC2",                                   TRUE},
    {"MSDC3",					TRUE},
    {"USB3.0",					FALSE},//
    {"USB3.0 SIF",				FALSE},//
    {"USB3.0 SIF2",				FALSE},//
    {"MD_PERIPHERALS",                          TRUE},
	{"G3D_CONFIG",								TRUE},
	{"MMSYS_CONFIG",							TRUE},
	{"MDP_RDMA0",								TRUE},
	{"MDP_RDMA1",								TRUE},
	{"MDP_RSZ0",								TRUE},
	{"MDP_RSZ1",								TRUE},
	{"MDP_RSZ2",								TRUE},
	{"MDP_WDMA",								TRUE},
	{"MDP_WROT0",								TRUE},
	{"MDP_WROT1",								TRUE},
	{"MDP_TDSHP0",								TRUE},
	{"MDP_TDSHP1",								TRUE},
	{"MDP_CROP",								TRUE},
	{"DISP_OVL0",								TRUE},
	{"DISP_OVL1",								TRUE},
	{"DISP_RDMA0",								TRUE},
	{"DISP_RDMA1",								TRUE},
	{"DISP_RDMA2",								TRUE},
	{"DISP_WDMA0",								TRUE},
	{"DISP_WDMA1",								TRUE},
	{"DISP_COLOR0",								TRUE},
	{"DISP_COLOR1",								TRUE},
	{"DISP_AAL",								TRUE},
	{"DISP_GAMMA",								TRUE},
	{"DISP_MERGE",								TRUE},
	{"DISP_SPLIT",								TRUE},
	{"DSI_UFO",								TRUE},
	{"DSI_DISPATCHER",							TRUE},
	{"DSI0",								TRUE},
	{"DSI1",								TRUE},
	{"DPI",									TRUE},
	{"DISP_PWM0",								TRUE},
	{"DISP_PWM1",								TRUE},
	{"MM_MUTEX",								TRUE},
	{"SMI_LARB0",								TRUE},
	{"SMI_COMMON",								TRUE},	
	{"IMGSYS_CONFIG",							TRUE},
	{"IMGSYS_SMI_LARB2",							TRUE},
	{"IMGSYS_CAM0",							    	TRUE},
	{"IMGSYS_CAM1",								TRUE},
	{"IMGSYS_CAM2",								TRUE},
	{"IMGSYS_CAM3",								TRUE},
	{"IMGSYS_SENINF",							TRUE},
	{"IMGSYS_CAMSV",							TRUE},
	{"IMGSYS_FD",							    	TRUE},
	{"IMGSYS_MIPI_RX",							TRUE},
	{"CAM0",							        TRUE},
    	{"CAM2",							        TRUE},
    	{"CAM3",							        TRUE},
	{"VDECSYS_GLOBAL_CONFIGURATION",		    			TRUE},
	{"VDECSYS_SMI_LARB1",							TRUE},
	{"VDEC_FULL_TOP",							TRUE},
	{"MJC_CONFIG",								TRUE},
	{"MJC_TOP",								TRUE},
	{"SMI_LARB5",								TRUE},
	{"VENC_GLOBAL_CON",							TRUE},
	{"SMI_LARB3",								TRUE},
	{"VENC_FULL_TOP",							TRUE},
	{"JPGENC",							        TRUE},
	{"JPGDEC",	        						TRUE},
    	{NULL,                                      				FALSE},
  };

 
 
 
#define SET_SINGLE_MODULE(apcnum, domnum, index, module, permission_control)     \
 {                                                                               \
     mt_reg_sync_writel(readl(DEVAPC##apcnum##_D##domnum##_APC_##index) & ~(0x3 << (2 * module)), DEVAPC##apcnum##_D##domnum##_APC_##index); \
     mt_reg_sync_writel(readl(DEVAPC##apcnum##_D##domnum##_APC_##index) | (permission_control << (2 * module)),DEVAPC##apcnum##_D##domnum##_APC_##index); \
 }                                                                               \
 
#define UNMASK_SINGLE_MODULE_IRQ(apcnum, domnum, module_index)                  \
 {                                                                               \
     mt_reg_sync_writel(readl(DEVAPC##apcnum##_D##domnum##_VIO_MASK) & ~(module_index),      \
         DEVAPC##apcnum##_D##domnum##_VIO_MASK);                                 \
 }                                                                               \
 
#define CLEAR_SINGLE_VIO_STA(apcnum, domnum, module_index)                     \
 {                                                                               \
     mt_reg_sync_writel(readl(DEVAPC##apcnum##_D##domnum##_VIO_STA) | (module_index),        \
         DEVAPC##apcnum##_D##domnum##_VIO_STA);                                  \
 }                                                                               \

 
