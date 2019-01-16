/*! \file
    \brief  Declaration of library functions

    Any definitions in this file will be shared among GLUE Layer and internal Driver Stack.
*/




/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/


#if CONFIG_HAS_WAKELOCK
#include <linux/wakelock.h>
#define CFG_WMT_WAKELOCK_SUPPORT 1
#endif

#ifdef DFT_TAG
#undef DFT_TAG
#endif
#define DFT_TAG         "[WMT-PLAT]"


/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

/* ALPS header files */
#include <mach/mtk_rtc.h>
#include <cust_gpio_usage.h>
#include <cust_eint.h>
#include <mach/eint.h>
#include <mach/mtk_rtc.h>
#include <mach/mt_gpio.h>
#include <cust_gpio_usage.h>
#ifdef CONFIG_MTK_MT6306_SUPPORT
#include <mach/dcl_sim_gpio.h>
#endif

/* ALPS and COMBO header files */
#include <mach/mtk_wcn_cmb_stub.h>
/* MTK_WCN_COMBO header files */
#include "wmt_plat.h"
#include "wmt_dev.h"
#include "wmt_lib.h"
#include "mtk_wcn_cmb_hw.h"
#include "osal_typedef.h"

#ifdef GPIO_COMBO_BGF_EINT_PIN
#define CONFIG_EINT_DEVICE_TREE 1

#if CONFIG_EINT_DEVICE_TREE 1
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/irqreturn.h>

static UINT32 bfg_irq = 0;
#endif
#endif

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/




/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
#ifdef GPIO_COMBO_BGF_EINT_PIN
static VOID wmt_plat_bgf_eirq_cb(VOID);
#endif
static INT32 wmt_plat_ldo_ctrl(ENUM_PIN_STATE state);
static INT32 wmt_plat_pmu_ctrl(ENUM_PIN_STATE state);
static INT32 wmt_plat_rtc_ctrl(ENUM_PIN_STATE state);
static INT32 wmt_plat_rst_ctrl(ENUM_PIN_STATE state);
static INT32 wmt_plat_bgf_eint_ctrl(ENUM_PIN_STATE state);
static INT32 wmt_plat_wifi_eint_ctrl(ENUM_PIN_STATE state);
static INT32 wmt_plat_all_eint_ctrl(ENUM_PIN_STATE state);
static INT32 wmt_plat_uart_ctrl(ENUM_PIN_STATE state);
static INT32 wmt_plat_pcm_ctrl(ENUM_PIN_STATE state);
static INT32 wmt_plat_i2s_ctrl(ENUM_PIN_STATE state);
static INT32 wmt_plat_sdio_pin_ctrl(ENUM_PIN_STATE state);
static INT32 wmt_plat_gps_sync_ctrl(ENUM_PIN_STATE state);
static INT32 wmt_plat_gps_lna_ctrl(ENUM_PIN_STATE state);
static INT32 wmt_plat_uart_rx_ctrl(ENUM_PIN_STATE state);
#ifdef CONFIG_MTK_COMBO_COMM_NPWR
static INT32 wmt_plat_i2s_dat_ctrl (ENUM_PIN_STATE state);
static INT32 wmt_plat_pcm_sync_ctrl (ENUM_PIN_STATE state);
#endif
static INT32 wmt_plat_dump_pin_conf(VOID);
extern int board_sdio_ctrl(unsigned int sdio_port_num, unsigned int on);



/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/
UINT32 gWmtDbgLvl = WMT_LOG_INFO;
INT32 gWmtMergeIfSupport = 0;
static ENUM_STP_TX_IF_TYPE gCommIfType = STP_MAX_IF_TX;


/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/




#if CFG_WMT_WAKELOCK_SUPPORT
static OSAL_SLEEPABLE_LOCK gOsSLock;
static struct wake_lock wmtWakeLock;
#endif



irq_cb wmt_plat_bgf_irq_cb = NULL;
device_audio_if_cb wmt_plat_audio_if_cb = NULL;



const static fp_set_pin gfp_set_pin_table[] = {
	[PIN_LDO] = wmt_plat_ldo_ctrl,
	[PIN_PMU] = wmt_plat_pmu_ctrl,
	[PIN_RTC] = wmt_plat_rtc_ctrl,
	[PIN_RST] = wmt_plat_rst_ctrl,
	[PIN_BGF_EINT] = wmt_plat_bgf_eint_ctrl,
	[PIN_WIFI_EINT] = wmt_plat_wifi_eint_ctrl,
	[PIN_ALL_EINT] = wmt_plat_all_eint_ctrl,
	[PIN_UART_GRP] = wmt_plat_uart_ctrl,
	[PIN_PCM_GRP] = wmt_plat_pcm_ctrl,
	[PIN_I2S_GRP] = wmt_plat_i2s_ctrl,
	[PIN_SDIO_GRP] = wmt_plat_sdio_pin_ctrl,
	[PIN_GPS_SYNC] = wmt_plat_gps_sync_ctrl,
	[PIN_GPS_LNA] = wmt_plat_gps_lna_ctrl,
	[PIN_UART_RX] = wmt_plat_uart_rx_ctrl,
#ifdef CONFIG_MTK_COMBO_COMM_NPWR
    [PIN_I2S_DAT] = wmt_plat_i2s_dat_ctrl,
    [PIN_PCM_SYNC] = wmt_plat_pcm_sync_ctrl,
#endif
};

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

/*!
 * \brief audio control callback function for CMB_STUB on ALPS
 *
 * A platform function required for dynamic binding with CMB_STUB on ALPS.
 *
 * \param state desired audio interface state to use
 * \param flag audio interface control options
 *
 * \retval 0 operation success
 * \retval -1 invalid parameters
 * \retval < 0 error for operation fail
 */
INT32 wmt_plat_audio_ctrl(CMB_STUB_AIF_X state, CMB_STUB_AIF_CTRL ctrl)
{
	INT32 iRet = 0;
	UINT32 pinShare = 0;
	UINT32 mergeIfSupport = 0;

	/* input sanity check */
	if ((CMB_STUB_AIF_MAX <= state)
	    || (CMB_STUB_AIF_CTRL_MAX <= ctrl)) {
		return -1;
	}

	iRet = 0;

	/* set host side first */
	switch (state) {
	case CMB_STUB_AIF_0:
		/* BT_PCM_OFF & FM line in/out */
		iRet += wmt_plat_gpio_ctrl(PIN_PCM_GRP, PIN_STA_DEINIT);
		iRet += wmt_plat_gpio_ctrl(PIN_I2S_GRP, PIN_STA_DEINIT);
		break;

	case CMB_STUB_AIF_1:
		iRet += wmt_plat_gpio_ctrl(PIN_I2S_GRP, PIN_STA_DEINIT);
		iRet += wmt_plat_gpio_ctrl(PIN_PCM_GRP, PIN_STA_INIT);
		break;

	case CMB_STUB_AIF_2:
		iRet += wmt_plat_gpio_ctrl(PIN_PCM_GRP, PIN_STA_DEINIT);
		iRet += wmt_plat_gpio_ctrl(PIN_I2S_GRP, PIN_STA_INIT);
		break;

	case CMB_STUB_AIF_3:
		iRet += wmt_plat_gpio_ctrl(PIN_PCM_GRP, PIN_STA_INIT);
		iRet += wmt_plat_gpio_ctrl(PIN_I2S_GRP, PIN_STA_INIT);
		break;

	default:
		/* FIXME: move to cust folder? */
		WMT_ERR_FUNC("invalid state [%d]\n", state);
		return -1;
		break;
	}
	if (0 != wmt_plat_merge_if_flag_get()) {

#if (MTK_WCN_CMB_MERGE_INTERFACE_SUPPORT)
		WMT_INFO_FUNC("[MT6628]<Merge IF> no need to ctrl combo chip side GPIO\n");
#else
		mergeIfSupport = 1;
#endif
	} else {
		mergeIfSupport = 1;
	}

	if (0 != mergeIfSupport) {
		if (CMB_STUB_AIF_CTRL_EN == ctrl) {
			WMT_INFO_FUNC("call chip aif setting\n");
			/* need to control chip side GPIO */
			if (NULL != wmt_plat_audio_if_cb) {
				iRet +=
				    (*wmt_plat_audio_if_cb) (state,
							     (pinShare) ? MTK_WCN_BOOL_TRUE :
							     MTK_WCN_BOOL_FALSE);
			} else {
				WMT_WARN_FUNC("wmt_plat_audio_if_cb is not registered\n");
				iRet -= 1;
			}


		} else {
			WMT_INFO_FUNC("skip chip aif setting\n");
		}
	}
	return iRet;

}

#ifdef GPIO_COMBO_BGF_EINT_PIN
irqreturn_t wmt_plat_bgf_eirq_cb (int irq, void *data)
{
#if CFG_WMT_PS_SUPPORT
/* #error "need to disable EINT here" */
	/* wmt_lib_ps_irq_cb(); */
	if (NULL != wmt_plat_bgf_irq_cb) {
		(*(wmt_plat_bgf_irq_cb)) ();
	} else {
		WMT_WARN_FUNC("WMT-PLAT: wmt_plat_bgf_irq_cb not registered\n");
	}
	return IRQ_HANDLED;
#else
	return IRQ_HANDLED;
#endif

}
#endif


VOID wmt_lib_plat_irq_cb_reg(irq_cb bgf_irq_cb)
{
	wmt_plat_bgf_irq_cb = bgf_irq_cb;
}

VOID wmt_lib_plat_aif_cb_reg(device_audio_if_cb aif_ctrl_cb)
{
	wmt_plat_audio_if_cb = aif_ctrl_cb;
}






INT32 wmt_plat_init(P_PWR_SEQ_TIME pPwrSeqTime)
{

	/*PWR_SEQ_TIME pwr_seq_time; */
	INT32 iret = -1;
	/* init cmb_hw */
	iret += mtk_wcn_cmb_hw_init(pPwrSeqTime);

	/*init wmt function ctrl wakelock if wake lock is supported by host platform */
#ifdef CFG_WMT_WAKELOCK_SUPPORT
	wake_lock_init(&wmtWakeLock, WAKE_LOCK_SUSPEND, "wmtFuncCtrl");
	osal_sleepable_lock_init(&gOsSLock);
#endif

	WMT_DBG_FUNC("WMT-PLAT: ALPS platform init (%d)\n", iret);

	return 0;
}


INT32 wmt_plat_deinit(VOID)
{
	INT32 iret;

	/* 1. de-init cmb_hw */
	iret = mtk_wcn_cmb_hw_deinit();
	/* 2. unreg to cmb_stub */
	iret += mtk_wcn_cmb_stub_unreg();
	/*3. wmt wakelock deinit */
#ifdef CFG_WMT_WAKELOCK_SUPPORT
	wake_lock_destroy(&wmtWakeLock);
	osal_sleepable_lock_deinit(&gOsSLock);
	WMT_DBG_FUNC("destroy wmtWakeLock\n");
#endif
	WMT_DBG_FUNC("WMT-PLAT: ALPS platform init (%d)\n", iret);

	return 0;
}

INT32 wmt_plat_sdio_ctrl(WMT_SDIO_SLOT_NUM sdioPortType, ENUM_FUNC_STATE on)
{
	return board_sdio_ctrl(sdioPortType, (FUNC_OFF == on) ? 0 : 1);
}

INT32 wmt_plat_irq_ctrl(ENUM_FUNC_STATE state)
{
	return -1;
}


static INT32 wmt_plat_dump_pin_conf(VOID)
{
	WMT_INFO_FUNC("[WMT-PLAT]=>dump wmt pin configuration start<=\n");

#ifdef GPIO_COMBO_6620_LDO_EN_PIN
	WMT_INFO_FUNC("LDO(GPIO%d)\n", GPIO_COMBO_6620_LDO_EN_PIN);
#else
	WMT_INFO_FUNC("LDO(not defined)\n");
#endif

#ifdef GPIO_COMBO_PMU_EN_PIN
	WMT_INFO_FUNC("PMU(GPIO%d)\n", GPIO_COMBO_PMU_EN_PIN);
#else
	WMT_INFO_FUNC("PMU(not defined)\n");
#endif

#ifdef GPIO_COMBO_PMUV28_EN_PIN
	WMT_INFO_FUNC("PMUV28(GPIO%d)\n", GPIO_COMBO_PMUV28_EN_PIN);
#else
	WMT_INFO_FUNC("PMUV28(not defined)\n");
#endif

#ifdef GPIO_COMBO_RST_PIN
	WMT_INFO_FUNC("RST(GPIO%d)\n", GPIO_COMBO_RST_PIN);
#else
	WMT_INFO_FUNC("RST(not defined)\n");
#endif

#ifdef GPIO_COMBO_BGF_EINT_PIN
	WMT_INFO_FUNC("BGF_EINT(GPIO%d)\n", GPIO_COMBO_BGF_EINT_PIN);
#else
	WMT_INFO_FUNC("BGF_EINT(not defined)\n");
#endif

#ifdef CUST_EINT_COMBO_BGF_NUM
	WMT_INFO_FUNC("BGF_EINT_NUM(%d)\n", CUST_EINT_COMBO_BGF_NUM);
#else
	WMT_INFO_FUNC("BGF_EINT_NUM(not defined)\n");
#endif

#ifdef GPIO_WIFI_EINT_PIN
	WMT_INFO_FUNC("WIFI_EINT(GPIO%d)\n", GPIO_WIFI_EINT_PIN);
#else
	WMT_INFO_FUNC("WIFI_EINT(not defined)\n");
#endif

#ifdef CUST_EINT_WIFI_NUM
	WMT_INFO_FUNC("WIFI_EINT_NUM(%d)\n", CUST_EINT_WIFI_NUM);
#else
	WMT_INFO_FUNC("WIFI_EINT_NUM(not defined)\n");
#endif

#ifdef GPIO_COMBO_URXD_PIN
	WMT_INFO_FUNC("UART_RX(GPIO%d)\n", GPIO_COMBO_URXD_PIN);
#else
	WMT_INFO_FUNC("UART_RX(not defined)\n");
#endif
#ifdef GPIO_COMBO_UTXD_PIN
	WMT_INFO_FUNC("UART_TX(GPIO%d)\n", GPIO_COMBO_UTXD_PIN);
#else
	WMT_INFO_FUNC("UART_TX(not defined)\n");
#endif
#ifdef GPIO_PCM_DAICLK_PIN
	WMT_INFO_FUNC("DAICLK(GPIO%d)\n", GPIO_PCM_DAICLK_PIN);
#else
	WMT_INFO_FUNC("DAICLK(not defined)\n");
#endif
#ifdef GPIO_PCM_DAIPCMOUT_PIN
	WMT_INFO_FUNC("PCMOUT(GPIO%d)\n", GPIO_PCM_DAIPCMOUT_PIN);
#else
	WMT_INFO_FUNC("PCMOUT(not defined)\n");
#endif
#ifdef GPIO_PCM_DAIPCMIN_PIN
	WMT_INFO_FUNC("PCMIN(GPIO%d)\n", GPIO_PCM_DAIPCMIN_PIN);
#else
	WMT_INFO_FUNC("PCMIN(not defined)\n");
#endif
#ifdef GPIO_PCM_DAISYNC_PIN
	WMT_INFO_FUNC("PCMSYNC(GPIO%d)\n", GPIO_PCM_DAISYNC_PIN);
#else
	WMT_INFO_FUNC("PCMSYNC(not defined)\n");
#endif
#if defined(FM_DIGITAL_INPUT) || defined(FM_DIGITAL_OUTPUT)
#ifdef GPIO_COMBO_I2S_CK_PIN
	WMT_INFO_FUNC("I2S_CK(GPIO%d)\n", GPIO_COMBO_I2S_CK_PIN);
#else
	WMT_INFO_FUNC("I2S_CK(not defined)\n");
#endif
#ifdef GPIO_COMBO_I2S_WS_PIN
	WMT_INFO_FUNC("I2S_WS(GPIO%d)\n", GPIO_COMBO_I2S_WS_PIN);
#else
	WMT_INFO_FUNC("I2S_WS(not defined)\n");
#endif
#ifdef GPIO_COMBO_I2S_DAT_PIN
	WMT_INFO_FUNC("I2S_DAT(GPIO%d)\n", GPIO_COMBO_I2S_DAT_PIN);
#else
	WMT_INFO_FUNC("I2S_DAT(not defined)\n");
#endif
#else				/* FM_ANALOG_INPUT || FM_ANALOG_OUTPUT */
	WMT_INFO_FUNC("FM digital mode is not set, no need for I2S GPIOs\n");
#endif
#ifdef GPIO_GPS_SYNC_PIN
	WMT_INFO_FUNC("GPS_SYNC(GPIO%d)\n", GPIO_GPS_SYNC_PIN);
#else
	WMT_INFO_FUNC("GPS_SYNC(not defined)\n");
#endif

#ifdef GPIO_GPS_LNA_PIN
	WMT_INFO_FUNC("GPS_LNA(GPIO%d)\n", GPIO_GPS_LNA_PIN);
#else
	WMT_INFO_FUNC("GPS_LNA(not defined)\n");
#endif
	WMT_INFO_FUNC("[WMT-PLAT]=>dump wmt pin configuration emds<=\n");
	return 0;
}


INT32 wmt_plat_pwr_ctrl(ENUM_FUNC_STATE state)
{
	INT32 ret = -1;

	switch (state) {
	case FUNC_ON:
		/* TODO:[ChangeFeature][George] always output this or by request throuth /proc or sysfs? */
		wmt_plat_dump_pin_conf();
		ret = mtk_wcn_cmb_hw_pwr_on();
		break;

	case FUNC_OFF:
		ret = mtk_wcn_cmb_hw_pwr_off();
		break;

	case FUNC_RST:
		ret = mtk_wcn_cmb_hw_rst();
		break;
	case FUNC_STAT:
		ret = mtk_wcn_cmb_hw_state_show();
		break;
	default:
		WMT_WARN_FUNC("WMT-PLAT:Warnning, invalid state(%d) in pwr_ctrl\n", state);
		break;
	}

	return ret;
}

INT32 wmt_plat_ps_ctrl(ENUM_FUNC_STATE state)
{
	return -1;
}

INT32 wmt_plat_eirq_ctrl(ENUM_PIN_ID id, ENUM_PIN_STATE state)
{
	INT32 iret;

	/* TODO: [ChangeFeature][GeorgeKuo]: use another function to handle this, as done in gpio_ctrls */

	if ((PIN_STA_INIT != state)
	    && (PIN_STA_DEINIT != state)
	    && (PIN_STA_EINT_EN != state)
	    && (PIN_STA_EINT_DIS != state)) {
		WMT_WARN_FUNC("WMT-PLAT:invalid PIN_STATE(%d) in eirq_ctrl for PIN(%d)\n", state,
			      id);
		return -1;
	}

	iret = -2;
	switch (id) {
	case PIN_BGF_EINT:
#ifdef GPIO_COMBO_BGF_EINT_PIN
		if (PIN_STA_INIT == state) {
#ifdef GPIO_COMBO_BGF_EINT_PIN

#if CONFIG_EINT_DEVICE_TREE
	struct device_node *node;
	UINT32 ints[2] = {0,0};
	INT32 ret = -EINVAL;

	node = of_find_compatible_node(NULL, NULL, "mediatek, BGF-eint");/*BGF-eint name maybe wrong*/
	if(node) {
		
#if CUST_EINT_COMBO_BGF_DEBOUNCE_EN
		of_property_read_u32_array(node, "debounce", ints, ARRAY_SIZE(ints));
		mt_gpio_set_debounce(ints[0], ints[1]);
#endif
		bfg_irq = irq_of_parse_and_map(node, 0);
		ret = request_irq(bfg_irq, wmt_plat_bgf_eirq_cb, IRQF_TRIGGER_NONE,"BGF-eint", NULL);
		if(ret)
			printk(KERN_ERR "BGF EINT IRQ LINE NOT AVAILABLE!!\n");
		else
			printk(KERN_INFO "BGF EINT request_irq sucess!!\n");
	} else
		printk(KERN_ERR "[%s] can't find BGF eint compatible node\n",__func__);
#else
#if CUST_EINT_COMBO_BGF_DEBOUNCE_EN
            mt_eint_set_hw_debounce(CUST_EINT_COMBO_BGF_NUM, CUST_EINT_COMBO_BGF_DEBOUNCE_CN);
#endif
			mt_eint_registration(CUST_EINT_COMBO_BGF_NUM,
					     CUST_EINT_COMBO_BGF_TYPE, wmt_plat_bgf_eirq_cb, 0);
			mt_eint_mask(CUST_EINT_COMBO_BGF_NUM);	/*2 */
#endif
#endif
		} else if (PIN_STA_EINT_EN == state) {
#if CONFIG_EINT_DEVICE_TREE
			 enable_irq(bfg_irq);
#else
             mt_eint_unmask(CUST_EINT_COMBO_BGF_NUM);
#endif			 
			WMT_DBG_FUNC("WMT-PLAT:BGFInt (en)\n");
		} else if (PIN_STA_EINT_DIS == state) {
#if CONFIG_EINT_DEVICE_TREE
			disable_irq_nosync(bfg_irq);
#else
            mt_eint_mask(CUST_EINT_COMBO_BGF_NUM);
#endif
			WMT_DBG_FUNC("WMT-PLAT:BGFInt (dis)\n");
		} else {
#if CONFIG_EINT_DEVICE_TREE
			disable_irq_nosync(bfg_irq);	
#else
            mt_eint_mask(CUST_EINT_COMBO_BGF_NUM);
#endif
            WMT_DBG_FUNC("WMT-PLAT:BGFInt (dis) \n");
			/* de-init: nothing to do in ALPS, such as un-registration... */
		}
#else
		WMT_INFO_FUNC("WMT-PLAT:BGF EINT not defined\n");
#endif
		iret = 0;
		break;

	case PIN_ALL_EINT:
#ifdef GPIO_COMBO_ALL_EINT_PIN
		if (PIN_STA_INIT == state) {
#if 0
#if CUST_EINT_COMBO_ALL_DEBOUNCE_EN
			mt_eint_set_hw_debounce(CUST_EINT_COMBO_ALL_NUM,
						CUST_EINT_COMBO_ALL_DEBOUNCE_CN);
#endif
			mt_eint_registration(CUST_EINT_COMBO_ALL_NUM,
					     CUST_EINT_COMBO_ALL_TYPE, combo_bgf_eirq_handler, 0);
#endif
			mt_eint_mask(CUST_EINT_COMBO_ALL_NUM);	/*2 */
			WMT_DBG_FUNC("WMT-PLAT:ALLInt (INIT but not used yet)\n");
		} else if (PIN_STA_EINT_EN == state) {
			/*mt_eint_unmask(CUST_EINT_COMBO_ALL_NUM); */
			WMT_DBG_FUNC("WMT-PLAT:ALLInt (EN but not used yet)\n");
		} else if (PIN_STA_EINT_DIS == state) {
			mt_eint_mask(CUST_EINT_COMBO_ALL_NUM);
			WMT_DBG_FUNC("WMT-PLAT:ALLInt (DIS but not used yet)\n");
		} else {
			mt_eint_mask(CUST_EINT_COMBO_ALL_NUM);
			WMT_DBG_FUNC("WMT-PLAT:ALLInt (DEINIT but not used yet)\n");
			/* de-init: nothing to do in ALPS, such as un-registration... */
		}
#else
		WMT_INFO_FUNC("WMT-PLAT:ALL EINT not defined\n");
#endif
		iret = 0;
		break;

	default:
		WMT_WARN_FUNC("WMT-PLAT:unsupported EIRQ(PIN_ID:%d) in eirq_ctrl\n", id);
		iret = -1;
		break;
	}

	return iret;
}

INT32 wmt_plat_gpio_ctrl(ENUM_PIN_ID id, ENUM_PIN_STATE state)
{
	if ((PIN_ID_MAX > id)
	    && (PIN_STA_MAX > state)) {

		/* TODO: [FixMe][GeorgeKuo] do sanity check to const function table when init and skip checking here */
		if (gfp_set_pin_table[id]) {
			return (*(gfp_set_pin_table[id])) (state);	/* .handler */
		} else {
			WMT_WARN_FUNC("WMT-PLAT: null fp for gpio_ctrl(%d)\n", id);
			return -2;
		}
	}
	return -1;
}

INT32 wmt_plat_ldo_ctrl(ENUM_PIN_STATE state)
{
#ifdef GPIO_COMBO_6620_LDO_EN_PIN
	switch (state) {
	case PIN_STA_INIT:
		/*set to gpio output low, disable pull */
		mt_set_gpio_pull_enable(GPIO_COMBO_6620_LDO_EN_PIN, GPIO_PULL_DISABLE);
		mt_set_gpio_dir(GPIO_COMBO_6620_LDO_EN_PIN, GPIO_DIR_OUT);
		mt_set_gpio_mode(GPIO_COMBO_6620_LDO_EN_PIN, GPIO_MODE_GPIO);
		mt_set_gpio_out(GPIO_COMBO_6620_LDO_EN_PIN, GPIO_OUT_ZERO);
		WMT_DBG_FUNC("WMT-PLAT:LDO init (out 0)\n");
		break;

	case PIN_STA_OUT_H:
		mt_set_gpio_out(GPIO_COMBO_6620_LDO_EN_PIN, GPIO_OUT_ONE);
		WMT_DBG_FUNC("WMT-PLAT:LDO (out 1)\n");
		break;

	case PIN_STA_OUT_L:
		mt_set_gpio_out(GPIO_COMBO_6620_LDO_EN_PIN, GPIO_OUT_ZERO);
		WMT_DBG_FUNC("WMT-PLAT:LDO (out 0)\n");
		break;

	case PIN_STA_IN_L:
	case PIN_STA_DEINIT:
		/*set to gpio input low, pull down enable */
		mt_set_gpio_mode(GPIO_COMBO_6620_LDO_EN_PIN, GPIO_COMBO_6620_LDO_EN_PIN_M_GPIO);
		mt_set_gpio_dir(GPIO_COMBO_6620_LDO_EN_PIN, GPIO_DIR_IN);
		mt_set_gpio_pull_select(GPIO_COMBO_6620_LDO_EN_PIN, GPIO_PULL_DOWN);
		mt_set_gpio_pull_enable(GPIO_COMBO_6620_LDO_EN_PIN, GPIO_PULL_ENABLE);
		WMT_DBG_FUNC("WMT-PLAT:LDO deinit (in pd)\n");
		break;

	default:
		WMT_WARN_FUNC("WMT-PLAT:Warnning, invalid state(%d) on LDO\n", state);
		break;
	}
#else
	WMT_INFO_FUNC("WMT-PLAT:LDO is not used\n");
#endif
	return 0;
}

INT32 wmt_plat_pmu_ctrl(ENUM_PIN_STATE state)
{
	switch (state) {
	case PIN_STA_INIT:
		/*set to gpio output low, disable pull */
		mt_set_gpio_pull_enable(GPIO_COMBO_PMU_EN_PIN, GPIO_PULL_DISABLE);
		mt_set_gpio_dir(GPIO_COMBO_PMU_EN_PIN, GPIO_DIR_OUT);
		mt_set_gpio_mode(GPIO_COMBO_PMU_EN_PIN, GPIO_MODE_GPIO);
		mt_set_gpio_out(GPIO_COMBO_PMU_EN_PIN, GPIO_OUT_ZERO);
#ifdef GPIO_COMBO_PMUV28_EN_PIN
		mt_set_gpio_pull_enable(GPIO_COMBO_PMUV28_EN_PIN, GPIO_PULL_DISABLE);
		mt_set_gpio_dir(GPIO_COMBO_PMUV28_EN_PIN, GPIO_DIR_OUT);
		mt_set_gpio_mode(GPIO_COMBO_PMUV28_EN_PIN, GPIO_MODE_GPIO);
		mt_set_gpio_out(GPIO_COMBO_PMUV28_EN_PIN, GPIO_OUT_ZERO);
#endif
		WMT_DBG_FUNC("WMT-PLAT:PMU init (out 0)\n");
		break;

	case PIN_STA_OUT_H:
		mt_set_gpio_out(GPIO_COMBO_PMU_EN_PIN, GPIO_OUT_ONE);
#ifdef GPIO_COMBO_PMUV28_EN_PIN
		mt_set_gpio_out(GPIO_COMBO_PMUV28_EN_PIN, GPIO_OUT_ONE);
#endif
		WMT_DBG_FUNC("WMT-PLAT:PMU (out 1)\n");
		break;

	case PIN_STA_OUT_L:
		mt_set_gpio_out(GPIO_COMBO_PMU_EN_PIN, GPIO_OUT_ZERO);
#ifdef GPIO_COMBO_PMUV28_EN_PIN
		mt_set_gpio_out(GPIO_COMBO_PMUV28_EN_PIN, GPIO_OUT_ZERO);
#endif
		WMT_DBG_FUNC("WMT-PLAT:PMU (out 0)\n");
		break;

	case PIN_STA_IN_L:
	case PIN_STA_DEINIT:
		/*set to gpio input low, pull down enable */
		mt_set_gpio_mode(GPIO_COMBO_PMU_EN_PIN, GPIO_COMBO_PMU_EN_PIN_M_GPIO);
		mt_set_gpio_dir(GPIO_COMBO_PMU_EN_PIN, GPIO_DIR_IN);
		mt_set_gpio_pull_select(GPIO_COMBO_PMU_EN_PIN, GPIO_PULL_DOWN);
		mt_set_gpio_pull_enable(GPIO_COMBO_PMU_EN_PIN, GPIO_PULL_ENABLE);
#ifdef GPIO_COMBO_PMUV28_EN_PIN
		mt_set_gpio_mode(GPIO_COMBO_PMUV28_EN_PIN, GPIO_COMBO_PMUV28_EN_PIN_M_GPIO);
		mt_set_gpio_dir(GPIO_COMBO_PMUV28_EN_PIN, GPIO_DIR_IN);
		mt_set_gpio_pull_select(GPIO_COMBO_PMUV28_EN_PIN, GPIO_PULL_DOWN);
		mt_set_gpio_pull_enable(GPIO_COMBO_PMUV28_EN_PIN, GPIO_PULL_ENABLE);
#endif
		WMT_DBG_FUNC("WMT-PLAT:PMU deinit (in pd)\n");
		break;
	case PIN_STA_SHOW:
		WMT_INFO_FUNC("WMT-PLAT:PMU PIN_STA_SHOW start\n");
		WMT_INFO_FUNC("WMT-PLAT:PMU Mode(%d)\n", mt_get_gpio_mode(GPIO_COMBO_PMU_EN_PIN));
		WMT_INFO_FUNC("WMT-PLAT:PMU Dir(%d)\n", mt_get_gpio_dir(GPIO_COMBO_PMU_EN_PIN));
		WMT_INFO_FUNC("WMT-PLAT:PMU Pull enable(%d)\n",
			      mt_get_gpio_pull_enable(GPIO_COMBO_PMU_EN_PIN));
		WMT_INFO_FUNC("WMT-PLAT:PMU Pull select(%d)\n",
			      mt_get_gpio_pull_select(GPIO_COMBO_PMU_EN_PIN));
		WMT_INFO_FUNC("WMT-PLAT:PMU out(%d)\n", mt_get_gpio_out(GPIO_COMBO_PMU_EN_PIN));
		WMT_INFO_FUNC("WMT-PLAT:PMU PIN_STA_SHOW end\n");
		break;
	default:
		WMT_WARN_FUNC("WMT-PLAT:Warnning, invalid state(%d) on PMU\n", state);
		break;
	}

	return 0;
}

INT32 wmt_plat_rtc_ctrl(ENUM_PIN_STATE state)
{
	switch (state) {
	case PIN_STA_INIT:
		rtc_gpio_enable_32k(RTC_GPIO_USER_GPS);
		WMT_DBG_FUNC("WMT-PLAT:RTC init\n");
		break;
	case PIN_STA_SHOW:
		WMT_INFO_FUNC("WMT-PLAT:RTC PIN_STA_SHOW start\n");
		/* TakMan: Temp. solution for building pass. Hongcheng Xia should check with vend_ownen.chen */
		/* WMT_INFO_FUNC("WMT-PLAT:RTC Status(%d)\n", rtc_gpio_32k_status()); */
		WMT_INFO_FUNC("WMT-PLAT:RTC PIN_STA_SHOW end\n");
		break;
	default:
		WMT_WARN_FUNC("WMT-PLAT:Warnning, invalid state(%d) on RTC\n", state);
		break;
	}
	return 0;
}


INT32 wmt_plat_rst_ctrl(ENUM_PIN_STATE state)
{
	switch (state) {
	case PIN_STA_INIT:
		/*set to gpio output low, disable pull */
		mt_set_gpio_pull_enable(GPIO_COMBO_RST_PIN, GPIO_PULL_DISABLE);
		mt_set_gpio_dir(GPIO_COMBO_RST_PIN, GPIO_DIR_OUT);
		mt_set_gpio_mode(GPIO_COMBO_RST_PIN, GPIO_MODE_GPIO);
		mt_set_gpio_out(GPIO_COMBO_RST_PIN, GPIO_OUT_ZERO);
		WMT_DBG_FUNC("WMT-PLAT:RST init (out 0)\n");
		break;

	case PIN_STA_OUT_H:
		mt_set_gpio_out(GPIO_COMBO_RST_PIN, GPIO_OUT_ONE);
		WMT_DBG_FUNC("WMT-PLAT:RST (out 1)\n");
		break;

	case PIN_STA_OUT_L:
		mt_set_gpio_out(GPIO_COMBO_RST_PIN, GPIO_OUT_ZERO);
		WMT_DBG_FUNC("WMT-PLAT:RST (out 0)\n");
		break;

	case PIN_STA_IN_L:
	case PIN_STA_DEINIT:
		/*set to gpio input low, pull down enable */
		mt_set_gpio_mode(GPIO_COMBO_RST_PIN, GPIO_COMBO_RST_PIN_M_GPIO);
		mt_set_gpio_dir(GPIO_COMBO_RST_PIN, GPIO_DIR_IN);
		mt_set_gpio_pull_select(GPIO_COMBO_RST_PIN, GPIO_PULL_DOWN);
		mt_set_gpio_pull_enable(GPIO_COMBO_RST_PIN, GPIO_PULL_ENABLE);
		WMT_DBG_FUNC("WMT-PLAT:RST deinit (in pd)\n");
		break;
	case PIN_STA_SHOW:
		WMT_INFO_FUNC("WMT-PLAT:RST PIN_STA_SHOW start\n");
		WMT_INFO_FUNC("WMT-PLAT:RST Mode(%d)\n", mt_get_gpio_mode(GPIO_COMBO_RST_PIN));
		WMT_INFO_FUNC("WMT-PLAT:RST Dir(%d)\n", mt_get_gpio_dir(GPIO_COMBO_RST_PIN));
		WMT_INFO_FUNC("WMT-PLAT:RST Pull enable(%d)\n",
			      mt_get_gpio_pull_enable(GPIO_COMBO_RST_PIN));
		WMT_INFO_FUNC("WMT-PLAT:RST Pull select(%d)\n",
			      mt_get_gpio_pull_select(GPIO_COMBO_RST_PIN));
		WMT_INFO_FUNC("WMT-PLAT:RST out(%d)\n", mt_get_gpio_out(GPIO_COMBO_RST_PIN));
		WMT_INFO_FUNC("WMT-PLAT:RST PIN_STA_SHOW end\n");
		break;

	default:
		WMT_WARN_FUNC("WMT-PLAT:Warnning, invalid state(%d) on RST\n", state);
		break;
	}

	return 0;
}

INT32 wmt_plat_bgf_eint_ctrl(ENUM_PIN_STATE state)
{
#ifdef GPIO_COMBO_BGF_EINT_PIN
	switch (state) {
	case PIN_STA_INIT:
		/*set to gpio input low, pull down enable */
		mt_set_gpio_mode(GPIO_COMBO_BGF_EINT_PIN, GPIO_COMBO_BGF_EINT_PIN_M_GPIO);
		mt_set_gpio_dir(GPIO_COMBO_BGF_EINT_PIN, GPIO_DIR_IN);
		mt_set_gpio_pull_select(GPIO_COMBO_BGF_EINT_PIN, GPIO_PULL_DOWN);
		mt_set_gpio_pull_enable(GPIO_COMBO_BGF_EINT_PIN, GPIO_PULL_ENABLE);
		WMT_DBG_FUNC("WMT-PLAT:BGFInt init(in pd)\n");
		break;

	case PIN_STA_MUX:
		mt_set_gpio_mode(GPIO_COMBO_BGF_EINT_PIN, GPIO_COMBO_BGF_EINT_PIN_M_GPIO);
		mt_set_gpio_pull_enable(GPIO_COMBO_BGF_EINT_PIN, GPIO_PULL_ENABLE);
		mt_set_gpio_pull_select(GPIO_COMBO_BGF_EINT_PIN, GPIO_PULL_UP);
		mt_set_gpio_mode(GPIO_COMBO_BGF_EINT_PIN, GPIO_COMBO_BGF_EINT_PIN_M_EINT);
		WMT_DBG_FUNC("WMT-PLAT:BGFInt mux (eint)\n");
		break;

	case PIN_STA_IN_L:
	case PIN_STA_DEINIT:
		/*set to gpio input low, pull down enable */
		mt_set_gpio_mode(GPIO_COMBO_BGF_EINT_PIN, GPIO_COMBO_BGF_EINT_PIN_M_GPIO);
		mt_set_gpio_dir(GPIO_COMBO_BGF_EINT_PIN, GPIO_DIR_IN);
		mt_set_gpio_pull_select(GPIO_COMBO_BGF_EINT_PIN, GPIO_PULL_DOWN);
		mt_set_gpio_pull_enable(GPIO_COMBO_BGF_EINT_PIN, GPIO_PULL_ENABLE);
		WMT_DBG_FUNC("WMT-PLAT:BGFInt deinit(in pd)\n");
		break;

	default:
		WMT_WARN_FUNC("WMT-PLAT:Warnning, invalid state(%d) on BGF EINT\n", state);
		break;
	}
#else
	WMT_INFO_FUNC("WMT-PLAT:BGF EINT not defined\n");
#endif
	return 0;
}


INT32 wmt_plat_wifi_eint_ctrl(ENUM_PIN_STATE state)
{
#if 0				/*def GPIO_WIFI_EINT_PIN */
	switch (state) {
	case PIN_STA_INIT:
		mt_set_gpio_pull_enable(GPIO_WIFI_EINT_PIN, GPIO_PULL_DISABLE);
		mt_set_gpio_dir(GPIO_WIFI_EINT_PIN, GPIO_DIR_OUT);
		mt_set_gpio_mode(GPIO_WIFI_EINT_PIN, GPIO_MODE_GPIO);
		mt_set_gpio_out(GPIO_WIFI_EINT_PIN, GPIO_OUT_ONE);
		break;
	case PIN_STA_MUX:
		mt_set_gpio_mode(GPIO_WIFI_EINT_PIN, GPIO_WIFI_EINT_PIN_M_GPIO);
		mt_set_gpio_pull_enable(GPIO_WIFI_EINT_PIN, GPIO_PULL_ENABLE);
		mt_set_gpio_pull_select(GPIO_WIFI_EINT_PIN, GPIO_PULL_UP);
		mt_set_gpio_mode(GPIO_WIFI_EINT_PIN, GPIO_WIFI_EINT_PIN_M_EINT);

		break;
	case PIN_STA_EINT_EN:
		mt_eint_unmask(CUST_EINT_WIFI_NUM);
		break;
	case PIN_STA_EINT_DIS:
		mt_eint_mask(CUST_EINT_WIFI_NUM);
		break;
	case PIN_STA_IN_L:
	case PIN_STA_DEINIT:
		/*set to gpio input low, pull down enable */
		mt_set_gpio_mode(GPIO_WIFI_EINT_PIN, GPIO_COMBO_BGF_EINT_PIN_M_GPIO);
		mt_set_gpio_dir(GPIO_WIFI_EINT_PIN, GPIO_DIR_IN);
		mt_set_gpio_pull_select(GPIO_WIFI_EINT_PIN, GPIO_PULL_DOWN);
		mt_set_gpio_pull_enable(GPIO_WIFI_EINT_PIN, GPIO_PULL_ENABLE);
		break;
	default:
		WMT_WARN_FUNC("WMT-PLAT:Warnning, invalid state(%d) on WIFI EINT\n", state);
		break;
	}
#else
	WMT_INFO_FUNC("WMT-PLAT:WIFI EINT is controlled by MSDC driver\n");
#endif
	return 0;
}


INT32 wmt_plat_all_eint_ctrl(ENUM_PIN_STATE state)
{
#ifdef GPIO_COMBO_ALL_EINT_PIN
	switch (state) {
	case PIN_STA_INIT:
		mt_set_gpio_mode(GPIO_COMBO_ALL_EINT_PIN, GPIO_COMBO_ALL_EINT_PIN_M_GPIO);
		mt_set_gpio_dir(GPIO_COMBO_ALL_EINT_PIN, GPIO_DIR_IN);
		mt_set_gpio_pull_select(GPIO_COMBO_ALL_EINT_PIN, GPIO_PULL_DOWN);
		mt_set_gpio_pull_enable(GPIO_COMBO_ALL_EINT_PIN, GPIO_PULL_ENABLE);
		WMT_DBG_FUNC("WMT-PLAT:ALLInt init(in pd)\n");
		break;

	case PIN_STA_MUX:
		mt_set_gpio_mode(GPIO_COMBO_ALL_EINT_PIN, GPIO_COMBO_ALL_EINT_PIN_M_GPIO);
		mt_set_gpio_pull_enable(GPIO_COMBO_ALL_EINT_PIN, GPIO_PULL_ENABLE);
		mt_set_gpio_pull_select(GPIO_COMBO_ALL_EINT_PIN, GPIO_PULL_UP);
		mt_set_gpio_mode(GPIO_COMBO_ALL_EINT_PIN, GPIO_COMBO_ALL_EINT_PIN_M_EINT);
		break;

	case PIN_STA_IN_L:
	case PIN_STA_DEINIT:
		/*set to gpio input low, pull down enable */
		mt_set_gpio_mode(GPIO_COMBO_ALL_EINT_PIN, GPIO_COMBO_ALL_EINT_PIN_M_GPIO);
		mt_set_gpio_dir(GPIO_COMBO_ALL_EINT_PIN, GPIO_DIR_IN);
		mt_set_gpio_pull_select(GPIO_COMBO_ALL_EINT_PIN, GPIO_PULL_DOWN);
		mt_set_gpio_pull_enable(GPIO_COMBO_ALL_EINT_PIN, GPIO_PULL_ENABLE);
		break;

	default:
		WMT_WARN_FUNC("WMT-PLAT:Warnning, invalid state(%d) on ALL EINT\n", state);
		break;
	}
#else
	WMT_INFO_FUNC("WMT-PLAT:ALL EINT not defined\n");
#endif
	return 0;
}

INT32 wmt_plat_uart_ctrl(ENUM_PIN_STATE state)
{
	switch (state) {
	case PIN_STA_MUX:
	case PIN_STA_INIT:
		mt_set_gpio_mode(GPIO_COMBO_URXD_PIN, GPIO_COMBO_URXD_PIN_M_URXD);
		mt_set_gpio_mode(GPIO_COMBO_UTXD_PIN, GPIO_COMBO_UTXD_PIN_M_UTXD);
		WMT_DBG_FUNC("WMT-PLAT:UART init (mode_01, uart)\n");
		break;
	case PIN_STA_IN_L:
	case PIN_STA_DEINIT:
		mt_set_gpio_mode(GPIO_COMBO_URXD_PIN, GPIO_COMBO_URXD_PIN_M_GPIO);
		mt_set_gpio_dir(GPIO_COMBO_URXD_PIN, GPIO_DIR_OUT);
		mt_set_gpio_out(GPIO_COMBO_URXD_PIN, GPIO_OUT_ZERO);

		mt_set_gpio_mode(GPIO_COMBO_UTXD_PIN, GPIO_COMBO_UTXD_PIN_M_GPIO);
		mt_set_gpio_dir(GPIO_COMBO_UTXD_PIN, GPIO_DIR_OUT);
		mt_set_gpio_out(GPIO_COMBO_UTXD_PIN, GPIO_OUT_ZERO);
		WMT_DBG_FUNC("WMT-PLAT:UART deinit (out 0)\n");
		break;
	case PIN_STA_IN_PU:
		mt_set_gpio_mode(GPIO_COMBO_URXD_PIN, GPIO_COMBO_URXD_PIN_M_GPIO);
		mt_set_gpio_dir(GPIO_COMBO_URXD_PIN, GPIO_DIR_IN);
		mt_set_gpio_pull_enable(GPIO_COMBO_URXD_PIN, GPIO_PULL_ENABLE);
		mt_set_gpio_pull_select(GPIO_COMBO_URXD_PIN, GPIO_PULL_UP);


	default:
		WMT_WARN_FUNC("WMT-PLAT:Warnning, invalid state(%d) on UART Group\n", state);
		break;
	}

	return 0;
}


INT32 wmt_plat_pcm_ctrl(ENUM_PIN_STATE state)
{
	UINT32 normalPCMFlag = 0;

	/*check if combo chip support merge if or not */
	if (0 != wmt_plat_merge_if_flag_get()) {
#if (MTK_WCN_CMB_MERGE_INTERFACE_SUPPORT)
		/* Hardware support Merge IF function */
		WMT_DBG_FUNC("WMT-PLAT:<Merge IF>set to Merge PCM function\n");
		/*merge PCM function define */
		switch (state) {
		case PIN_STA_MUX:
		case PIN_STA_INIT:
			mt_set_gpio_mode(GPIO_PCM_DAICLK_PIN, GPIO_PCM_DAICLK_PIN_PINMUX_MODE);
			mt_set_gpio_mode(GPIO_PCM_DAIPCMOUT_PIN,
					 GPIO_PCM_DAIPCMOUT_PIN_PINMUX_MODE);
			mt_set_gpio_mode(GPIO_PCM_DAIPCMIN_PIN, GPIO_PCM_DAIPCMIN_PIN_PINMUX_MODE);
			mt_set_gpio_mode(GPIO_PCM_DAISYNC_PIN, GPIO_PCM_DAISYNC_PIN_PINMUX_MODE);
			WMT_DBG_FUNC("WMT-PLAT:<Merge IF>PCM init (pcm)\n");
			break;

		case PIN_STA_IN_L:
		case PIN_STA_DEINIT:
			mt_set_gpio_mode(GPIO_PCM_DAICLK_PIN, GPIO_PCM_DAICLK_PIN_PINMUX_MODE);	/* GPIO_PCM_DAICLK_PIN_M_GPIO); */
			/* mt_set_gpio_dir(GPIO_PCM_DAICLK_PIN, GPIO_DIR_OUT); */
			/* mt_set_gpio_out(GPIO_PCM_DAICLK_PIN, GPIO_OUT_ZERO); */

			mt_set_gpio_mode(GPIO_PCM_DAIPCMOUT_PIN, GPIO_PCM_DAIPCMOUT_PIN_PINMUX_MODE);	/* GPIO_PCM_DAIPCMOUT_PIN_M_GPIO); */
			/* mt_set_gpio_dir(GPIO_PCM_DAIPCMOUT_PIN, GPIO_DIR_OUT); */
			/* mt_set_gpio_out(GPIO_PCM_DAIPCMOUT_PIN, GPIO_OUT_ZERO); */

			mt_set_gpio_mode(GPIO_PCM_DAIPCMIN_PIN, GPIO_PCM_DAIPCMIN_PIN_PINMUX_MODE);	/* GPIO_PCM_DAIPCMIN_PIN_M_GPIO); */
			/* mt_set_gpio_dir(GPIO_PCM_DAIPCMIN_PIN, GPIO_DIR_OUT); */
			/* mt_set_gpio_out(GPIO_PCM_DAIPCMIN_PIN, GPIO_OUT_ZERO); */

			mt_set_gpio_mode(GPIO_PCM_DAISYNC_PIN, GPIO_PCM_DAISYNC_PIN_PINMUX_MODE);	/* GPIO_PCM_DAISYNC_PIN_M_GPIO); */
			/* mt_set_gpio_dir(GPIO_PCM_DAISYNC_PIN, GPIO_DIR_OUT); */
			/* mt_set_gpio_out(GPIO_PCM_DAISYNC_PIN, GPIO_OUT_ZERO); */
			WMT_DBG_FUNC("WMT-PLAT:<Merge IF>PCM deinit (out 0)\n");
			break;
#ifdef CONFIG_MTK_COMBO_COMM_NPWR
		case PIN_STA_OUT_L:
			mt_set_gpio_mode(GPIO_PCM_DAICLK_PIN, GPIO_PCM_DAICLK_PIN_M_GPIO);
			mt_set_gpio_dir(GPIO_PCM_DAICLK_PIN, GPIO_DIR_OUT);
			mt_set_gpio_out(GPIO_PCM_DAICLK_PIN, GPIO_OUT_ZERO);

			mt_set_gpio_mode(GPIO_PCM_DAIPCMOUT_PIN, GPIO_PCM_DAIPCMOUT_PIN_M_GPIO);
			mt_set_gpio_dir(GPIO_PCM_DAIPCMOUT_PIN, GPIO_DIR_OUT);
			mt_set_gpio_out(GPIO_PCM_DAIPCMOUT_PIN, GPIO_OUT_ZERO);

			mt_set_gpio_mode(GPIO_PCM_DAIPCMIN_PIN, GPIO_PCM_DAIPCMIN_PIN_M_GPIO);
			mt_set_gpio_dir(GPIO_PCM_DAIPCMIN_PIN, GPIO_DIR_OUT);
			mt_set_gpio_out(GPIO_PCM_DAIPCMIN_PIN, GPIO_OUT_ZERO);

			mt_set_gpio_mode(GPIO_PCM_DAISYNC_PIN, GPIO_PCM_DAISYNC_PIN_M_GPIO);
			mt_set_gpio_dir(GPIO_PCM_DAISYNC_PIN, GPIO_DIR_OUT);
			mt_set_gpio_out(GPIO_PCM_DAISYNC_PIN, GPIO_OUT_ZERO);
			break;
#endif
		default:
			WMT_WARN_FUNC
			    ("WMT-PLAT:<Merge IF>Warnning, invalid state(%d) on PCM Group\n",
			     state);
			break;
		}

#else
		/* Hardware does not support Merge IF function */
		normalPCMFlag = 1;
		WMT_DBG_FUNC("WMT-PLAT:set to normal PCM function\n");
#endif

	} else {
		normalPCMFlag = 1;
	}

	if (0 != normalPCMFlag) {
		/*normal PCM function define */
		switch (state) {
		case PIN_STA_MUX:
		case PIN_STA_INIT:
			mt_set_gpio_mode(GPIO_PCM_DAICLK_PIN, GPIO_PCM_DAICLK_PIN_PCMONLY_MODE);
			mt_set_gpio_mode(GPIO_PCM_DAIPCMOUT_PIN,
					 GPIO_PCM_DAIPCMOUT_PIN_PCMONLY_MODE);
			mt_set_gpio_mode(GPIO_PCM_DAIPCMIN_PIN, GPIO_PCM_DAIPCMIN_PIN_PCMONLY_MODE);
			mt_set_gpio_mode(GPIO_PCM_DAISYNC_PIN, GPIO_PCM_DAISYNC_PIN_PCMONLY_MODE);
			WMT_DBG_FUNC("WMT-PLAT:MT6589 PCM init (pcm)\n");
			break;

		case PIN_STA_IN_L:
		case PIN_STA_DEINIT:
			mt_set_gpio_mode(GPIO_PCM_DAICLK_PIN, GPIO_PCM_DAICLK_PIN_PCMONLY_MODE);	/* GPIO_PCM_DAICLK_PIN_M_GPIO); */
			/* mt_set_gpio_dir(GPIO_PCM_DAICLK_PIN, GPIO_DIR_OUT); */
			/* mt_set_gpio_out(GPIO_PCM_DAICLK_PIN, GPIO_OUT_ZERO); */

			mt_set_gpio_mode(GPIO_PCM_DAIPCMOUT_PIN, GPIO_PCM_DAIPCMOUT_PIN_PCMONLY_MODE);	/* GPIO_PCM_DAIPCMOUT_PIN_M_GPIO); */
			/* mt_set_gpio_dir(GPIO_PCM_DAIPCMOUT_PIN, GPIO_DIR_OUT); */
			/* mt_set_gpio_out(GPIO_PCM_DAIPCMOUT_PIN, GPIO_OUT_ZERO); */

			mt_set_gpio_mode(GPIO_PCM_DAIPCMIN_PIN, GPIO_PCM_DAIPCMIN_PIN_PCMONLY_MODE);	/* GPIO_PCM_DAIPCMIN_PIN_M_GPIO); */
			/* mt_set_gpio_dir(GPIO_PCM_DAIPCMIN_PIN, GPIO_DIR_OUT); */
			/* mt_set_gpio_out(GPIO_PCM_DAIPCMIN_PIN, GPIO_OUT_ZERO); */

			mt_set_gpio_mode(GPIO_PCM_DAISYNC_PIN, GPIO_PCM_DAISYNC_PIN_PCMONLY_MODE);	/* GPIO_PCM_DAISYNC_PIN_M_GPIO); */
			/* mt_set_gpio_dir(GPIO_PCM_DAISYNC_PIN, GPIO_DIR_OUT); */
			/* mt_set_gpio_out(GPIO_PCM_DAISYNC_PIN, GPIO_OUT_ZERO); */
			WMT_DBG_FUNC("WMT-PLAT:MT6589 PCM deinit (out 0)\n");
			break;

		default:
			WMT_WARN_FUNC("WMT-PLAT:MT6589 Warnning, invalid state(%d) on PCM Group\n",
				      state);
			break;
		}
	}

	return 0;
}


INT32 wmt_plat_i2s_ctrl(ENUM_PIN_STATE state)
{
	/* TODO: [NewFeature][GeorgeKuo]: GPIO_I2Sx is changed according to different project. */
	/* TODO: provide a translation table in board_custom.h for different ALPS project customization. */

	UINT32 normalI2SFlag = 0;

	/*check if combo chip support merge if or not */
	if (0 != wmt_plat_merge_if_flag_get()) {
#if (MTK_WCN_CMB_MERGE_INTERFACE_SUPPORT)
		/* Hardware support Merge IF function */
#if defined(FM_DIGITAL_INPUT) || defined(FM_DIGITAL_OUTPUT)
#if defined(GPIO_COMBO_I2S_CK_PIN)
		switch (state) {
		case PIN_STA_INIT:
		case PIN_STA_MUX:
			mt_set_gpio_mode(GPIO_COMBO_I2S_CK_PIN, GPIO_COMBO_I2S_CK_PIN_PINMUX_MODE);
			mt_set_gpio_mode(GPIO_COMBO_I2S_WS_PIN, GPIO_COMBO_I2S_WS_PIN_PINMUX_MODE);
			mt_set_gpio_mode(GPIO_COMBO_I2S_DAT_PIN,
					 GPIO_COMBO_I2S_DAT_PIN_PINMUX_MODE);
			mt_set_gpio_mode(GPIO_PCM_DAIPCMOUT_PIN,
					 GPIO_PCM_DAIPCMOUT_PIN_PINMUX_MODE);
			WMT_DBG_FUNC("WMT-PLAT:<Merge IF>I2S init (I2S0 system)\n");
			break;
		case PIN_STA_IN_L:
		case PIN_STA_DEINIT:
			mt_set_gpio_mode(GPIO_COMBO_I2S_CK_PIN, GPIO_COMBO_I2S_CK_PIN_M_GPIO);
			mt_set_gpio_dir(GPIO_COMBO_I2S_CK_PIN, GPIO_DIR_OUT);
			mt_set_gpio_out(GPIO_COMBO_I2S_CK_PIN, GPIO_OUT_ZERO);

			mt_set_gpio_mode(GPIO_COMBO_I2S_WS_PIN, GPIO_COMBO_I2S_WS_PIN_M_GPIO);
			mt_set_gpio_dir(GPIO_COMBO_I2S_WS_PIN, GPIO_DIR_OUT);
			mt_set_gpio_out(GPIO_COMBO_I2S_WS_PIN, GPIO_OUT_ZERO);

			mt_set_gpio_mode(GPIO_COMBO_I2S_DAT_PIN, GPIO_COMBO_I2S_DAT_PIN_M_GPIO);
			mt_set_gpio_dir(GPIO_COMBO_I2S_DAT_PIN, GPIO_DIR_OUT);
			mt_set_gpio_out(GPIO_COMBO_I2S_DAT_PIN, GPIO_OUT_ZERO);
			WMT_DBG_FUNC("WMT-PLAT:<Merge IF>I2S deinit (out 0)\n");
			break;
		default:
			WMT_WARN_FUNC
			    ("WMT-PLAT:<Merge IF>Warnning, invalid state(%d) on I2S Group\n",
			     state);
			break;
		}
#else
		WMT_ERR_FUNC
		    ("[MT662x]<Merge IF>Error:FM digital mode set, but no I2S GPIOs defined\n");
#endif
#else
		WMT_INFO_FUNC
		    ("[MT662x]<Merge IF>warnning:FM digital mode is not set, no I2S GPIO settings should be modified by combo driver\n");
#endif
#else
		/* Hardware does support Merge IF function */
		normalI2SFlag = 1;
#endif
	} else {
		normalI2SFlag = 1;
	}
	if (0 != normalI2SFlag) {
#if defined(FM_DIGITAL_INPUT) || defined(FM_DIGITAL_OUTPUT)
#if defined(GPIO_COMBO_I2S_CK_PIN)
		switch (state) {
		case PIN_STA_INIT:
		case PIN_STA_MUX:
			mt_set_gpio_mode(GPIO_COMBO_I2S_CK_PIN, GPIO_COMBO_I2S_CK_PIN_I2SONLY_MODE);
			mt_set_gpio_mode(GPIO_COMBO_I2S_WS_PIN, GPIO_COMBO_I2S_WS_PIN_I2SONLY_MODE);
			mt_set_gpio_mode(GPIO_COMBO_I2S_DAT_PIN,
					 GPIO_COMBO_I2S_DAT_PIN_I2SONLY_MODE);
			WMT_DBG_FUNC("WMT-PLAT:<I2S IF>I2S init (I2S0 system)\n");
			break;
		case PIN_STA_IN_L:
		case PIN_STA_DEINIT:
			mt_set_gpio_mode(GPIO_COMBO_I2S_CK_PIN, GPIO_COMBO_I2S_CK_PIN_M_GPIO);
			mt_set_gpio_dir(GPIO_COMBO_I2S_CK_PIN, GPIO_DIR_OUT);
			mt_set_gpio_out(GPIO_COMBO_I2S_CK_PIN, GPIO_OUT_ZERO);

			mt_set_gpio_mode(GPIO_COMBO_I2S_WS_PIN, GPIO_COMBO_I2S_WS_PIN_M_GPIO);
			mt_set_gpio_dir(GPIO_COMBO_I2S_WS_PIN, GPIO_DIR_OUT);
			mt_set_gpio_out(GPIO_COMBO_I2S_WS_PIN, GPIO_OUT_ZERO);

			mt_set_gpio_mode(GPIO_COMBO_I2S_DAT_PIN, GPIO_COMBO_I2S_DAT_PIN_M_GPIO);
			mt_set_gpio_dir(GPIO_COMBO_I2S_DAT_PIN, GPIO_DIR_OUT);
			mt_set_gpio_out(GPIO_COMBO_I2S_DAT_PIN, GPIO_OUT_ZERO);
			WMT_DBG_FUNC("WMT-PLAT:<I2S IF>I2S deinit (out 0)\n");
			break;
		default:
			WMT_WARN_FUNC("WMT-PLAT:<I2S IF>Warnning, invalid state(%d) on I2S Group\n",
				      state);
			break;
		}
#else
		WMT_ERR_FUNC
		    ("[MT662x]<I2S IF>Error:FM digital mode set, but no I2S GPIOs defined\n");
#endif
#else
		WMT_INFO_FUNC
		    ("[MT662x]<I2S IF>warnning:FM digital mode is not set, no I2S GPIO settings should be modified by combo driver\n");
#endif
	}

	return 0;
}

INT32 wmt_plat_sdio_pin_ctrl(ENUM_PIN_STATE state)
{
#if 0
	switch (state) {
	case PIN_STA_INIT:
	case PIN_STA_MUX:
#if defined(CONFIG_MTK_WCN_CMB_SDIO_SLOT)
#if (CONFIG_MTK_WCN_CMB_SDIO_SLOT == 1)
		/* TODO: [FixMe][GeorgeKuo]: below are used for MT6573 only! Find a better way to do ALPS customization for different platform. */
		/* WMT_INFO_FUNC( "[mt662x] pull up sd1 bus(gpio62~68)\n"); */
		mt_set_gpio_pull_enable(GPIO62, GPIO_PULL_ENABLE);
		mt_set_gpio_pull_select(GPIO62, GPIO_PULL_UP);
		mt_set_gpio_pull_enable(GPIO63, GPIO_PULL_ENABLE);
		mt_set_gpio_pull_select(GPIO63, GPIO_PULL_UP);
		mt_set_gpio_pull_enable(GPIO64, GPIO_PULL_ENABLE);
		mt_set_gpio_pull_select(GPIO64, GPIO_PULL_UP);
		mt_set_gpio_pull_enable(GPIO65, GPIO_PULL_ENABLE);
		mt_set_gpio_pull_select(GPIO65, GPIO_PULL_UP);
		mt_set_gpio_pull_enable(GPIO66, GPIO_PULL_ENABLE);
		mt_set_gpio_pull_select(GPIO66, GPIO_PULL_UP);
		mt_set_gpio_pull_enable(GPIO67, GPIO_PULL_ENABLE);
		mt_set_gpio_pull_select(GPIO67, GPIO_PULL_UP);
		mt_set_gpio_pull_enable(GPIO68, GPIO_PULL_ENABLE);
		mt_set_gpio_pull_select(GPIO68, GPIO_PULL_UP);
		WMT_DBG_FUNC("WMT-PLAT:SDIO init (pu)\n");
#elif (CONFIG_MTK_WCN_CMB_SDIO_SLOT == 2)
#error "fix sdio2 gpio settings"
#endif
#else
#error "CONFIG_MTK_WCN_CMB_SDIO_SLOT undefined!!!"
#endif

		break;

	case PIN_STA_DEINIT:
#if defined(CONFIG_MTK_WCN_CMB_SDIO_SLOT)
#if (CONFIG_MTK_WCN_CMB_SDIO_SLOT == 1)
		/* TODO: [FixMe][GeorgeKuo]: below are used for MT6573 only! Find a better way to do ALPS customization for different platform. */
		/* WMT_INFO_FUNC( "[mt662x] pull down sd1 bus(gpio62~68)\n"); */
		mt_set_gpio_pull_select(GPIO62, GPIO_PULL_DOWN);
		mt_set_gpio_pull_enable(GPIO62, GPIO_PULL_ENABLE);
		mt_set_gpio_pull_select(GPIO63, GPIO_PULL_DOWN);
		mt_set_gpio_pull_enable(GPIO63, GPIO_PULL_ENABLE);
		mt_set_gpio_pull_select(GPIO64, GPIO_PULL_DOWN);
		mt_set_gpio_pull_enable(GPIO64, GPIO_PULL_ENABLE);
		mt_set_gpio_pull_select(GPIO65, GPIO_PULL_DOWN);
		mt_set_gpio_pull_enable(GPIO65, GPIO_PULL_ENABLE);
		mt_set_gpio_pull_select(GPIO66, GPIO_PULL_DOWN);
		mt_set_gpio_pull_enable(GPIO66, GPIO_PULL_ENABLE);
		mt_set_gpio_pull_select(GPIO67, GPIO_PULL_DOWN);
		mt_set_gpio_pull_enable(GPIO67, GPIO_PULL_ENABLE);
		mt_set_gpio_pull_select(GPIO68, GPIO_PULL_DOWN);
		mt_set_gpio_pull_enable(GPIO68, GPIO_PULL_ENABLE);
		WMT_DBG_FUNC("WMT-PLAT:SDIO deinit (pd)\n");
#elif (CONFIG_MTK_WCN_CMB_SDIO_SLOT == 2)
#error "fix sdio2 gpio settings"
#endif
#else
#error "CONFIG_MTK_WCN_CMB_SDIO_SLOT undefined!!!"
#endif
		break;

	default:
		WMT_WARN_FUNC("WMT-PLAT:Warnning, invalid state(%d) on SDIO Group\n", state);
		break;
	}
#endif
	return 0;
}

static INT32 wmt_plat_gps_sync_ctrl(ENUM_PIN_STATE state)
{
#ifdef GPIO_GPS_SYNC_PIN
#ifndef GPIO_GPS_SYNC_PIN_M_GPS_SYNC
#ifdef GPIO_GPS_SYNC_PIN_M_MD1_GPS_SYNC
#define GPIO_GPS_SYNC_PIN_M_GPS_SYNC GPIO_GPS_SYNC_PIN_M_MD1_GPS_SYNC
#elif defined(GPIO_GPS_SYNC_PIN_M_MD2_GPS_SYNC)
#ifdef GPIO_GPS_SYNC_PIN_M_MD2_GPS_SYNC
#define GPIO_GPS_SYNC_PIN_M_GPS_SYNC GPIO_GPS_SYNC_PIN_M_MD2_GPS_SYNC
#endif
#elif defined(GPIO_GPS_SYNC_PIN_M_GPS_FRAME_SYNC)
#ifdef GPIO_GPS_SYNC_PIN_M_GPS_FRAME_SYNC
#define GPIO_GPS_SYNC_PIN_M_GPS_SYNC GPIO_GPS_SYNC_PIN_M_GPS_FRAME_SYNC
#endif
#endif
#endif
	switch (state) {
	case PIN_STA_INIT:
	case PIN_STA_DEINIT:
		mt_set_gpio_mode(GPIO_GPS_SYNC_PIN, GPIO_GPS_SYNC_PIN_M_GPIO);
		mt_set_gpio_dir(GPIO_GPS_SYNC_PIN, GPIO_DIR_OUT);
		mt_set_gpio_out(GPIO_GPS_SYNC_PIN, GPIO_OUT_ZERO);
		break;

	case PIN_STA_MUX:
		mt_set_gpio_mode(GPIO_GPS_SYNC_PIN, GPIO_GPS_SYNC_PIN_M_GPS_SYNC);
		break;

	default:
		break;
	}
#endif
	return 0;
}


static INT32 wmt_plat_gps_lna_ctrl(ENUM_PIN_STATE state)
{
#ifdef GPIO_GPS_LNA_PIN
	switch (state) {
	case PIN_STA_INIT:
	case PIN_STA_DEINIT:
		mt_set_gpio_pull_enable(GPIO_GPS_LNA_PIN, GPIO_PULL_DISABLE);
		mt_set_gpio_dir(GPIO_GPS_LNA_PIN, GPIO_DIR_OUT);
		mt_set_gpio_mode(GPIO_GPS_LNA_PIN, GPIO_GPS_LNA_PIN_M_GPIO);
		mt_set_gpio_out(GPIO_GPS_LNA_PIN, GPIO_OUT_ZERO);
		break;
	case PIN_STA_OUT_H:
		mt_set_gpio_out(GPIO_GPS_LNA_PIN, GPIO_OUT_ONE);
		break;
	case PIN_STA_OUT_L:
		mt_set_gpio_out(GPIO_GPS_LNA_PIN, GPIO_OUT_ZERO);
		break;

	default:
		WMT_WARN_FUNC("%d mode not defined for  gps lna pin !!!\n", state);
		break;
	}
	return 0;
#else
#ifdef CONFIG_MTK_MT6306_SUPPORT
	WMT_WARN_FUNC("/******************************************************************/\n");
    	WMT_WARN_FUNC("use MT6306 GPIO7 for  gps lna pin.\n this HARD CODE may hurt other system module, if GPIO7 of MT6306 is not defined as GPS_LNA function\n");
	WMT_WARN_FUNC("/******************************************************************/\n");

    	switch (state) {
    	case PIN_STA_INIT:
    	case PIN_STA_DEINIT:
		mt6306_set_gpio_dir (GPIO7, GPIO_DIR_OUT);
		mt6306_set_gpio_out (GPIO7, GPIO_OUT_ZERO);
        	break;
    	case PIN_STA_OUT_H:
		mt6306_set_gpio_out (GPIO7, GPIO_OUT_ONE);
        	break;
    	case PIN_STA_OUT_L:
		mt6306_set_gpio_out (GPIO7, GPIO_OUT_ZERO);
        	break;

    	default:
        	WMT_WARN_FUNC("%d mode not defined for  gps lna pin !!!\n", state);
        	break;
    }


#else
    	WMT_WARN_FUNC("host gps lna pin not defined!!!\n");
	WMT_WARN_FUNC("if you donot use eighter AP or MT6306's pin as GPS_LNA, please customize your own GPS_LNA related code here\n");

#endif
	    return 0;
#endif
}


static INT32 wmt_plat_uart_rx_ctrl(ENUM_PIN_STATE state)
{
#ifdef GPIO_COMBO_URXD_PIN
	switch (state) {
	case PIN_STA_MUX:
	case PIN_STA_INIT:
		mt_set_gpio_mode(GPIO_COMBO_URXD_PIN, GPIO_COMBO_URXD_PIN_M_URXD);
		WMT_DBG_FUNC("WMT-PLAT:UART Rx init\n");
		break;
	case PIN_STA_IN_L:
	case PIN_STA_DEINIT:
		mt_set_gpio_mode(GPIO_COMBO_URXD_PIN, GPIO_COMBO_URXD_PIN_M_GPIO);
		mt_set_gpio_dir(GPIO_COMBO_URXD_PIN, GPIO_DIR_OUT);
		mt_set_gpio_out(GPIO_COMBO_URXD_PIN, GPIO_OUT_ZERO);

		WMT_DBG_FUNC("WMT-PLAT:UART Rx deinit (out 0)\n");
		break;
	case PIN_STA_IN_NP:
		mt_set_gpio_mode(GPIO_COMBO_URXD_PIN, GPIO_COMBO_URXD_PIN_M_GPIO);
		mt_set_gpio_dir(GPIO_COMBO_URXD_PIN, GPIO_DIR_IN);
		mt_set_gpio_pull_enable(GPIO_COMBO_URXD_PIN, GPIO_PULL_DISABLE);
		WMT_DBG_FUNC("WMT-PLAT:UART Rx input pull none\n");
		break;
	case PIN_STA_OUT_H:
		mt_set_gpio_mode(GPIO_COMBO_URXD_PIN, GPIO_COMBO_URXD_PIN_M_GPIO);
		mt_set_gpio_dir(GPIO_COMBO_URXD_PIN, GPIO_DIR_OUT);
		mt_set_gpio_out(GPIO_COMBO_URXD_PIN, GPIO_OUT_ONE);
		WMT_DBG_FUNC("WMT-PLAT:UART Rx output high\n");
		break;
	default:
		WMT_WARN_FUNC("WMT-PLAT:Warnning, invalid state(%d) on UART Rx\n", state);
		break;
	}
#else
		WMT_WARN_FUNC("WMT-PLAT:Warnning, GPIO_COMBO_URXD_PIN is not define\n");
#endif

	return 0;
}

#ifdef CONFIG_MTK_COMBO_COMM_NPWR
static INT32 wmt_plat_i2s_dat_ctrl (ENUM_PIN_STATE state)
{

#ifdef GPIO_COMBO_I2S_DAT_PIN
    switch(state)
    {

		case PIN_STA_OUT_H:
			mt_set_gpio_mode(GPIO_COMBO_I2S_DAT_PIN, GPIO_COMBO_I2S_DAT_PIN_M_GPIO);
			mt_set_gpio_dir(GPIO_COMBO_I2S_DAT_PIN, GPIO_DIR_OUT);
			mt_set_gpio_out(GPIO_COMBO_I2S_DAT_PIN, GPIO_OUT_ONE);
			WMT_DBG_FUNC("WMT-PLAT:I2S DAT output high\n", state);
			break;
		case PIN_STA_OUT_L:
			mt_set_gpio_mode(GPIO_COMBO_I2S_DAT_PIN, GPIO_COMBO_I2S_DAT_PIN_M_GPIO);
			mt_set_gpio_dir(GPIO_COMBO_I2S_DAT_PIN, GPIO_DIR_OUT);
			mt_set_gpio_out(GPIO_COMBO_I2S_DAT_PIN, GPIO_OUT_ZERO);
			WMT_DBG_FUNC("WMT-PLAT:I2S DAT output low\n", state);
			break;

    default:
        WMT_WARN_FUNC("WMT-PLAT:Warnning, invalid state(%d) on I2S DAT\n", state);
        break;
    }
#else
	WMT_WARN_FUNC("WMT-PLAT:Warnning, no I2S_DAT defination\n");
#endif
    return 0;
}

static INT32 wmt_plat_pcm_sync_ctrl (ENUM_PIN_STATE state)
{
#ifdef GPIO_PCM_DAISYNC_PIN
		switch(state)
		{
	
			case PIN_STA_OUT_H:
				mt_set_gpio_mode(GPIO_PCM_DAISYNC_PIN, GPIO_PCM_DAISYNC_PIN_M_GPIO);
				mt_set_gpio_dir(GPIO_PCM_DAISYNC_PIN, GPIO_DIR_OUT);
				mt_set_gpio_out(GPIO_PCM_DAISYNC_PIN, GPIO_OUT_ONE);
				WMT_DBG_FUNC("WMT-PLAT:PCM SYNC output high\n");
				break;
			case PIN_STA_OUT_L:
				mt_set_gpio_mode(GPIO_PCM_DAISYNC_PIN, GPIO_PCM_DAISYNC_PIN_M_GPIO);
				mt_set_gpio_dir(GPIO_PCM_DAISYNC_PIN, GPIO_DIR_OUT);
				mt_set_gpio_out(GPIO_PCM_DAISYNC_PIN, GPIO_OUT_ZERO);
				WMT_DBG_FUNC("WMT-PLAT:PCM SYNC output low\n");
				break;
	
		default:
			WMT_WARN_FUNC("WMT-PLAT:Warnning, invalid state(%d) on PCM SYNC\n", state);
			break;
		}
#else
		WMT_WARN_FUNC("WMT-PLAT:Warnning, no PCM SYNC defination\n");
#endif
	return 0;

}
#endif

INT32 wmt_plat_wake_lock_ctrl(ENUM_WL_OP opId)
{
#ifdef CFG_WMT_WAKELOCK_SUPPORT
	static INT32 counter;
	INT32 ret = 0;


	ret = osal_lock_sleepable_lock(&gOsSLock);
	if (ret) {
		WMT_ERR_FUNC("--->lock gOsSLock failed, ret=%d\n", ret);
		return ret;
	}

	if (WL_OP_GET == opId) {
		++counter;
	} else if (WL_OP_PUT == opId) {
		--counter;
	}
	osal_unlock_sleepable_lock(&gOsSLock);
	if (WL_OP_GET == opId && counter == 1) {
		wake_lock(&wmtWakeLock);
		WMT_DBG_FUNC("WMT-PLAT: after wake_lock(%d), counter(%d)\n",
			     wake_lock_active(&wmtWakeLock), counter);

	} else if (WL_OP_PUT == opId && counter == 0) {
		wake_unlock(&wmtWakeLock);
		WMT_DBG_FUNC("WMT-PLAT: after wake_unlock(%d), counter(%d)\n",
			     wake_lock_active(&wmtWakeLock), counter);
	} else {
		WMT_WARN_FUNC("WMT-PLAT: wakelock status(%d), counter(%d)\n",
			      wake_lock_active(&wmtWakeLock), counter);
	}
	return 0;
#else
	WMT_WARN_FUNC("WMT-PLAT: host awake function is not supported.");
	return 0;

#endif
}


INT32 wmt_plat_merge_if_flag_ctrl(UINT32 enable)
{
	if (enable) {
#if (MTK_WCN_CMB_MERGE_INTERFACE_SUPPORT)
		gWmtMergeIfSupport = 1;
#else
		gWmtMergeIfSupport = 0;
		WMT_WARN_FUNC
		    ("neither MT6589, MTK_MERGE_INTERFACE_SUPPORT nor MT6628 is not set to true, so set gWmtMergeIfSupport to %d\n",
		     gWmtMergeIfSupport);
#endif
	} else {
		gWmtMergeIfSupport = 0;
	}

	WMT_INFO_FUNC("set gWmtMergeIfSupport to %d\n", gWmtMergeIfSupport);
	return gWmtMergeIfSupport;
}

INT32 wmt_plat_merge_if_flag_get(VOID)
{
	return gWmtMergeIfSupport;
}

INT32 wmt_plat_set_comm_if_type(ENUM_STP_TX_IF_TYPE type)
{
	gCommIfType = type;
	return 0;
}


ENUM_STP_TX_IF_TYPE wmt_plat_get_comm_if_type(VOID)
{
	return gCommIfType;
}
