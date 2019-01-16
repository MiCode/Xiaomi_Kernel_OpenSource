#ifndef _MTK_DEVICE_APC_H
#define _MTK_DEVICE_APC_H
#include <mach/mt_typedefs.h>
#define DEVAPC_TAG                          "DEVAPC"


extern void __iomem * DEVAPC0_AO_BASE ;
extern void __iomem * DEVAPC0_PD_BASE ;

/*For EMI API*/

//#define ABORT_EMI_BUS_INTERFACE 0x00800000 //DEVAPC0_D0_VIO_STA_0, idx:23
#define ABORT_EMI               0x00008000 //DEVAPC0_D0_VIO_STA_4, idx:143


/*
 * Define constants.
 */
#define DEVAPC_DOMAIN_NUMBER    3
#define DEVAPC_DEVICE_NUMBER    136 

#define DEVAPC_DOMAIN_AP        0
#define DEVAPC_DOMAIN_MD        1
#define DEVAPC_DOMAIN_CONN      2
#define DEVAPC_DOMAIN_MM        3


// device apc attribute
 typedef enum
 {
     E_L0=0,
     E_L1,
     E_L2,
     E_L3,
     E_MAX_APC_ATTR
 }APC_ATTR;
 

 // domain index 
 typedef enum
 {
     E_DOMAIN_0 = 0,
     E_DOMAIN_1 ,
     E_DOMAIN_2 , 
     E_DOMAIN_3 ,
     E_MAX
 }E_MASK_DOM;


/******************************************************************************
*
 * REGISTER ADDRESS DEFINATION
 
******************************************************************************/
#define DEVAPC0_D0_APC_0	    ((P_kal_uint32)(DEVAPC0_AO_BASE+0x0000))
#define DEVAPC0_D0_APC_1            ((P_kal_uint32)(DEVAPC0_AO_BASE+0x0004))
#define DEVAPC0_D0_APC_2            ((P_kal_uint32)(DEVAPC0_AO_BASE+0x0008))
#define DEVAPC0_D0_APC_3            ((P_kal_uint32)(DEVAPC0_AO_BASE+0x000C))
#define DEVAPC0_D0_APC_4            ((P_kal_uint32)(DEVAPC0_AO_BASE+0x0010))
#define DEVAPC0_D0_APC_5            ((P_kal_uint32)(DEVAPC0_AO_BASE+0x0014))
#define DEVAPC0_D0_APC_6            ((P_kal_uint32)(DEVAPC0_AO_BASE+0x0018))
#define DEVAPC0_D0_APC_7            ((P_kal_uint32)(DEVAPC0_AO_BASE+0x001C))
#define DEVAPC0_D0_APC_8            ((P_kal_uint32)(DEVAPC0_AO_BASE+0x0020))

#define DEVAPC0_D1_APC_0            ((P_kal_uint32)(DEVAPC0_AO_BASE+0x0100))
#define DEVAPC0_D1_APC_1            ((P_kal_uint32)(DEVAPC0_AO_BASE+0x0104))
#define DEVAPC0_D1_APC_2            ((P_kal_uint32)(DEVAPC0_AO_BASE+0x0108))
#define DEVAPC0_D1_APC_3            ((P_kal_uint32)(DEVAPC0_AO_BASE+0x010C))
#define DEVAPC0_D1_APC_4            ((P_kal_uint32)(DEVAPC0_AO_BASE+0x0110))
#define DEVAPC0_D1_APC_5            ((P_kal_uint32)(DEVAPC0_AO_BASE+0x0114))
#define DEVAPC0_D1_APC_6            ((P_kal_uint32)(DEVAPC0_AO_BASE+0x0118))
#define DEVAPC0_D1_APC_7            ((P_kal_uint32)(DEVAPC0_AO_BASE+0x011C))
#define DEVAPC0_D1_APC_8            ((P_kal_uint32)(DEVAPC0_AO_BASE+0x0120))

#define DEVAPC0_D2_APC_0            ((P_kal_uint32)(DEVAPC0_AO_BASE+0x0200))
#define DEVAPC0_D2_APC_1            ((P_kal_uint32)(DEVAPC0_AO_BASE+0x0204))
#define DEVAPC0_D2_APC_2            ((P_kal_uint32)(DEVAPC0_AO_BASE+0x0208))
#define DEVAPC0_D2_APC_3            ((P_kal_uint32)(DEVAPC0_AO_BASE+0x020C))
#define DEVAPC0_D2_APC_4            ((P_kal_uint32)(DEVAPC0_AO_BASE+0x0210))
#define DEVAPC0_D2_APC_5            ((P_kal_uint32)(DEVAPC0_AO_BASE+0x0214))
#define DEVAPC0_D2_APC_6            ((P_kal_uint32)(DEVAPC0_AO_BASE+0x0218))
#define DEVAPC0_D2_APC_7            ((P_kal_uint32)(DEVAPC0_AO_BASE+0x021C))
#define DEVAPC0_D2_APC_8            ((P_kal_uint32)(DEVAPC0_AO_BASE+0x0220))

#define DEVAPC0_D3_APC_0            ((P_kal_uint32)(DEVAPC0_AO_BASE+0x0300))
#define DEVAPC0_D3_APC_1            ((P_kal_uint32)(DEVAPC0_AO_BASE+0x0304))
#define DEVAPC0_D3_APC_2            ((P_kal_uint32)(DEVAPC0_AO_BASE+0x0308))
#define DEVAPC0_D3_APC_3            ((P_kal_uint32)(DEVAPC0_AO_BASE+0x030C))
#define DEVAPC0_D3_APC_4            ((P_kal_uint32)(DEVAPC0_AO_BASE+0x0310))
#define DEVAPC0_D3_APC_5            ((P_kal_uint32)(DEVAPC0_AO_BASE+0x0314))
#define DEVAPC0_D3_APC_6            ((P_kal_uint32)(DEVAPC0_AO_BASE+0x0318))
#define DEVAPC0_D3_APC_7            ((P_kal_uint32)(DEVAPC0_AO_BASE+0x031C))
#define DEVAPC0_D3_APC_8            ((P_kal_uint32)(DEVAPC0_AO_BASE+0x0320))

#define DEVAPC0_MAS_DOM_0           ((P_kal_uint32)(DEVAPC0_AO_BASE+0x0400))
#define DEVAPC0_MAS_DOM_1           ((P_kal_uint32)(DEVAPC0_AO_BASE+0x0404))
#define DEVAPC0_MAS_SEC             ((P_kal_uint32)(DEVAPC0_AO_BASE+0x0500))
#define DEVAPC0_APC_CON             ((P_kal_uint32)(DEVAPC0_AO_BASE+0x0F00))
#define DEVAPC0_APC_LOCK_0          ((P_kal_uint32)(DEVAPC0_AO_BASE+0x0F04))
#define DEVAPC0_APC_LOCK_1          ((P_kal_uint32)(DEVAPC0_AO_BASE+0x0F08))
#define DEVAPC0_APC_LOCK_2          ((P_kal_uint32)(DEVAPC0_AO_BASE+0x0F0C))
#define DEVAPC0_APC_LOCK_3          ((P_kal_uint32)(DEVAPC0_AO_BASE+0x0F10))
#define DEVAPC0_APC_LOCK_4          ((P_kal_uint32)(DEVAPC0_AO_BASE+0x0F14))

#define DEVAPC0_PD_APC_CON          ((P_kal_uint32)(DEVAPC0_PD_BASE+0x0F00))
#define DEVAPC0_D0_VIO_MASK_0       ((P_kal_uint32)(DEVAPC0_PD_BASE+0x0000))
#define DEVAPC0_D0_VIO_MASK_1       ((P_kal_uint32)(DEVAPC0_PD_BASE+0x0004))
#define DEVAPC0_D0_VIO_MASK_2       ((P_kal_uint32)(DEVAPC0_PD_BASE+0x0008))
#define DEVAPC0_D0_VIO_MASK_3       ((P_kal_uint32)(DEVAPC0_PD_BASE+0x000C))
#define DEVAPC0_D0_VIO_MASK_4       ((P_kal_uint32)(DEVAPC0_PD_BASE+0x0010))
#define DEVAPC0_D0_VIO_STA_0        ((P_kal_uint32)(DEVAPC0_PD_BASE+0x0400))
#define DEVAPC0_D0_VIO_STA_1        ((P_kal_uint32)(DEVAPC0_PD_BASE+0x0404))
#define DEVAPC0_D0_VIO_STA_2        ((P_kal_uint32)(DEVAPC0_PD_BASE+0x0408))
#define DEVAPC0_D0_VIO_STA_3        ((P_kal_uint32)(DEVAPC0_PD_BASE+0x040C))
#define DEVAPC0_D0_VIO_STA_4        ((P_kal_uint32)(DEVAPC0_PD_BASE+0x0410))
#define DEVAPC0_VIO_DBG0            ((P_kal_uint32)(DEVAPC0_PD_BASE+0x0900))
#define DEVAPC0_VIO_DBG1            ((P_kal_uint32)(DEVAPC0_PD_BASE+0x0904))

#define DEVAPC0_DEC_ERR_CON         ((P_kal_uint32)(DEVAPC0_PD_BASE+0x0F80))
#define DEVAPC0_DEC_ERR_ADDR        ((P_kal_uint32)(DEVAPC0_PD_BASE+0x0F84))
#define DEVAPC0_DEC_ERR_ID          ((P_kal_uint32)(DEVAPC0_PD_BASE+0x0F88))


extern int mt_devapc_emi_initial(void);
extern int  mt_devapc_check_emi_violation(void);
extern void mt_devapc_clear_emi_violation(void);

#endif
