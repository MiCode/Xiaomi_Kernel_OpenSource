#ifdef CONFIG_MTK_MT8193_HDMI_SUPPORT

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/earlysuspend.h>
#include <linux/platform_device.h>
#include <asm/atomic.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/byteorder/generic.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/rtpm_prio.h>
#include <linux/dma-mapping.h>
#include <linux/syscalls.h>
#include <linux/reboot.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/completion.h>

#include "mt8193_ctrl.h"
#include "mt8193cec.h"

#include "hdmi_drv.h"
#include <cust_eint.h>
#include "cust_gpio_usage.h"
#include "mach/eint.h"
#include "mach/irqs.h"

#include <mach/devs.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>

#define ENABLE_CEC_IRQ()	(mt8193_write(0x1500, 0x20))
#define DISABLE_CEC_IRQ()	(mt8193_write(0x1500, 0x0))
#define CLR_CEC_IRQ()	    (mt8193_write(0x1504, 0x20))

static u16 _CEC_Status;

#define SetCECStatus(arg) (_CEC_Status |= (arg))
#define ClrCECStatus(arg) (_CEC_Status &= (~(arg)))
#define IsCECStatus(arg) ((_CEC_Status & (arg)) > 0)

static u16 _CEC_ErrStatus;
#define SetCECErrorFlag(arg) (_CEC_ErrStatus |= (arg))
#define ClrCECErrorFlag(arg) (_CEC_ErrStatus &= (~(arg)))
#define IsCECErrorFlag(arg) ((_CEC_ErrStatus & (arg)) > 0)


static CEC_FRAME_DESCRIPTION ActiveRXFrame;
static CEC_FRAME_DESCRIPTION CTSTestFrame;

u32 mt8193_cec_2n(u32 u4Data);
u32 mt8193_cec_maskvalue(u32 u4Width, u32 u4Startbit);

u32 mt8193_cec_read(u16 u2Reg);
void mt8193_cec_write(u16 u2Reg, u32 u4Data);
void mt8193_cec_init(void);

static CEC_FRAME_DESCRIPTION CEC_rx_msg_queue[RX_Q_SIZE];
static CEC_FRAME_DESCRIPTION CEC_tx_msg_queue[TX_Q_SIZE];

CEC_FRAME_DESCRIPTION *ActiveTXFrame;
CEC_LA_ADDRESS _rCECLaAddr;
static CEC_FRAME_DESCRIPTION cecMwTxMsg;
CEC_ADDRESS _rCECPhysicAddr;

static u8 _u1TxFailCause;
static u8 _u1ReTxCnt;
static u8 CEC_rxQ_read_idx;
static u8 CEC_rxQ_write_idx;
static u8 CEC_txQ_read_idx;
static u8 CEC_txQ_write_idx;
static u8 _fgUICECEnable;

#define IS_RX_Q_EMPTY() (CEC_rxQ_read_idx == CEC_rxQ_write_idx)
#define IS_RX_Q_FULL() (((CEC_rxQ_write_idx+1)%RX_Q_SIZE) == CEC_rxQ_read_idx)
#define IS_TX_Q_EMPTY() (CEC_txQ_read_idx == CEC_txQ_write_idx)
#define IS_TX_Q_FULL() (((CEC_txQ_write_idx+1)%TX_Q_SIZE) == CEC_txQ_read_idx)

#define HDMICEC_BASE	(0x1600)

#define u1RegRead1B(reg16)		(mt8193_cec_read(reg16)&0xff)
#define u4RegRead4B(reg16)		(mt8193_cec_read(reg16))


#define	RegReadFldAlign(reg16, fld) ((mt8193_cec_read(reg16)>>(Fld_shft(fld)))&(mt8193_cec_2n(Fld_wid(fld))))
#define	vRegWriteFldAlign(reg16, val, fld) (mt8193_cec_write(reg16, (((mt8193_cec_read(reg16))&(mt8193_cec_maskvalue(Fld_wid(fld), Fld_shft(fld))))|(val<<(Fld_shft(fld))))))

#define IS_INT_DATA_RDY() (RegReadFldAlign(RX_EVENT, DATA_RDY))
#define IS_INT_HEADER_RDY() (RegReadFldAlign(RX_EVENT, HEADER_RDY))
#define IS_INT_MODE_RDY() (RegReadFldAlign(RX_EVENT, MODE_RDY))
#define IS_INT_OV() (RegReadFldAlign(RX_EVENT, OV))
#define IS_INT_BR_SB_RDY() (RegReadFldAlign(RX_EVENT, BR_SB_RDY))
#define IS_INT_SB_RDY() (RegReadFldAlign(RX_EVENT, SB_RDY))
#define IS_INT_BR_RDY() (RegReadFldAlign(RX_EVENT, BR_RDY))
#define ENABLE_INT_DATA_RDY(onoff) vRegWriteFldAlign(RX_EVENT, onoff, I_EN_DATA)
#define ENABLE_INT_HEADER_RDY(onoff) vRegWriteFldAlign(RX_EVENT, onoff, I_EN_HEADER)
#define ENABLE_INT_MODE_RDY(onoff) vRegWriteFldAlign(RX_EVENT, onoff, I_EN_MODE)
#define ENABLE_INT_OV(onoff) vRegWriteFldAlign(RX_EVENT, onoff, I_EN_OV)
#define ENABLE_INT_PULSE(onoff) vRegWriteFldAlign(RX_EVENT, onoff, I_EN_PULSE)
#define ENABLE_INT_BR_SB_RDY(onoff) vRegWriteFldAlign(RX_EVENT, onoff, I_EN_BR_SB)
#define ENABLE_INT_SB_RDY(onoff) vRegWriteFldAlign(RX_EVENT, onoff, I_EN_SB)
#define ENABLE_INT_BR_RDY(onoff) vRegWriteFldAlign(RX_EVENT, onoff, I_EN_BR)
#define CLR_INT_DATA_RDY() vRegWriteFldAlign(RX_EVENT, 0, DATA_RDY)
#define CLR_INT_HEADER_RDY() vRegWriteFldAlign(RX_EVENT, 0, HEADER_RDY)
#define CLR_INT_MODE_RDY() vRegWriteFldAlign(RX_EVENT, 0, MODE_RDY)
#define CLR_INT_OV() vRegWriteFldAlign(RX_EVENT, 0, OV)
#define NOTIFY_RX_HW_DATA_TAKEN() vRegWriteFldAlign(RX_EVENT, 0, BR_RDY)
#define HW_RX_DATA_ARRIVED() IS_INT_BR_RDY()
#define HW_RX_HEADER_ARRIVED() IS_INT_HEADER_RDY()

/* TX_EVENT */
#define IS_INT_UN() (RegReadFldAlign(TX_EVENT, UN))
#define IS_INT_LOW() (RegReadFldAlign(TX_EVENT, LOWB))
#define IS_TX_FINISH() (RegReadFldAlign(TX_EVENT, BS))
#define IS_INT_RB_RDY() (RegReadFldAlign(TX_EVENT, RB_RDY))
#define ENABLE_INT_UN(onoff) vRegWriteFldAlign(TX_EVENT, onoff, I_EN_UN)
#define ENABLE_INT_FAIL(onoff) vRegWriteFldAlign(TX_EVENT, onoff, I_EN_FAIL)
#define ENABLE_INT_LOW(onoff) vRegWriteFldAlign(TX_EVENT, onoff, I_EN_LOW)
#define ENABLE_INT_BS(onoff) vRegWriteFldAlign(TX_EVENT, onoff, I_EN_BS)
#define ENABLE_INT_RB(onoff) vRegWriteFldAlign(TX_EVENT, onoff, I_EN_RB)
#define CLR_INT_UN() vRegWriteFldAlign(TX_EVENT, 0, UN)
#define CLR_INT_LOW() vRegWriteFldAlign(TX_EVENT, 0, LOWB)
#define CLR_TX_FINISH() vRegWriteFldAlign(TX_EVENT, 0, BS)
#define TRIGGER_TX_HW() vRegWriteFldAlign(TX_EVENT, 1, RB_RDY)
#define IS_TX_DATA_TAKEN() (!(IS_INT_RB_RDY()))
#define IS_INT_RB_ENABLE() (RegReadFldAlign(TX_EVENT, I_EN_RB))
#define IS_INT_FAIL_ENABLE() (RegReadFldAlign(TX_EVENT, I_EN_FAIL))
#define DISABLE_ALL_TX_INT() \
do { \
	ENABLE_INT_FAIL(0); \
	ENABLE_INT_RB(0); \
	ENABLE_INT_LOW(0); \
	ENABLE_INT_UN(0); \
	ENABLE_INT_BS(0); \
} while (0)

/* RX FSM status */
#define IS_RX_FSM_IDLE() (RegReadFldAlign(RX_STATUS, RX_FSM) == 0x01)
#define IS_RX_FSM_START() (RegReadFldAlign(RX_STATUS, RX_FSM) == 0x02)
#define IS_RX_FSM_MODE() (RegReadFldAlign(RX_STATUS, RX_FSM) == 0x04)
#define IS_RX_FSM_MODE1_HEADER() (RegReadFldAlign(RX_STATUS, RX_FSM) == 0x08)
#define IS_RX_FSM_MODE1_ARB() (RegReadFldAlign(RX_STATUS, RX_FSM) == 0x10)
#define IS_RX_FSM_MODE1_FLAG() (RegReadFldAlign(RX_STATUS, RX_FSM) == 0x20)
#define IS_RX_FSM_MODE2_HEADER() (RegReadFldAlign(RX_STATUS, RX_FSM) == 0x40)
#define IS_RX_FSM_MODE2_CMD() (RegReadFldAlign(RX_STATUS, RX_FSM) == 0x80)
#define IS_RX_FSM_MODE3_ID() (RegReadFldAlign(RX_STATUS, RX_FSM) == 0x0100)
#define IS_RX_FSM_MODE3_HEADER() (RegReadFldAlign(RX_STATUS, RX_FSM) == 0x0200)
#define IS_RX_FSM_MODE3_DATA() (RegReadFldAlign(RX_STATUS, RX_FSM) == 0x0400)
#define IS_RX_FSM_GENERAL() (RegReadFldAlign(RX_STATUS, RX_FSM) == 0x0800)
#define IS_RX_FSM_ERROR_S() (RegReadFldAlign(RX_STATUS, RX_FSM) == 0x1000)
#define IS_RX_FSM_ERROR_D() (RegReadFldAlign(RX_STATUS, RX_FSM) == 0x2000)
#define RX_FSM_STATUS() (RegReadFldAlign(RX_STATUS, RX_FSM))

/* TX FSM status */
#define IS_TX_FSM_IDLE() (RegReadFldAlign(TX_STATUS, TX_FSM) == 0x01)
#define IS_TX_FSM_INIT() (RegReadFldAlign(TX_STATUS, TX_FSM) == 0x02)
#define IS_TX_FSM_EOM() (RegReadFldAlign(TX_STATUS, TX_FSM) == 0x04)
#define IS_TX_FSM_RETRASMIT() (RegReadFldAlign(TX_STATUS, TX_FSM) == 0x08)
#define IS_TX_FSM_FAIL() (RegReadFldAlign(TX_STATUS, TX_FSM) == 0x10)
#define IS_TX_FSM_START() (RegReadFldAlign(TX_STATUS, TX_FSM) == 0x20)
#define IS_TX_FSM_MODE() (RegReadFldAlign(TX_STATUS, TX_FSM) == 0x40)
#define IS_TX_FSM_MODE1_HEADER() (RegReadFldAlign(TX_STATUS, TX_FSM) == 0x80)
#define IS_TX_FSM_MODE1_DATA() (RegReadFldAlign(TX_STATUS, TX_FSM) == 0x100)
#define IS_TX_FSM_MODE2_HEADER() (RegReadFldAlign(TX_STATUS, TX_FSM) == 0x200)
#define IS_TX_FSM_MODE2_CMD() (RegReadFldAlign(TX_STATUS, TX_FSM) == 0x400)
#define IS_TX_FSM_MODE3_ID() (RegReadFldAlign(TX_STATUS, TX_FSM) == 0x800)
#define IS_TX_FSM_MODE3_HEADER() (RegReadFldAlign(TX_STATUS, TX_FSM) == 0x1000)
#define IS_TX_FSM_MODE3_DATA() (RegReadFldAlign(TX_STATUS, TX_FSM) == 0x2000)
#define IS_TX_FSM_GENERAL() (RegReadFldAlign(TX_STATUS, TX_FSM) == 0x4000)
#define TX_FSM_STATUS() (RegReadFldAlign(TX_STATUS, TX_FSM))

#define ENABLE_TX_EN() vRegWriteFldAlign(TR_CONFIG, 1, TX_EN)
#define DISABLE_TX_EN() vRegWriteFldAlign(TR_CONFIG, 0, TX_EN)
#define ENABLE_RX_EN() vRegWriteFldAlign(TR_CONFIG, 1, RX_EN)
#define DISABLE_RX_EN() vRegWriteFldAlign(TR_CONFIG, 0, RX_EN)

#define SET_HW_TX_LEN(num) vRegWriteFldAlign(TX_HD_NEXT, num, WTX_M3_DATA_MASK)
#define FILL_SRC_FIELD(addr) vRegWriteFldAlign(TX_HD_NEXT, addr, WTX_SRC)
#define FILL_DST_FIELD(addr) vRegWriteFldAlign(TX_HD_NEXT, addr, WTX_DST)
#define MARK_H_EOM(onoff) vRegWriteFldAlign(TX_HD_NEXT, onoff, WTX_H_EOM)
#define MARK_D_EOM(onoff) vRegWriteFldAlign(TX_HD_NEXT, onoff, WTX_D_EOM)

#define FILL_TX_DATA(data) vRegWriteFldAlign(TX_DATA_NEXT, data, WTX_DATA)

#define GET_HW_RX_LEN() (RegReadFldAlign(RX_HEADER, RXED_M3_DATA_MASK))
#define GET_SRC_FIELD() (RegReadFldAlign(RX_HEADER, RXED_SRC))
#define GET_DST_FIELD() (RegReadFldAlign(RX_HEADER, RXED_DST))
#define GET_SRC_FIELD_RECEIVING() (RegReadFldAlign(RX_HD_NEXT, RXING_SRC))
#define GET_DST_FIELD_RECEIVING() (RegReadFldAlign(RX_HD_NEXT, RXING_DST))
#define IS_RX_H_EOM() (RegReadFldAlign(RX_HEADER, RXED_H_EOM))
#define IS_RX_D_EOM() (RegReadFldAlign(RX_HEADER, RXED_D_EOM))

#define GET_HW_RX_DATA() (RegReadFldAlign(RX_DATA, RXED_DATA))

#define FLOW_CONTROL_ACK(onoff) \
do {\
	vRegWriteFldAlign(RX_HD_NEXT, (!(onoff)), RXING_H_ACK);\
	vRegWriteFldAlign(RX_HD_NEXT, (!(onoff)), RXING_D_ACK);\
} while (0)

#define GET_FOLLOWER_H_ACK() (RegReadFldAlign(TX_HEADER, TXING_H_ACK))
#define GET_FOLLOWER_D_ACK() (RegReadFldAlign(TX_HEADER, TXING_D_ACK))

#define TX_FAIL_MAX() (RegReadFldAlign(TX_FAIL, RETX_MAX))
#define CLR_TX_FAIL_MAX()  vRegWriteFldAlign(TX_FAIL, 0, RETX_MAX)

#define TX_FAIL_RECORD() u4RegRead4B(TX_FAIL)

#define TX_FAIL_SOURCE() (RegReadFldAlign(TX_FAIL, SOURCE))
#define CLR_TX_FAIL_SOURCE()  vRegWriteFldAlign(TX_FAIL, 0, SOURCE)
#define CHECK_RX_EN() (RegReadFldAlign(TR_CONFIG, RX_EN))

#define SET_LA1(La1) vRegWriteFldAlign(TR_CONFIG, La1, DEVICE_ADDR)
#define SET_LA2(La2) vRegWriteFldAlign(TR_CONFIG, La2, TR_DEVICE_ADDR2)
#define SET_LA3(La3) vRegWriteFldAlign(TR_CONFIG, La3, TR_DEVICE_ADDR3)



#define RESET_HW_TX() \
do { \
	DISABLE_TX_EN();\
	ENABLE_TX_EN();\
} while (0)

#define GET_TX_BIT_COUNTER() (RegReadFldAlign(TX_STATUS, TX_BIT_COUNTER))

void vRegWrite4BMsk(u16 reg16, u32 val32, u32 msk32)
{
	MT8193_CEC_FUNC();
	val32 &= msk32;
	mt8193_cec_write(reg16, ((mt8193_cec_read(reg16)) & ~msk32) | val32);
}

/*----------------------------------------------------------------------------*/
u32 mt8193_cec_read(u16 u2Reg)
{
	u32 u4Data;
	mt8193_i2c_read(HDMICEC_BASE + u2Reg, &u4Data);
	MT8193_CEC_LOG("[R]cec = 0x%04x, data = 0x%08x\n", u2Reg, u4Data);
	return u4Data;
}

void mt8193_cec_write(u16 u2Reg, u32 u4Data)
{
	MT8193_CEC_LOG("[W]cec= 0x%04x, data = 0x%08x\n", u2Reg, u4Data);
	mt8193_i2c_write(HDMICEC_BASE + u2Reg, u4Data);
}


u32 mt8193_cec_2n(u32 u4Data)
{
	u32 u4resultvalue = 1;
	u8 u1number;
	/* MT8193_CEC_FUNC(); */

	if (u4Data == 0)
		return 0;	/* must be not 0 */
	if (u4Data == 0x20)
		return 0xffffffff;
	if (u4Data > 0x20)
		return 0;	/* must  not exceed over 0x20 */

	for (u1number = 0; u1number < u4Data; u1number++) {
		u4resultvalue *= 2;
	}

	return (u4resultvalue - 1);
}

u32 mt8193_cec_maskvalue(u32 u4Width, u32 u4Startbit)
{
	u32 u4Data = 0xffffffff, i;
	/* MT8193_CEC_FUNC(); */

	for (i = 0; i < u4Width; i++) {
		u4Data &= (~(mt8193_cec_2n(u4Startbit + i) + 1));
	}

	return u4Data;
}

/*----------------------------------------------------------------------------*/

void vRegWrite4B(u16 u2Reg, u32 u4Data)
{
	MT8193_CEC_FUNC();
	mt8193_cec_write(u2Reg, u4Data);
}

void mt8193_cec_init(void)
{
	MT8193_CEC_FUNC();
	_rCECPhysicAddr.ui2_pa = 0xffff;
	_rCECPhysicAddr.ui1_la = 0x0;

	_CEC_Status = 0;
	_CEC_ErrStatus = 0;
	_u1TxFailCause = FAIL_NONE;
	_u1ReTxCnt = 0;
	CEC_rxQ_write_idx = 0;
	CEC_rxQ_read_idx = 0;
	CEC_txQ_write_idx = 0;
	CEC_txQ_read_idx = 0;

	_rCECLaAddr.aui1_la[0] = 0x0F;
	_rCECLaAddr.aui1_la[1] = 0x0F;
	_rCECLaAddr.aui1_la[2] = 0x0F;

	vRegWrite4B(CEC_CKGEN, 0x00020082);	/* 3MHz //different from BD /100k */
	vRegWrite4B(TR_TEST, 0x40004019);	/* Bpad enable¡BTx compared timing 0x19 */

	/* CYJ.NOTE TX_EN, RX_EN: disable it */
	vRegWrite4B(TR_CONFIG, 0x00000001);

	vRegWrite4B(RX_T_START_R, 0x01980154);
	vRegWrite4B(RX_T_START_F, 0x01e801a9);
	vRegWrite4B(RX_T_DATA, 0x006e00c8);
	vRegWrite4B(RX_T_ACK, 0x00000096);
	vRegWrite4B(RX_T_ERROR, 0x01680212);
	vRegWrite4B(TX_T_START, 0x01c20172);
	vRegWrite4B(TX_T_DATA_R, 0x003c0096);
	vRegWrite4B(TX_T_DATA_F, 0x00f000f0);
	vRegWrite4B(TX_ARB, 0x00000596);

	/* turn off interrupt of general mode */
	vRegWrite4B(TX_GEN_INTR, 0x00000000);
	vRegWrite4B(RX_CAP_90, 0x00000000);
	vRegWrite4B(TX_GEN_MASK, 0x00000000);
	vRegWrite4B(RX_GEN_WD, 0x00000000);
	vRegWrite4B(RX_GEN_MASK, 0x00000000);
	vRegWrite4B(RX_GEN_INTR, 0x00000000);

	FLOW_CONTROL_ACK(1);

	vRegWriteFldAlign(TX_HD_NEXT, 0, WTX_M3_ID);
	vRegWriteFldAlign(TX_HD_NEXT, 0, WTX_M1_DIR);
	vRegWriteFldAlign(TX_HD_NEXT, 0, WTX_M1_PAS);
	vRegWriteFldAlign(TX_HD_NEXT, 0, WTX_M1_NAS);
	vRegWriteFldAlign(TX_HD_NEXT, 0, WTX_M1_DES);
	vRegWriteFldAlign(TX_HD_NEXT, 3, WTX_MODE);

	vRegWrite4B(TR_CONFIG, 0x0fff1101);

	vRegWrite4BMsk(TX_EVENT, 0x00, 0xff);
	vRegWrite4BMsk(RX_EVENT, 0x00, 0xff);
	/* RX_EVENT */
	ENABLE_INT_OV(1);
	ENABLE_INT_BR_RDY(1);
	ENABLE_INT_HEADER_RDY(1);
	/* TX_EVENT */
	ENABLE_INT_UN(0);
	ENABLE_INT_LOW(0);
	ENABLE_INT_FAIL(0);
	ENABLE_INT_BS(0);
	ENABLE_INT_RB(0);
	/* la */
	SET_LA1(0x04);
	SET_LA2(0x05);
	SET_LA3(0x0f);

	return;
}


static u8 CEC_rx_enqueue(CEC_FRAME_DESCRIPTION *frame)
{
	/* check if queue is full */
	MT8193_CEC_FUNC();
	if (IS_RX_Q_FULL()) {
		return FALSE;
	}

	/* copy the new incoming message to rx queue */
	memcpy(&(CEC_rx_msg_queue[CEC_rxQ_write_idx]), frame, sizeof(CEC_FRAME_DESCRIPTION));
	/* CYJ.NOTE: no critical section */
	CEC_rxQ_write_idx = (CEC_rxQ_write_idx + 1) % RX_Q_SIZE;

	return TRUE;
}

static void _CEC_Receiving(void)
{
	static u8 *size;
	static CEC_FRAME_DESCRIPTION *frame = &ActiveRXFrame;
	u32 data;
	u8 i, rxlen, is_d_eom, ret;

	MT8193_CEC_FUNC();

	/* no data available */
	if (!IS_INT_BR_RDY()) {
		ASSERT(0);
		return;
	}
	/* <polling message> only */
	if (GET_HW_RX_LEN() == 0) {
		NOTIFY_RX_HW_DATA_TAKEN();
		ClrCECStatus(STATE_RX_GET_NEW_HEADER);	/* CM 20081210 */
		return;
	}

	/* new incoming message */
	if (IsCECStatus(STATE_RX_GET_NEW_HEADER)) {
		ClrCECStatus(STATE_RX_GET_NEW_HEADER);
		if (IsCECStatus(STATE_WAIT_RX_FRAME_COMPLETE)) {
			MT8193_CEC_LOG("[CEC] Lost EOM:2\n");
			SetCECErrorFlag(ERR_RX_LOST_EOM);
		}
		SetCECStatus(STATE_WAIT_RX_FRAME_COMPLETE);

		size = &(frame->size);
		(*size) = 0;
		frame->blocks.header.initiator = GET_SRC_FIELD();
		frame->blocks.header.destination = GET_DST_FIELD();
		(*size)++;
	}

	if (!IsCECStatus(STATE_WAIT_RX_FRAME_COMPLETE)) {
		NOTIFY_RX_HW_DATA_TAKEN();
		SetCECErrorFlag(ERR_RX_LOST_HEADER);
		return;
	}

	rxlen = GET_HW_RX_LEN();
	data = GET_HW_RX_DATA();
	is_d_eom = IS_RX_D_EOM();
	NOTIFY_RX_HW_DATA_TAKEN();

	if (rxlen == 0x3) {
		rxlen = 2;
	} else if (rxlen == 0x7) {
		rxlen = 3;
	} else if (rxlen == 0xf) {
		rxlen = 4;
	} else if (rxlen != 0x1) {
		MT8193_CEC_LOG("[CEC] invalid rx length occurs\n");
		/* assert(0); */
	}
	/* for opcode */
	if ((*size) == 1) {
		frame->blocks.opcode = data & 0xff;
		data >>= 8;
		(*size)++;
		rxlen--;
	}
	/* for operand */
	for (i = 0; i < rxlen; i++) {

		if ((*size) > 15) {
			MT8193_CEC_LOG("[CEC] Receive Data Length is wrong !\n");
			break;
		} else {
			ASSERT((*size) >= 2);
			frame->blocks.operand[(*size) - 2] = data & 0xff;
			data >>= 8;
			(*size)++;
		}
	}

	if (is_d_eom) {
		ClrCECStatus(STATE_WAIT_RX_FRAME_COMPLETE);
		SetCECStatus(STATE_RX_COMPLETE_NEW_FRAME);

		/* push into rx_queue */
		ret = CEC_rx_enqueue(frame);
		if (ret == FALSE) {
			SetCECErrorFlag(ERR_RXQ_OVERFLOW);
			MT8193_CEC_LOG("[CEC] cec rx buffer overflow\n");
			/* ASSERT(0); */
		}
	}
}

static u8 _CEC_SendRemainingDataBlocks(void)
{
	CEC_FRAME_DESCRIPTION *frame;
	u8 errcode = 0;
	u8 size;
	u8 *sendidx;
	u8 *blocks;
	u32 data;
	u8 i, j;
	MT8193_CEC_FUNC();

	if (!IsCECStatus(STATE_TXING_FRAME)) {
		return 0;
	}

	if (IsCECStatus(STATE_WAIT_TX_DATA_TAKEN)) {
		if (IS_INT_RB_ENABLE() && IS_TX_DATA_TAKEN()) {
			ClrCECStatus(STATE_WAIT_TX_DATA_TAKEN);
		} else {
			/* tx buffer is not emply */
			return 0;
		}
	} else {
		return 0;
	}

	/* request current active TX frame */
	frame = ActiveTXFrame;

	size = frame->size;
	sendidx = &(frame->sendidx);
	blocks = &(frame->blocks.opcode);

	/* CYJ.NOTE: Leave "TX hardware error handling" to _CEC_Mainloop */
	if (IS_TX_FSM_FAIL() | (TX_FAIL_RECORD() > 0)) {
		MT8193_CEC_LOG("[CEC] Detect TX FAIL in %s\n", __func__);
		return 3;
		/* RESET_HW_TX(); */
		/* ASSERT(0); */
	}

	size -= ((*sendidx) + 1);

	if (size == 0) {
		return 0;
	}

	/* CYJ:TODO duplicate (as _CEC_SendFrame())! */
	/* fill data */
	if (size > 4) {
		SET_HW_TX_LEN(0xf);
		MARK_H_EOM(0);
		MARK_D_EOM(0);
	} else if (size == 4) {
		SET_HW_TX_LEN(0xf);
		MARK_H_EOM(0);
		MARK_D_EOM(1);
	} else if (size == 3) {
		SET_HW_TX_LEN(0x7);
		MARK_H_EOM(0);
		MARK_D_EOM(1);
	} else if (size == 2) {
		SET_HW_TX_LEN(0x3);
		MARK_H_EOM(0);
		MARK_D_EOM(1);
	} else if (size == 1) {
		SET_HW_TX_LEN(0x1);
		MARK_H_EOM(0);
		MARK_D_EOM(1);
	}

	data = 0;
	for (i = 0, j = size; i < 4; i++) {
		data >>= 8;
		data |= (blocks[(*sendidx)]) << 24;
		if (i < j) {
			(*sendidx)++;
			size--;
		}
	}

	/* EOM */
	if (size == 0) {
		ENABLE_INT_FAIL(1);
		ENABLE_INT_RB(0);
		ENABLE_INT_LOW(0);
		ENABLE_INT_UN(0);
		ENABLE_INT_BS(0);
	} else {
		ENABLE_INT_FAIL(1);
		ENABLE_INT_RB(1);
		ENABLE_INT_LOW(1);
		ENABLE_INT_UN(1);
		ENABLE_INT_BS(0);

		SetCECStatus(STATE_WAIT_TX_DATA_TAKEN);
	}

	FILL_TX_DATA(data);
	MT8193_CEC_LOG("[CEC] TRIGGER_TX_HW in %s, size: %x\n", __func__, size);

	CLR_TX_FINISH();

	TRIGGER_TX_HW();

	return errcode;
}

static void _CEC_Check_Active_Tx_Result(void)
{
	MT8193_CEC_FUNC();

	if (IsCECStatus(STATE_TXING_FRAME)) {

		if (IsCECStatus(STATE_HW_RETX)) {
			if (TX_FAIL_SOURCE()) {
				_u1TxFailCause = FAIL_SOURCE;
				CLR_TX_FAIL_SOURCE();
			}
			if ((TX_FAIL_RECORD() != 0)) {
				DISABLE_ALL_TX_INT();

				SetCECStatus(STATE_TX_NOACK);
				ClrCECStatus(STATE_HW_RETX);

				ClrCECStatus(STATE_TXING_FRAME);
				if (IS_TX_FSM_FAIL()) {
					MT8193_CEC_LOG("[CEC] TX NO ACK\n");
				} else {
					MT8193_CEC_LOG("[CEC] other TX error\n");
				}
				MT8193_CEC_LOG("[CEC] H ACK: %x, D ACK: %x\n", GET_FOLLOWER_H_ACK(),
					       GET_FOLLOWER_D_ACK());

				RESET_HW_TX();
			}
		} else if ((ActiveTXFrame->sendidx + 1) == (ActiveTXFrame->size)) {

			if (IS_TX_FSM_IDLE() && IS_TX_FINISH()) {
				DISABLE_ALL_TX_INT();
				SetCECStatus(STATE_TX_FRAME_SUCCESS);
				ClrCECStatus(STATE_TXING_FRAME);
				MT8193_CEC_LOG
				    ("[CEC] TX is COMPLETED with: H ACK: %x and D ACK %x\n",
				     (unsigned int)GET_FOLLOWER_H_ACK(),
				     (unsigned int)GET_FOLLOWER_D_ACK());
			}
		}
	}

}

static CEC_FRAME_DESCRIPTION *_CEC_Get_Cur_TX_Q_Msg(void)
{
	MT8193_CEC_FUNC();

	if (IS_TX_Q_EMPTY()) {
		return NULL;
	}

	return (&(CEC_tx_msg_queue[CEC_txQ_read_idx]));
}

static u8 _CEC_TX_Dequeue(void)
{
	MT8193_CEC_FUNC();

	if (IS_TX_Q_EMPTY()) {
		return FALSE;
	}
	/* CYJ.NOTE: no critical section */
	CEC_txQ_read_idx = (CEC_txQ_read_idx + 1) % TX_Q_SIZE;

	return TRUE;
}

static void PrintFrameDescription(CEC_FRAME_DESCRIPTION *frame)
{
	u8 i;

	MT8193_CEC_LOG("[CEC] >>>>>>>>>>>>>>>>>>>>>>>>\n");
	MT8193_CEC_LOG("[CEC] frame description:\n");
	MT8193_CEC_LOG("[CEC] size: 0x%x\n", frame->size);
	MT8193_CEC_LOG("[CEC] sendidx: 0x%x\n", frame->sendidx);
	MT8193_CEC_LOG("[CEC] reTXcnt: 0x%x\n", frame->reTXcnt);
	MT8193_CEC_LOG("[CEC] initiator: 0x%x\n", frame->blocks.header.initiator);
	MT8193_CEC_LOG("[CEC] destination: 0x%x\n", frame->blocks.header.destination);
	if (frame->size > 1) {
		MT8193_CEC_LOG("[CEC] opcode: 0x%x\n", frame->blocks.opcode);
	}
	if ((frame->size > 2) && (frame->size <= 16)) {
		for (i = 0; i < (frame->size - 2); i++) {
			MT8193_CEC_LOG("0x%02x\n", frame->blocks.operand[i]);
		}
	}
	MT8193_CEC_LOG("[CEC]<<<<<<<<<<<<<<<<<<<<<<<<<<\n");
}

static u8 _CEC_SendFrame(CEC_FRAME_DESCRIPTION *frame)
{
	u8 errcode = 0;
	u8 size;
	u8 *sendidx;
	u8 *blocks;
	u32 data;
	u8 i, j;
	MT8193_CEC_FUNC();

	if (IsCECStatus(STATE_TXING_FRAME)) {
		return 1;
	}
	SetCECStatus(STATE_TXING_FRAME);

	/* CYJ.NOTE: Leave "TX hardware error handling" to _CEC_Mainloop */
	if (IS_TX_FSM_FAIL() | (TX_FAIL_RECORD() > 0)) {
		MT8193_CEC_LOG("[CEC] Detect TX FAIL in %s\n", __func__);
		/* return 3; */
		RESET_HW_TX();
		/* ASSERT(0); // CAN NOT HAPPEN HERE */
	}

	size = frame->size;
	sendidx = &(frame->sendidx);
	blocks = &(frame->blocks.opcode);

	if (size == 0) {
		ClrCECStatus(STATE_TXING_FRAME);
		return 2;
	} else if (size > 16) {
		ClrCECStatus(STATE_TXING_FRAME);
		return 2;
	}
	/* CYJ.NOTE: TX HW is not idle */
	if (!IS_TX_FSM_IDLE()) {
		RESET_HW_TX();
	}
	ActiveTXFrame = frame;

	ClrCECStatus(STATE_TX_FRAME_SUCCESS);
	ClrCECStatus(STATE_TX_NOACK);


	/* fill header */
	FILL_SRC_FIELD(frame->blocks.header.initiator);
	FILL_DST_FIELD(frame->blocks.header.destination);
	size -= 1;

	/* header-only */
	if (size == 0) {
		SET_HW_TX_LEN(0);
		MARK_H_EOM(1);
		MARK_D_EOM(0);
		/* TRIGGER_TX_HW(); */
		size = 0;
	}

	/* fill data */
	if (size > 4) {
		SET_HW_TX_LEN(0xf);
		MARK_H_EOM(0);
		MARK_D_EOM(0);
	} else if (size == 4) {
		SET_HW_TX_LEN(0xf);
		MARK_H_EOM(0);
		MARK_D_EOM(1);
	} else if (size == 3) {
		SET_HW_TX_LEN(0x7);
		MARK_H_EOM(0);
		MARK_D_EOM(1);
	} else if (size == 2) {
		SET_HW_TX_LEN(0x3);
		MARK_H_EOM(0);
		MARK_D_EOM(1);
	} else if (size == 1) {
		SET_HW_TX_LEN(0x1);
		MARK_H_EOM(0);
		MARK_D_EOM(1);
	}

	data = 0;
	for (i = 0, j = size; i < 4; i++) {
		data >>= 8;
		data |= (blocks[(*sendidx)]) << 24;
		if (i < j) {
			(*sendidx)++;
			size--;
		}
	}

	/* EOM */
	if (size == 0) {
		ENABLE_INT_FAIL(1);
		ENABLE_INT_RB(0);
		ENABLE_INT_LOW(0);
		ENABLE_INT_UN(0);
		ENABLE_INT_BS(0);
	} else {
		ENABLE_INT_FAIL(1);
		ENABLE_INT_RB(1);
		ENABLE_INT_LOW(1);
		ENABLE_INT_UN(1);
		ENABLE_INT_BS(0);

		SetCECStatus(STATE_WAIT_TX_DATA_TAKEN);
	}

	FILL_TX_DATA(data);
	MT8193_CEC_LOG("[CEC] TRIGGER_TX_HW in %s, size: %x\n", __func__, size);


	CLR_TX_FINISH();
	_u1TxFailCause = FAIL_NONE;
	_u1ReTxCnt = 0;
	TRIGGER_TX_HW();

	return errcode;
}

static void _CEC_TX_Queue_Loop(void)
{
	CEC_FRAME_DESCRIPTION *frame;
	/* MT8193_CEC_FUNC(); */

	/* if the tx message queue is empty */
	if (IS_TX_Q_EMPTY()) {
		return;
	}

	/* if the tx is active, check the result */
	if (IsCECStatus(STATE_TXING_FRAME)) {
		if (IsCECErrorFlag(ERR_TX_MISALARM) && (TX_FAIL_RECORD() != 0) && IS_TX_FSM_FAIL()) {
			DISABLE_ALL_TX_INT();
			ClrCECErrorFlag(ERR_TX_MISALARM);
			SetCECStatus(STATE_TX_NOACK);
			ClrCECStatus(STATE_HW_RETX);
			ClrCECStatus(STATE_TXING_FRAME);
			RESET_HW_TX();
		}

		_CEC_Check_Active_Tx_Result();
		if (IsCECStatus(STATE_TX_FRAME_SUCCESS)) {
			MT8193_CEC_LOG("[CEC] This message is successful\n");
			frame = _CEC_Get_Cur_TX_Q_Msg();
			if (frame == NULL) {
				ASSERT(0);
			}
			_CEC_TX_Dequeue();
			ClrCECStatus(STATE_TX_FRAME_SUCCESS);
		}
		if (IsCECStatus(STATE_TX_NOACK)) {
			frame = _CEC_Get_Cur_TX_Q_Msg();
			if (frame == NULL) {
				ASSERT(0);
			}
			MT8193_CEC_LOG("[CEC] This message is failed: %d\n", frame->reTXcnt);
			frame->reTXcnt++;
			frame->sendidx = 0;
			ClrCECStatus(STATE_TX_NOACK);
			/* CYJ.NOTE: retransmission */
			if (frame->reTXcnt == RETX_MAX_CNT) {
				_u1TxFailCause = FAIL_NONE;
				MT8193_CEC_LOG("[CEC] ReTX reach MAX\n");
				_CEC_TX_Dequeue();
			}
		}
	} else
		/* if the tx is not active, send the next message */
	{
		frame = _CEC_Get_Cur_TX_Q_Msg();
		if (frame == NULL) {
			ASSERT(0);
		}
		if (_u1TxFailCause == FAIL_SOURCE) {
			if (_u1ReTxCnt < 15) {
				_u1ReTxCnt++;
				return;
			} else {
				_u1ReTxCnt = 0;
			}
		}
		MT8193_CEC_LOG("[CEC] Send a new message\n");
		PrintFrameDescription(frame);
		_CEC_SendFrame(frame);
	}
}

static CEC_FRAME_DESCRIPTION *CEC_rx_dequeue(void)
{
	CEC_FRAME_DESCRIPTION *ret;
	/* MT8193_CEC_FUNC(); */

	/* check if queue is empty */
	if (IS_RX_Q_EMPTY()) {
		return NULL;
	}

	/* return next available entry for middleware */
	ret = &(CEC_rx_msg_queue[CEC_rxQ_read_idx]);

	/* CYJ.NOTE: no critical section */
	CEC_rxQ_read_idx = (CEC_rxQ_read_idx + 1) % RX_Q_SIZE;

	return ret;
}

static u8 CEC_frame_validation(CEC_FRAME_DESCRIPTION *frame)
{
	u8 size = frame->size;
	u8 i1ret = TRUE;
	MT8193_CEC_FUNC();

	/* opcode-aware */
	/* CYJ.NOTE: code size issue */
	switch (frame->blocks.opcode) {
		/* length == 2 */
	case OPCODE_IMAGE_VIEW_ON:
	case OPCODE_TEXT_VIEW_ON:
	case OPCODE_REQUEST_ACTIVE_SOURCE:
	case OPCODE_STANDBY:
	case OPCODE_RECORD_OFF:
	case OPCODE_RECORD_TV_SCREEN:
	case OPCODE_GET_CEC_VERSION:
	case OPCODE_GIVE_PHYSICAL_ADDRESS:
	case OPCODE_GET_MENU_LANGUAGE:
	case OPCODE_TUNER_STEP_DECREMENT:
	case OPCODE_TUNER_STEP_INCREMENT:
	case OPCODE_GIVE_DEVICE_VENDOR_ID:
	case OPCODE_VENDOR_REMOTE_BUTTON_UP:
	case OPCODE_GIVE_OSD_NAME:
	case OPCODE_USER_CONTROL_RELEASED:
	case OPCODE_GIVE_DEVICE_POWER_STATUS:
	case OPCODE_ABORT:
	case OPCODE_GIVE_AUDIO_STATUS:
	case OPCODE_GIVE_SYSTEM_AUDIO_MODE_STATUS:
		if (size != 2) {
			i1ret = FALSE;
		}
		break;
	case OPCODE_SYSTEM_AUDIO_MODE_REQUEST:
		if ((size != 2) && (size != 4)) {
			i1ret = FALSE;
		}
		break;
		/* length == 3 */
	case OPCODE_RECORD_STATUS:
	case OPCODE_TIMER_CLEARED_STATUS:
	case OPCODE_CEC_VERSION:
	case OPCODE_DECK_CONTROL:
	case OPCODE_DECK_STATUS:
	case OPCODE_GIVE_DECK_STATUS:
	case OPCODE_PLAY:
	case OPCODE_GIVE_TUNER_DEVICE_STATUS:
	case OPCODE_MENU_REQUEST:
	case OPCODE_MENU_STATUS:
	case OPCODE_REPORT_POWER_STATUS:
	case OPCODE_REPORT_AUDIO_STATUS:
	case OPCODE_SET_SYSTEM_AUDIO_MODE:
	case OPCODE_SYSTEM_AUDIO_MODE_STATUS:
	case OPCODE_SET_AUDIO_RATE:
		if (size != 3) {
			i1ret = FALSE;
		}
		break;
	case OPCODE_USER_CONTROL_PRESSED:
		if ((size != 3) && (size != 4)) {
			i1ret = FALSE;
		}
		break;
		/* length == 4 */
	case OPCODE_ACTIVE_SOURCE:
	case OPCODE_INACTIVE_SOURCE:
	case OPCODE_ROUTING_INFORMATION:
	case OPCODE_SET_STREAM_PATH:
	case OPCODE_FEATURE_ABORT:
		if (size != 4) {
			i1ret = FALSE;
		}
		break;
		/* length == 5 */
	case OPCODE_REPORT_PHYSICAL_ADDRESS:
	case OPCODE_SET_MENU_LANGUAGE:
	case OPCODE_DEVICE_VENDOR_ID:
		if (size != 5) {
			i1ret = FALSE;
		}
		break;
		/* length == 6 */
	case OPCODE_ROUTING_CHANGE:
	case OPCODE_SELECT_ANALOGUE_SERVICE:
		if (size != 6) {
			i1ret = FALSE;
		}
		break;
		/* length == 9 */
	case OPCODE_SELECT_DIGITAL_SERVICE:
		if (size != 9) {
			i1ret = FALSE;
		}
		break;
		/* length == 13 */
	case OPCODE_CLEAR_ANALOGUE_TIMER:
	case OPCODE_SET_ANALOGUE_TIMER:
		if (size != 13) {
			i1ret = FALSE;
		}
		break;
		/* length == 16 */
	case OPCODE_CLEAR_DIGITAL_TIMER:
	case OPCODE_SET_DIGITAL_TIMER:
		if (size != 16) {
			i1ret = FALSE;
		}
		break;
	case OPCODE_RECORD_ON:
		if ((size < 3) || (size > 10)) {
			i1ret = FALSE;
		}
		break;
		/* length == 10 ~ 11 */
	case OPCODE_CLEAR_EXTERNAL_TIMER:
	case OPCODE_SET_EXTERNAL_TIMER:
		if ((size < 10) || (size > 11)) {
			i1ret = FALSE;
		}
		break;
	case OPCODE_TIMER_STATUS:
		if ((size != 3) && (size != 5)) {
			i1ret = FALSE;
		}
		break;
	case OPCODE_TUNER_DEVICE_STATUS:
		if ((size != 7) && (size != 10)) {
			i1ret = FALSE;
		}
		break;
	case OPCODE_VENDOR_COMMAND:
	case OPCODE_VENDOR_COMMAND_WITH_ID:
	case OPCODE_VENDOR_REMOTE_BUTTON_DOWN:
		if (size > 16) {
			i1ret = FALSE;
		}
		break;
	case OPCODE_SET_OSD_STRING:
		if ((size < 3) || (size > 16)) {
			i1ret = FALSE;
		}
		break;
	case OPCODE_SET_TIMER_PROGRAM_TITLE:
	case OPCODE_SET_OSD_NAME:
		if ((size < 3) || (size > 16)) {
			i1ret = FALSE;
		}
		break;
	}
	if (i1ret == FALSE) {
		MT8193_CEC_LOG("[CEC] receive invalid frame: %x\n", frame->blocks.opcode);
		PrintFrameDescription(frame);
	}
	return i1ret;
}

static u8 check_and_init_tx_frame(CEC_FRAME_DESCRIPTION *frame)
{
	u8 ret = 0x00;
	MT8193_CEC_FUNC();

	if ((frame->size > CEC_MAX_MESG_SIZE) || (frame->size == 0)) {
		MT8193_CEC_LOG("[CEC] Tx fram size is not correct\n");
		ret = 0x01;
	}
	/* valid tx frame */
	if (ret == 0x00) {
		frame->reTXcnt = 0;
		frame->sendidx = 0;
	}

	return ret;
}

u8 _CEC_TX_Enqueue(CEC_FRAME_DESCRIPTION *frame)
{
	MT8193_CEC_FUNC();
	if (frame->size == 1) {
		MT8193_CEC_LOG("[CEC] Polling LA = 0x%x\n",
			       (u8) ((frame->blocks.header.initiator << 4) | frame->blocks.header.
				     destination));
	} else {
		MT8193_CEC_LOG("[CEC] Opcode = 0x%x, size = 0x%x\n", frame->blocks.opcode,
			       frame->size);
	}


	if (check_and_init_tx_frame(frame)) {
		return 0x01;
	}

	if (IS_TX_Q_FULL()) {
		MT8193_CEC_LOG("[CEC] Tx queue is full\n");
		return 0x01;
	}

	memcpy(&(CEC_tx_msg_queue[CEC_txQ_write_idx]), frame, sizeof(CEC_FRAME_DESCRIPTION));
	/* CYJ.NOTE: no critical section */
	CEC_txQ_write_idx = (CEC_txQ_write_idx + 1) % TX_Q_SIZE;

	return 0x00;
}

void mt8193_u4CecSendSLTData(u8 *pu1Data)
{
	u8 i;

	if (*pu1Data > 14) {
		*pu1Data = 14;
	}
	CTSTestFrame.size = *pu1Data + 2;
	CTSTestFrame.sendidx = 0;
	CTSTestFrame.reTXcnt = 0;
	CTSTestFrame.txtag = NULL;
	CTSTestFrame.blocks.header.destination = 0x00;
	CTSTestFrame.blocks.header.initiator = 0x04;
	CTSTestFrame.blocks.opcode = 0x01;
	for (i = 0; i < *pu1Data; i++) {
		CTSTestFrame.blocks.operand[i] = *(pu1Data + i + 1);
	}
	_CEC_TX_Enqueue(&CTSTestFrame);

}

void mt8193_GetSLTData(CEC_SLT_DATA *rCecSltData)
{
	u8 i;
	CEC_FRAME_DESCRIPTION *frame;
	MT8193_CEC_FUNC();

	frame = CEC_rx_dequeue();

	if (frame == NULL) {
		rCecSltData->u1Size = 5;
		for (i = 0; i < rCecSltData->u1Size; i++) {
			rCecSltData->au1Data[i] = i;
		}
		ClrCECStatus(STATE_RX_COMPLETE_NEW_FRAME);
		return;
	}

	if (frame->blocks.opcode == 0x01) {
		if (CEC_frame_validation(frame)) {
			PrintFrameDescription(frame);
		}

		rCecSltData->u1Size = frame->size - 2;
		if (rCecSltData->u1Size > 14) {
			rCecSltData->u1Size = 14;
		}
		for (i = 0; i < rCecSltData->u1Size; i++) {
			rCecSltData->au1Data[i] = frame->blocks.operand[i];
		}
		MT8193_CEC_LOG("[CEC SLT] Receive data\n");
		MT8193_CEC_LOG("[CEC SLT] size = 0x%x\n", rCecSltData->u1Size);
		MT8193_CEC_LOG("[CEC SLT] data = ");

		for (i = 0; i < rCecSltData->u1Size; i++) {
			MT8193_CEC_LOG(" 0x%x  ", rCecSltData->au1Data[i]);
		}
		MT8193_CEC_LOG(" \n");
	}
}

void CTS_RXProcess(CEC_FRAME_DESCRIPTION *frame)
{
	MT8193_CEC_FUNC();

	if (frame->blocks.opcode == OPCODE_ABORT) {
		CTSTestFrame.size = 4;
		CTSTestFrame.sendidx = 0;
		CTSTestFrame.reTXcnt = 0;
		CTSTestFrame.txtag = NULL;
		CTSTestFrame.blocks.header.destination = frame->blocks.header.initiator;
		CTSTestFrame.blocks.header.initiator = 4;
		CTSTestFrame.blocks.opcode = OPCODE_FEATURE_ABORT;
		CTSTestFrame.blocks.operand[0] = OPCODE_ABORT;
		CTSTestFrame.blocks.operand[1] = 4;

/* ///////////////////// Test////////////////////////////////// */
		CTSTestFrame.blocks.operand[2] = 2;
		CTSTestFrame.blocks.operand[3] = 3;
		CTSTestFrame.blocks.operand[4] = 4;
		CTSTestFrame.blocks.operand[5] = 5;
		CTSTestFrame.blocks.operand[6] = 6;
		CTSTestFrame.blocks.operand[7] = 7;
		CTSTestFrame.blocks.operand[8] = 8;
		CTSTestFrame.blocks.operand[9] = 9;
		CTSTestFrame.blocks.operand[10] = 10;
		CTSTestFrame.blocks.operand[11] = 11;
		CTSTestFrame.blocks.operand[12] = 12;
		CTSTestFrame.blocks.operand[13] = 13;
/* /////////////////////Test//////////////////////////////////// */

		_CEC_TX_Enqueue(&CTSTestFrame);
		MT8193_CEC_LOG("[CEC] CTS Send: OPCODE_FEATURE_ABORT\n");
	} else if (frame->blocks.opcode == OPCODE_GIVE_PHYSICAL_ADDRESS) {
		CTSTestFrame.size = 5;
		CTSTestFrame.sendidx = 0;
		CTSTestFrame.reTXcnt = 0;
		CTSTestFrame.txtag = NULL;
		CTSTestFrame.blocks.header.destination = 0xf;
		CTSTestFrame.blocks.header.initiator = 4;
		CTSTestFrame.blocks.opcode = OPCODE_REPORT_PHYSICAL_ADDRESS;
		CTSTestFrame.blocks.operand[0] = 0x10;
		CTSTestFrame.blocks.operand[1] = 0x00;
		CTSTestFrame.blocks.operand[2] = 0x04;

		_CEC_TX_Enqueue(&CTSTestFrame);
		MT8193_CEC_LOG("[CEC] CTS Send: OPCODE_REPORT_PHYSICAL_ADDRESS\n");
	}
}

static void CEC_rx_msg_notify(u8 u1rxmode)
{
	CEC_FRAME_DESCRIPTION *frame;
	/* MT8193_CEC_FUNC(); */
	if (u1rxmode == CEC_SLT_MODE)
		return;

	if (u1rxmode == CEC_NORMAL_MODE)
		return;

	while (1) {
		frame = CEC_rx_dequeue();

		if (frame == NULL) {
			ClrCECStatus(STATE_RX_COMPLETE_NEW_FRAME);
			return;
		}

		if (CEC_frame_validation(frame)) {
			MT8193_CEC_LOG("[CEC} Receive message\n");
			PrintFrameDescription(frame);

			if (u1rxmode == CEC_CTS_MODE) {
				CTS_RXProcess(frame);
			}
		}
	}
}

void mt8193_cec_mainloop(u8 u1rxmode)
{
	/* MT8193_CEC_FUNC(); */
	_CEC_TX_Queue_Loop();

	/* NOTE: the priority between tx and rx */
	if (!IsCECStatus(STATE_TXING_FRAME)) {
		CEC_rx_msg_notify(u1rxmode);
	}

}

u8 mt8193_cec_isrprocess(u8 u1rxmode)
{
	u8 u1ReceivedDst;

	MT8193_CEC_FUNC();

	if (HW_RX_HEADER_ARRIVED()) {
		u1ReceivedDst = GET_DST_FIELD_RECEIVING();
		MT8193_CEC_LOG("[CEC]u1ReceivedDst = 0x%08x\n", u1ReceivedDst);
		if (u1rxmode == CEC_CTS_MODE)
			_rCECLaAddr.aui1_la[0] = 4;
		if ((u1ReceivedDst == _rCECLaAddr.aui1_la[0])
		    || (u1ReceivedDst == _rCECLaAddr.aui1_la[1])
		    || (u1ReceivedDst == _rCECLaAddr.aui1_la[2]) || (u1ReceivedDst == 0xf)) {
			MT8193_CEC_LOG("[CEC] RX:H\n");
			if (IsCECStatus(STATE_RX_GET_NEW_HEADER)) {
				MT8193_CEC_LOG("[CEC] Lost EOM:1\n");
				SetCECErrorFlag(ERR_RX_LOST_EOM);
			}
			SetCECStatus(STATE_RX_GET_NEW_HEADER);
		} else {
			ClrCECStatus(STATE_RX_GET_NEW_HEADER);
			MT8193_CEC_LOG("[CEC] RX:H False\n");
		}
	}
	if (HW_RX_DATA_ARRIVED()) {
		MT8193_CEC_LOG("[CEC] RX:D\n");
		_CEC_Receiving();
	}
	if (IS_INT_OV()) {
		MT8193_CEC_LOG("[CEC] Overflow\n");
		CLR_INT_OV();
		SetCECStatus(STATE_HW_RX_OVERFLOW);
	}
	/* TX_EVENT */
	if (IsCECStatus(STATE_TXING_FRAME)) {
		if (IS_INT_UN()) {
			MT8193_CEC_LOG("[CEC] Underrun\n");
			CLR_INT_UN();
			SetCECErrorFlag(ERR_TX_UNDERRUN);
		}
		if (IS_INT_LOW()) {
			MT8193_CEC_LOG("[CEC] Buffer Low\n");
			CLR_INT_LOW();
			if (!IS_INT_RB_RDY()) {
				MT8193_CEC_LOG
				    ("[CEC] FW is slow to trigger the following blocks\n");
				SetCECErrorFlag(ERR_TX_BUFFER_LOW);
			}
		}
		if (IS_INT_RB_ENABLE() && IS_TX_DATA_TAKEN()) {
			MT8193_CEC_LOG("[CEC] TX Data Taken\n");
			_CEC_SendRemainingDataBlocks();
		}
		/* CYJ.NOTE TX Failure Detection */
		if (IS_INT_FAIL_ENABLE() && (TX_FAIL_RECORD() != 0)) {
			DISABLE_ALL_TX_INT();
			SetCECStatus(STATE_HW_RETX);

			if (TX_FAIL_MAX() | IS_TX_FSM_FAIL()) {
				MT8193_CEC_LOG("[CEC] TX Fail: %x\n", TX_FAIL_RECORD());
				MT8193_CEC_LOG("[CEC] TX Fail MAX\n");
			} else {
				MT8193_CEC_LOG("[CEC] TX Fail: %x\n", TX_FAIL_RECORD());
			}
		}
		MT8193_CEC_LOG("[CEC] TX HW FSM: %x\n", TX_FSM_STATUS());
	}

	if (IS_RX_Q_EMPTY())
		return 0;
	else
		return 1;
}

void CECMWSetLA(CEC_LA_ADDRESS *prLA)
{
	MT8193_CEC_FUNC();

	memcpy(&_rCECLaAddr, prLA, sizeof(CEC_LA_ADDRESS));

	ASSERT(_rCECLaAddr.ui1_num <= 3);

	if (_rCECLaAddr.ui1_num == 0) {
		SET_LA3(0x0F);
		SET_LA2(0x0F);
		SET_LA1(0x0F);
		_rCECLaAddr.aui1_la[0] = 0x0F;
		_rCECLaAddr.aui1_la[1] = 0x0F;
		_rCECLaAddr.aui1_la[2] = 0x0F;
	} else if (_rCECLaAddr.ui1_num == 1) {
		SET_LA3(0x0F);
		SET_LA2(0x0F);
		SET_LA1(_rCECLaAddr.aui1_la[0]);
		_rCECLaAddr.aui1_la[1] = 0x0F;
		_rCECLaAddr.aui1_la[2] = 0x0F;
	} else if (_rCECLaAddr.ui1_num == 2) {
		SET_LA3(0x0F);
		SET_LA2(_rCECLaAddr.aui1_la[1]);
		SET_LA1(_rCECLaAddr.aui1_la[0]);
		_rCECLaAddr.aui1_la[2] = 0x0F;
	} else if (_rCECLaAddr.ui1_num == 3) {
		SET_LA3(_rCECLaAddr.aui1_la[2]);
		SET_LA2(_rCECLaAddr.aui1_la[1]);
		SET_LA1(_rCECLaAddr.aui1_la[0]);
	}

	MT8193_CEC_LOG("MW Set LA & PA\n");
	MT8193_CEC_LOG("LA num = 0x%x , LA = 0x%x 0x%x 0x%x\n", _rCECLaAddr.ui1_num,
		       _rCECLaAddr.aui1_la[0], _rCECLaAddr.aui1_la[1], _rCECLaAddr.aui1_la[2]);
	MT8193_CEC_LOG("PA = %04x\n", _rCECLaAddr.ui2_pa);
}

void mt8193_CECMWSetLA(CEC_DRV_ADDR_CFG_T *prAddr)
{
	CEC_LA_ADDRESS rLA;

	MT8193_CEC_FUNC();

	if (prAddr->ui1_la_num > 3) {
		return;
	}

	rLA.ui1_num = prAddr->ui1_la_num;
	rLA.aui1_la[0] = prAddr->e_la[0];
	rLA.aui1_la[1] = prAddr->e_la[1];
	rLA.aui1_la[2] = prAddr->e_la[2];
	rLA.ui2_pa = prAddr->ui2_pa;
	CECMWSetLA(&rLA);
}

void mt8193_CECMWGet(CEC_FRAME_DESCRIPTION *frame)
{
	u8 i;
	CEC_FRAME_DESCRIPTION *frame1;

	MT8193_CEC_FUNC();

	frame1 = CEC_rx_dequeue();
	if (frame1 == NULL) {
		frame->blocks.operand[14] = GET_CMD_ERR;
		frame->size = 5;
		frame->sendidx = 1;
		frame->reTXcnt = 1;
		frame->txtag = NULL;
		frame->blocks.header.destination = 1;
		frame->blocks.header.initiator = 1;
		frame->blocks.opcode = frame1->blocks.opcode;
		for (i = 0; i < 14; i++)
			frame->blocks.operand[i] = i;
		ClrCECStatus(STATE_RX_COMPLETE_NEW_FRAME);
		return;
	}

	if (CEC_frame_validation(frame1)) {
		MT8193_CEC_LOG("app get message\n");
		PrintFrameDescription(frame1);
	}

	frame->size = frame1->size;
	frame->sendidx = frame1->sendidx;
	frame->reTXcnt = frame1->reTXcnt;
	frame->txtag = NULL;
	frame->blocks.header.destination = frame1->blocks.header.destination;
	frame->blocks.header.initiator = frame1->blocks.header.initiator;
	frame->blocks.opcode = frame1->blocks.opcode;
	for (i = 0; i < 14; i++)
		frame->blocks.operand[i] = frame1->blocks.operand[i];

	if (IS_RX_Q_EMPTY())
		frame->blocks.operand[14] = GET_CMD_EMPTY;
	else
		frame->blocks.operand[14] = GET_CMD_UNEMPTY;

}

void mt8193_CECMWSend(CEC_SEND_MSG_T *msg)
{
	u32 i4Ret;
	u8 i;

	MT8193_CEC_FUNC();

	if ((msg->t_frame_info.ui1_init_addr > 0xf) || (msg->t_frame_info.ui1_dest_addr > 0xf)
	    || (msg->t_frame_info.z_operand_size > LOCAL_CEC_MAX_OPERAND_SIZE)) {
		return;
	}

	cecMwTxMsg.txtag = NULL;
	cecMwTxMsg.blocks.header.initiator = msg->t_frame_info.ui1_init_addr;
	cecMwTxMsg.blocks.header.destination = msg->t_frame_info.ui1_dest_addr;
	cecMwTxMsg.sendidx = 0;
	cecMwTxMsg.reTXcnt = 0;
	if (msg->t_frame_info.ui2_opcode == 0xffff) {
		cecMwTxMsg.size = 1;
	} else {
		cecMwTxMsg.blocks.opcode = msg->t_frame_info.ui2_opcode;
		cecMwTxMsg.size = msg->t_frame_info.z_operand_size + 2;
	}
	for (i = 0; i < cecMwTxMsg.size - 2; i++) {
		cecMwTxMsg.blocks.operand[i] = msg->t_frame_info.aui1_operand[i];
	}

	i4Ret = _CEC_TX_Enqueue(&cecMwTxMsg);
	if (i4Ret == 0x01) {
		msg->b_enqueue_ok = FALSE;
		MT8193_CEC_LOG("MW Set cmd fail\n");
		return;
	} else {
		msg->b_enqueue_ok = TRUE;
		MT8193_CEC_LOG("MW Set cmd sucess\n");
		return;
	}

}

void mt8193_CECMWSetEnableCEC(u8 u1EnCec)
{
	MT8193_CEC_FUNC();

	if (u1EnCec == 1) {
		MT8193_CEC_LOG("UI ON\n");
		_fgUICECEnable = 1;
	} else {
		MT8193_CEC_LOG("UI off\n");
		_fgUICECEnable = 0;
	}
}

void mt8193_NotifyApiCECAddress(CEC_ADDRESS *cecaddr)
{
	MT8193_CEC_FUNC();

	cecaddr->ui2_pa = _rCECPhysicAddr.ui2_pa;
	cecaddr->ui1_la = _rCECPhysicAddr.ui1_la;
}

void mt8193_SetPhysicCECAddress(u16 u2pa, u8 u1la)
{
	MT8193_CEC_FUNC();

	_rCECPhysicAddr.ui2_pa = u2pa;
	_rCECPhysicAddr.ui1_la = u1la;
}

#endif
