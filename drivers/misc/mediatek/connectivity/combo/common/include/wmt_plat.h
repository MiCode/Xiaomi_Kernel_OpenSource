/*! \file
    \brief  Declaration of library functions

    Any definitions in this file will be shared among GLUE Layer and internal Driver Stack.
*/



#ifndef _WMT_PLAT_H_
#define _WMT_PLAT_H_
#include <mach/mtk_wcn_cmb_stub.h>
#include "mtk_wcn_cmb_hw.h"
#include "stp_exp.h"
#include "osal.h"

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/
#if defined(MT6630)
#define CONSYS_WMT_REG_SUSPEND_CB_ENABLE 1
/*disable early_suspend/late_resume handling by default*/
#ifdef CONFIG_EARLYSUSPEND
#define CONSYS_EARLYSUSPEND_ENABLE 0
#else
#define CONSYS_EARLYSUSPEND_ENABLE 0
#endif
#else
#define CONSYS_WMT_REG_SUSPEND_CB_ENABLE 0
#define CONSYS_EARLYSUSPEND_ENABLE 0
#endif

#if defined(MTK_MERGE_INTERFACE_SUPPORT) && (defined(MT6628) || defined(MT6630))
#define MTK_WCN_CMB_MERGE_INTERFACE_SUPPORT 1
#else
#define MTK_WCN_CMB_MERGE_INTERFACE_SUPPORT 0
#endif

#if MTK_WCN_CMB_FOR_SDIO_1V_AUTOK
#define WMT_FOR_SDIO_1V_AUTOK 1
#else
#define WMT_FOR_SDIO_1V_AUTOK 0
#endif

#if (MTK_WCN_CMB_MERGE_INTERFACE_SUPPORT)
/* Supported AP platform:MT6589, MT6595, MT8135 */
/* Supported Connectiity platform: MT6628, MT6630 */

#if defined(MTK_WCN_CMB_AUD_IO_NAMING_STYLE_1) || defined(MTK_WCN_CMB_AUD_IO_NAMING_STYLE_2)

#define GPIO_PCM_DAICLK_PIN_PINMUX_MODE     GPIO_PCM_DAICLK_PIN_M_CLK
#define GPIO_PCM_DAIPCMOUT_PIN_PINMUX_MODE  GPIO_PCM_DAIPCMOUT_PIN_M_MRG_I2S_PCM_TX
#define GPIO_PCM_DAIPCMIN_PIN_PINMUX_MODE   GPIO_PCM_DAIPCMIN_PIN_M_MRG_I2S_PCM_RX
#define GPIO_PCM_DAISYNC_PIN_PINMUX_MODE    GPIO_PCM_DAISYNC_PIN_M_MRG_I2S_PCM_SYNC

#if defined(GPIO_COMBO_I2S_CK_PIN)
#define GPIO_COMBO_I2S_CK_PIN_PINMUX_MODE   GPIO_COMBO_I2S_CK_PIN_M_CLK
#define GPIO_COMBO_I2S_WS_PIN_PINMUX_MODE   GPIO_COMBO_I2S_WS_PIN_M_MRG_I2S_PCM_SYNC
#define GPIO_COMBO_I2S_DAT_PIN_PINMUX_MODE  GPIO_COMBO_I2S_DAT_PIN_M_MRG_I2S_PCM_RX
#define GPIO_PCM_DAIPCMOUT_PIN_PINMUX_MODE  GPIO_PCM_DAIPCMOUT_PIN_M_MRG_I2S_PCM_TX
#endif

#elif defined(MTK_WCN_CMB_AUD_IO_NAMING_STYLE_3)
/* MT6595 */

#define GPIO_PCM_DAICLK_PIN_PINMUX_MODE     GPIO_PCM_DAICLK_PIN_M_CLK
#define GPIO_PCM_DAIPCMOUT_PIN_PINMUX_MODE  GPIO_PCM_DAIPCMOUT_PIN_M_MRG_DO
#define GPIO_PCM_DAIPCMIN_PIN_PINMUX_MODE   GPIO_PCM_DAIPCMIN_PIN_M_MRG_DI
#define GPIO_PCM_DAISYNC_PIN_PINMUX_MODE    GPIO_PCM_DAISYNC_PIN_M_MRG_SYNC

#if defined(GPIO_COMBO_I2S_CK_PIN)
#error "need to config I2S Only mode pinmux marco"
#endif


#endif

#endif


#if defined(MTK_WCN_CMB_AUD_IO_NAMING_STYLE_1)
/* platform: MT6589, MT8135 */
/* PCM Pin */
#define GPIO_PCM_DAICLK_PIN_PCMONLY_MODE       GPIO_PCM_DAICLK_PIN_M_PCM0_CK
#define GPIO_PCM_DAIPCMOUT_PIN_PCMONLY_MODE    GPIO_PCM_DAIPCMOUT_PIN_M_PCM0_DO
#define GPIO_PCM_DAIPCMIN_PIN_PCMONLY_MODE     GPIO_PCM_DAIPCMIN_PIN_M_PCM0_DI
#define GPIO_PCM_DAISYNC_PIN_PCMONLY_MODE      GPIO_PCM_DAISYNC_PIN_M_PCM0_WS

/* I2S Pin */
#if defined(GPIO_COMBO_I2S_CK_PIN)
#define GPIO_COMBO_I2S_CK_PIN_I2SONLY_MODE     GPIO_COMBO_I2S_CK_PIN_M_I2SIN_CK
#define GPIO_COMBO_I2S_WS_PIN_I2SONLY_MODE     GPIO_COMBO_I2S_WS_PIN_M_I2SIN_WS
#define GPIO_COMBO_I2S_DAT_PIN_I2SONLY_MODE    GPIO_COMBO_I2S_DAT_PIN_M_I2SIN_DAT
#endif

#elif defined(MTK_WCN_CMB_AUD_IO_NAMING_STYLE_2)
/* platform: MT6592 */
/* PCM Pin */
#define GPIO_PCM_DAICLK_PIN_PCMONLY_MODE       GPIO_PCM_DAICLK_PIN_M_F2W_CK
#define GPIO_PCM_DAIPCMOUT_PIN_PCMONLY_MODE    GPIO_PCM_DAIPCMOUT_PIN_M_MRG_I2S_PCM_TX
#define GPIO_PCM_DAIPCMIN_PIN_PCMONLY_MODE     GPIO_PCM_DAIPCMIN_PIN_M_MRG_I2S_PCM_RX
#define GPIO_PCM_DAISYNC_PIN_PCMONLY_MODE      GPIO_PCM_DAISYNC_PIN_M_MRG_I2S_PCM_SYNC

/* I2S Pin */
#if defined(GPIO_COMBO_I2S_CK_PIN)
#define GPIO_COMBO_I2S_CK_PIN_I2SONLY_MODE     GPIO_COMBO_I2S_CK_PIN_M_I2SIN1_BCK
#define GPIO_COMBO_I2S_WS_PIN_I2SONLY_MODE     GPIO_COMBO_I2S_WS_PIN_M_I2SIN1_LRCK
#define GPIO_COMBO_I2S_DAT_PIN_I2SONLY_MODE    GPIO_COMBO_I2S_DAT_PIN_M_I2SIN1_DATA_IN
#endif

#elif defined(MTK_WCN_CMB_AUD_IO_NAMING_STYLE_3)
/* platform: MT6595 */
/* PCM Pin */
#define GPIO_PCM_DAICLK_PIN_PCMONLY_MODE       GPIO_PCM_DAICLK_PIN_M_PCM0_CLK
#define GPIO_PCM_DAIPCMOUT_PIN_PCMONLY_MODE    GPIO_PCM_DAIPCMIN_PIN_M_PCM0_DI
#define GPIO_PCM_DAIPCMIN_PIN_PCMONLY_MODE     GPIO_PCM_DAIPCMOUT_PIN_M_PCM0_DO
#define GPIO_PCM_DAISYNC_PIN_PCMONLY_MODE      GPIO_PCM_DAISYNC_PIN_M_PCM0_SYNC

/* I2S Pin */
#if defined(GPIO_COMBO_I2S_CK_PIN)
#error "need to config I2S Only mode pinmux marco"
#endif

#else
/* platform: MT6573/MT6575/MT6577 */
#define GPIO_PCM_DAICLK_PIN_PCMONLY_MODE       GPIO_PCM_DAICLK_PIN_M_CLK
#define GPIO_PCM_DAIPCMOUT_PIN_PCMONLY_MODE    GPIO_PCM_DAIPCMOUT_PIN_M_DAIPCMOUT
#define GPIO_PCM_DAIPCMIN_PIN_PCMONLY_MODE     GPIO_PCM_DAIPCMIN_PIN_M_DAIPCMIN
#define GPIO_PCM_DAISYNC_PIN_PCMONLY_MODE      GPIO_PCM_DAISYNC_PIN_M_BTSYNC

#if defined(GPIO_COMBO_I2S_CK_PIN)
#define GPIO_COMBO_I2S_CK_PIN_I2SONLY_MODE     GPIO_COMBO_I2S_CK_PIN_M_I2S0_CK
#define GPIO_COMBO_I2S_WS_PIN_I2SONLY_MODE     GPIO_COMBO_I2S_WS_PIN_M_I2S0_WS
#define GPIO_COMBO_I2S_DAT_PIN_I2SONLY_MODE    GPIO_COMBO_I2S_DAT_PIN_M_I2S0_DAT
#endif

#endif
/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

#if 0				/* [GeorgeKuo] remove COMBO_AUDIO FLAG */
#define COMBO_AUDIO_BT_MASK (0x1UL)
#define COMBO_AUDIO_BT_PCM_ON (0x1UL << 0)
#define COMBO_AUDIO_BT_PCM_OFF (0x0UL << 0)

#define COMBO_AUDIO_FM_MASK (0x2UL)
#define COMBO_AUDIO_FM_LINEIN (0x0UL << 1)
#define COMBO_AUDIO_FM_I2S (0x1UL << 1)

#define COMBO_AUDIO_PIN_MASK     (0x4UL)
#define COMBO_AUDIO_PIN_SHARE    (0x1UL << 2)
#define COMBO_AUDIO_PIN_SEPARATE (0x0UL << 2)
#endif

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

typedef enum _ENUM_FUNC_STATE_ {
	FUNC_ON = 0,
	FUNC_OFF = 1,
	FUNC_RST = 2,
	FUNC_STAT = 3,
	FUNC_CTRL_MAX,
} ENUM_FUNC_STATE, *P_ENUM_FUNC_STATE;

typedef enum _ENUM_PIN_ID_ {
	PIN_LDO = 0,
	PIN_PMU = 1,
	PIN_RTC = 2,
	PIN_RST = 3,
	PIN_BGF_EINT = 4,
	PIN_WIFI_EINT = 5,
	PIN_ALL_EINT = 6,
	PIN_UART_GRP = 7,
	PIN_PCM_GRP = 8,
	PIN_I2S_GRP = 9,
	PIN_SDIO_GRP = 10,
	PIN_GPS_SYNC = 11,
	PIN_GPS_LNA = 12,
	PIN_UART_RX = 13,
#ifdef CONFIG_MTK_COMBO_COMM_NPWR
    PIN_I2S_DAT = 14,
    PIN_PCM_SYNC = 15,
#endif
	PIN_ID_MAX
} ENUM_PIN_ID, *P_ENUM_PIN_ID;

typedef enum _ENUM_PIN_STATE_ {
	PIN_STA_INIT = 0,
	PIN_STA_OUT_L = 1,
	PIN_STA_OUT_H = 2,
	PIN_STA_IN_L = 3,
	PIN_STA_MUX = 4,
	PIN_STA_EINT_EN = 5,
	PIN_STA_EINT_DIS = 6,
	PIN_STA_DEINIT = 7,
	PIN_STA_SHOW = 8,
	PIN_STA_IN_PU = 9,
	PIN_STA_IN_NP = 10,
	PIN_STA_MAX
} ENUM_PIN_STATE, *P_ENUM_PIN_STATE;

typedef enum _CMB_IF_TYPE_ {
	CMB_IF_UART = 0,
	CMB_IF_WIFI_SDIO = 1,
	CMB_IF_BGF_SDIO = 2,
	CMB_IF_BGWF_SDIO = 3,
	CMB_IF_TYPE_MAX
} CMB_IF_TYPE, *P_CMB_IF_TYPE;

typedef INT32(*fp_set_pin) (ENUM_PIN_STATE);

typedef enum _ENUM_WL_OP_ {
	WL_OP_GET = 0,
	WL_OP_PUT = 1,
	WL_OP_MAX
} ENUM_WL_OP, *P_ENUM_WL_OP;

typedef VOID(*irq_cb) (VOID);
typedef INT32(*device_audio_if_cb) (CMB_STUB_AIF_X aif, MTK_WCN_BOOL share);


/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/


/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

INT32 wmt_plat_init(P_PWR_SEQ_TIME pPwrSeqTime);

INT32 wmt_plat_deinit(VOID);

INT32 wmt_plat_irq_ctrl(ENUM_FUNC_STATE state);

INT32 wmt_plat_pwr_ctrl(ENUM_FUNC_STATE state);

INT32 wmt_plat_ps_ctrl(ENUM_FUNC_STATE state);

INT32 wmt_plat_gpio_ctrl(ENUM_PIN_ID id, ENUM_PIN_STATE state);

INT32 wmt_plat_eirq_ctrl(ENUM_PIN_ID id, ENUM_PIN_STATE state);

INT32 wmt_plat_sdio_ctrl(UINT32 sdioPortNum, ENUM_FUNC_STATE on);


INT32 wmt_plat_wake_lock_ctrl(ENUM_WL_OP opId);

VOID wmt_lib_plat_irq_cb_reg(irq_cb bgf_irq_cb);

INT32 wmt_plat_audio_ctrl(CMB_STUB_AIF_X state, CMB_STUB_AIF_CTRL ctrl);

VOID wmt_lib_plat_aif_cb_reg(device_audio_if_cb aif_ctrl_cb);

INT32 wmt_plat_merge_if_flag_ctrl(UINT32 enagle);

INT32 wmt_plat_merge_if_flag_get(VOID);

INT32 wmt_plat_set_comm_if_type(ENUM_STP_TX_IF_TYPE type);

ENUM_STP_TX_IF_TYPE wmt_plat_get_comm_if_type(VOID);



/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

#endif				/* _WMT_PLAT_H_ */
