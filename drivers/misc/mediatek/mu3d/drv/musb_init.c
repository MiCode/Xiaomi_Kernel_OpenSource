
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/prefetch.h>
#include <linux/usb/nop-usb-xceiv.h>

#include <asm/cacheflush.h>

#include "musb_core.h"
#include "ssusb_qmu.h"
#include <linux/mu3d/hal/mu3d_hal_osal.h>
#include <linux/mu3d/hal/mu3d_hal_phy.h>
#include <linux/mu3d/hal/mu3d_hal_usb_drv.h>

#ifdef CONFIG_PROJECT_PHY
#include <linux/mu3phy/mtk-phy-asic.h>
#endif

#ifdef CONFIG_OF
#include <linux/of_device.h>
#endif

#ifdef CONFIG_USBIF_COMPLIANCE
static struct musb_fifo_cfg mtu3d_cfg[] = {
#else
static struct musb_fifo_cfg __initdata mtu3d_cfg[] = {
#endif
{ .hw_ep_num =  1, .style = FIFO_TX, .maxpacket = 1024, },
{ .hw_ep_num =  1, .style = FIFO_RX, .maxpacket = 1024, },
{ .hw_ep_num =  2, .style = FIFO_TX, .maxpacket = 1024, },
{ .hw_ep_num =  2, .style = FIFO_RX, .maxpacket = 1024, },
{ .hw_ep_num =  3, .style = FIFO_TX, .maxpacket = 1024, },
{ .hw_ep_num =  3, .style = FIFO_RX, .maxpacket = 1024, },
{ .hw_ep_num =  4, .style = FIFO_TX, .maxpacket = 1024, },
{ .hw_ep_num =  4, .style = FIFO_RX, .maxpacket = 1024, },
{ .hw_ep_num =  5, .style = FIFO_TX, .maxpacket = 1024, },
{ .hw_ep_num =  5, .style = FIFO_RX, .maxpacket = 1024, },
{ .hw_ep_num =  6, .style = FIFO_TX, .maxpacket = 1024, },
{ .hw_ep_num =  6, .style = FIFO_RX, .maxpacket = 1024, },
{ .hw_ep_num =  7, .style = FIFO_TX, .maxpacket = 1024, },
{ .hw_ep_num =  7, .style = FIFO_RX, .maxpacket = 1024, },
{ .hw_ep_num =  8, .style = FIFO_TX, .maxpacket = 1024, },
{ .hw_ep_num =  8, .style = FIFO_RX, .maxpacket = 1024, },
};

static struct musb_hdrc_config mtu3d_config = {
	.multipoint     = false,
	/* FIXME:Seems no need*/
	/* .soft_con       = true,*/
	.dma            = true,
	.num_eps        = 9, /*EP0 ~ EP8*/
	.dma_channels   = 8,
	.ram_bits       = 12,
	.fifo_cfg = mtu3d_cfg,
	.fifo_cfg_size = ARRAY_SIZE(mtu3d_cfg),
};

static int mtu3d_musb_init(struct musb *musb);
static int mtu3d_musb_exit(struct musb *musb);
void mtu3d_musb_enable(struct musb *musb);
void mtu3d_musb_disable(struct musb *musb);

static const struct musb_platform_ops mtu3d_ops = {
	.init		= mtu3d_musb_init,
	.exit		= mtu3d_musb_exit,

	.enable   = mtu3d_musb_enable,
	.disable  = mtu3d_musb_disable
};

static struct musb_hdrc_platform_data mtu3d_data = {
	.mode	= MUSB_PERIPHERAL,
	.config	= &mtu3d_config,
	.platform_ops	= &mtu3d_ops,
};

static const struct of_device_id mtu3d_of_match[] = {
	{
		.compatible = "mediatek,USB3",
		.data = &mtu3d_data,
	},
	{ },
};

struct mtu3d_glue {
	struct device		*dev;
	struct platform_device	*musb;
};
#define glue_to_musb(g)		platform_get_drvdata(g->musb)

static void mtu3d_musb_reg_init(struct musb *musb);
static int mtu3d_set_power(struct usb_phy *x, unsigned mA);

static u32 sts_ltssm = 0;

#ifndef CONFIG_USBIF_COMPLIANCE
extern struct timespec get_connect_timestamp(void);
#endif

#ifdef CONFIG_USBIF_COMPLIANCE
void init_check_ltssm_work(void){
	sts_ltssm = 0;
}
#endif

/*
 * This work is to fix IOT problem.
 * 1. PC(Lenovo MT-M5852-B88) + USB Host PCI-e TI(TUSB7340) or Fresco,
 *  --> PC enter SLERP/HIBERNATION --> USER wake up PC --> PC can not detect device.
 * 2. PC(Lenovo MT-M5852-B88) + USB Host PCI-e Fresco,
 *  --> PC COLD reboot --> PC can not detect device.
 * The root cause is that device try to link host too early. XHCI host does not ready to linkup.
 * So create a delayed work to re-tran when device is stuck at RxDetect.
 */
void check_ltssm_work(struct work_struct *data)
{
	//struct musb *musb = container_of(to_delayed_work(data), struct musb, check_ltssm_work);
#ifndef CONFIG_USBIF_COMPLIANCE
	os_printk(K_INFO, "%s %x\n", __func__, sts_ltssm);

	if(sts_ltssm == RXDET_SUCCESS_INTR) {
		sts_ltssm = 0;
		mu3d_hal_u3dev_dis();
		mdelay(10);
		mu3d_hal_u3dev_en();
	}
#endif
}

#ifndef CONFIG_USBIF_COMPLIANCE
/*
 * The workaround for IOT issue.
 * Lenovo PC(MT-M 5852-B88) + Asmedia U3 Host controller
 * The device stays at "SS" when connecting with PC.
 * But after restarting PC, the device would stay at "HS".
 */
void reconnect_work(struct work_struct *data)
{
	os_printk(K_INFO, "%s\n", __func__);

	/* Disable U2 detect*/
	mu3d_hal_u3dev_dis();
	mu3d_hal_u2dev_disconn();
	mdelay(1000);
	mu3d_hal_u3dev_en();
}
#endif

static inline void mtu3d_u3_ltssm_intr_handler(struct musb *musb, u32 dwLtssmValue)
{
	static u32 soft_conn_num = 0;

	if (dwLtssmValue & SS_DISABLE_INTR) {
		os_printk(K_INFO, "LTSSM: SS_DISABLE_INTR [%d] & Set SOFT_CONN=1\n", soft_conn_num++);
		//enable U2 link. after host reset, HS/FS EP0 configuration is applied in musb_g_reset
		os_clrmsk(U3D_SSUSB_U2_CTRL_0P, SSUSB_U2_PORT_PDN);
		os_setmsk(U3D_POWER_MANAGEMENT, SOFT_CONN);
		sts_ltssm = SS_DISABLE_INTR;
	}

	if (dwLtssmValue & ENTER_U0_INTR) {
		soft_conn_num = 0;
		//do not apply U3 EP0 setting again, if the speed is already U3
		//LTSSM may go to recovery and back to U0
		if (musb->g.speed != USB_SPEED_SUPER) {
			os_printk(K_INFO, "LTSSM: ENTER_U0_INTR %d\n", musb->g.speed);
			musb_conifg_ep0(musb);
		}
		cancel_delayed_work(&musb->check_ltssm_work);
		sts_ltssm = ENTER_U0_INTR;
	}

	if (dwLtssmValue & VBUS_FALL_INTR) {
		os_printk(K_INFO, "LTSSM: VBUS_FALL_INTR\n");
		mu3d_hal_pdn_ip_port(1, 1, 1, 1);
		mu3d_hal_u3dev_dis();
	}

	if (dwLtssmValue & VBUS_RISE_INTR) {
		os_printk(K_INFO, "LTSSM: VBUS_RISE_INTR\n");
		mu3d_hal_u3dev_en();
	}

	if (dwLtssmValue & ENTER_U3_INTR) {
		os_printk(K_INFO, "LTSSM: ENTER_U3_INTR\n");
		mu3d_hal_pdn_ip_port(0, 0, 1, 0);
		sts_ltssm = ENTER_U3_INTR;
	}

#ifndef POWER_SAVING_MODE
	if (dwLtssmValue & U3_RESUME_INTR) {
		os_printk(K_INFO, "LTSSM: RESUME_INTR\n");
		mu3d_hal_pdn_ip_port(1, 0, 1, 0);
		os_writel(U3D_LINK_POWER_CONTROL, os_readl(U3D_LINK_POWER_CONTROL) | UX_EXIT);
	}
#endif

	if (dwLtssmValue & EXIT_U3_INTR) {
		os_printk(K_INFO, "LTSSM: EXIT_U3_INTR\n");
		mu3d_hal_pdn_ip_port(1, 0, 1, 0);
		sts_ltssm = EXIT_U3_INTR;
	}

	/*7.5.12.2 Hot Reset Requirements
	* 1. A downstream port shall reset its Link Error Count as defined in Section 7.4.2.
	* 2. A downstream port shall reset its PM timers and the associated U1 and U2 timeout values to zero.
	* 3. The port Configuration information shall remain unchanged (refer to Section 8.4.6 for details).
	* 4. The port shall maintain its transmitter specifications defined in Table 6-10.
	* 5. The port shall maintain its low-impedance receiver termination (RRX-DC) defined in Table 6-13.
	*/
	if (dwLtssmValue & HOT_RST_INTR) {
		DEV_INT32 link_err_cnt;
		DEV_INT32 timeout_val;
		os_printk(K_INFO, "LTSSM: HOT_RST_INTR\n");
		/* Clear link error count */
		link_err_cnt=os_readl(U3D_LINK_ERR_COUNT);
		os_printk(K_INFO, "LTSSM: link_err_cnt=%x\n", link_err_cnt);
		os_writel(U3D_LINK_ERR_COUNT, CLR_LINK_ERR_CNT);

		/* Clear U1 & U2 Enable*/
		os_clrmsk(U3D_LINK_POWER_CONTROL, (SW_U1_ACCEPT_ENABLE|SW_U2_ACCEPT_ENABLE));

		musb->g.pwr_params.bU1Enabled = 0;
		musb->g.pwr_params.bU2Enabled = 0;

		/* Reset U1 & U2 timeout value*/
		timeout_val = os_readl(U3D_LINK_UX_INACT_TIMER);
		os_printk(K_INFO, "LTSSM: timer_val =%x\n", timeout_val);
		timeout_val &= ~ (U1_INACT_TIMEOUT_VALUE | DEV_U2_INACT_TIMEOUT_VALUE);
		os_writel(U3D_LINK_UX_INACT_TIMER, timeout_val);
	}

	if (dwLtssmValue & SS_INACTIVE_INTR) {
		os_printk(K_INFO, "LTSSM: SS_INACTIVE_INTR\n");
		sts_ltssm = SS_INACTIVE_INTR;
	}

	if (dwLtssmValue & RECOVERY_INTR) {
		os_printk(K_DEBUG, "LTSSM: RECOVERY_INTR\n");
		sts_ltssm = RECOVERY_INTR;
	}

	/* A completion of a Warm Reset shall result in the following.
	* 1. A downstream port shall reset its Link Error Count.
	* 2. Port configuration information of an upstream port shall be reset to default values. Refer to
	*	 Sections 8.4.5 and 8.4.6 for details.
	* 3. The PHY level variables (such as Rx equalization settings) shall be reinitialized or retrained.
	* 4. The LTSSM of a port shall transition to U0 through RxDetect and Polling.
	*/
	if (dwLtssmValue & WARM_RST_INTR) {
		DEV_INT32 link_err_cnt;
		os_printk(K_INFO, "LTSSM: WARM_RST_INTR\n");
		/* Clear link error count */
		link_err_cnt=os_readl(U3D_LINK_ERR_COUNT);
		os_printk(K_INFO, "LTSSM: link_err_cnt=%x\n", link_err_cnt);
		os_writel(U3D_LINK_ERR_COUNT, CLR_LINK_ERR_CNT);

		cancel_delayed_work(&musb->check_ltssm_work);
		sts_ltssm = WARM_RST_INTR;
	}

	if (dwLtssmValue & ENTER_U2_INTR) os_printk(K_DEBUG, "LTSSM: ENTER_U2_INTR\n");
	if (dwLtssmValue & ENTER_U1_INTR) os_printk(K_DEBUG, "LTSSM: ENTER_U1_INTR\n");
	if (dwLtssmValue & RXDET_SUCCESS_INTR) {
		/*create a delay work. This work will work after 0.5sec.
		If LTSSM state is still at RxDet. Clear USB3_EN and set again.*/
		os_printk(K_INFO, "LTSSM: RXDET_SUCCESS_INTR\n");
		sts_ltssm = RXDET_SUCCESS_INTR;
		schedule_delayed_work_on(0, &musb->check_ltssm_work, msecs_to_jiffies(1000));
	}
}

static inline void mtu3d_u2_common_intr_handler(u32 dwIntrUsbValue)
{
	if (dwIntrUsbValue & DISCONN_INTR) {
		mu3d_hal_pdn_ip_port(1, 0, 1, 1);

		os_printk(K_NOTICE, "[U2 DISCONN_INTR] Set SOFT_CONN=0\n");
		os_clrmsk(U3D_POWER_MANAGEMENT, SOFT_CONN);

		/*TODO-J: ADD musb_g_disconnect(musb);??*/
	}

	if (dwIntrUsbValue & LPM_INTR) 	{
		u32 rmwake ;
		rmwake = os_readl(U3D_POWER_MANAGEMENT);

		os_printk(K_NOTICE, "[U2 LPM interrupt] last rmwake is 0x%x\n", rmwake & LPM_RWP);

		if (!((os_readl(U3D_POWER_MANAGEMENT) & LPM_HRWE))) {
			mu3d_hal_pdn_ip_port(0, 0, 0, 1);
		}

#ifdef CONFIG_USBIF_COMPLIANCE
		// SW word around for USBIF test with Fresco FL1100 with LPM L1C enabling
		#if 0
		if (rmwake & LPM_RWP){
			os_writel(U3D_USB20_MISC_CONTROL, os_readl(U3D_USB20_MISC_CONTROL) | LPM_U3_ACK_EN);
			os_writel(U3D_POWER_MANAGEMENT, os_readl(U3D_POWER_MANAGEMENT) | RESUME);
		}
		#endif
#endif
	}

	if (dwIntrUsbValue & LPM_RESUME_INTR) {
		if (!(os_readl(U3D_POWER_MANAGEMENT) & LPM_HRWE)) {
			mu3d_hal_pdn_ip_port(1, 0, 0, 1);

			os_writel(U3D_USB20_MISC_CONTROL, os_readl(U3D_USB20_MISC_CONTROL) | LPM_U3_ACK_EN);
		}
	}

	if(dwIntrUsbValue & SUSPEND_INTR) {
		os_printk(K_NOTICE, "[U2 SUSPEND_INTR]\n");
		mu3d_hal_pdn_ip_port(0, 0, 0, 1);
	}

	if (dwIntrUsbValue & RESUME_INTR) {
		os_printk(K_NOTICE,"[U2 RESUME_INTR]\n");
		mu3d_hal_pdn_ip_port(1, 0, 0, 1);
	}

	if (dwIntrUsbValue & RESET_INTR) {
		os_printk(K_NOTICE,"[U2 RESET_INTR]\n");
	}

}

static inline void mtu3d_link_intr_handler(struct musb *musb, u32 dwLinkIntValue)
{
	u32 dwTemp;
	#ifndef CONFIG_USBIF_COMPLIANCE
	static u32 speed_last = SSUSB_SPEED_INACTIVE;
	static u32 speed = SSUSB_SPEED_INACTIVE;
	static struct timespec ss_timestamp = { 0, 0 };
	#endif

	dwTemp = os_readl(U3D_DEVICE_CONF) & SSUSB_DEV_SPEED;
	mu3d_hal_pdn_cg_en();

	switch (dwTemp)
	{
		case SSUSB_SPEED_FULL:
			os_printk(K_ALET,"USB Speed = Full Speed\n");
			#ifndef CONFIG_USBIF_COMPLIANCE
			speed_last = speed;
			speed = SSUSB_SPEED_FULL;
			#endif

			#ifdef CONFIG_PROJECT_PHY
			/* Comment from CC Chou.
			 * When detecting HS or FS and setting RG_USB20_SW_PLLMODE=1, It is OK to enter LPM L1 with BESL=0.
			 * When disconnecting, set RG_USB20_SW_PLLMODE=0 back.
			 */
			os_setmsk(U3D_U2PHYDCR1, (0x1<<E60802_RG_USB20_SW_PLLMODE_OFST));

			/*BESLCK = 0 < BESLCK_U3 = 1 < BESLDCK = 15*/
			os_writel(U3D_USB20_LPM_PARAMETER, 0x10f0);

			/*
			 * The default value of LPM_BESL_STALL and LPM_BESLD_STALL are 1.
			 * So Does _NOT_ need to set.
			 */
			/*os_setmsk(U3D_POWER_MANAGEMENT, (LPM_BESL_STALL|LPM_BESLD_STALL));*/
			#else
			/*BESLCK = 4 < BESLCK_U3 = 10 < BESLDCK = 15*/
			os_writel(U3D_USB20_LPM_PARAMETER, 0xa4f0);
			os_setmsk(U3D_POWER_MANAGEMENT, (LPM_BESL_STALL|LPM_BESLD_STALL));
			#endif
		break;

		case SSUSB_SPEED_HIGH:
			os_printk(K_ALET,"USB Speed = High Speed\n");
			#ifndef CONFIG_USBIF_COMPLIANCE
			if ((speed == SSUSB_SPEED_INACTIVE) && (speed_last == SSUSB_SPEED_SUPER)) {
				struct timespec tmp = get_connect_timestamp();
				if (timespec_compare(&ss_timestamp, &tmp)>0) {
					os_printk(K_INFO,"queue reconnect work\n");

					schedule_delayed_work_on(0, &musb->reconnect_work, 0);
				}
			}
			speed_last = speed;
			speed = SSUSB_SPEED_HIGH;
			#endif

			#ifdef CONFIG_PROJECT_PHY
			/* Comment from CC Chou.
			 * When detecting HS or FS and setting RG_USB20_SW_PLLMODE=1, It is OK to enter LPM L1 with BESL=0.
			 * When disconnecting, set RG_USB20_SW_PLLMODE=0 back.
			 */
			os_setmsk(U3D_U2PHYDCR1, (0x1<<E60802_RG_USB20_SW_PLLMODE_OFST));

			/*BESLCK = 0 < BESLCK_U3 = 1 < BESLDCK = 15*/
			os_writel(U3D_USB20_LPM_PARAMETER, 0x10f0);
			/*
			 * The default value of LPM_BESL_STALL and LPM_BESLD_STALL are 1.
			 * So Does _NOT_ need to set.
			 */
			 /*os_setmsk(U3D_POWER_MANAGEMENT, (LPM_BESL_STALL|LPM_BESLD_STALL));*/
			#else
			/*BESLCK = 4 < BESLCK_U3 = 10 < BESLDCK = 15*/
			os_writel(U3D_USB20_LPM_PARAMETER, 0xa4f0);
			os_setmsk(U3D_POWER_MANAGEMENT, (LPM_BESL_STALL|LPM_BESLD_STALL));
			#endif
		break;

		case SSUSB_SPEED_SUPER:
			os_printk(K_ALET,"USB Speed = Super Speed\n");
			#ifndef CONFIG_USBIF_COMPLIANCE
			speed_last = speed;
			speed = SSUSB_SPEED_SUPER;
			ss_timestamp = CURRENT_TIME;
			#endif
		break;

		default:
			os_printk(K_ALET,"USB Speed = Invalid (%x)\n", dwTemp);
			#ifndef CONFIG_USBIF_COMPLIANCE
			speed_last = speed;
			speed = SSUSB_SPEED_INACTIVE;
			#endif
		break;
	}
}

#ifdef SUPPORT_OTG
static inline void mtu3d_otg_intr_handler(u32 dwOtgIntValue)
{
	if (dwOtgIntValue & VBUS_CHG_INTR) {
		os_printk(K_NOTICE, "OTG: VBUS_CHG_INTR\n");
		os_setmsk(U3D_SSUSB_OTG_STS_CLR, SSUSB_VBUS_INTR_CLR);
	}

	//this interrupt is issued when B device becomes device
	if (dwOtgIntValue & SSUSB_CHG_B_ROLE_B) {
		os_printk(K_NOTICE, "OTG: CHG_B_ROLE_B\n");
		os_setmsk(U3D_SSUSB_OTG_STS_CLR, SSUSB_CHG_B_ROLE_B_CLR);

		//switch DMA module to device
		os_printk(K_NOTICE, "Switch DMA to device\n");
		os_clrmsk(U3D_SSUSB_U2_CTRL_0P, SSUSB_U2_PORT_HOST_SEL);
	}

	//this interrupt is issued when B device becomes host
	if (dwOtgIntValue & SSUSB_CHG_A_ROLE_B) {
		os_printk(K_NOTICE, "OTG: CHG_A_ROLE_B\n");
		os_setmsk(U3D_SSUSB_OTG_STS_CLR, SSUSB_CHG_A_ROLE_B_CLR);
	}

	//this interrupt is issued when IDDIG reads B
	if (dwOtgIntValue & SSUSB_ATTACH_B_ROLE) {
		os_printk(K_NOTICE, "OTG: CHG_ATTACH_B_ROLE\n");
		os_setmsk(U3D_SSUSB_OTG_STS_CLR, SSUSB_ATTACH_B_ROLE_CLR);

		//switch DMA module to device
		os_printk(K_NOTICE, "Switch DMA to device\n");
		os_clrmsk(U3D_SSUSB_U2_CTRL_0P, SSUSB_U2_PORT_HOST_SEL);
	}

}
#endif

static irqreturn_t generic_interrupt(int irq, void *__hci)
{
	unsigned long flags;
	irqreturn_t	retval = IRQ_HANDLED;
	struct musb	*musb = __hci;

	u32 dwL1Value = 0;
	u32 dwIntrUsbValue = 0;
	u32 dwDmaIntrValue = 0;
	u32 dwIntrEPValue = 0;
	u16 wIntrTxValue = 0;
	u16 wIntrRxValue = 0;
#ifdef USE_SSUSB_QMU
	u32 wIntrQMUValue = 0;
	u32 wIntrQMUDoneValue = 0;
#endif
	u32 dwLtssmValue = 0;
	u32 dwLinkIntValue = 0;

#ifdef SUPPORT_OTG
	u32 dwOtgIntValue = 0;
#endif

	spin_lock_irqsave(&musb->lock, flags);

	dwL1Value = os_readl(U3D_LV1ISR) & os_readl(U3D_LV1IER);

	if (dwL1Value & EP_CTRL_INTR) {
		u32 dwRxEpDataerrVal = os_readl(U3D_USB2_RX_EP_DATAERR_INTR);
		if (dwRxEpDataerrVal != 0) {
			/* Write 1 clear*/
			os_writel(U3D_USB2_RX_EP_DATAERR_INTR, dwRxEpDataerrVal);
			os_printk(K_INFO, "===L1[%x] RxDataErr[%x]\n", dwL1Value,
				(dwRxEpDataerrVal>>USB2_RX_EP_DATAERR_INTR_EN_OFST && dwRxEpDataerrVal));
		}
		dwLinkIntValue = os_readl(U3D_DEV_LINK_INTR) & os_readl(U3D_DEV_LINK_INTR_ENABLE);
		if (dwLinkIntValue != 0) {
			/* Write 1 clear*/
			os_writel(U3D_DEV_LINK_INTR, dwLinkIntValue);
			os_printk(K_INFO, "===L1[%x] LinkInt[%x]\n", dwL1Value, dwLinkIntValue);
		}
	}

	if (dwL1Value & MAC2_INTR) {
		dwIntrUsbValue = os_readl(U3D_COMMON_USB_INTR) & os_readl(U3D_COMMON_USB_INTR_ENABLE);
		/* Write 1 clear*/
		os_writel(U3D_COMMON_USB_INTR, dwIntrUsbValue);
		os_printk(K_INFO, "===L1[%x] U2[%x]\n", dwL1Value, dwIntrUsbValue);
	}

	if (dwL1Value & DMA_INTR) {
		dwDmaIntrValue = os_readl(U3D_DMAISR) & os_readl(U3D_DMAIER);
		/* Write 1 clear*/
		os_writel(U3D_DMAISR, dwDmaIntrValue);
		os_printk(K_INFO, "===L1[%x] DMA[%x]\n", dwL1Value, dwDmaIntrValue);
	}

	if (dwL1Value & MAC3_INTR) {
		dwLtssmValue = os_readl(U3D_LTSSM_INTR) & os_readl(U3D_LTSSM_INTR_ENABLE);
		/* Write 1 clear*/
		os_writel(U3D_LTSSM_INTR, dwLtssmValue);
		os_printk(K_DEBUG, "===L1[%x] LTSSM[%x]\n", dwL1Value, dwLtssmValue);
	}

#ifdef USE_SSUSB_QMU
	if (dwL1Value & QMU_INTR) {
		wIntrQMUValue = os_readl(U3D_QISAR1) & os_readl(U3D_QIER1);
		wIntrQMUDoneValue = os_readl(U3D_QISAR0) & os_readl(U3D_QIER0);
		/* Write 1 clear */
		os_writel(U3D_QISAR0, wIntrQMUDoneValue);
		qmu_printk(K_DEBUG, "===L1[%x] QMUDone[Tx=%x,Rx=%x] QMU[%x]===\n",\
			dwL1Value, ((wIntrQMUDoneValue & 0xFFFF) >> 1), wIntrQMUDoneValue>>17, wIntrQMUValue);
	}
#endif

	if (dwL1Value & BMU_INTR) {
		dwIntrEPValue = os_readl(U3D_EPISR) & os_readl(U3D_EPIER);
		wIntrTxValue = dwIntrEPValue & 0xFFFF;
		wIntrRxValue = (dwIntrEPValue>>16);
		os_writel(U3D_EPISR, dwIntrEPValue);
		os_printk(K_DEBUG, "===L1[%x] Tx[%x] Rx[%x]===\n",\
			dwL1Value, wIntrTxValue, wIntrRxValue);

	}

	/*TODO: need to handle SetupEnd Interrupt!!!*/

	/*--Handle each interrupt--*/
	if ((dwL1Value & (BMU_INTR | MAC2_INTR)) || dwIntrUsbValue) {
		musb->int_usb = dwIntrUsbValue;
		musb->int_tx = wIntrTxValue;
		musb->int_rx = wIntrRxValue;

		if (musb->int_usb || musb->int_tx || musb->int_rx)
			retval = musb_interrupt(musb);
		else
			os_printk(K_INFO, "===L1[%x] Nothing can do?? Tx[%x] Rx[%x] U2[%x]===\n",\
						dwL1Value, wIntrTxValue, wIntrRxValue, dwIntrUsbValue );
	}

#ifdef SUPPORT_OTG
	dwOtgIntValue = os_readl(U3D_SSUSB_OTG_STS) & os_readl(U3D_SSUSB_OTG_INT_EN);;
#endif

#if defined (USE_SSUSB_QMU)
	if (wIntrQMUDoneValue) {
		if (musb->qmu_done_intr != 0) {
		 	musb->qmu_done_intr = wIntrQMUDoneValue | musb->qmu_done_intr;
			qmu_printk(K_DEBUG, "Has not handle yet %x\n", musb->qmu_done_intr);
		} else {
		 	musb->qmu_done_intr = wIntrQMUDoneValue;
		}
	 	tasklet_schedule(&musb->qmu_done);
	}

 	if (wIntrQMUValue) {
		qmu_exception_interrupt(musb, wIntrQMUValue);
 	}
#endif

	if (dwLtssmValue) {
		mtu3d_u3_ltssm_intr_handler(musb, dwLtssmValue);
	}

	if (dwIntrUsbValue) {
		mtu3d_u2_common_intr_handler(dwIntrUsbValue);
	}

	if (dwLinkIntValue & SSUSB_DEV_SPEED_CHG_INTR) {
		mtu3d_link_intr_handler(musb, dwLinkIntValue);
	}

#ifdef SUPPORT_OTG
	if (dwOtgIntValue) {
		mtu3d_otg_intr_handler(dwOtgIntValue);
	}
#endif

	spin_unlock_irqrestore(&musb->lock, flags);

	return retval;
}


void mtu3d_musb_enable(struct musb *musb)
{
	os_printk(K_INFO, "%s\n", __func__);
}

void mtu3d_musb_disable(struct musb *musb)
{
	os_printk(K_INFO, "%s\n", __func__);

#ifdef CONFIG_PROJECT_PHY
	/* Comment from CC Chou.
	 * When detecting HS or FS and setting RG_USB20_SW_PLLMODE=1, It is OK to enter LPM L1 with BESL=0.
	 * When disconnecting, set RG_USB20_SW_PLLMODE=0 back.
	 */
	os_clrmsk(U3D_U2PHYDCR1, E60802_RG_USB20_SW_PLLMODE);
#endif
}

static int mtu3d_musb_init(struct musb *musb)
{
	os_printk(K_DEBUG, "%s\n", __func__);

	usb_nop_xceiv_register();
	musb->xceiv = usb_get_phy(USB_PHY_TYPE_USB2);
	if (IS_ERR_OR_NULL(musb->xceiv))
		goto unregister;

	mtu3d_musb_reg_init(musb);

	if (is_peripheral_enabled(musb))
		musb->xceiv->set_power = mtu3d_set_power;

	musb->isr = generic_interrupt;
	return 0;

unregister:
	usb_nop_xceiv_unregister();
	return -ENODEV;

}

static int mtu3d_set_power(struct usb_phy *x, unsigned mA)
{
	os_printk(K_DEBUG, "%s\n", __func__);
	return 0;
}

static int mtu3d_musb_exit(struct musb *musb)
{
#ifdef NEVER
	struct device *dev = musb->controller;
	struct musb_hdrc_platform_data *plat = dev->platform_data;
#endif /* NEVER */

	usb_put_phy(musb->xceiv);
	usb_nop_xceiv_unregister();

	return 0;
}

#ifdef CONFIG_MTK_UART_USB_SWITCH
extern bool usb_phy_check_in_uart_mode(void);
extern bool in_uart_mode;
#endif

extern void usb20_pll_settings(bool host, bool forceOn);

static void mtu3d_musb_reg_init(struct musb *musb)
{
	int ret = 1;
	os_printk(K_DEBUG, "%s\n", __func__);

	/* initialize PHY related data structure */
	if (!u3phy_ops)
		ret = u3phy_init();

	if (ret || u3phy_ops) {

#ifdef CONFIG_MTK_UART_USB_SWITCH
		if (usb_phy_check_in_uart_mode()) {
			os_printk(K_INFO, "%s+ UART_MODE\n", __func__);
			in_uart_mode = true;
		} else {
			os_printk(K_INFO, "%s+ USB_MODE\n", __func__);
		}
#endif

		u3phy_ops->init(u3phy);

		musb->is_clk_on = 1;

#ifndef CONFIG_MTK_FPGA
		usb_phy_recover(musb->is_clk_on);
#endif
		/* USB 2.0 slew rate calibration */
		u3phy_ops->u2_slew_rate_calibration(u3phy);

		/* disable ip power down, disable U2/U3 ip power down */
		_ex_mu3d_hal_ssusb_en();

		/* USB PLL Force settings */
		usb20_pll_settings(false, false);

		/* reset U3D all dev module. */
		mu3d_hal_rst_dev();

	} else {
		os_printk(K_ERR, "%s: PHY initialization fail!\n", __func__);
		BUG_ON(1);
	}
}

static u64 usb_dmamask = DMA_BIT_MASK(32);

static int mtu3d_probe(struct platform_device *pdev)
{
	const struct musb_hdrc_platform_data	*pdata = dev_get_platdata(&pdev->dev);
	const struct of_device_id *match;
	struct platform_device		*musb;
	struct mtu3d_glue		*glue;

	int ret = -ENOMEM;

	os_printk(K_DEBUG, "%s\n", __func__);

	glue = kzalloc(sizeof(*glue), GFP_KERNEL);
	if (!glue) {
		dev_err(&pdev->dev, "failed to allocate glue context\n");
		goto err0;
	}

	musb = platform_device_alloc(MUSB_DRIVER_NAME, PLATFORM_DEVID_NONE);
	if (!musb) {
		dev_err(&pdev->dev, "failed to allocate musb device\n");
		goto err1;
	}

	musb->dev.parent		= &pdev->dev;
	musb->dev.dma_mask		= &usb_dmamask;
	musb->dev.coherent_dma_mask	= usb_dmamask;

	glue->dev			= &pdev->dev;
	glue->musb			= musb;

	match = of_match_device(of_match_ptr(mtu3d_of_match), &pdev->dev);
	if (match) {
		pdata = match->data;
	}

	platform_set_drvdata(pdev, glue);

	ret = platform_device_add_resources(musb, pdev->resource,
			pdev->num_resources);
	if (ret) {
		dev_err(&pdev->dev, "failed to add resources\n");
		goto err2;
	}

	ret = platform_device_add_data(musb, pdata, sizeof(*pdata));
	if (ret) {
		dev_err(&pdev->dev, "failed to add platform_data\n");
		goto err2;
	}

	ret = platform_device_add(musb);
	if (ret) {
		dev_err(&pdev->dev, "failed to register musb device\n");
		goto err2;
	}

	return 0;

err2:
	platform_device_put(musb);

err1:
	kfree(glue);

err0:
	return ret;
}

static int mtu3d_remove(struct platform_device *pdev)
{
	struct mtu3d_glue		*glue = platform_get_drvdata(pdev);

	platform_device_del(glue->musb);
	platform_device_put(glue->musb);
	kfree(glue);

	return 0;
}

#ifdef CONFIG_PM
static int mtu3d_suspend_noirq(struct device *dev)
{
	os_printk(K_INFO, "%s\n", __func__);

	/*J:I think it does _NOT_ have to do PHY savecurrent. Because when the phone with USB cable,
	 * the system does _NOT_ enter suspend mode. At the normal case, the USB driver calls PHY savecurrent,
	 * when USB cable is plugged out.
	 */
	/*usb_phy_savecurrent();*/

	/*
	 * SSUSB IP Software Reset - When this bit is set, whole SSUSB IP is reset.
	 * All MAC regs(DEV, EPCTL CSR, USB3 MAC/SYS CSR, USB2 CSR can _NOT_ be read/written when this bit is set.)
	 * Setting this bit when suspend is to improve USB current leakage problem of AVDD18_USB_P0.
	 * After clearing this bit, the whole MAC registers would reset to the default value.
	 */
	//os_setmsk(U3D_SSUSB_IP_PW_CTRL0, SSUSB_IP_SW_RST);

	return 0;
}

static int mtu3d_resume_noirq(struct device *dev)
{
	os_printk(K_INFO, "%s\n", __func__);

	/* disable ip power down, disable U2/U3 ip power down */
	//_ex_mu3d_hal_ssusb_en();

	/* reset U3D all dev module. */
	//mu3d_hal_rst_dev();

	/*J:I think it does _NOT_ have to do PHY savecurrent. Because after USB cable is plugged in,
	 * the USB driver calls PHY recovery. So does _NOT_ have to do PHY recovery at this moment.*/
	//usb_phy_recover();

	return 0;
}

static struct dev_pm_ops mtu3d_pm_ops = {
	.suspend_noirq	= mtu3d_suspend_noirq,
	.resume_noirq		= mtu3d_resume_noirq,
};

#define DEV_PM_OPS	&mtu3d_pm_ops
#else
#define DEV_PM_OPS	NULL
#endif

static struct platform_driver mtu3d_driver = {
	.probe		= mtu3d_probe,
	.remove		= mtu3d_remove,
	.driver		= {
		.name	= "musb-mtu3d",
		.owner 	= THIS_MODULE,
		.pm	= DEV_PM_OPS,
		.of_match_table = of_match_ptr(mtu3d_of_match),
	},
};

MODULE_DESCRIPTION("mtu3d MUSB Glue Layer");
MODULE_AUTHOR("MediaTek");
MODULE_LICENSE("GPL v2");
module_platform_driver(mtu3d_driver);
