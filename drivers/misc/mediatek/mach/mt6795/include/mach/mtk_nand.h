#ifndef __MTK_NAND_H
#define __MTK_NAND_H
#ifndef CONFIG_MTK_EMMC_SUPPORT
#include "nand_device_list.h"
#endif

/*******************************************************************************
 * NFI Register Definition 
 *******************************************************************************/

#define NFI_CNFG_REG16  	((volatile P_U16)(NFI_BASE+0x0000))
#define NFI_PAGEFMT_REG16   ((volatile P_U16)(NFI_BASE+0x0004))
#define NFI_CON_REG16      	((volatile P_U16)(NFI_BASE+0x0008))
#define NFI_ACCCON_REG32   	((volatile P_U32)(NFI_BASE+0x000C))
#define NFI_INTR_EN_REG16   ((volatile P_U16)(NFI_BASE+0x0010))
#define NFI_INTR_REG16      ((volatile P_U16)(NFI_BASE+0x0014))

#define NFI_CMD_REG16   	((volatile P_U16)(NFI_BASE+0x0020))

#define NFI_ADDRNOB_REG16   ((volatile P_U16)(NFI_BASE+0x0030))
#define NFI_COLADDR_REG32  	((volatile P_U32)(NFI_BASE+0x0034))
#define NFI_ROWADDR_REG32  	((volatile P_U32)(NFI_BASE+0x0038))

#define NFI_STRDATA_REG16   ((volatile P_U16)(NFI_BASE+0x0040))

#define NFI_DATAW_REG32    	((volatile P_U32)(NFI_BASE+0x0050))
#define NFI_DATAR_REG32    	((volatile P_U32)(NFI_BASE+0x0054))
#define NFI_PIO_DIRDY_REG16 ((volatile P_U16)(NFI_BASE+0x0058))

#define NFI_STA_REG32      	((volatile P_U32)(NFI_BASE+0x0060))
#define NFI_FIFOSTA_REG16   ((volatile P_U16)(NFI_BASE+0x0064))
#define NFI_LOCKSTA_REG16   ((volatile P_U16)(NFI_BASE+0x0068))

#define NFI_ADDRCNTR_REG16  ((volatile P_U16)(NFI_BASE+0x0070))

#define NFI_STRADDR_REG32  	((volatile P_U32)(NFI_BASE+0x0080))
#define NFI_BYTELEN_REG16   ((volatile P_U16)(NFI_BASE+0x0084))

#define NFI_CSEL_REG16      ((volatile P_U16)(NFI_BASE+0x0090))
#define NFI_IOCON_REG16     ((volatile P_U16)(NFI_BASE+0x0094))

#define NFI_FDM0L_REG32    	((volatile P_U32)(NFI_BASE+0x00A0))
#define NFI_FDM0M_REG32    	((volatile P_U32)(NFI_BASE+0x00A4))

#define NFI_LOCK_REG16	  	((volatile P_U16)(NFI_BASE+0x0100))
#define NFI_LOCKCON_REG32  	((volatile P_U32)(NFI_BASE+0x0104))
#define NFI_LOCKANOB_REG16  ((volatile P_U16)(NFI_BASE+0x0108))
#define NFI_LOCK00ADD_REG32 ((volatile P_U32)(NFI_BASE+0x0110))
#define NFI_LOCK00FMT_REG32 ((volatile P_U32)(NFI_BASE+0x0114))
#define NFI_LOCK01ADD_REG32 ((volatile P_U32)(NFI_BASE+0x0118))
#define NFI_LOCK01FMT_REG32 ((volatile P_U32)(NFI_BASE+0x011C))
#define NFI_LOCK02ADD_REG32 ((volatile P_U32)(NFI_BASE+0x0120))
#define NFI_LOCK02FMT_REG32 ((volatile P_U32)(NFI_BASE+0x0124))
#define NFI_LOCK03ADD_REG32 ((volatile P_U32)(NFI_BASE+0x0128))
#define NFI_LOCK03FMT_REG32 ((volatile P_U32)(NFI_BASE+0x012C))
#define NFI_LOCK04ADD_REG32 ((volatile P_U32)(NFI_BASE+0x0130))
#define NFI_LOCK04FMT_REG32 ((volatile P_U32)(NFI_BASE+0x0134))
#define NFI_LOCK05ADD_REG32 ((volatile P_U32)(NFI_BASE+0x0138))
#define NFI_LOCK05FMT_REG32 ((volatile P_U32)(NFI_BASE+0x013C))
#define NFI_LOCK06ADD_REG32 ((volatile P_U32)(NFI_BASE+0x0140))
#define NFI_LOCK06FMT_REG32 ((volatile P_U32)(NFI_BASE+0x0144))
#define NFI_LOCK07ADD_REG32 ((volatile P_U32)(NFI_BASE+0x0148))
#define NFI_LOCK07FMT_REG32 ((volatile P_U32)(NFI_BASE+0x014C))
#define NFI_LOCK08ADD_REG32 ((volatile P_U32)(NFI_BASE+0x0150))
#define NFI_LOCK08FMT_REG32 ((volatile P_U32)(NFI_BASE+0x0154))
#define NFI_LOCK09ADD_REG32 ((volatile P_U32)(NFI_BASE+0x0158))
#define NFI_LOCK09FMT_REG32 ((volatile P_U32)(NFI_BASE+0x015C))
#define NFI_LOCK10ADD_REG32 ((volatile P_U32)(NFI_BASE+0x0160))
#define NFI_LOCK10FMT_REG32 ((volatile P_U32)(NFI_BASE+0x0164))
#define NFI_LOCK11ADD_REG32 ((volatile P_U32)(NFI_BASE+0x0168))
#define NFI_LOCK11FMT_REG32 ((volatile P_U32)(NFI_BASE+0x016C))
#define NFI_LOCK12ADD_REG32 ((volatile P_U32)(NFI_BASE+0x0170))
#define NFI_LOCK12FMT_REG32 ((volatile P_U32)(NFI_BASE+0x0174))
#define NFI_LOCK13ADD_REG32 ((volatile P_U32)(NFI_BASE+0x0178))
#define NFI_LOCK13FMT_REG32 ((volatile P_U32)(NFI_BASE+0x017C))
#define NFI_LOCK14ADD_REG32 ((volatile P_U32)(NFI_BASE+0x0180))
#define NFI_LOCK14FMT_REG32 ((volatile P_U32)(NFI_BASE+0x0184))
#define NFI_LOCK15ADD_REG32 ((volatile P_U32)(NFI_BASE+0x0188))
#define NFI_LOCK15FMT_REG32 ((volatile P_U32)(NFI_BASE+0x018C))

#define NFI_FIFODATA0_REG32 ((volatile P_U32)(NFI_BASE+0x0190))
#define NFI_FIFODATA1_REG32 ((volatile P_U32)(NFI_BASE+0x0194))
#define NFI_FIFODATA2_REG32 ((volatile P_U32)(NFI_BASE+0x0198))
#define NFI_FIFODATA3_REG32 ((volatile P_U32)(NFI_BASE+0x019C))
#define NFI_MASTERSTA_REG16 ((volatile P_U16)(NFI_BASE+0x0210))


/*******************************************************************************
 * NFI Register Field Definition 
 *******************************************************************************/

/* NFI_CNFG */
#define CNFG_AHB             (0x0001)
#define CNFG_READ_EN         (0x0002)
#define CNFG_DMA_BURST_EN    (0x0004)
#define CNFG_BYTE_RW         (0x0040)
#define CNFG_HW_ECC_EN       (0x0100)
#define CNFG_AUTO_FMT_EN     (0x0200)
#define CNFG_OP_IDLE         (0x0000)
#define CNFG_OP_READ         (0x1000)
#define CNFG_OP_SRD          (0x2000)
#define CNFG_OP_PRGM         (0x3000)
#define CNFG_OP_ERASE        (0x4000)
#define CNFG_OP_RESET        (0x5000)
#define CNFG_OP_CUST         (0x6000)
#define CNFG_OP_MODE_MASK    (0x7000)
#define CNFG_OP_MODE_SHIFT   (12)

/* NFI_PAGEFMT */
#define PAGEFMT_512          (0x0000)
#define PAGEFMT_2K           (0x0001)
#define PAGEFMT_4K           (0x0002)

#define PAGEFMT_PAGE_MASK    (0x0003)

#define PAGEFMT_DBYTE_EN     (0x0008)

#define PAGEFMT_SPARE_16     (0x0000)
#define PAGEFMT_SPARE_26     (0x0001)
#define PAGEFMT_SPARE_27     (0x0002)
#define PAGEFMT_SPARE_28     (0x0003)
#define PAGEFMT_SPARE_MASK   (0x0030)
#define PAGEFMT_SPARE_SHIFT  (4)

#define PAGEFMT_FDM_MASK     (0x0F00)
#define PAGEFMT_FDM_SHIFT    (8)

#define PAGEFMT_FDM_ECC_MASK  (0xF000)
#define PAGEFMT_FDM_ECC_SHIFT (12)

/* NFI_CON */
#define CON_FIFO_FLUSH       (0x0001)
#define CON_NFI_RST          (0x0002)
#define CON_NFI_SRD          (0x0010)

#define CON_NFI_NOB_MASK     (0x0060)
#define CON_NFI_NOB_SHIFT    (5)

#define CON_NFI_BRD          (0x0100)
#define CON_NFI_BWR          (0x0200)

#define CON_NFI_SEC_MASK     (0xF000)
#define CON_NFI_SEC_SHIFT    (12)

/* NFI_ACCCON */
#define ACCCON_SETTING       ()

/* NFI_INTR_EN */
#define INTR_RD_DONE_EN      (0x0001)
#define INTR_WR_DONE_EN      (0x0002)
#define INTR_RST_DONE_EN     (0x0004)
#define INTR_ERASE_DONE_EN   (0x0008)
#define INTR_BSY_RTN_EN      (0x0010)
#define INTR_ACC_LOCK_EN     (0x0020)
#define INTR_AHB_DONE_EN     (0x0040)
#define INTR_ALL_INTR_DE     (0x0000)
#define INTR_ALL_INTR_EN     (0x007F)

/* NFI_INTR */
#define INTR_RD_DONE         (0x0001)
#define INTR_WR_DONE         (0x0002)
#define INTR_RST_DONE        (0x0004)
#define INTR_ERASE_DONE      (0x0008)
#define INTR_BSY_RTN         (0x0010)
#define INTR_ACC_LOCK        (0x0020)
#define INTR_AHB_DONE        (0x0040)

/* NFI_ADDRNOB */
#define ADDR_COL_NOB_MASK    (0x0003)
#define ADDR_COL_NOB_SHIFT   (0)
#define ADDR_ROW_NOB_MASK    (0x0030)
#define ADDR_ROW_NOB_SHIFT   (4)

/* NFI_STA */
#define STA_READ_EMPTY       (0x00001000)
#define STA_ACC_LOCK         (0x00000010)
#define STA_CMD_STATE        (0x00000001)
#define STA_ADDR_STATE       (0x00000002)
#define STA_DATAR_STATE      (0x00000004)
#define STA_DATAW_STATE      (0x00000008)

#define STA_NAND_FSM_MASK    (0x1F000000)
#define STA_NAND_BUSY        (0x00000100)
#define STA_NAND_BUSY_RETURN (0x00000200)
#define STA_NFI_FSM_MASK     (0x000F0000)
#define STA_NFI_OP_MASK      (0x0000000F)

/* NFI_FIFOSTA */
#define FIFO_RD_EMPTY        (0x0040)
#define FIFO_RD_FULL         (0x0080)
#define FIFO_WR_FULL         (0x8000)
#define FIFO_WR_EMPTY        (0x4000)
#define FIFO_RD_REMAIN(x)    (0x1F&(x))
#define FIFO_WR_REMAIN(x)    ((0x1F00&(x))>>8)

/* NFI_ADDRCNTR */
#define ADDRCNTR_CNTR(x)     ((0xF000&(x))>>12)
#define ADDRCNTR_OFFSET(x)   (0x03FF&(x))

/* NFI_LOCK */
#define NFI_LOCK_ON          (0x0001)

/* NFI_LOCKANOB */
#define PROG_RADD_NOB_MASK   (0x7000)
#define PROG_RADD_NOB_SHIFT  (12)
#define PROG_CADD_NOB_MASK   (0x0300)
#define PROG_CADD_NOB_SHIFT  (8)
#define ERASE_RADD_NOB_MASK   (0x0070)
#define ERASE_RADD_NOB_SHIFT  (4)
#define ERASE_CADD_NOB_MASK   (0x0007)
#define ERASE_CADD_NOB_SHIFT  (0)

/*******************************************************************************
 * ECC Register Definition 
 *******************************************************************************/

#define ECC_ENCCON_REG16	((volatile P_U16)(NFIECC_BASE+0x0000))
#define ECC_ENCCNFG_REG32  	((volatile P_U32)(NFIECC_BASE+0x0004))
#define ECC_ENCDIADDR_REG32	((volatile P_U32)(NFIECC_BASE+0x0008))
#define ECC_ENCIDLE_REG32  	((volatile P_U32)(NFIECC_BASE+0x000C))
#define ECC_ENCPAR0_REG32   ((volatile P_U32)(NFIECC_BASE+0x0010))
#define ECC_ENCPAR1_REG32   ((volatile P_U32)(NFIECC_BASE+0x0014))
#define ECC_ENCPAR2_REG32   ((volatile P_U32)(NFIECC_BASE+0x0018))
#define ECC_ENCPAR3_REG32   ((volatile P_U32)(NFIECC_BASE+0x001C))
#define ECC_ENCPAR4_REG32   ((volatile P_U32)(NFIECC_BASE+0x0020))
#define ECC_ENCPAR5_REG32   ((volatile P_U32)(NFIECC_BASE+0x0024))
#define ECC_ENCPAR6_REG32   ((volatile P_U32)(NFIECC_BASE+0x0028))
#define ECC_ENCSTA_REG32    ((volatile P_U32)(NFIECC_BASE+0x002C))
#define ECC_ENCIRQEN_REG16  ((volatile P_U16)(NFIECC_BASE+0x0030))
#define ECC_ENCIRQSTA_REG16 ((volatile P_U16)(NFIECC_BASE+0x0034))

#define ECC_DECCON_REG16    ((volatile P_U16)(NFIECC_BASE+0x0100))
#define ECC_DECCNFG_REG32   ((volatile P_U32)(NFIECC_BASE+0x0104))
#define ECC_DECDIADDR_REG32 ((volatile P_U32)(NFIECC_BASE+0x0108))
#define ECC_DECIDLE_REG16   ((volatile P_U16)(NFIECC_BASE+0x010C))
#define ECC_DECFER_REG16    ((volatile P_U16)(NFIECC_BASE+0x0110))
#define ECC_DECENUM0_REG32   ((volatile P_U32)(NFIECC_BASE+0x0114))
#define ECC_DECENUM1_REG32   ((volatile P_U32)(NFIECC_BASE+0x0118))
#define ECC_DECDONE_REG16   ((volatile P_U16)(NFIECC_BASE+0x011C))
#define ECC_DECEL0_REG32    ((volatile P_U32)(NFIECC_BASE+0x0120))
#define ECC_DECEL1_REG32    ((volatile P_U32)(NFIECC_BASE+0x0124))
#define ECC_DECEL2_REG32    ((volatile P_U32)(NFIECC_BASE+0x0128))
#define ECC_DECEL3_REG32    ((volatile P_U32)(NFIECC_BASE+0x012C))
#define ECC_DECEL4_REG32    ((volatile P_U32)(NFIECC_BASE+0x0130))
#define ECC_DECEL5_REG32    ((volatile P_U32)(NFIECC_BASE+0x0134))
#define ECC_DECEL6_REG32    ((volatile P_U32)(NFIECC_BASE+0x0138))
#define ECC_DECEL7_REG32    ((volatile P_U32)(NFIECC_BASE+0x013C))
#define ECC_DECIRQEN_REG16  ((volatile P_U16)(NFIECC_BASE+0x0140))
#define ECC_DECIRQSTA_REG16 ((volatile P_U16)(NFIECC_BASE+0x0144))
#define ECC_FDMADDR_REG32   ((volatile P_U32)(NFIECC_BASE+0x0148))
#define ECC_DECFSM_REG32    ((volatile P_U32)(NFIECC_BASE+0x014C))
#define ECC_SYNSTA_REG32    ((volatile P_U32)(NFIECC_BASE+0x0150))
#define ECC_DECNFIDI_REG32  ((volatile P_U32)(NFIECC_BASE+0x0154))
#define ECC_SYN0_REG32      ((volatile P_U32)(NFIECC_BASE+0x0158))

/*******************************************************************************
 * ECC register definition
 *******************************************************************************/
/* ECC_ENCON */
#define ENC_EN             		(0x0001)
#define ENC_DE                 	(0x0000)

/* ECC_ENCCNFG */
#define ECC_CNFG_ECC4          	(0x0000)
#define ECC_CNFG_ECC6          	(0x0001)
#define ECC_CNFG_ECC8          	(0x0002)
#define ECC_CNFG_ECC10         	(0x0003)
#define ECC_CNFG_ECC12         	(0x0004)
#define ECC_CNFG_ECC_MASK      	(0x00000007)

#define ENC_CNFG_NFI           	(0x0010)
#define ENC_CNFG_MODE_MASK     	(0x0010)

#define ENC_CNFG_META6         	(0x10300000)
#define ENC_CNFG_META8         	(0x10400000)

#define ENC_CNFG_MSG_MASK  		(0x1FFF0000)
#define ENC_CNFG_MSG_SHIFT 		(0x10)

/* ECC_ENCIDLE */
#define ENC_IDLE           		(0x0001)

/* ECC_ENCSTA */
#define STA_FSM            		(0x001F)
#define STA_COUNT_PS       		(0xFF10)
#define STA_COUNT_MS       		(0x3FFF0000)

/* ECC_ENCIRQEN */
#define ENC_IRQEN          		(0x0001)

/* ECC_ENCIRQSTA */
#define ENC_IRQSTA         		(0x0001)

/* ECC_DECCON */
#define DEC_EN             		(0x0001)
#define DEC_DE             		(0x0000)

/* ECC_ENCCNFG */
#define DEC_CNFG_ECC4          (0x0000)
//#define DEC_CNFG_ECC6          (0x0001)
//#define DEC_CNFG_ECC12         (0x0002)
#define DEC_CNFG_NFI           (0x0010)
//#define DEC_CNFG_META6         (0x10300000)
//#define DEC_CNFG_META8         (0x10400000)

#define DEC_CNFG_FER           (0x01000)
#define DEC_CNFG_EL            (0x02000)
#define DEC_CNFG_CORRECT       (0x03000)
#define DEC_CNFG_TYPE_MASK     (0x03000)

#define DEC_CNFG_EMPTY_EN      (0x80000000)

#define DEC_CNFG_CODE_MASK     (0x1FFF0000)
#define DEC_CNFG_CODE_SHIFT    (0x10)

/* ECC_DECIDLE */
#define DEC_IDLE           		(0x0001)

/* ECC_DECFER */
#define DEC_FER0               (0x0001)
#define DEC_FER1               (0x0002)
#define DEC_FER2               (0x0004)
#define DEC_FER3               (0x0008)
#define DEC_FER4               (0x0010)
#define DEC_FER5               (0x0020)
#define DEC_FER6               (0x0040)
#define DEC_FER7               (0x0080)

/* ECC_DECENUM */
#define ERR_NUM0               (0x0000000F)
#define ERR_NUM1               (0x000000F0)
#define ERR_NUM2               (0x00000F00)
#define ERR_NUM3               (0x0000F000)
#define ERR_NUM4               (0x000F0000)
#define ERR_NUM5               (0x00F00000)
#define ERR_NUM6               (0x0F000000)
#define ERR_NUM7               (0xF0000000)

/* ECC_DECDONE */
#define DEC_DONE0               (0x0001)
#define DEC_DONE1               (0x0002)
#define DEC_DONE2               (0x0004)
#define DEC_DONE3               (0x0008)
#define DEC_DONE4               (0x0010)
#define DEC_DONE5               (0x0020)
#define DEC_DONE6               (0x0040)
#define DEC_DONE7               (0x0080)

/* ECC_DECIRQEN */
#define DEC_IRQEN         		(0x0001)

/* ECC_DECIRQSTA */
#define DEC_IRQSTA      		(0x0001)

#define CHIPVER_ECO_1           (0x8a00)
#define CHIPVER_ECO_2           (0x8a01)

//#define NAND_PFM

/*******************************************************************************
 * Data Structure Definition
 *******************************************************************************/
struct mtk_nand_host 
{
	struct nand_chip		nand_chip;
	struct mtd_info			mtd;
	struct mtk_nand_host_hw	*hw;
};

struct NAND_CMD
{
	u32	u4ColAddr;
	u32 u4RowAddr;
	u32 u4OOBRowAddr;
	u8	au1OOB[128];
	u8*	pDataBuf;
#ifdef NAND_PFM	
	u32 pureReadOOB;
	u32 pureReadOOBNum;
#endif
};

/*
 *	ECC layout control structure. Exported to userspace for
 *  diagnosis and to allow creation of raw images
struct nand_ecclayout {
	uint32_t eccbytes;
	uint32_t eccpos[64];
	uint32_t oobavail;
	struct nand_oobfree oobfree[MTD_MAX_OOBFREE_ENTRIES];
};
*/
#define __DEBUG_NAND		1			/* Debug information on/off */

/* Debug message event */
#define DBG_EVT_NONE		0x00000000	/* No event */
#define DBG_EVT_INIT		0x00000001	/* Initial related event */
#define DBG_EVT_VERIFY		0x00000002	/* Verify buffer related event */
#define DBG_EVT_PERFORMANCE	0x00000004	/* Performance related event */
#define DBG_EVT_READ		0x00000008	/* Read related event */
#define DBG_EVT_WRITE		0x00000010	/* Write related event */
#define DBG_EVT_ERASE		0x00000020	/* Erase related event */
#define DBG_EVT_BADBLOCK	0x00000040	/* Badblock related event */
#define DBG_EVT_POWERCTL	0x00000080	/* Suspend/Resume related event */

#define DBG_EVT_ALL			0xffffffff

#define DBG_EVT_MASK      	(DBG_EVT_INIT)

#if __DEBUG_NAND
#define MSG(evt, fmt, args...) \
do {	\
	if ((DBG_EVT_##evt) & DBG_EVT_MASK) { \
		printk(fmt, ##args); \
	} \
} while(0)

#define MSG_FUNC_ENTRY(f)	MSG(FUC, "<FUN_ENT>: %s\n", __FUNCTION__)
#else
#define MSG(evt, fmt, args...) do{}while(0)
#define MSG_FUNC_ENTRY(f)	   do{}while(0)
#endif

#endif
