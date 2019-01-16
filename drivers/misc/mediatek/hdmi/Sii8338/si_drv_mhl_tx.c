#ifndef __KERNEL__
#include <string.h>
#include "hal_local.h"
#include "si_common.h"
#else
#ifdef DEBUG
#define TRACE_INT_TIME
#endif
#ifdef TRACE_INT_TIME
#include <linux/jiffies.h>
#endif
#include <linux/string.h>
#endif
#include <linux/delay.h>
#include "si_cra.h"
#include "si_cra_cfg.h"
#include "si_mhl_defs.h"
#include "si_mhl_tx_api.h"
#include "si_mhl_tx_base_drv_api.h"
#include "si_8338_regs.h"
#include "si_drv_mhl_tx.h"
#include "si_platform.h"
#include "smartbook.h"
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <mach/mt_gpio.h>
#include <cust_gpio_usage.h>
#include <cust_eint.h>
#include "si_drv_mdt_tx.h"
#include "si_mdt_inputdev.h"
///#include "hdmitx_i2c.h"
#include "hdmi_cust.h"

static ktime_t hr_timer_AbortTimer_CHK_ktime;
unsigned long delay_in_ms;
#define MS_TO_NS(x)	(x * 1E6L)
extern struct hrtimer hr_timer_AbortTimer_CHK;

#define SILICON_IMAGE_ADOPTER_ID 322
#define	POWER_STATE_D3				3
#define	POWER_STATE_D0_NO_MHL		2
#define	POWER_STATE_D0_MHL			0
#define	POWER_STATE_FIRST_INIT		0xFF
#define TX_HW_RESET_PERIOD		10
#define TX_HW_RESET_DELAY			100
static uint8_t fwPowerState = POWER_STATE_FIRST_INIT;
static bool_t mscCmdInProgress;
bool_t mscAbortFlag = false;
static uint8_t dsHpdStatus;
#define WriteByteCBUS(offset, value)  SiiRegWrite(TX_PAGE_CBUS | (uint16_t)offset, value)
#define ReadByteCBUS(offset)         SiiRegRead(TX_PAGE_CBUS | (uint16_t)offset)
#define	SET_BIT(offset, bitnumber)		SiiRegModify(offset, (1<<bitnumber), (1<<bitnumber))
#define	CLR_BIT(offset, bitnumber)		SiiRegModify(offset, (1<<bitnumber), 0x00)
#define	DISABLE_DISCOVERY				SiiRegModify(TX_PAGE_3 | 0x0010, BIT0, 0)
#define	ENABLE_DISCOVERY				SiiRegModify(TX_PAGE_3 | 0x0010, BIT0, BIT0)
#define STROBE_POWER_ON					SiiRegModify(TX_PAGE_3 | 0x0010, BIT1, 0)
#define INTR_1_DESIRED_MASK			(BIT7|BIT6)
#define	UNMASK_INTR_1_INTERRUPTS		SiiRegWrite(TX_PAGE_L0 | 0x0075, INTR_1_DESIRED_MASK)
#define	MASK_INTR_1_INTERRUPTS			SiiRegWrite(TX_PAGE_L0 | 0x0075, 0x00)
#define INTR_2_DESIRED_MASK			(BIT1)
#define	UNMASK_INTR_2_INTERRUPTS		SiiRegWrite(TX_PAGE_L0 | 0x0076, INTR_2_DESIRED_MASK)
#define	INTR_4_DESIRED_MASK				(BIT0 | BIT2 | BIT3 | BIT4 | BIT5 | BIT6)
#define	UNMASK_INTR_4_INTERRUPTS		SiiRegWrite(TX_PAGE_3 | 0x0022, INTR_4_DESIRED_MASK)
#define	MASK_INTR_4_INTERRUPTS			SiiRegWrite(TX_PAGE_3 | 0x0022, 0x00)
#define	INTR_5_DESIRED_MASK				0	/* (BIT2 | BIT3 | BIT4) */
#define	UNMASK_INTR_5_INTERRUPTS		SiiRegWrite(TX_PAGE_3 | 0x0024, INTR_5_DESIRED_MASK)
#define	MASK_INTR_5_INTERRUPTS			SiiRegWrite(TX_PAGE_3 | 0x0024, 0x00)
#define	INTR_CBUS1_DESIRED_MASK			(BIT2 | BIT3 | BIT4 | BIT5 | BIT6)
#define	UNMASK_CBUS1_INTERRUPTS			SiiRegWrite(TX_PAGE_CBUS | 0x0009, INTR_CBUS1_DESIRED_MASK)
#define	MASK_CBUS1_INTERRUPTS			SiiRegWrite(TX_PAGE_CBUS | 0x0009, 0x00)
#define	INTR_CBUS2_DESIRED_MASK			(BIT0 | BIT2 | BIT3)
#define	UNMASK_CBUS2_INTERRUPTS			SiiRegWrite(TX_PAGE_CBUS | 0x001F, INTR_CBUS2_DESIRED_MASK)
#define	MASK_CBUS2_INTERRUPTS			SiiRegWrite(TX_PAGE_CBUS | 0x001F, 0x00)
#define I2C_INACCESSIBLE -1
#define I2C_ACCESSIBLE 1
#define SIZE_AVI_INFOFRAME				17
static void Int1Isr(void);
static int Int4Isr(void);
static void Int5Isr(void);
static void MhlCbusIsr(void);
static void CbusReset(void);
static void SwitchToD0(void);
void SwitchToD3(void);
static void WriteInitialRegisterValues(void);
static void InitCBusRegs(void);
static void ForceUsbIdSwitchOpen(void);
static void ReleaseUsbIdSwitchOpen(void);
static void MhlTxDrvProcessConnection(void);
static void MhlTxDrvProcessDisconnection(void);
static bool_t tmdsPowRdy;
video_data_t video_data;
static AVMode_t AVmode = { VM_INVALID, AUD_INVALID };

static void AudioVideoIsr(bool_t force_update);
static uint8_t CalculateInfoFrameChecksum(uint8_t *infoFrameData);
static void SendAviInfoframe(void);
static void ProcessScdtStatusChange(void);
static void SetAudioMode(inAudioTypes_t audiomode);
static void SetACRNValue(void);
static void SendAudioInfoFrame(void);
void SiiMhlTxHwReset(uint16_t hwResetPeriod, uint16_t hwResetDelay);

#ifdef __KERNEL__
static SiiOsTimer_t MscAbortTimer;
static void SiiMhlMscAbortTimerCB(void *pArg)
{
	SiiOsTimerDelete(MscAbortTimer);
	MscAbortTimer = NULL;
	mscAbortFlag = false;
	SiiMhlTriggerSoftInt();
}

uint8_t SiiCheckDevice(uint8_t dev)
{
#ifdef _RGB_BOARD
	if (HalI2cReadByte(0x60, 0x70) == 0xff)
		return false;
#endif
	if (dev == 0) {
		if (POWER_STATE_D3 != fwPowerState && HalI2cReadByte(0x9a, 0x21) == 0xff)
			return false;
	} else if (dev == 1) {
		if (POWER_STATE_D3 == fwPowerState)
			return false;
	}
	return true;
}

void SiiMhlTriggerSoftInt(void)
{
	SiiRegBitsSet(TX_PAGE_3 | 0x0020, BIT3, true);
	HalTimerWait(5);
	SiiRegBitsSet(TX_PAGE_3 | 0x0020, BIT3, false);
}
#endif

#ifdef MDT_SUPPORT
enum mdt_state g_state_for_mdt = IDLE;
extern struct msc_request g_prior_msc_request;
#endif

static void Int1Isr(void)
{
	uint8_t regIntr1;
	regIntr1 = SiiRegRead(TX_PAGE_L0 | 0x0071);
	if (regIntr1) {
		SiiRegWrite(TX_PAGE_L0 | 0x0071, regIntr1);
		if (BIT6 & regIntr1) {
			uint8_t cbusStatus;
			cbusStatus = SiiRegRead(TX_PAGE_CBUS | 0x000D);
			TX_DEBUG_PRINT(("Drv: dsHpdStatus =%02X\n", (int)dsHpdStatus));
			if (BIT6 & (dsHpdStatus ^ cbusStatus)) {
				uint8_t status = cbusStatus & BIT6;
				TX_DEBUG_PRINT(("Drv: Downstream HPD changed to: %02X\n",
						(int)cbusStatus));
				SiiMhlTxNotifyDsHpdChange(status);
				if (status) {
					AudioVideoIsr(true);
				}
				dsHpdStatus = cbusStatus;
			}
		}
		if (BIT7 & regIntr1) {
			TX_DEBUG_PRINT(("MHL soft interrupt triggered\n"));
		}
	}
}

static int Int2Isr(void)
{
	if (SiiRegRead(TX_PAGE_L0 | 0x0072) & INTR_2_DESIRED_MASK) {
		SiiRegWrite(TX_PAGE_L0 | 0x0072, INTR_2_DESIRED_MASK);
		if (SiiRegRead(TX_PAGE_L0 | 0x0009) & BIT1) {
			TX_DEBUG_PRINT(("PCLK is STABLE\n"));
			if (tmdsPowRdy) {
				SendAudioInfoFrame();
				SendAviInfoframe();
			}
		}
	}
	return 0;
}

bool_t SiiMhlTxChipInitialize(void)
{
	unsigned int initState = 0, g_chipRevId = 0;
	tmdsPowRdy = false;
	mscCmdInProgress = false;
	dsHpdStatus = 0;
	fwPowerState = POWER_STATE_D0_NO_MHL;
	SI_OS_DISABLE_DEBUG_CHANNEL(SII_OSAL_DEBUG_SCHEDULER);
	/* memset(&video_data, 0x00, sizeof(video_data)); */
	video_data.inputColorSpace = COLOR_SPACE_RGB;
	video_data.outputColorSpace = COLOR_SPACE_RGB;
	video_data.outputVideoCode = 2;
	video_data.inputcolorimetryAspectRatio = 0x18;
	video_data.outputcolorimetryAspectRatio = 0x18;
	video_data.output_AR = 0;
	/* SiiMhlTxHwReset(TX_HW_RESET_PERIOD,TX_HW_RESET_DELAY); */
	/* SiiCraInitialize(); */

	g_chipRevId = SiiRegRead(TX_PAGE_L0 | 0x04);

	initState = (SiiRegRead(TX_PAGE_L0 | 0x03) << 8) | SiiRegRead(TX_PAGE_L0 | 0x02);
	TX_DEBUG_PRINT(("Drv: SiiMhlTxChipInitialize:%04X,g_chipRevId=0x%x\n", initState,
			g_chipRevId));
	if ((initState & 0xFF) != 0x52) {
		SwitchToD3();
		return false;
	}
	WriteInitialRegisterValues();
#ifndef __KERNEL__
	SiiOsMhlTxInterruptEnable();
#endif
	SwitchToD3();
	if (0xFFFF == initState)	/* 0x8356 */
	{
		return false;
	}
	return true;
}

typedef enum {
	HDMI_STATE_NO_DEVICE,
	HDMI_STATE_ACTIVE,
	HDMI_STATE_DPI_ENABLE
} HDMI_STATE;
extern void hdmi_state_callback(HDMI_STATE state);


void SiiMhlTxDeviceIsr(void)
{
	uint8_t intMStatus, i;
#ifdef TRACE_INT_TIME
	unsigned long K1;
	unsigned long K2;
	printk("-------------------SiiMhlTxDeviceIsr start -----------------\n");
	K1 = get_jiffies_64();
#endif
	i = 0;
	do {
		TX_DEBUG_PRINT(("Drv: SiiMhlTxDeviceIsr %d\n", fwPowerState));
#ifdef MDT_SUPPORT
		if (g_state_for_mdt != IDLE) {
			/* TX_DEBUG_PRINT(("g_state_for_mdt=%d,HalGpioGetTxIntPin22222222222=%d#######################\n",g_state_for_mdt,HalGpioGetTxIntPin())); */

			while ((sii8338_irq_for_mdt(&g_state_for_mdt) == MDT_EVENT_HANDLED)
			       || (g_state_for_mdt != WAIT_FOR_REQ_WRT)) {
				/* TX_DEBUG_PRINT(("g_state_for_mdt=%d\n",g_state_for_mdt)); */
				Int4Isr();
				if (fwPowerState == POWER_STATE_D3)
					return;
			}	/* &&(SiiRegRead(TX_PAGE_L0| 0x0070)&0x01));// */
			/* &&    (HalGpioGetTxIntPin() == MHL_INT_ASSERTED_VALUE)); */
			/* { */

			/* TX_DEBUG_PRINT(("g_state_for_mdt=%d,HalGpioGetTxIntPin=%d@@@@@@@@@@@@@@@@@@@@@@\n",g_state_for_mdt,HalGpioGetTxIntPin())); */
			/* if (HalGpioGetTxIntPin() != MHL_INT_ASSERTED_VALUE) { */
			/* MHL_log_event(IRQ_RECEIVED, 1, HalGpioGetTxIntPin()); */
			/* if(!(SiiRegRead(TX_PAGE_L0| 0x0070)&0x01)) */
			/* return; */
			/* } */
			/* } */
		}
		/* TX_DEBUG_PRINT(("g_state_for_mdt=%d,HalGpioGetTxIntPin3333333333333=%d#######################\n",g_state_for_mdt,HalGpioGetTxIntPin())); */

		MHL_log_event(ISR_FULL_BEGIN, 0, HalGpioGetTxIntPin());
#endif

		if (POWER_STATE_D0_MHL != fwPowerState) {

			if (I2C_INACCESSIBLE == Int4Isr()) {
				TX_DEBUG_PRINT(("Drv: I2C_INACCESSIBLE in Int4Isr in not D0 mode\n"));
				return;
			}
		} else if (POWER_STATE_D0_MHL == fwPowerState) {

#if 0
			TX_DEBUG_PRINT(("********* EXITING ISR *************\n"));
			TX_DEBUG_PRINT(("Drv: INT1 Status = %02X\n",
					(int)SiiRegRead((TX_PAGE_L0 | 0x0071))));
			TX_DEBUG_PRINT(("Drv: INT2 Status = %02X\n",
					(int)SiiRegRead((TX_PAGE_L0 | 0x0072))));
			TX_DEBUG_PRINT(("Drv: INT3 Status = %02X\n",
					(int)SiiRegRead((TX_PAGE_L0 | 0x0073))));
			TX_DEBUG_PRINT(("Drv: INT4 Status = %02X\n",
					(int)SiiRegRead(TX_PAGE_3 | 0x0021)));
			TX_DEBUG_PRINT(("Drv: INT5 Status = %02X\n",
					(int)SiiRegRead(TX_PAGE_3 | 0x0023)));


			TX_DEBUG_PRINT(("Drv: cbusInt Status = %02X\n",
					(int)SiiRegRead(TX_PAGE_CBUS | 0x0008)));

			TX_DEBUG_PRINT(("Drv: CBUS INTR_2: 0x1E: %02X\n",
					(int)SiiRegRead(TX_PAGE_CBUS | 0x001E)));
			TX_DEBUG_PRINT(("Drv: A0 INT Set = %02X\n",
					(int)SiiRegRead((TX_PAGE_CBUS | 0x00A0))));
			TX_DEBUG_PRINT(("Drv: A1 INT Set = %02X\n",
					(int)SiiRegRead((TX_PAGE_CBUS | 0x00A1))));
			TX_DEBUG_PRINT(("Drv: A2 INT Set = %02X\n",
					(int)SiiRegRead((TX_PAGE_CBUS | 0x00A2))));
			TX_DEBUG_PRINT(("Drv: A3 INT Set = %02X\n",
					(int)SiiRegRead((TX_PAGE_CBUS | 0x00A3))));

			TX_DEBUG_PRINT(("Drv: B0 STATUS Set = %02X\n",
					(int)SiiRegRead((TX_PAGE_CBUS | 0x00B0))));
			TX_DEBUG_PRINT(("Drv: B1 STATUS Set = %02X\n",
					(int)SiiRegRead((TX_PAGE_CBUS | 0x00B1))));
			TX_DEBUG_PRINT(("Drv: B2 STATUS Set = %02X\n",
					(int)SiiRegRead((TX_PAGE_CBUS | 0x00B2))));
			TX_DEBUG_PRINT(("Drv: B3 STATUS Set = %02X\n",
					(int)SiiRegRead((TX_PAGE_CBUS | 0x00B3))));

			TX_DEBUG_PRINT(("Drv: E0 STATUS Set = %02X\n",
					(int)SiiRegRead((TX_PAGE_CBUS | 0x00E0))));
			TX_DEBUG_PRINT(("Drv: E1 STATUS Set = %02X\n",
					(int)SiiRegRead((TX_PAGE_CBUS | 0x00E1))));
			TX_DEBUG_PRINT(("Drv: E2 STATUS Set = %02X\n",
					(int)SiiRegRead((TX_PAGE_CBUS | 0x00E2))));
			TX_DEBUG_PRINT(("Drv: E3 STATUS Set = %02X\n",
					(int)SiiRegRead((TX_PAGE_CBUS | 0x00E3))));

			TX_DEBUG_PRINT(("Drv: F0 INT Set = %02X\n",
					(int)SiiRegRead((TX_PAGE_CBUS | 0x00F0))));
			TX_DEBUG_PRINT(("Drv: F1 INT Set = %02X\n",
					(int)SiiRegRead((TX_PAGE_CBUS | 0x00F1))));
			TX_DEBUG_PRINT(("Drv: F2 INT Set = %02X\n",
					(int)SiiRegRead((TX_PAGE_CBUS | 0x00F2))));
			TX_DEBUG_PRINT(("Drv: F3 INT Set = %02X\n",
					(int)SiiRegRead((TX_PAGE_CBUS | 0x00F3))));
			TX_DEBUG_PRINT(("********* END OF EXITING ISR *************\n"));
#endif
			if (I2C_INACCESSIBLE == Int4Isr()) {
				TX_DEBUG_PRINT(("Drv: I2C_INACCESSIBLE in Int4Isr in D0 mode\n"));
				return;
			}
			MhlCbusIsr();
			Int5Isr();
			Int1Isr();
			Int2Isr();
		}
		if (POWER_STATE_D3 != fwPowerState) {
			MhlTxProcessEvents();
		}
		intMStatus = SiiRegRead(TX_PAGE_L0 | 0x0070);
		if (0xFF == intMStatus) {
			intMStatus = 0;
			TX_DEBUG_PRINT(("Drv: EXITING ISR DUE TO intMStatus - 0xFF loop = [%02X] intMStatus = [%02X]\n\n", (int)i, (int)intMStatus));
		}
		i++;
		intMStatus &= 0x01;
		if (i > 60) {
			TX_DEBUG_PRINT(("force exit SiiMhlTxDeviceIsr\n"));
			break;
		} else if (i > 50) {
			TX_DEBUG_PRINT(("something error in SiiMhlTxDeviceIsr\n"));
		}
	} while (intMStatus);
#ifdef TRACE_INT_TIME
	K2 = get_jiffies_64();
	printk("-------------------SiiMhlTxDeviceIsr last %d ms----------------\n", (int)(K2 - K1));
#endif
}

void SiiExtDeviceIsr(void)
{
	if (fwPowerState <= POWER_STATE_D0_NO_MHL) {
#ifdef TRACE_INT_TIME
		unsigned long K1;
		unsigned long K2;
		K1 = get_jiffies_64();
#endif
		AudioVideoIsr(false);
#ifdef TRACE_INT_TIME
		K2 = get_jiffies_64();
#endif
	} else {
		TX_DEBUG_PRINT(("in D3 mode , SiiExtDeviceIsr not handled\n"));
	}
}

void SiiMhlTxDrvTmdsControl(bool_t enable)
{
	if (enable) {
#ifdef MDT_SUPPORT
		mdt_init();
#endif
		tmdsPowRdy = true;
		SiiRegModify(TX_PAGE_L1 | 0x2F, BIT0, SET_BITS);
		if (1)		/* (SiiVideoInputIsValid()) */
		{
			SiiRegModify(TX_PAGE_L0 | 0x0080, BIT4, SET_BITS);
			SendAudioInfoFrame();
			SendAviInfoframe();
			TX_DEBUG_PRINT(("TMDS Output Enabled\n"));
			/* Timon */
#ifdef CONFIG_MTK_SMARTBOOK_SUPPORT
			SiiHandshakeCommand(Init);
#endif
		} else {
			TX_DEBUG_PRINT(("TMDS Output not Enabled due to invalid input\n"));
		}
	} else {
		SiiRegModify(TX_PAGE_L0 | 0x0080, BIT4, CLEAR_BITS);
		SiiRegModify(TX_PAGE_L1 | 0x2F, BIT0, CLEAR_BITS);
		tmdsPowRdy = false;
		TX_DEBUG_PRINT(("TMDS Ouput Disabled\n"));
#ifdef MDT_SUPPORT
		mdt_deregister();
#endif
	}
}

void SiiMhlTxDrvNotifyEdidChange(void)
{
	TX_DEBUG_PRINT(("SiiMhlTxDrvNotifyEdidChange\n"));
	SiiDrvMhlTxReadEdid();
}

bool_t SiiMhlTxDrvSendCbusCommand(cbus_req_t *pReq)
{
	bool_t success = true;
	uint8_t i, startbit;
	if ((POWER_STATE_D0_MHL != fwPowerState) || (mscCmdInProgress)) {
		TX_DEBUG_PRINT(("Error: fwPowerState: %02X, or CBUS(0x0A):%02X mscCmdInProgress = %d\n", (int)fwPowerState, (int)SiiRegRead(TX_PAGE_CBUS | 0x000A), (int)mscCmdInProgress));
		return false;
	}
	mscCmdInProgress = true;
	TX_DEBUG_PRINT(("Sending MSC command %02X, %02X, %02X\n",
			(int)pReq->command,
			(int)((MHL_MSC_MSG ==
			       pReq->command) ? pReq->payload_u.msgData[0] : pReq->offsetData),
			(int)((MHL_MSC_MSG ==
			       pReq->command) ? pReq->payload_u.msgData[1] : pReq->payload_u.
			      msgData[0])));
	SiiRegWrite(TX_PAGE_CBUS | 0x0013, pReq->offsetData);
	SiiRegWrite(TX_PAGE_CBUS | 0x0014, pReq->payload_u.msgData[0]);
	startbit = 0x00;
	switch (pReq->command) {
	case MHL_SET_INT:
		startbit = 0x01 << 3;
		break;
	case MHL_WRITE_STAT:
		startbit = 0x01 << 3;
		break;
	case MHL_READ_DEVCAP:
		startbit = 0x01 << 2;
		break;
	case MHL_GET_STATE:
	case MHL_GET_VENDOR_ID:
	case MHL_SET_HPD:
	case MHL_CLR_HPD:
	case MHL_GET_SC1_ERRORCODE:
	case MHL_GET_DDC_ERRORCODE:
	case MHL_GET_MSC_ERRORCODE:
	case MHL_GET_SC3_ERRORCODE:
		SiiRegWrite(TX_PAGE_CBUS | 0x0013, pReq->command);
		startbit = 0x01 << 0;
		break;
	case MHL_MSC_MSG:
		SiiRegWrite(TX_PAGE_CBUS | 0x0015, pReq->payload_u.msgData[1]);
		SiiRegWrite(TX_PAGE_CBUS | 0x0013, pReq->command);
		startbit = 0x01 << 1;
		break;
	case MHL_WRITE_BURST:
		SiiRegWrite(TX_PAGE_CBUS | 0x0020, pReq->length - 1);
		uint8_t *pData = pReq->payload_u.msgData;
		TX_DEBUG_PRINT(("\nWriting data into scratchpad\n\n"));
		for (i = 0; i < pReq->length; i++) {
			SiiRegWrite(REG_CBUS_SCRATCHPAD_0 + i, *pData++);
		}
		startbit = MSC_START_BIT_WRITE_BURST;
		break;
	default:
		success = false;
		break;
	}
	if (success) {
		SiiRegWrite(TX_PAGE_CBUS | 0x0012, startbit);
	} else {
		TX_DEBUG_PRINT(("\nSiiMhlTxDrvSendCbusCommand failed\n\n"));
	}
	return (success);
}

bool_t SiiMhlTxDrvCBusBusy(void)
{
	return mscCmdInProgress ? true : false;
}

static void WriteInitialRegisterValues(void)
{
	TX_DEBUG_PRINT(("WriteInitialRegisterValues\n"));
	SiiRegWrite(TX_PAGE_L1 | 0x003D, 0x3F);
	SiiRegWrite(TX_PAGE_3 | 0x0035, 0xBC);
	SiiRegWrite(TX_PAGE_3 | 0x0031, 0x3C);
	SiiRegWrite(TX_PAGE_3 | 0x0033, 0xC8);
	SiiRegWrite(TX_PAGE_3 | 0x0036, 0x03);
	SiiRegWrite(TX_PAGE_3 | 0x0037, 0x0A);
	SiiRegWrite(TX_PAGE_L0 | 0x0080, 0x08);
	SiiRegWrite(TX_PAGE_L0 | 0x00F7, 0x03);
	SiiRegWrite(TX_PAGE_L0 | 0x00F8, 0x8C);
#ifdef MHL_SET_24PIN_MODE
	SiiRegWrite(TX_PAGE_L0 | 0x0008, 0x35);
#else
	SiiRegWrite(TX_PAGE_L0 | 0x0008, 0x31);
#endif
	SiiRegWrite(TX_PAGE_3 | 0x0014, 0x57);
	SiiRegWrite(TX_PAGE_3 | 0x0018, 0x04);	/* //0x24 -->0x4 for some smartbook will disconnect. */
	SiiRegWrite(TX_PAGE_3 | 0x0010, 0x27);
	SiiRegWrite(TX_PAGE_3 | 0x0012, 0x86);
	CbusReset();
	InitCBusRegs();
	SiiRegModify(TX_PAGE_L0 | 0x00C7, BIT5, SET_BITS);
	SiiRegModify(TX_PAGE_L1 | 0x2F, BIT2, SET_BITS);
}

static void InitCBusRegs(void)
{
	SiiRegWrite(TX_PAGE_CBUS | 0x0007, 0xF2);
	SiiRegWrite(TX_PAGE_CBUS | 0x0030, 0x01);
	SiiRegWrite(TX_PAGE_CBUS | 0x0031, 0x2D);
	SiiRegWrite(TX_PAGE_CBUS | 0x0036, 0x0a);
	SiiRegWrite(TX_PAGE_CBUS | 0x0040, 0x03);
	SiiRegWrite(TX_PAGE_CBUS | 0x002E, 0x11);
	SiiRegWrite(TX_PAGE_CBUS | 0x0022, 0x0F);
#define DEVCAP_REG(x) (TX_PAGE_CBUS | 0x0080 | DEVCAP_OFFSET_##x)
	SiiRegWrite(DEVCAP_REG(DEV_STATE), DEVCAP_VAL_DEV_STATE);
	SiiRegWrite(DEVCAP_REG(MHL_VERSION), DEVCAP_VAL_MHL_VERSION);
	SiiRegWrite(DEVCAP_REG(DEV_CAT), DEVCAP_VAL_DEV_CAT);
	SiiRegWrite(DEVCAP_REG(ADOPTER_ID_H), DEVCAP_VAL_ADOPTER_ID_H);
	SiiRegWrite(DEVCAP_REG(ADOPTER_ID_L), DEVCAP_VAL_ADOPTER_ID_L);
	SiiRegWrite(DEVCAP_REG(VID_LINK_MODE), DEVCAP_VAL_VID_LINK_MODE);
	SiiRegWrite(DEVCAP_REG(AUD_LINK_MODE), DEVCAP_VAL_AUD_LINK_MODE);
	SiiRegWrite(DEVCAP_REG(VIDEO_TYPE), DEVCAP_VAL_VIDEO_TYPE);
	SiiRegWrite(DEVCAP_REG(LOG_DEV_MAP), DEVCAP_VAL_LOG_DEV_MAP);
	SiiRegWrite(DEVCAP_REG(BANDWIDTH), DEVCAP_VAL_BANDWIDTH);
	SiiRegWrite(DEVCAP_REG(FEATURE_FLAG), DEVCAP_VAL_FEATURE_FLAG);
	SiiRegWrite(DEVCAP_REG(DEVICE_ID_H), DEVCAP_VAL_DEVICE_ID_H);
	SiiRegWrite(DEVCAP_REG(DEVICE_ID_L), DEVCAP_VAL_DEVICE_ID_L);
	SiiRegWrite(DEVCAP_REG(SCRATCHPAD_SIZE), DEVCAP_VAL_SCRATCHPAD_SIZE);
	SiiRegWrite(DEVCAP_REG(INT_STAT_SIZE), DEVCAP_VAL_INT_STAT_SIZE);
	SiiRegWrite(DEVCAP_REG(RESERVED), DEVCAP_VAL_RESERVED);
}

static void ForceUsbIdSwitchOpen(void)
{
	DISABLE_DISCOVERY;
	SiiRegModify(TX_PAGE_3 | 0x0015, BIT6, BIT6);
	SiiRegWrite(TX_PAGE_3 | 0x0012, 0x86);
}

static void ReleaseUsbIdSwitchOpen(void)
{
	HalTimerWait(15);
	SiiRegModify(TX_PAGE_3 | 0x0015, BIT6, 0x00);
	ENABLE_DISCOVERY;
}

void SiiMhlTxDrvProcessRgndMhl(void)
{
	SiiRegModify(TX_PAGE_3 | 0x0018, BIT0, BIT0);
}

void ProcessRgnd(void)
{
	uint8_t rgndImpedance;
	rgndImpedance = SiiRegRead(TX_PAGE_3 | 0x001C) & 0x03;
	TX_DEBUG_PRINT(("RGND = %02X :\n", (int)rgndImpedance));
	if (0x02 == rgndImpedance) {
		TX_DEBUG_PRINT(("(MHL Device)\n"));
		SiiMhlTxNotifyRgndMhl();
	} else {
		SiiRegModify(TX_PAGE_3 | 0x0018, BIT3, BIT3);
		TX_DEBUG_PRINT(("(Non-MHL Device)\n"));
	}
}

void SwitchToD0(void)
{
	TX_DEBUG_PRINT(("Switch to D0\n"));
	WriteInitialRegisterValues();
	STROBE_POWER_ON;
	fwPowerState = POWER_STATE_D0_NO_MHL;
	AudioVideoIsr(true);
}


extern void CBusQueueReset(void);
void SwitchToD3(void)
{
	if (POWER_STATE_D3 != fwPowerState) {
		TX_DEBUG_PRINT(("Switch To D3\n"));
		SiiRegWrite(TX_PAGE_2 | 0x0001, 0x03);
		SiiRegWrite(TX_PAGE_3 | 0x0030, 0xD0);


		SiiRegWrite(TX_PAGE_L0 | 0x0071, 0xFF);
		SiiRegWrite(TX_PAGE_L0 | 0x0072, 0xFF);
		SiiRegWrite(TX_PAGE_3 | 0x0021, 0xBF);
		SiiRegWrite(TX_PAGE_3 | 0x0023, 0xFF);
		SiiRegWrite(TX_PAGE_CBUS | 0x0008, 0xFF);
		SiiRegWrite(TX_PAGE_CBUS | 0x001E, 0xFF);


		ForceUsbIdSwitchOpen();
		/* HalTimerWait(50); */
		ReleaseUsbIdSwitchOpen();


		CLR_BIT(TX_PAGE_L1 | 0x003D, 0);
		CBusQueueReset();
#ifdef MDT_SUPPORT		/* MDT initialization. */
		/* SiiRegWrite(TX_PAGE_CBUS | 0x00F0, */
		/* MHL_INT_REQ_WRT | MHL_INT_DCAP_CHG);                  //       handle DSCR_CHG as part of REQ_WRT */

		g_state_for_mdt = IDLE;
		g_prior_msc_request.offset = 0;
		g_prior_msc_request.first_data = 0;
#endif
		fwPowerState = POWER_STATE_D3;
	}
}

void ForceSwitchToD3(void)
{
#ifdef MDT_SUPPORT		/* MDT initialization. */
	/* SiiRegWrite(TX_PAGE_CBUS | 0x00F0, */
	/* MHL_INT_REQ_WRT | MHL_INT_DCAP_CHG);                    //       handle DSCR_CHG as part of REQ_WRT */

	g_state_for_mdt = IDLE;
	g_prior_msc_request.offset = 0;
	g_prior_msc_request.first_data = 0;
#endif

	CLR_BIT(TX_PAGE_L1 | 0x003D, 0);
	CBusQueueReset();
	fwPowerState = POWER_STATE_D3;
}

static int Int4Isr(void)
{
	uint8_t int4Status;
	int4Status = SiiRegRead(TX_PAGE_3 | 0x0021);
	if (!int4Status) {
	} else if (0xFF == int4Status) {
		return I2C_INACCESSIBLE;
	} else {
		TX_DEBUG_PRINT(("INT4 Status = %02X\n", (int)int4Status));
		if (int4Status & BIT0) {
			ProcessScdtStatusChange();
		}
		if (int4Status & BIT2) {
			MhlTxDrvProcessConnection();
		} else if (int4Status & BIT3) {
			TX_DEBUG_PRINT(("uUSB-A type device detected.\n"));
			SiiRegWrite(TX_PAGE_3 | 0x001C, 0x80);
			SwitchToD3();
			return I2C_INACCESSIBLE;
		}
		if (int4Status & BIT5) {
			MhlTxDrvProcessDisconnection();
			return I2C_INACCESSIBLE;
		}
		if ((POWER_STATE_D0_MHL != fwPowerState) && (int4Status & BIT6)) {
			SwitchToD0();
			ProcessRgnd();
		}
		if (fwPowerState != POWER_STATE_D3) {
			if (int4Status & BIT4) {
				TX_DEBUG_PRINT(("CBus Lockout\n"));
				ForceUsbIdSwitchOpen();
				ReleaseUsbIdSwitchOpen();
			}
		}
	}
	SiiRegWrite(TX_PAGE_3 | 0x0021, int4Status);
	return I2C_ACCESSIBLE;
}

static void Int5Isr(void)
{
	uint8_t int5Status;
	int5Status = SiiRegRead(TX_PAGE_3 | 0x0023);
	if (int5Status) {
#if 0				/* (SYSTEM_BOARD == SB_STARTER_KIT_X01) */
		if ((int5Status & BIT3) || (int5Status & BIT2)) {
			TX_DEBUG_PRINT(("** Apply MHL FIFO Reset\n"));
			SiiRegModify(TX_PAGE_3 | 0x0000, BIT4, SET_BITS);
			SiiRegModify(TX_PAGE_3 | 0x0000, BIT4, CLEAR_BITS);
		}
#endif
		if (int5Status & BIT4) {
			TX_DEBUG_PRINT(("** PXL Format changed\n"));
		}
		SiiRegWrite(TX_PAGE_3 | 0x0023, int5Status);
	}
}

static void MhlTxDrvProcessConnection(void)
{
	TX_DEBUG_PRINT(("MHL Cable Connected. CBUS:0x0A = %02X\n",
			(int)SiiRegRead(TX_PAGE_CBUS | 0x000A)));
	if (POWER_STATE_D0_MHL == fwPowerState) {
		return;
	}
	SiiRegWrite(TX_PAGE_3 | 0x0030, 0x10);
	fwPowerState = POWER_STATE_D0_MHL;

/* change TMDS termination to 50 ohm termination(default) */
/* bits 1:0 set to 00 */
	SiiRegWrite(TX_PAGE_2 | 0x0001, 0x00);

	ENABLE_DISCOVERY;
	SiiMhlTxNotifyConnection(true);
}

static void MhlTxDrvProcessDisconnection(void)
{
	TX_DEBUG_PRINT(("MhlTxDrvProcessDisconnection\n"));
	SiiRegWrite(TX_PAGE_3 | 0x0021, SiiRegRead(TX_PAGE_3 | 0x0021));
	dsHpdStatus &= ~BIT6;
	SiiRegWrite(TX_PAGE_CBUS | 0x000D, dsHpdStatus);
	SiiMhlTxNotifyDsHpdChange(0);
	if (POWER_STATE_D0_MHL == fwPowerState) {
		SiiMhlTxNotifyConnection(false);
	}
	SwitchToD3();
}

void CbusReset(void)
{
	uint8_t idx;
	TX_DEBUG_PRINT(("CBUS reset!!!\n"));
	SET_BIT(TX_PAGE_3 | 0x0000, 3);
	HalTimerWait(2);
	CLR_BIT(TX_PAGE_3 | 0x0000, 3);
	mscCmdInProgress = false;
	UNMASK_INTR_4_INTERRUPTS;
#if (SYSTEM_BOARD == SB_STARTER_KIT_X01)
	UNMASK_INTR_1_INTERRUPTS;
	UNMASK_INTR_2_INTERRUPTS;
	UNMASK_INTR_5_INTERRUPTS;
#else
	MASK_INTR_1_INTERRUPTS;
	MASK_INTR_5_INTERRUPTS;
#endif
	UNMASK_CBUS1_INTERRUPTS;
	UNMASK_CBUS2_INTERRUPTS;
	for (idx = 0; idx < 4; idx++) {
		WriteByteCBUS((0xE0 + idx), 0xFF);
		WriteByteCBUS((0xF0 + idx), 0xFF);
	}
#ifdef MDT_SUPPORT		/* MDT initialization. */
	SiiRegWrite(TX_PAGE_CBUS | 0x00F0, MHL_INT_REQ_WRT | MHL_INT_DCAP_CHG);	/* handle DSCR_CHG as part of REQ_WRT */

	g_state_for_mdt = IDLE;
	g_prior_msc_request.offset = 0;
	g_prior_msc_request.first_data = 0;
#endif
}

static uint8_t CBusProcessErrors(uint8_t intStatus)
{
	uint8_t result = 0;
	uint8_t abortReason = 0;

	intStatus &= (BIT6 | BIT5 | BIT2);
	if (intStatus) {
		if (intStatus & BIT2) {
			abortReason |= SiiRegRead(TX_PAGE_CBUS | 0x000B);
			TX_DEBUG_PRINT(("CBUS:: DDC ABORT happened. Clearing 0x0C\n"));
			SiiRegWrite(TX_PAGE_CBUS | 0x000B, 0xFF);
		}
		if (intStatus & BIT5) {
			abortReason |= SiiRegRead(TX_PAGE_CBUS | 0x000D);
			TX_DEBUG_PRINT(("CBUS:: MSC Requester ABORTED. Clearing 0x0D\n"));
			SiiRegWrite(TX_PAGE_CBUS | 0x000D, 0xFF);
		}
		if (intStatus & BIT6) {
			abortReason |= SiiRegRead(TX_PAGE_CBUS | 0x000E);
			TX_DEBUG_PRINT(("CBUS:: MSC Responder ABORT. Clearing 0x0E\n"));
			SiiRegWrite(TX_PAGE_CBUS | 0x000E, 0xFF);
		}
		if (abortReason & (BIT0 | BIT1 | BIT2 | BIT3 | BIT4 | BIT7)) {
			TX_DEBUG_PRINT(("CBUS:: Reason for ABORT is ....0x%02X\n",
					(int)abortReason));
			if (abortReason & (0x01 << 0)) {
				TX_DEBUG_PRINT(("Retry threshold exceeded\n"));
			}
			if (abortReason & (0x01 << 1)) {
				TX_DEBUG_PRINT(("Protocol Error\n"));
			}
			if (abortReason & (0x01 << 2)) {
				TX_DEBUG_PRINT(("Translation layer timeout\n"));
			}
			if (abortReason & (0x01 << 3)) {
				TX_DEBUG_PRINT(("Undefined opcode\n"));
			}
			if (abortReason & (0x01 << 4)) {
				TX_DEBUG_PRINT(("Undefined offset\n"));
			}
			if (abortReason & (0x01 << 5)) {
				TX_DEBUG_PRINT(("Opposite device is busy\n"));
			}
			if (abortReason & (0x01 << 7)) {
#ifndef __KERNEL__
				HalTimerSet(TIMER_ABORT, T_ABORT_NEXT);
				mscAbortFlag = true;
#else
				if (MscAbortTimer) {
					SiiOsTimerDelete(MscAbortTimer);
					MscAbortTimer = NULL;
				}
				mscAbortFlag = true;
				/* SiiOsTimerCreate("Abort Time Out", SiiMhlMscAbortTimerCB, NULL, true, */
				/* 2000, false, &MscAbortTimer); */
				delay_in_ms = 2000L;
				hr_timer_AbortTimer_CHK_ktime = ktime_set(0, MS_TO_NS(delay_in_ms));
				printk(KERN_INFO
				       "Starting timer to fire in hr_timer_AbortTimer_CHK  %ldms (%ld)\n",
				       delay_in_ms, jiffies);
				hrtimer_start(&hr_timer_AbortTimer_CHK,
					      hr_timer_AbortTimer_CHK_ktime, HRTIMER_MODE_REL);
#endif
				TX_DEBUG_PRINT(("Peer sent an abort, start 2s timer abort_next\n"));
			}
		}
	}
	return (result);
}

void SiiMhlTxDrvGetScratchPad(uint8_t startReg, uint8_t *pData, uint8_t length)
{
	int i;
	for (i = 0; i < length; ++i, ++startReg) {
		*pData++ = SiiRegRead((TX_PAGE_CBUS | 0x00C0) + startReg);
	}
}

static void MhlCbusIsr(void)
{
	uint8_t cbusInt;
	uint8_t gotData[4];
	uint8_t i;
	cbusInt = SiiRegRead(TX_PAGE_CBUS | 0x001E);
	if (cbusInt == 0xFF) {
		return;
	}
	if (cbusInt) {
#ifdef CONFIG_MTK_SMARTBOOK_SUPPORT
		if (BIT0 & cbusInt) {
			SiiMhlTxMscWriteBurstDone(cbusInt);
		}
#endif
		/* TX_DEBUG_PRINT(("g_state_for_mdt=%d^^^^^^^^^^^^^^^^^^^^\n",g_state_for_mdt)); */
#ifdef MDT_SUPPORT
		if ((g_state_for_mdt != WAIT_FOR_WRITE_BURST_COMPLETE)
		    && (g_state_for_mdt != WAIT_FOR_REQ_WRT)
		    && (g_state_for_mdt != WAIT_FOR_GRT_WRT_COMPLETE)) {

			if (BIT0 & cbusInt) {
				SiiMhlTxMscWriteBurstDone(cbusInt);

				g_state_for_mdt = WAIT_FOR_REQ_WRT;
				SiiRegWrite(TX_PAGE_CBUS | 0x00A0,
					    (MHL_INT_REQ_WRT | MHL_INT_DSCR_CHG));
				/* TX_DEBUG_PRINT(("g_state_for_mdt=%d$$$$$$$$$$$$$$$$$$$$$$$$\n",g_state_for_mdt)); */

			}
#endif				/* MTK_SMARTBOOK_SUPPORt */
			/* } */
			if (cbusInt & BIT2) {
				uint8_t intr[4] = { 0 };
				TX_DEBUG_PRINT(("MHL INTR Received\n"));
				SiiRegReadBlock(TX_PAGE_CBUS | 0x00A0, intr, 4);
				SiiMhlTxGotMhlIntr(intr[0], intr[1]);
				SiiRegWriteBlock(TX_PAGE_CBUS | 0x00A0, intr, 4);
			}
#ifdef MDT_SUPPORT
		}
#endif
		if (cbusInt & BIT3) {
			uint8_t status[4] = { 0 };
			TX_DEBUG_PRINT(("MHL STATUS Received\n"));
			for (i = 0; i < 4; ++i) {
				status[i] = SiiRegRead((TX_PAGE_CBUS | 0x00B0) + i);
				SiiRegWrite(((TX_PAGE_CBUS | 0x00B0) + i), 0xFF);
			}
			SiiMhlTxGotMhlStatus(status[0], status[1]);
		}
#ifdef MDT_SUPPORT
		if ((g_state_for_mdt == WAIT_FOR_WRITE_BURST_COMPLETE)
		    || (g_state_for_mdt == WAIT_FOR_REQ_WRT)
		    || (g_state_for_mdt == WAIT_FOR_GRT_WRT_COMPLETE))
		{
			SiiRegWrite(TX_PAGE_CBUS | 0x001E, (cbusInt & 0xFE));
			/* /TX_DEBUG_PRINT(("Drv: Clear CBUS INTR_2: %02X,g_state_for_mdt=%d\n", (int) (cbusInt&0xFE),g_state_for_mdt)); */
		} else
#endif
		{
			SiiRegWrite(TX_PAGE_CBUS | 0x001E, cbusInt);
			TX_DEBUG_PRINT(("Drv: Clear CBUS INTR_2: %02X\n", (int)cbusInt));
		}
	}
	cbusInt = SiiRegRead(TX_PAGE_CBUS | 0x0008);
	if (cbusInt) {
#ifdef MDT_SUPPORT
		if ((g_state_for_mdt == WAIT_FOR_WRITE_BURST_COMPLETE)
		    || (g_state_for_mdt == WAIT_FOR_REQ_WRT)
		    || (g_state_for_mdt == WAIT_FOR_GRT_WRT_COMPLETE)) {
			SiiRegWrite(TX_PAGE_CBUS | 0x0008, (cbusInt & 0xEF));
			/* /TX_DEBUG_PRINT(("Drv: Clear CBUS INTR_1: %02X,g_state_for_mdt=%d\n", (int) (cbusInt&0xEF),g_state_for_mdt)); */
		} else
#endif
		{
			SiiRegWrite(TX_PAGE_CBUS | 0x0008, cbusInt);
			/* /TX_DEBUG_PRINT(("Drv: Clear CBUS INTR_1: %02X,g_state_for_mdt=%d\n", (int) cbusInt,g_state_for_mdt)); */
		}
	}
	if ((cbusInt & BIT3)) {
		uint8_t mscMsg[2];
		TX_DEBUG_PRINT(("MSC_MSG Received\n"));
		mscMsg[0] = SiiRegRead(TX_PAGE_CBUS | 0x0018);
		mscMsg[1] = SiiRegRead(TX_PAGE_CBUS | 0x0019);
		TX_DEBUG_PRINT(("MSC MSG: %02X %02X\n", (int)mscMsg[0], (int)mscMsg[1]));
		SiiMhlTxGotMhlMscMsg(mscMsg[0], mscMsg[1]);
	}
	if (cbusInt & (BIT6 | BIT5 | BIT2)) {
		gotData[0] = CBusProcessErrors(cbusInt);
		mscCmdInProgress = false;
	}
#ifdef MDT_SUPPORT
	if ((g_state_for_mdt != WAIT_FOR_WRITE_BURST_COMPLETE)
	    && (g_state_for_mdt != WAIT_FOR_REQ_WRT)
	    && (g_state_for_mdt != WAIT_FOR_GRT_WRT_COMPLETE))
#endif
	{
		if (cbusInt & BIT4) {
			TX_DEBUG_PRINT(("MSC_REQ_DONE\n"));
			mscCmdInProgress = false;
			SiiMhlTxMscCommandDone(SiiRegRead(TX_PAGE_CBUS | 0x0016));
		}
	}
	if (cbusInt & BIT7) {
		TX_DEBUG_PRINT(("Parity error count reaches 15\n"));
		SiiRegWrite(TX_PAGE_CBUS | 0x0038, 0x00);
	}
}

static void ProcessScdtStatusChange(void)
{
}

void SiiMhlTxDrvPowBitChange(bool_t enable)
{
	if (enable) {
		SiiRegModify(TX_PAGE_3 | 0x0017, BIT2, SET_BITS);
		TX_DEBUG_PRINT(("POW bit 0->1, set DISC_CTRL8[2] = 1\n"));
	}
}

void SiMhlTxDrvSetClkMode(uint8_t clkMode)
{
	TX_DEBUG_PRINT(("SiMhlTxDrvSetClkMode:0x%02x\n", (int)clkMode));
}

static void SendAviInfoframe(void)
{
	uint8_t ifData[SIZE_AVI_INFOFRAME];
	extern uint8_t VIDEO_CAPABILITY_D_BLOCK_found;
	ifData[0] = 0x82;
	ifData[1] = 0x02;
	ifData[2] = 0x0D;
	ifData[3] = 0x00;
	ifData[4] = video_data.outputColorSpace << 5;
	ifData[5] = video_data.outputcolorimetryAspectRatio;

	if (VIDEO_CAPABILITY_D_BLOCK_found) {
		ifData[6] = 0x04;
		TX_DEBUG_PRINT(("VIDEO_CAPABILITY_D_BLOCK_found = true, limited range\n"));
	} else {
		ifData[6] = 0x00;
		TX_DEBUG_PRINT(("VIDEO_CAPABILITY_D_BLOCK_found= false. defult range\n"));
	}
	/* ifData[4] = video_data.outputColorSpace << 5; */
	ifData[7] = video_data.inputVideoCode;
	TX_DEBUG_PRINT(("video_data.inputVideoCode:0x%02x, video_data.outputVideoCode=0x%x\n",
			(int)video_data.inputVideoCode, video_data.outputVideoCode));

	/* ifData[7] = video_data.outputVideoCode; */
	ifData[8] = 0x00;
	ifData[9] = 0x00;
	ifData[10] = 0x00;
	ifData[11] = 0x00;
	ifData[12] = 0x00;
	ifData[13] = 0x00;
	ifData[14] = 0x00;
	ifData[15] = 0x00;
	ifData[16] = 0x00;
	ifData[3] = CalculateInfoFrameChecksum(ifData);
	SiiRegModify(TX_PAGE_L1 | 0x3E, BIT0 | BIT1, CLEAR_BITS);
	SiiRegWriteBlock(TX_PAGE_L1 | 0x0040, ifData, SIZE_AVI_INFOFRAME);
	SiiRegModify(TX_PAGE_L1 | 0x3E, BIT0 | BIT1, SET_BITS);
}

static uint8_t CalculateInfoFrameChecksum(uint8_t *infoFrameData)
{
	uint8_t i, checksum;
	checksum = 0x00;
	for (i = 0; i < SIZE_AVI_INFOFRAME; i++) {
		checksum += infoFrameData[i];
	}
	checksum = 0x100 - checksum;
	return checksum;
}

PLACE_IN_CODE_SEG const audioConfig_t audioData[AUD_TYP_NUM] = {
	{0x11, 0x40, 0x0E, 0x03, 0x00, 0x05},
	{0x11, 0x40, 0x0A, 0x01, 0x00, 0x05},
	{0x11, 0x40, 0x02, 0x00, 0x00, 0x05},
	{0x11, 0x40, 0x0C, 0x03, 0x00, 0x05},
	{0x11, 0x40, 0x08, 0x01, 0x00, 0x05},
	{0x11, 0x40, 0x00, 0x00, 0x00, 0x05},
	{0x11, 0x40, 0x03, 0x00, 0x00, 0x05},
	{0x11, 0x40, 0x0E, 0x03, 0x03, 0x05},
	{0x11, 0x40, 0x0A, 0x01, 0x03, 0x05},
	{0x11, 0x40, 0x02, 0x00, 0x03, 0x05},
	{0x11, 0x40, 0x0C, 0x03, 0x03, 0x05},
	{0x11, 0x40, 0x08, 0x01, 0x03, 0x05},
	{0x11, 0x40, 0x00, 0x00, 0x03, 0x05},
	{0x11, 0x40, 0x03, 0x00, 0x03, 0x05},
	{0xF1, 0x40, 0x0E, 0x00, 0x03, 0x07},
	{0x03, 0x00, 0x00, 0x00, 0x00, 0x05}
};

static void SetAudioMode(inAudioTypes_t audiomode)
{
	if (audiomode >= AUD_TYP_NUM)
		audiomode = I2S_48;
	SiiRegWrite(TX_PAGE_L1 | 0x2F, audioData[audiomode].regAUD_path);
	SiiRegWrite(TX_PAGE_L1 | 0x14, audioData[audiomode].regAUD_mode);
	SiiRegWrite(TX_PAGE_L1 | 0x1D, audioData[audiomode].regAUD_ctrl);
	SiiRegWrite(TX_PAGE_L1 | 0x21, audioData[audiomode].regAUD_freq);
	SiiRegWrite(TX_PAGE_L1 | 0x23, audioData[audiomode].regAUD_src);
	SiiRegWrite(TX_PAGE_L1 | 0x28, audioData[audiomode].regAUD_tdm_ctrl);
/* SiiRegWrite(TX_PAGE_L1 | 0x22, 0x0B); */
/* 0x02 for word length=16bits */
	SiiRegWrite(TX_PAGE_L1 | 0x22, 0x02);
	SiiRegWrite(TX_PAGE_L1 | 0x24, 0x02);

	SiiRegWrite(TX_PAGE_L1 | 0x15, 0x00);
	/* 0x7A:0x24 = 0x0B for word lenth is defult 24bit */

	TX_DEBUG_PRINT(("SiiRegRead(TX_PAGE_L1 | 0x21)=0x%x\n", SiiRegRead(TX_PAGE_L1 | 0x21)));
	TX_DEBUG_PRINT(("SiiRegRead(TX_PAGE_L1 | 0x1D)=0x%x\n", SiiRegRead(TX_PAGE_L1 | 0x1D)));
}

static void SetACRNValue(void)
{
	uint8_t audioFs;
	if ((SiiRegRead(TX_PAGE_L1 | 0x14) & BIT1) && !(SiiRegRead(TX_PAGE_L1 | 0x15) & BIT1))
		audioFs = SiiRegRead(TX_PAGE_L1 | 0x18) & 0x0F;
	else
		audioFs = SiiRegRead(TX_PAGE_L1 | 0x21) & 0x0F;
	switch (audioFs) {
	case 0x03:
		SiiRegWrite(TX_PAGE_L1 | 0x05, (uint8_t) (ACR_N_value_32k >> 16));
		SiiRegWrite(TX_PAGE_L1 | 0x04, (uint8_t) (ACR_N_value_32k >> 8));
		SiiRegWrite(TX_PAGE_L1 | 0x03, (uint8_t) (ACR_N_value_32k));
		break;
	case 0x00:
		SiiRegWrite(TX_PAGE_L1 | 0x05, (uint8_t) (ACR_N_value_44k >> 16));
		SiiRegWrite(TX_PAGE_L1 | 0x04, (uint8_t) (ACR_N_value_44k >> 8));
		SiiRegWrite(TX_PAGE_L1 | 0x03, (uint8_t) (ACR_N_value_44k));
		break;
	case 0x02:
		SiiRegWrite(TX_PAGE_L1 | 0x05, (uint8_t) (ACR_N_value_48k >> 16));
		SiiRegWrite(TX_PAGE_L1 | 0x04, (uint8_t) (ACR_N_value_48k >> 8));
		SiiRegWrite(TX_PAGE_L1 | 0x03, (uint8_t) (ACR_N_value_48k));
		break;
	case 0x08:
		SiiRegWrite(TX_PAGE_L1 | 0x05, (uint8_t) (ACR_N_value_88k >> 16));
		SiiRegWrite(TX_PAGE_L1 | 0x04, (uint8_t) (ACR_N_value_88k >> 8));
		SiiRegWrite(TX_PAGE_L1 | 0x03, (uint8_t) (ACR_N_value_88k));
		break;
	case 0x0A:
		SiiRegWrite(TX_PAGE_L1 | 0x05, (uint8_t) (ACR_N_value_96k >> 16));
		SiiRegWrite(TX_PAGE_L1 | 0x04, (uint8_t) (ACR_N_value_96k >> 8));
		SiiRegWrite(TX_PAGE_L1 | 0x03, (uint8_t) (ACR_N_value_96k));
		break;
	case 0x0C:
		SiiRegWrite(TX_PAGE_L1 | 0x05, (uint8_t) (ACR_N_value_176k >> 16));
		SiiRegWrite(TX_PAGE_L1 | 0x04, (uint8_t) (ACR_N_value_176k >> 8));
		SiiRegWrite(TX_PAGE_L1 | 0x03, (uint8_t) (ACR_N_value_176k));
		break;
	case 0x0E:
		SiiRegWrite(TX_PAGE_L1 | 0x05, (uint8_t) (ACR_N_value_192k >> 16));
		SiiRegWrite(TX_PAGE_L1 | 0x04, (uint8_t) (ACR_N_value_192k >> 8));
		SiiRegWrite(TX_PAGE_L1 | 0x03, (uint8_t) (ACR_N_value_192k));
		break;
	default:
		SiiRegWrite(TX_PAGE_L1 | 0x05, (uint8_t) (ACR_N_value_default >> 16));
		SiiRegWrite(TX_PAGE_L1 | 0x04, (uint8_t) (ACR_N_value_default >> 8));
		SiiRegWrite(TX_PAGE_L1 | 0x03, (uint8_t) (ACR_N_value_default));
		break;
	}
	SiiRegModify(TX_PAGE_L1 | 0x2F, BIT2, CLEAR_BITS);
}

static void SendAudioInfoFrame(void)
{
	SiiRegModify(TX_PAGE_L1 | 0x3E, BIT4 | BIT5, CLEAR_BITS);
	SiiRegWrite(TX_PAGE_L1 | 0x80, 0x84);
	SiiRegWrite(TX_PAGE_L1 | 0x81, 0x01);
	SiiRegWrite(TX_PAGE_L1 | 0x82, 0x0A);
	SiiRegWrite(TX_PAGE_L1 | 0x83, 0x70);
	SiiRegWrite(TX_PAGE_L1 | 0x84, 0x01);
	SiiRegWrite(TX_PAGE_L1 | 0x8D, 0x00);
	SiiRegModify(TX_PAGE_L1 | 0x3E, BIT4 | BIT5, SET_BITS);
}

static void AudioVideoIsr(bool_t force_update)
{
	AVModeChange_t mode_change = { false, false };
	/* static AVMode_t mode = {VM_INVALID, AUD_INVALID}; */
	if (force_update) {
		if (1)		/* (SiiVideoInputIsValid()) */
		{
			TX_DEBUG_PRINT(("SiiVideoInputIsValid,audio_changed,video_changed\n"));
			mode_change.audio_change = true;
			mode_change.video_change = true;
		}
	} else {
		TX_DEBUG_PRINT(("force_update=false...............\n"));
		/* AVModeDetect(&mode_change, &AVmode); */
	}
	if (mode_change.audio_change) {
		TX_DEBUG_PRINT(("SetAudioMode & SetACRNValue\n"));
		/* SetAudioMode(mode.audio_mode); */
		SetAudioMode(AVmode.audio_mode);
		SetACRNValue();
	}
	if (mode_change.video_change)	/* && SiiVideoInputIsValid()) */
	{
		TX_DEBUG_PRINT(("mode_change.video_changed =true\n "));
		SiiRegModify(TX_PAGE_L0 | 0x00C7, BIT5, SET_BITS);
		SiiRegModify(TX_PAGE_3 | 0x0000, BIT0, SET_BITS);
		/* video_data.outputColorSpace = video_data.inputColorSpace; */
		video_data.outputVideoCode = video_data.inputVideoCode;
		video_data.outputcolorimetryAspectRatio = video_data.inputcolorimetryAspectRatio;
		SiiRegModify(TX_PAGE_3 | 0x0000, BIT0, CLEAR_BITS);
		SiiRegModify(TX_PAGE_L0 | 0x00C7, BIT5, CLEAR_BITS);
	}
	if ((mode_change.video_change || mode_change.audio_change) && tmdsPowRdy) {
		if (1)		/* (SiiVideoInputIsValid()) */
		{
			SiiRegModify(TX_PAGE_L0 | 0x0080, BIT4, SET_BITS);
			SendAudioInfoFrame();
			TX_DEBUG_PRINT(("((mode_change.video_change || mode_change.audio_change) && tmdsPowRdy)\n"));
			SendAviInfoframe();
		} else {
			SiiRegModify(TX_PAGE_L0 | 0x0080, BIT4, CLEAR_BITS);
			TX_DEBUG_PRINT(("TMDS Ouput Disabled due to invalid input\n"));
		}
	}
}

#if 0				/* /!defined GPIO_MHL_RST_B_PIN */
#error GPIO_MHL_RST_B_PIN no defined
#endif

void SiiMhlTxHwResetKeepLow(void)
{
	printk("%s,%d\n", __func__, __LINE__);
#ifdef GPIO_MHL_RST_B_PIN
	mt_set_gpio_out(GPIO_MHL_RST_B_PIN, GPIO_OUT_ZERO);
#else
	printk("%s,%d Error: GPIO_MHL_RST_B_PIN is not defined\n", __func__, __LINE__);
#endif
}

void SiiMhlTxHwReset(uint16_t hwResetPeriod, uint16_t hwResetDelay)
{
#ifdef GPIO_MHL_RST_B_PIN
	printk("%s,%d\n", __func__, __LINE__);
	mt_set_gpio_out(GPIO_MHL_RST_B_PIN, GPIO_OUT_ONE);
	msleep(hwResetPeriod);
	mt_set_gpio_out(GPIO_MHL_RST_B_PIN, GPIO_OUT_ZERO);
	msleep(hwResetPeriod);
	mt_set_gpio_out(GPIO_MHL_RST_B_PIN, GPIO_OUT_ONE);
	msleep(hwResetDelay);
#else
	printk("%s,%d Error: GPIO_MHL_RST_B_PIN is not defined\n", __func__, __LINE__);
#endif
}

#if 0
/* mt6577 */
void SiiMhlTxHwGpioSuspend(void)
{
	int i;
	u32 gpio[] = {
		GPIO19, GPIO20, GPIO21, GPIO22, GPIO23,
		GPIO24, GPIO25, GPIO26, GPIO27, GPIO28,
		GPIO29, GPIO30, GPIO31, GPIO32, GPIO33,
		GPIO34, GPIO35, GPIO36, GPIO37, GPIO38,
		GPIO39, GPIO40, GPIO41, GPIO42, GPIO43,
		GPIO44, GPIO45, GPIO46, GPIO53, GPIO54,
		GPIO55,
	};

	printk("%s,%d\n", __func__, __LINE__);

	for (i = 0; i < ARRAY_SIZE(gpio); i++) {
		mt_set_gpio_mode(gpio[i], GPIO_MODE_00);
		mt_set_gpio_dir(gpio[i], GPIO_DIR_IN);
		mt_set_gpio_pull_select(gpio[i], GPIO_PULL_DOWN);
		mt_set_gpio_pull_enable(gpio[i], GPIO_PULL_ENABLE);
	}
}
#else
void SiiMhlTxHwGpioSuspend(void)
{
#ifdef MHL_SET_GPIO_MODE
	int i;
	u32 gpio[] = {
		GPIO143, GPIO144, GPIO145, GPIO146, GPIO147,
		GPIO148, GPIO149, GPIO150, GPIO151, GPIO152,
		GPIO153, GPIO154, GPIO155, GPIO156, GPIO157,
		GPIO158, GPIO159, GPIO160, GPIO161, GPIO162,
		GPIO163, GPIO164, GPIO165, GPIO166, GPIO167,
		GPIO168, GPIO169, GPIO170, GPIO120, GPIO121,
		GPIO122,
	};
	printk("%s,%d\n", __func__, __LINE__);
	for (i = 0; i < ARRAY_SIZE(gpio); i++) {
		mt_set_gpio_mode(gpio[i], GPIO_MODE_00);
		mt_set_gpio_dir(gpio[i], GPIO_DIR_IN);
		mt_set_gpio_pull_select(gpio[i], GPIO_PULL_DOWN);
		mt_set_gpio_pull_enable(gpio[i], GPIO_PULL_ENABLE);
	}
#endif
}
#endif

#if 0
/* mt6577 */
void SiiMhlTxHwGpioResume(void)
{
	int i;
	u32 gpio_rgb[] = {
		GPIO19, GPIO20, GPIO21, GPIO22, GPIO23,
		GPIO24, GPIO25, GPIO26, GPIO27, GPIO28,
		GPIO29, GPIO30, GPIO31, GPIO32, GPIO33,
		GPIO34, GPIO35, GPIO36, GPIO37, GPIO38,
		GPIO39, GPIO40, GPIO41, GPIO42, GPIO43,
		GPIO44, GPIO45, GPIO46
	};

	u32 gpio_i2s[] = { GPIO53, GPIO54, GPIO55 };

	printk("%s,%d\n", __func__, __LINE__);

	for (i = 0; i < ARRAY_SIZE(gpio_rgb); i++) {
		mt_set_gpio_mode(gpio_rgb[i], GPIO_MODE_01);
		mt_set_gpio_pull_enable(gpio_rgb[i], GPIO_PULL_DISABLE);
	}

	for (i = 0; i < ARRAY_SIZE(gpio_i2s); i++) {
		mt_set_gpio_mode(gpio_i2s[i], GPIO_MODE_04);
		mt_set_gpio_pull_select(gpio_i2s[i], GPIO_PULL_DOWN);
		mt_set_gpio_pull_enable(gpio_i2s[i], GPIO_PULL_ENABLE);
	}
}
#else
void SiiMhlTxHwGpioResume(void)
{
#ifdef MHL_SET_GPIO_MODE
	int i;
	u32 gpio_rgb[] = {
		GPIO143, GPIO144, GPIO145, GPIO146, GPIO147,
		GPIO148, GPIO149, GPIO150, GPIO151, GPIO152,
		GPIO153, GPIO154, GPIO155, GPIO156, GPIO157,
		GPIO158, GPIO159, GPIO160, GPIO161, GPIO162,
		GPIO163, GPIO164, GPIO165, GPIO166, GPIO167,
		GPIO168, GPIO169, GPIO170
	};
	u32 gpio_i2s[] = { GPIO120, GPIO121, GPIO122 };
	printk("%s,%d\n", __func__, __LINE__);
	for (i = 0; i < ARRAY_SIZE(gpio_rgb); i++) {
		mt_set_gpio_mode(gpio_rgb[i], GPIO_MODE_01);
		mt_set_gpio_pull_enable(gpio_rgb[i], GPIO_PULL_DISABLE);
	}

	for (i = 0; i < ARRAY_SIZE(gpio_i2s); i++) {
		mt_set_gpio_mode(gpio_i2s[i], GPIO_MODE_01);
		mt_set_gpio_pull_select(gpio_i2s[i], GPIO_PULL_DOWN);
		mt_set_gpio_pull_enable(gpio_i2s[i], GPIO_PULL_ENABLE);
	}
#endif
}
#endif


#if defined(USE_PROC) && defined(__KERNEL__)
void drv_mhl_seq_show(struct seq_file *s)
{
	int gpio_value;
	switch (fwPowerState) {
	case POWER_STATE_D3:
		seq_puts(s, "MHL POWER STATE          [D3]\n");
		break;
	case POWER_STATE_D0_NO_MHL:
		seq_puts(s, "MHL POWER STATE          [D0_NO_MHL]\n");
		break;
	case POWER_STATE_D0_MHL:
		seq_puts(s, "MHL POWER STATE          [D0_MHL]\n");
		break;
	case POWER_STATE_FIRST_INIT:
		seq_puts(s, "MHL POWER STATE          [FIRST_INIT]\n");
		break;
	default:
		break;
	}
	if (tmdsPowRdy)
		seq_puts(s, "TMDS                     [ON]\n");
	else
		seq_puts(s, "TMDS                     [OFF]\n");
	HalGpioGetPin(GPIO_SRC_VBUS_ON, &gpio_value);
	if (gpio_value)
		seq_puts(s, "SRC BUS                  [ON]\n");
	else
		seq_puts(s, "SRC BUS                  [OFF]\n");
	HalGpioGetPin(GPIO_SINK_VBUS_ON, &gpio_value);
	if (gpio_value)
		seq_puts(s, "SINK BUS                  [ON]\n");
	else
		seq_puts(s, "SINK BUS                  [OFF]\n");
}
#endif

/* ------------------------------------------------------------------------------ */
/* Function Name: siHdmiTx_VideoSel() */
/* Function Description: Select output video mode */
/*  */
/* Accepts: Video mode */
/* Returns: none */
/* Globals: none */
/* ------------------------------------------------------------------------------ */
void siHdmiTx_VideoSel(int vmode)
{
	int AspectRatio = 0;
	video_data.inputColorSpace = COLOR_SPACE_RGB;
	video_data.outputColorSpace = COLOR_SPACE_RGB;
	video_data.inputVideoCode = vmode;
	/* siHdmiTx.ColorDepth                   = VMD_COLOR_DEPTH_8BIT; */
	/* siHdmiTx.SyncMode                     = EXTERNAL_HSVSDE; */

	switch (vmode) {
	case HDMI_480I60_4X3:
	case HDMI_576I50_4X3:
		AspectRatio = VMD_ASPECT_RATIO_4x3;
		break;

	case HDMI_480I60_16X9:
	case HDMI_576I50_16X9:
		AspectRatio = VMD_ASPECT_RATIO_16x9;
		break;

	case HDMI_480P60_4X3:
	case HDMI_576P50_4X3:
	case HDMI_640X480P:
		AspectRatio = VMD_ASPECT_RATIO_4x3;
		break;

	case HDMI_480P60_16X9:
	case HDMI_576P50_16X9:
		AspectRatio = VMD_ASPECT_RATIO_16x9;
		break;

	case HDMI_720P60:
	case HDMI_720P50:
	case HDMI_1080I60:
	case HDMI_1080I50:
	case HDMI_1080P24:
	case HDMI_1080P25:
	case HDMI_1080P30:
		AspectRatio = VMD_ASPECT_RATIO_16x9;
		break;

	default:
		break;
	}
	if (AspectRatio == VMD_ASPECT_RATIO_4x3)
		video_data.inputcolorimetryAspectRatio = 0x18;
	else
		video_data.inputcolorimetryAspectRatio = 0x28;
	video_data.input_AR = AspectRatio;

}

/* ------------------------------------------------------------------------------ */
/* Function Name: siHdmiTx_AudioSel() */
/* Function Description: Select output audio mode */
/*  */
/* Accepts: Audio Fs */
/* Returns: none */
/* Globals: none */
/* ------------------------------------------------------------------------------ */
void siHdmiTx_AudioSel(int AduioMode)
{
	AVmode.audio_mode = AduioMode;
	/*
	   siHdmiTx.AudioChannels               = ACHANNEL_2CH;
	   siHdmiTx.AudioFs                             = Afs;
	   siHdmiTx.AudioWordLength             = ALENGTH_24BITS;
	   siHdmiTx.AudioI2SFormat              = (MCLK256FS << 4) |SCK_SAMPLE_RISING_EDGE |0x00; //last num 0x00-->0x02
	 */
}

/* ------------------------------------------------------------------------------ */
/* Function Name: siMhlTx_AudioSet() */
/* Function Description: Set the 9022/4 audio interface to basic audio. */
/*  */
/* Accepts: none */
/* Returns: Success message if audio changed successfully. */
/* Error Code if resolution change failed */
/* Globals: mhlTxAv */
/* ------------------------------------------------------------------------------ */
uint8_t siMhlTx_AudioSet(void)
{
	TX_DEBUG_PRINT(("[MHL]: >>siMhlTx_AudioSet()\n"));

	/* SetAudioMute(AUDIO_MUTE_MUTED);       // mute output */
	SetAudioMode(AVmode.audio_mode);
	SetACRNValue();
	return 0;
}

/* ------------------------------------------------------------------------------ */
/* Function Name: siMhlTx_VideoAudioSet() */
/* Function Description: Set the 9022/4 video resolution */
/*  */
/* Accepts: none */
/* Returns: Success message if video resolution changed successfully. */
/* Error Code if resolution change failed */
/* Globals: mhlTxAv */
/* ------------------------------------------------------------------------------ */
/* ============================================================ */
#define T_RES_CHANGE_DELAY      128	/* delay between turning TMDS bus off and changing output resolution */

uint8_t siMhlTx_VideoAudioSet(void)
{
	TX_DEBUG_PRINT(("[MHL]: >>siMhlTx_VideoAudioSet()\n"));

	SiiRegModify(TX_PAGE_L1 | 0xDF, BIT0, SET_BITS);

	SiiMhlTxDrvTmdsControl(false);
	HalTimerWait(T_RES_CHANGE_DELAY);	/* allow control InfoFrames to pass through to the sink device. */
	/* siMhlTx_AudioSet(); */
	AudioVideoIsr(true);
	/* siMhlTx_Init(); */
	SiiRegModify(TX_PAGE_L1 | 0xDF, BIT0, CLEAR_BITS);
	SiiMhlTxTmdsEnable();
	return 0;
}
