#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/skbuff.h>
#include <linux/time.h>
#include <linux/delay.h>

#include <linux/rwlock_types.h>
#include <linux/completion.h>
#include "anx_ohio_private_interface.h"
#include "anx_ohio_public_interface.h"
#include "anx_ohio_driver.h"

/**
 * @desc:   The Interface AP set the source capability to Ohio
 *
 * @param:  pdo_buf: PDO buffer pointer of source capability,
 *                              which can be packed by PDO_FIXED_XXX macro
 *                eg: default5Vsafe src_cap(5V, 0.9A fixed) -->
 *                                          PDO_FIXED(5000,900, PDO_FIXED_FLAGS)
 *
 *                src_caps_size: source capability's size
 *
 * @return:  0: success 1: fail
 *
 */
u8 send_src_cap(const u8 *src_caps, u8 src_caps_size)
{
	u8 i;

	anx_printk(K_INFO, "send_src_cap\n");

	if (NULL == src_caps)
		return CMD_FAIL;

	if ((src_caps_size%PD_ONE_DATA_OBJECT_SIZE) != 0 ||
		(src_caps_size/PD_ONE_DATA_OBJECT_SIZE) >
		PD_MAX_DATA_OBJECT_NUM)
		return CMD_FAIL;

	for (i = 0; i < src_caps_size; i++)
		pd_src_pdo[i] = *src_caps++;

	pd_src_pdo_cnt = src_caps_size / PD_ONE_DATA_OBJECT_SIZE;

	/*send source capabilities message to Ohio really*/
	return interface_send_msg_timeout(TYPE_PWR_SRC_CAP,
			pd_src_pdo,
			pd_src_pdo_cnt * PD_ONE_DATA_OBJECT_SIZE,
			INTERFACE_TIMEOUT);
}

/**
 * @desc:   Interface AP fetch the source capability from Ohio
 *
 * @param:  pdo_buf: PDO buffer pointer of source capability in Ohio
 *          src_caps_size: source capability's size
 *
 * @return:  0: success 1: fail
 *
 */
u8 get_src_cap(const u8 *src_caps, u8 src_caps_size)
{
	src_caps = src_caps;
	src_caps_size = src_caps_size;
	return 1;
}

/**
 * @desc:   Interface that AP fetch the sink capability from Ohio's
 *          downstream device
 *
 * @param:  sink_caps: PDO buffer pointer of sink capability
 *                     which will be responsed by Ohio's SINK
 *                     Capablity Message
 *
 *          snk_caps_len: sink capability max length of the array
 *
 * @return:  sink capability array length>0: success.  0: fail
 *
 */
u8 get_snk_cap(u8 *snk_caps, u8 snk_caps_len)
{
	snk_caps = snk_caps;
	snk_caps_len = snk_caps_len;
	return 1;
}


/**
 * @desc:   Interface that AP send(configure) the sink capability to Ohio's
 *          downstream device
 *
 * @param:  snk_caps: PDO buffer pointer of sink capability
 *
 *          snk_caps_size: sink capability length
 *
 * @return:  1: success.  0: fail
 *
 */
u8 send_snk_cap(const u8 *snk_caps, u8 snk_caps_size)
{
	u8 i;

	if (NULL == snk_caps)
		return CMD_FAIL;

	if ((snk_caps_size%PD_ONE_DATA_OBJECT_SIZE) != 0 ||
		(snk_caps_size/PD_ONE_DATA_OBJECT_SIZE) >
		PD_MAX_DATA_OBJECT_NUM)
		return CMD_FAIL;

	for (i = 0; i < snk_caps_size; i++)
		pd_snk_pdo[i] = *snk_caps++;

	pd_snk_pdo_cnt = snk_caps_size / PD_ONE_DATA_OBJECT_SIZE;

	/*configure sink cap*/
	return interface_send_msg_timeout(
			TYPE_PWR_SNK_CAP,
			pd_snk_pdo,
			pd_snk_pdo_cnt * 4,
			INTERFACE_TIMEOUT);
}



/**
 * @desc:   Interface that AP send(configure) the DP's sink capability to Ohio's downstream device
 *
 * @param:  dp_snk_caps: PDO buffer pointer of DP sink capability
 *
 *                dp_snk_caps_size: DP sink capability length
 *
 * @return:  1: success.  0: fail
 *
 */
u8 send_dp_snk_cfg(const u8 *dp_snk_caps, u8 dp_snk_caps_size)
{
	u8 i;

	if (NULL == dp_snk_caps)
		return CMD_FAIL;

	if ((dp_snk_caps_size%PD_ONE_DATA_OBJECT_SIZE) != 0 ||
		(dp_snk_caps_size/PD_ONE_DATA_OBJECT_SIZE) >
		PD_MAX_DATA_OBJECT_NUM)
		return CMD_FAIL;

	for (i = 0; i < dp_snk_caps_size; i++)
		configure_DP_caps[i] = *dp_snk_caps++;

	interface_send_dp_caps();

	return 1;
}

/**
 * @desc:   Interface that AP initialze the DP's capability of Ohio, as source device
 *
 * @param:  dp_caps: DP's capability  pointer of source
 *
 *                dp_caps_size: source DP capability length
 *
 * @return:  1: success.  0: fail
 *
 */
u8 send_src_dp_cap(const u8 *dp_caps, u8 dp_caps_size)
{
	u8 i;

	if (NULL == dp_caps)
		return CMD_FAIL;

	if ((dp_caps_size%PD_ONE_DATA_OBJECT_SIZE) != 0 ||
		(dp_caps_size/PD_ONE_DATA_OBJECT_SIZE) >
		PD_MAX_DATA_OBJECT_NUM)
		return CMD_FAIL;

	if (dp_caps_size > PD_ONE_DATA_OBJECT_SIZE)
		return CMD_FAIL;


	/*stored src DP caps in local buffer*/
	for (i = 0; i < dp_caps_size; i++)
		src_dp_caps[i] = *dp_caps++;

	/*configure source DP cap*/
	return interface_send_msg_timeout(
			TYPE_DP_SNK_IDENDTITY,
			src_dp_caps,
			4,
			INTERFACE_TIMEOUT);
}

u8 sink_id_header[PD_ONE_DATA_OBJECT_SIZE] = {0x00, 0x00, 0x00, 0x6c};
u8 sink_cert_stat_vdo[PD_ONE_DATA_OBJECT_SIZE] = {0x00, 0x00, 0x00, 0x00};
u8 sink_prd_vdo[PD_ONE_DATA_OBJECT_SIZE] = {0x58, 0x01, 0x13, 0x10};
u8 sink_ama_vdo[PD_ONE_DATA_OBJECT_SIZE] = {0x39, 0x00, 0x00, 0x51};

u8 send_dp_snk_identity(void)
{
	u8 tmp[32] = {0};

	memcpy(tmp, sink_id_header, 4);
	memcpy(tmp + 4, sink_cert_stat_vdo, 4);
	memcpy(tmp + 8, sink_prd_vdo, 4);
	memcpy(tmp + 12, sink_ama_vdo, 4);

	return interface_send_msg_timeout(TYPE_DP_SNK_IDENDTITY,
			tmp,
			16,
			INTERFACE_TIMEOUT);
}


/**
 * @desc:   Interface that AP get the sink DP's capability
 *
 * @param:  dp_caps: DP's capability  pointer of sink
 *
 *                dp_caps_size: sink DP capability length
 *
 * @return:  1: success.  0: fail
 *
 */
u8 send_config_dp_cap(const u8 *dp_caps, u8 dp_caps_size)
{
	u8 i;

	if (NULL == dp_caps)
		return CMD_FAIL;

	if ((dp_caps_size % PD_ONE_DATA_OBJECT_SIZE) != 0 ||
		(dp_caps_size / PD_ONE_DATA_OBJECT_SIZE) >
		PD_MAX_DATA_OBJECT_NUM)
		return CMD_FAIL;

	if (dp_caps_size > PD_ONE_DATA_OBJECT_SIZE)
		return CMD_FAIL;

	/*stored src DP caps in local buffer*/
	for (i = 0; i < dp_caps_size; i++)
		DP_caps[i] = *dp_caps++;

	return interface_send_msg_timeout(
			TYPE_DP_SNK_CFG,
			DP_caps,
			4,
			INTERFACE_TIMEOUT);
}

/**
 * @desc:   The Interface AP set the VDM packet to Ohio
 *
 * @param:  vdm:  object buffer pointer of VDM,
 *
 *
 *
 *                size: vdm packet size
 *
 * @return:  0: success 1: fail
 *
 */
u8 send_vdm(const u8 *vdm, u8 size)
{
	u8 tmp[32] = {0};

	if (NULL == vdm)
		return CMD_FAIL;

	if (size > 3 && size < 32) {
		memcpy(tmp, vdm, size);

		if ((tmp[2] == 0x01) && (tmp[3] == 0x00)) {
			tmp[3] = 0x40;
			return interface_send_msg_timeout(TYPE_VDM,
					tmp,
					size,
					INTERFACE_TIMEOUT);
		}
	}
	return 1;
}

/**
 * @desc:   Interface that AP send(configure) the sink capability to Ohio's downstream device
 *
 * @param:  snk_caps: PDO buffer pointer of sink capability
 *
 *                snk_caps_size: sink capability length
 *
 * @return:  1: success.  0: fail
 *
 */
u8 send_rdo(const u8 *rdo, u8 size)
{
	u8 i;

	if (NULL == rdo)
		return CMD_FAIL;

	if ((size%PD_ONE_DATA_OBJECT_SIZE) != 0 ||
	     (size/PD_ONE_DATA_OBJECT_SIZE) > PD_MAX_DATA_OBJECT_NUM) {
		return CMD_FAIL;
	}

	for (i = 0; i < size; i++)
		pd_rdo[i] = *rdo++;

	/*configure sink cap*/
	return interface_send_msg_timeout(TYPE_PWR_OBJ_REQ,
		pd_rdo, size, INTERFACE_TIMEOUT);

}

/**
 * @desc:   The interface AP will get the ohio's data role
 *
 * @param:  none
 *
 * @return:  data role , dfp 1 , ufp 0, other error
 *
 */
u8 get_data_role(void)
{

	return 1;
}

/**
 * @desc:   The interface AP will get the ohio's power role
 *
 * @param:  none
 *
 * @return:  data role , source 1 , sink 0, other error
 *
 */
u8 get_power_role(void)
{
	  return 1;
}

/**
 * @desc:   The interface AP will send  PR_Swap command to Ohio
 *
 * @param:  none
 *
 * @return:  1: success.  0: fail
 *
 */
u8 send_power_swap(void)
{
	return interface_pr_swap();

}

/**
 * @desc:   The interface AP will send DR_Swap command to Ohio
 *
 * @param:  none
 *
 * @return:  1: success.  0: fail
 *
 */
u8 send_data_swap(void)
{
	return interface_dr_swap();
}


/**
 * @desc:   The interface AP will send accpet command to Ohio
 *
 * @param:  none
 *
 * @return:  1: success.  0: fail
 *
 */
u8 send_accept(void)
{
	return interface_send_msg_timeout(TYPE_ACCEPT,
		0, 0, INTERFACE_TIMEOUT);
}

/**
 * @desc:   The interface AP will send reject command to Ohio
 *
 * @param:  none
 *
 * @return:  1: success.  0: fail
 *
 */
u8 send_reject(void)
{
	return interface_send_msg_timeout(TYPE_REJECT,
		0, 0, INTERFACE_TIMEOUT);
}

/**
 * @desc:   The interface AP will send soft reset command to Ohio
 *
 * @param:  none
 *
 * @return:  1: success.  0: fail
 *
 */
u8 send_soft_reset(void)
{
	return interface_send_soft_rst();
}

/**
 * @desc:   The interface AP will send hard reset command to Ohio
 *
 * @param:  none
 *
 * @return:  1: success.  0: fail
 *
 */
u8 send_hard_reset(void)
{
	return interface_send_hard_rst();
}

char *interface_to_str(unsigned char header_type)
{
	return  (header_type == TYPE_PWR_SRC_CAP) ? "src cap" :
	(header_type == TYPE_PWR_SNK_CAP) ? "snk cap" :
	(header_type == TYPE_PWR_OBJ_REQ) ? "RDO" :
	(header_type == TYPE_DP_SNK_IDENDTITY) ? "src dp cap" :
	(header_type == TYPE_SVID) ? "svid" :
	(header_type == TYPE_PSWAP_REQ) ? "PR_SWAP" :
	(header_type == TYPE_DSWAP_REQ) ? "DR_SWAP" :
	(header_type == TYPE_GOTO_MIN_REQ) ? "GOTO_MIN" :
	(header_type == TYPE_DP_ALT_ENTER) ? "DPALT_ENTER" :
	(header_type == TYPE_DP_ALT_EXIT) ? "DPALT_EXIT" :
	(header_type == TYPE_VCONN_SWAP_REQ) ? "VCONN_SWAP" :
	(header_type == TYPE_GET_DP_SNK_CAP) ? "GET_SINK_DP_CAP" :
	(header_type == TYPE_DP_SNK_CFG) ? "dp cap" :
	(header_type == TYPE_SOFT_RST) ? "Software Reset" :
	(header_type == TYPE_HARD_RST) ? "Hardware Reset" :
	(header_type == TYPE_RESTART) ? "Restart" :
	(header_type == TYPE_PD_STATUS_REQ) ? "PD Status" :
	(header_type == TYPE_ACCEPT) ? "ACCEPT" :
	(header_type == TYPE_REJECT) ? "REJECT" :
	(header_type == TYPE_VDM) ? "VDM" :
	(header_type == TYPE_RESPONSE_TO_REQ) ? "Response to Request" :
	"Unknown";
}

unsigned char cac_checksum(unsigned char  *pSendBuf, unsigned char len)
{
	unsigned char i;
	unsigned char checksum;

	checksum = 0;
	for (i = 0; i < len; i++)
		checksum += *(pSendBuf + i);

	return (0 - checksum);
}

DEFINE_RWLOCK(usb_pd_cmd_rwlock);
int usb_pd_cmd_counter = 0;
u8 usb_pd_cmd_status = 0;

u8 wait_pd_cmd_timeout(int pd_cmd_timeout)
{
	unsigned long expire;
	u8 cmd_status = 0;

	write_lock_irq(&usb_pd_cmd_rwlock);
	usb_pd_cmd_counter = 1;
	write_unlock_irq(&usb_pd_cmd_rwlock);

	anx_printk(K_INFO, "wait_pd_cmd_timeout\n");

	/* looply check counter to be changed  to 0 by
	 * interrupt interface in timeout period
	 */
	expire = MSEC_TO_JIFFIES(pd_cmd_timeout) + jiffies;
	while (1) {
		read_lock(&usb_pd_cmd_rwlock);
		if (usb_pd_cmd_counter <= 0) {
			cmd_status = usb_pd_cmd_status;
			read_unlock(&usb_pd_cmd_rwlock);
			anx_printk(K_INFO, "fetch usb command status %x successful!\n",
				cmd_status);

			return cmd_status;
		}
		read_unlock(&usb_pd_cmd_rwlock);

		if (time_before(expire, jiffies)) {
			write_lock_irq(&usb_pd_cmd_rwlock);
			usb_pd_cmd_counter = 0;
			cmd_status = 0;
			write_unlock_irq(&usb_pd_cmd_rwlock);
			anx_printk(K_INFO, "ohio wait for command response expire timeout!!!!\n");
			return CMD_FAIL;
		}
		/*interface_recvd_msg();*/
	}
	return CMD_FAIL;
}

u8 pd_src_pdo_cnt = 2;
u8 pd_src_pdo[VDO_SIZE] = {
	/*5V 0.9A , 5V 1.5*/
	0x5A, 0x90, 0x01, 0x2A, 0x96, 0x90, 0x01, 0x2A
};

u8 sink_svid_vdo[PD_ONE_DATA_OBJECT_SIZE];
u8 pd_snk_pdo_cnt = 3;
u8 pd_snk_pdo[VDO_SIZE];
u8 pd_rdo[PD_ONE_DATA_OBJECT_SIZE];
u8 DP_caps[PD_ONE_DATA_OBJECT_SIZE];
u8 configure_DP_caps[PD_ONE_DATA_OBJECT_SIZE];
u8 src_dp_caps[PD_ONE_DATA_OBJECT_SIZE];

unsigned char OhioReadReg(unsigned char RegAddr)
{
	u8 c = 0;

	ohio_read_reg(0x50, RegAddr, &c);
	return c;
}

void OhioWriteReg(unsigned char RegAddr, unsigned char RegVal)
{
	ohio_write_reg(0x50, RegAddr, RegVal);
}


void  printb(const char *buf, size_t size)
{
	while (size--)
		anx_printk(K_INFO, "[0x%zx]0x%x\n", size, *buf++);
}

void send_initialized_setting(void)
{
	update_pwr_src_caps();

	if (interface_send_snk_cap() != CMD_SUCCESS)
		anx_printk(K_INFO, "ohio sink cap failed\n");

	if (interface_send_src_dp_cap() != CMD_SUCCESS)
		anx_printk(K_INFO, "ohio src dp failed\n");

	if (interface_send_svid() != CMD_SUCCESS)
		anx_printk(K_INFO, "ohio svid failed\n");
}

/*///////////////////Circle Buffer ////////////////////////////////*/
unsigned char recv_status;
unsigned char receiving_len;

unsigned char send_status;
unsigned char sending_len;


unsigned char  InterfaceSendBuf[32];
unsigned char  InterfaceRecvBuf[32];

/*circular buffer driver*/
#define MAX_SEND_BUF_SIZE 8

#define AP_BUF_FRONT 0x11
#define AP_BUF_REAR 0x12
#define OCM_BUF_FRONT 0x13
#define OCM_BUF_REAR 0x14
#define AP_ACK_STATUS 0x15
#define OCM_ACK_STATUS 0x16
#define AP_BUF_START 0x18 /*0x18-0x1f*/
#define OCM_BUF_START 0x20 /*0x20-0x27*/
bool is_interface_vdm_flag;


#define TX_BUF_FRONT AP_BUF_FRONT
#define TX_BUF_REAR AP_BUF_REAR
#define TX_BUF_START AP_BUF_START

#define RX_BUF_FRONT OCM_BUF_FRONT
#define RX_BUF_REAR OCM_BUF_REAR
#define RX_BUF_START OCM_BUF_START

#define SET_ACK_STATUS AP_ACK_STATUS
#define ACK_STATUS OCM_ACK_STATUS

struct tagInterfaceData InterfaceData;

enum send_state_machine {
	SENDING_INIT,
	SENDING_DATA,
	SENDING_WAITTING_ACK,
	SENDING_SUCCESS,
	SENDING_FAIL,
	SENDING_NUM
};

unsigned char pbuf_rx_front = 0;
unsigned char pbuf_tx_rear = 0;

#define tx_buf_rear() pbuf_tx_rear
#define rx_buf_front() pbuf_rx_front

#define rx_buf_rear() OhioReadReg(RX_BUF_REAR)
#define tx_buf_front() OhioReadReg(TX_BUF_FRONT)

#define set_ack_status(val) OhioWriteReg(SET_ACK_STATUS, val)
#define get_ack_status() OhioReadReg(ACK_STATUS)

#define up_rx_front() \
	OhioWriteReg(RX_BUF_FRONT, rx_buf_front())

#define up_tx_rear() \
	OhioWriteReg(TX_BUF_REAR, tx_buf_rear())

#define buf_read(val) \
	do { \
		*val = OhioReadReg(RX_BUF_START + rx_buf_front()); \
		rx_buf_front() = (rx_buf_front() + 1) % MAX_SEND_BUF_SIZE; \
	} while (0)

#define buf_write(val) \
	do { \
		OhioWriteReg(TX_BUF_START + tx_buf_rear(), val); \
		tx_buf_rear() = (tx_buf_rear() + 1) % MAX_SEND_BUF_SIZE; \
	} while (0)


#define full_tx_buf() \
	(tx_buf_front() == (tx_buf_rear() + 1) % MAX_SEND_BUF_SIZE)

#define empty_rx_buf() \
	(rx_buf_front() == rx_buf_rear())



bool buf_enque(unsigned char byte)
{
	int ret = -1;

	if (full_tx_buf())
		ret = 0;
	else {
		buf_write(byte);
		ret = 1;
	}
	return ret;
}

bool buf_deque(char *val)
{
	int ret = -1;

	if (empty_rx_buf())
		ret = 0;
	else {
		buf_read(val);
		ret = 1;
	}

	return ret;
}

void reset_queue(void)
{
	OhioWriteReg(AP_BUF_FRONT, 0);
	OhioWriteReg(AP_BUF_REAR, 0);
	OhioWriteReg(AP_ACK_STATUS, 0);

	OhioWriteReg(OCM_BUF_FRONT, 0);
	OhioWriteReg(OCM_BUF_REAR, 0);
	OhioWriteReg(OCM_ACK_STATUS, 0);

	pbuf_rx_front = 0;
	pbuf_tx_rear = 0;
}

/* 0, send interface msg timeout
  * 1 successful */
u8 interface_send_msg_timeout(u8 type, u8 *pbuf, u8 len, int timeout_ms)
{
	u8 i = 0;
	bool need_upd = 0;
	u8 snd_msg_total_len = 0;
	static unsigned long expire;

	anx_printk(K_INFO, "len=%d, type=%s timeout=%d\n",
			len, interface_to_str(type), timeout_ms);

	set_ack_status(0x00); /*clear ack bit*/
	InterfaceSendBuf[0] = len + 1; /* + cmd*/
	InterfaceSendBuf[1] = type;

	if (len > 0)
		memcpy(InterfaceSendBuf + 2, pbuf, len);

	/* cmd + checksum */
	InterfaceSendBuf[len + 2] =
		cac_checksum(InterfaceSendBuf, len + 1 + 1);

	snd_msg_total_len = len + 3;

	/*set timeout time*/
	expire = MSEC_TO_JIFFIES(timeout_ms) + jiffies;

	for (i = 0; i < snd_msg_total_len; ) {
		need_upd = 0;
		while (!full_tx_buf()) {   /*not full*/
			if (buf_enque(InterfaceSendBuf[i])) {
				i++;
				need_upd = 1;
			}
			if (i == snd_msg_total_len)
				break;
		}

		if (need_upd) {
			/*update tx buffer rear*/
			up_tx_rear();
		}

		if (time_before(expire, jiffies)) {
			anx_printk(K_INFO, "T%d:%d\n", snd_msg_total_len, i);
			return CMD_FAIL;
		}
	}

	i = get_ack_status();
	while (i == 0) {
		if (time_before(expire, jiffies)) {
			anx_printk(K_INFO, "ack timeout %d\n",
					get_ack_status());
			return CMD_FAIL;
		}
		anx_printk(K_INFO, "try again\n");
		mdelay(1);
		i = get_ack_status();
	}

	if (get_ack_status() == 0x01)
		anx_printk(K_INFO, "CMD OK >> %s\n",
			interface_to_str(InterfaceSendBuf[1]));
	else if (get_ack_status() == 0x02)
		anx_printk(K_INFO, "F%x\n", type);

	return CMD_SUCCESS;
}

void init_default_sink_svid_vdo(void)
{
	u8 i;
	u8 svid_vdo[PD_ONE_DATA_OBJECT_SIZE] = {0x00, 0x00, 0x01, 0xff};

	for (i = 0; i < PD_ONE_DATA_OBJECT_SIZE; i++)
		sink_svid_vdo[i] = svid_vdo[i];
}

void init_default_sink_pdo(void)
{
	/*5V 1.5A*/
	u8 i, sink_pdo[VDO_SIZE] = {
		0xF0, 0x90, 0x01, 0x38,
		0xc8, 0xA0, 0x04, 0x00,
		0x78, 0x60, 0x03, 0x59
	};

	for (i = 0; i < VDO_SIZE; i++)
		pd_snk_pdo[i] = sink_pdo[i];
}

void init_default_rdo(void)
{
	u8 i;
	u8 rdo[PD_ONE_DATA_OBJECT_SIZE] = { 0x0A, 0x78, 0x00, 0x10};

	for (i = 0; i < PD_ONE_DATA_OBJECT_SIZE; i++)
		pd_rdo[i] = rdo[i];
}

void init_default_dp_caps(void)
{
	u8 i = 0;

	for (i = 0; i < PD_ONE_DATA_OBJECT_SIZE; i++)
		DP_caps[i] = 0;
}

void init_default_cfg_dp_caps(void)
{
	u8 i;
	u8 dcap[PD_ONE_DATA_OBJECT_SIZE] = {0x06, 0x08, 0x08, 0x00};

	for (i = 0; i < PD_ONE_DATA_OBJECT_SIZE; i++)
		configure_DP_caps[i] = dcap[i];
}

void init_default_src_dp_caps(void)
{
	u8 i;
	u8 dcap[PD_ONE_DATA_OBJECT_SIZE] = {0x11, 0x00, 0x00, 0x00};

	for (i = 0; i < PD_ONE_DATA_OBJECT_SIZE; i++)
		src_dp_caps[i] = dcap[i];

}

interface_msg_t imsg_list[MAX_INTERFACE_COUNT];

void interface_init(void)
{
	unsigned char i;

	recv_status = 0;
	receiving_len = 0;
	send_status = 0;
	sending_len = 0;

	/*send_init_setting_state = 0;*/
	for (i = 0x11; i < 0x11 + 25; i++)
		OhioWriteReg(i, 0x00);

	is_interface_vdm_flag = 0;
	/*send_init_setting_state = 1;*/

	memset(imsg_list, 0, sizeof(interface_msg_t) * MAX_INTERFACE_COUNT);

	init_default_sink_svid_vdo();
	init_default_sink_pdo();
	init_default_rdo();
	init_default_dp_caps();
	init_default_cfg_dp_caps();

	/*disabled all Ohio's external interrupt*/
	OhioWriteReg(IRQ_EXT_MASK_2, 0xff);
	/*external low active*/
	/*OhioWriteReg(IRQ_STATUS,0x00);*/
	/*open soft interrupt mask for interface*/
	OhioWriteReg(IRQ_EXT_MASK_2,
			OhioReadReg(IRQ_EXT_MASK_2) &
				(~IRQ_EXT_SOFT_RESET_BIT));

	/*clear interruts bit for initial status*/
	OhioWriteReg(IRQ_EXT_SOURCE_2, 0xff);
}


bool is_interface_vdm(void)
{
	return is_interface_vdm_flag;
}
void set_interface_vdm(bool val)
{
	is_interface_vdm_flag = val;
}

void interface_initi_setting_en(bool en)
{
	OhioWriteReg(OCM_ACK_STATUS, (en == 1) ? 0x80 : 0x00);
}

void ap_send_msg(unsigned char *pSendBuf, unsigned char len)
{
	sending_len = len;
	memcpy(InterfaceSendBuf, pSendBuf, len);
}

unsigned char interface_recv_timer;
unsigned char interface_send_timer;

enum recive_state_machine {
	RECEIVING_INIT,
	RECEIVING_DATA,
	RECEIVING_CHECKSUM,
	RECEIVING_SUCCESS,
	RECEIVING_FAIL,
	RECEIVING_NUM
};

enum vdm_states {
	VDM_STATE_ERR_BUSY = -3,
	VDM_STATE_ERR_SEND = -2,
	VDM_STATE_ERR_TMOUT = -1,
	VDM_STATE_DONE = 0,
	/* Anything >0 represents an active state */
	VDM_STATE_READY = 1,
	VDM_STATE_BUSY = 2,
	VDM_STATE_WAIT_RSP_BUSY = 3,
};

enum vdm_states pd_port_vdm_state;
unsigned char vdm_data_buf[32];
unsigned char pd_port_vdo_count;

u8 is_recvd_msg_ok(void)
{
	return (recv_status == RECEIVING_SUCCESS ||
		recv_status == RECEIVING_FAIL);
}


u8 imsg_list_begin = 0;
u8 imsg_list_end = 0;
#define empty_imsg_list()  \
	(imsg_list_begin  == imsg_list_end)

#define full_imsg_list()  \
	(imsg_list_begin  == ((imsg_list_end+1) % MAX_INTERFACE_COUNT))

interface_msg_t *imsg_fetch(void)
{
	interface_msg_t *imsg = NULL;

	if (empty_imsg_list())
		return NULL;

	imsg = &imsg_list[imsg_list_begin];
	imsg_list_begin = (imsg_list_begin+1) % MAX_INTERFACE_COUNT;

	return imsg;
}


bool imsg_push(interface_msg_t *imsg)
{
	if (NULL == imsg)
		return 0;

	if (full_imsg_list())
		return 0;

	imsg_list[imsg_list_end] = *imsg;
	imsg_list_end = (imsg_list_end+1) % MAX_INTERFACE_COUNT;

	return 1;
}

interface_msg_t cur_interface_msg;

/* polling private interface interrupt request message
 *  timeout : block timeout time, if = 0, is noblock
 *  return: if return NULL, no message arrived
 *            if > 0, the message pointer, it's alloced by the function,
 *     !!! you should free it, when you don't use it
 */
u8 polling_interface_msg(int timeout_ms)
{
	u8 temp = 0;
	u8 checksum;
	u8 msg_total_len = 0;
	unsigned long expire = MSEC_TO_JIFFIES(timeout_ms) + jiffies;
	u8 rst = 0;
	u8 i = 0;

	/* first check if there is NEW interface message arrived? */
	if (!empty_rx_buf())
		buf_deque(InterfaceRecvBuf);
	else {
		anx_printk(K_INFO, "%s buffer empty\n", __func__);
		return 1; /*there is no data arrived*/
	}

	anx_printk(K_INFO, "%s\n", __func__);

	/* second interface's Data part,
	* check msg's length(index 0) should > 0,
	* not including checksum
	*/
	if (InterfaceRecvBuf[0] > 0) {
		bool need_upd = 0;

		/* printk("IR%x\n", InterfaceRecvBuf[0] ); */
		/* total length of the message =
		* 1B Data length + 1B type + Raw Data+ 1B checksum
		*/
		msg_total_len = InterfaceRecvBuf[0] + 2;

		anx_printk(K_INFO, "ttl len=%x\n", msg_total_len);

		/* copy data to pointer (begin &InterfaceRecvBuf[1]) */
		for (i = 1; i < msg_total_len; ) {
			need_upd = 0;
			while (!empty_rx_buf()) {
				if (buf_deque(&temp)) {
					need_upd = 1;
					InterfaceRecvBuf[i++] = temp;
				}
				if (i == msg_total_len)
					break;
			}

			if (need_upd) {
				/*update rx buffer front*/
				up_rx_front();
			}

			if (time_before(expire, jiffies)) {
				anx_printk(K_INFO, "FRONT[0x%x]=0x%x, REAR[0x%x]=0x%x\n",
					OCM_BUF_FRONT,
					OhioReadReg(OCM_BUF_FRONT),
					OCM_BUF_REAR,
					OhioReadReg(OCM_BUF_REAR));
				rst = 2;
				break;
			}
		}

		/* fetch a completely message, if not timeout event happens */
		if (i == msg_total_len) {
			/* calcalute checksum number,
			 * all checksum should be 0
			 */
			checksum = 0;
			for (temp = 0; temp < msg_total_len; temp++)
				checksum += InterfaceRecvBuf[temp];

			if (checksum == 0) {  /*correct chksm*/
				set_ack_status(0x01);

				memset(&cur_interface_msg,
					0,
					sizeof(cur_interface_msg));

				memcpy(&cur_interface_msg.data,
					InterfaceRecvBuf,
					msg_total_len);
				rst = 0;

				if (!imsg_push(&cur_interface_msg)) {
					/* printk("imsg list is full\n"); */
					rst = 3;
				}
			} else { /*incorrect chksm*/
				set_ack_status(0x02);
				anx_printk(K_INFO, "Chksum Err:%x\n",
						(u8)checksum);
			}

		} else {
			/*set_ack_status(0x02);*/
			anx_printk(K_INFO, "Err: total len=%d, act len=%d\n",
					msg_total_len, i);
			rst = 6;
		}

		/* Remove the print, it is the effect for the power
		 * neotiation timing
		 */
		anx_printk(K_INFO, ">>%s\n",
				interface_to_str(InterfaceRecvBuf[1]));
		printb(InterfaceRecvBuf, i);

	} else {
		anx_printk(K_INFO, "Recv 0 Error\n");
		rst = 5;
	}
	return rst;
}

void interface_send_dp_caps(void)
{
	memcpy(InterfaceSendBuf + 2, configure_DP_caps, 4);
	memcpy(InterfaceSendBuf + 2 + 4, DP_caps, 4);
	interface_send_msg_timeout(TYPE_DP_SNK_CFG,
						InterfaceSendBuf + 2,
						4 + 4,
						INTERFACE_TIMEOUT);
}
void interface_send_status(u8 cmd_type, u8 status)
{
	InterfaceSendBuf[2] = cmd_type;
	InterfaceSendBuf[3] = status;
	interface_send_msg_timeout(TYPE_RESPONSE_TO_REQ,
						InterfaceSendBuf + 2,
						2,
						INTERFACE_TIMEOUT);
}

/*define max request current 3A  and voltage 5V */
#define MAX_REQUEST_VOLTAGE 5000
#define MAX_REQUEST_CURRENT 900
#define set_rdo_value(v0, v1, v2, v3) \
	do { \
		pd_rdo[0] = (v0); \
		pd_rdo[1] = (v1); \
		pd_rdo[2] = (v2); \
		pd_rdo[3] = (v3); \
	} while (0)

u8  build_rdo_from_source_caps(u8 obj_cnt, u8  *buf)
{
	u8 i = 0;
	u16 pdo_h, pdo_l;

	set_rdo_value(0x0A, 0x78, 0x00, 0x10);

	/* get max current ... */
	for (i = 0; i < (obj_cnt & 0x7); i++) {
		u32 pdo_max = 0;

		pdo_l =  buf[i*4+0];
		pdo_l |= (u16)buf[i*4+1] << 8;

		pdo_h  = buf[i*4+2];
		pdo_h |= (u16)buf[i*4+3] << 8;

		/* get max voltage now we can support max is 5V */
		pdo_max = (u16)(((((pdo_h & 0xf) << 6) |
						(pdo_l >> 10)) & 0x3ff) * 50);
		if (pdo_max  <= MAX_REQUEST_VOLTAGE) {
			if ((pdo_h & (3 << 14)) != (PDO_TYPE_BATTERY >> 16)) {
				u16 max_request_ma =
					(u16)((pdo_l & 0x3ff) * 10);
				/* less than 3A */
				if (max_request_ma <= MAX_REQUEST_CURRENT) {

					pdo_max = RDO_FIXED(i+1,
								max_request_ma,
								max_request_ma,
								0);

					anx_printk(K_INFO, "MaxVoltage:%d mV, MaxCurrent %d mA\n",
						(u16)(((((pdo_h & 0xf) << 6) |
						(pdo_l >> 10)) & 0x3ff) * 50),
						max_request_ma);

					if (max_request_ma <= 200)
						pdo_max |=  RDO_CAP_MISMATCH;

					set_rdo_value(pdo_max & 0xff,
						(pdo_max >> 8) & 0xff,
						(pdo_max >> 16) & 0xff,
						(pdo_max >> 24) & 0xff);

					return 1;
				}
			}
		}
	}
	return 0;
}


u8 pd_check_requested_voltage(u32 rdo)
{
	int max_ma = rdo & 0x3FF;
	int op_ma = (rdo >> 10) & 0x3FF;
	int idx = rdo >> 28;

	if (!idx || idx > pd_src_pdo_cnt)
		return 0; /* Invalid index */

	/* check current ... */
	if (op_ma > MAX_REQUEST_CURRENT)
		return 0; /* too much op current */
	if (max_ma > MAX_REQUEST_VOLTAGE)
		return 0; /* too much max current */

	anx_printk(K_INFO, "Requested  %d/%d mA) idx %d\n",
		 ((rdo >> 10) & 0x3ff) * 10, (rdo & 0x3ff) * 10, idx);

	return 1;
}


#if 1
u8 parser_rdo(u8 obj_cnt, u8  *buf)
{
	u8 i = 0;
	u16 pdo_h, pdo_l;

	/* get max current ... */
	for (i = 0; i < (obj_cnt & 0x7); i++) {
		u32 pdo_max = 0;

		pdo_l = buf[i*4+0];
		pdo_l |= (u16)buf[i*4+1] << 8;

		pdo_h  = buf[i*4+2];
		pdo_h |= (u16)buf[i*4+3] << 8;

		/* get max voltage now we can support max is 5V */
		pdo_max = (u16)(((((pdo_h & 0xf) << 6) |
					 (pdo_l >> 10)) & 0x3ff) * 50);
		if (pdo_max <= MAX_REQUEST_VOLTAGE) {
			if ((pdo_h & (3 << 14)) != (PDO_TYPE_BATTERY >> 16)) {
				u16 max_req_ma = (u16)((pdo_l & 0x3ff) * 10);
				/* larger than current max */
				if (max_req_ma > MAX_REQUEST_CURRENT)
					return 0;
			}
		} else
		return 0;
	}
	return 1;
}
#endif

/* Receive Power Delivery Source Capability message's callback function.
  * it can be rewritten by customer just reimmplement this function,
  * through register_default_pd_message_callbacku_func
  *  void *para : in this function it means PDO pointer
  *   para_len : means PDO length
  * return:  0, fail;   1, success
  */
u8 recv_pd_source_caps_default_callback(void *para, u8 para_len)
{
	u8 *pdo = 0;

	pdo = (u8 *)para;

	if ((para_len % 4) != 0)
		return 0;

	if (build_rdo_from_source_caps(para_len/4, para))
		interface_send_request();

	return 1;
}

/* Receive Power Delivery Source Capability message's callback function.
  * it can be rewritten by customer just reimmplement this function,
  * through register_default_pd_message_callbacku_func
  *  void *para : in this function it means PDO pointer
  *   para_len : means PDO length
  * return:  0, fail;   1, success
  */
u8 recv_pd_sink_caps_default_callback(void *para, u8 para_len)
{
	u8 *pdo = 0;

	pdo = (u8 *)para;

	if ((para_len % 4) != 0)
		return 0;

	if (para_len > VDO_SIZE)
		return 0;

	memcpy(pd_snk_pdo, para, para_len);
	pd_snk_pdo_cnt = para_len / 4;
	return 1;
}

/* Receive Power Delivery Source Capability message's callback function.
  * it can be rewritten by customer just reimmplement this function,
  * through register_default_pd_message_callbacku_func
  *  void *para : in this function it means PDO pointer
  *   para_len : means PDO length
  * return:  0, fail;   1, success
  */
u8 recv_pd_pwr_object_req_default_callback(void *para, u8 para_len)
{
	u8 *pdo = (u8 *)para;
	u32 rdo = 0;

	if ((para_len % 4) != 0)
		return 0;

	if (para_len != 4)
		return 0;

	rdo = pdo[0] | (pdo[1] << 8) |  (pdo[2] << 16) | (pdo[3] << 24);

	if (pd_check_requested_voltage(rdo))
		send_accept();
	else
		interface_send_reject();

	return 1;
}

/* Receive accept message's callback function.
  * it can be rewritten by customer just reimmplement this function,
  * through register_default_pd_message_callbacku_func
  *  void *para : should be null
  *   para_len : 0
  * return:  0, fail;   1, success
  */
u8 recv_pd_accept_default_callback(void *para, u8 para_len)
{
	para = para;
	para_len = para_len;

	return 1;
}

/* Receive reject message's callback function.
  * it can be rewritten by customer just reimmplement this function,
  * through register_default_pd_message_callbacku_func
  *  void *para : should be null
  *   para_len : 0
  * return:  0, fail;   1, success
  */
u8 recv_pd_reject_default_callback(void *para, u8 para_len)
{
	para = para;
	para_len = para_len;

	return 1;
}

/* Receive reject message's callback function.
  * it can be rewritten by customer just reimmplement this function,
  * through register_default_pd_message_callbacku_func
  *  void *para : should be null
  *   para_len : 0
  * return:  0, fail;   1, success
  */
u8 recv_pd_goto_min_default_callback(void *para, u8 para_len)
{
	para = para;
	para_len = para_len;

	return 1;
}

/*PD Status command response, default callback function.
  *It can be change by customer for redevelopment
  * Byte0: CC status from ohio,
  * Byte1: misc status from ohio
  * Byte2: debug ocm FSM state from ohio
  */
u8 cc_status  = 0;
u8 misc_status = 0;
u8 pd_fsm_status = 0;

void interface_get_status_result(void)
{
	cc_status = InterfaceRecvBuf[3];
	anx_printk(K_INFO, "Ohio CC Status:%x\n", cc_status);

	misc_status = InterfaceRecvBuf[4];
	anx_printk(K_INFO, "Ohio misc status:%x\n", misc_status);

	pd_fsm_status = InterfaceRecvBuf[5];
	anx_printk(K_INFO, "Ohio pd_fsm_status:%x\n", pd_fsm_status);
}

u8 recv_pd_cmd_rsp_default_callback(void *para, u8 para_len)
{
	u8 need_notice_pd_cmd = 0;

	para = para;
	para_len = para_len;

	/* pr_info("recv_pd_cmd_rsp_default_callback req_type%x,
				response_status %x\n",
				RESPONSE_REQ_TYPE(), RESPONSE_REQ_RESULT()); */

	switch (RESPONSE_REQ_TYPE()) {
	case TYPE_PD_STATUS_REQ:
		usb_pd_cmd_status = CMD_SUCCESS;
		/*get cc result to Ohio */
		interface_get_status_result();
		break;

	case TYPE_DSWAP_REQ:
		need_notice_pd_cmd = 1;
		usb_pd_cmd_status = RESPONSE_REQ_RESULT();

		if (RESPONSE_REQ_RESULT() == CMD_SUCCESS)
			anx_printk(K_INFO, "pd_cmd DRSwap result is successful\n");
		else if (RESPONSE_REQ_RESULT() == CMD_REJECT)
			anx_printk(K_INFO, "pd_cmd DRSwap result is rejected\n");
		else if (RESPONSE_REQ_RESULT() == CMD_BUSY)
			anx_printk(K_INFO, "pd_cmd DRSwap result is busy\n");
		else if (RESPONSE_REQ_RESULT() == CMD_FAIL)
			anx_printk(K_INFO, "pd_cmd DRSwap result is fail\n");
		else
			anx_printk(K_INFO, "pd_cmd DRSwap result is unknown\n");

		break;

	case TYPE_PSWAP_REQ:
		need_notice_pd_cmd = 1;
		usb_pd_cmd_status = RESPONSE_REQ_RESULT();

		if (RESPONSE_REQ_RESULT() == CMD_SUCCESS)
			anx_printk(K_INFO, "pd_cmd PRSwap result is successful\n");
		else if (RESPONSE_REQ_RESULT() == CMD_REJECT)
			anx_printk(K_INFO, "pd_cmd PRSwap result is rejected\n");
		else if (RESPONSE_REQ_RESULT() == CMD_BUSY)
			anx_printk(K_INFO, "pd_cmd PRSwap result is busy\n");
		else if (RESPONSE_REQ_RESULT() == CMD_FAIL)
			anx_printk(K_INFO, "pd_cmd PRSwap result is fail\n");
		else
			anx_printk(K_INFO, "pd_cmd PRSwap result is unknown\n");

		break;

	default:
		break;
	}

	if (need_notice_pd_cmd) {
		/* check pd cmd has been locked ?*/
		write_lock_irq(&usb_pd_cmd_rwlock);
		if (usb_pd_cmd_counter)
			usb_pd_cmd_counter = 0;
		write_unlock_irq(&usb_pd_cmd_rwlock);
	}

	return CMD_SUCCESS;
}

u8 pd_check_data_swap(void)
{
	u8 i = 0;
	u8 drp_role_support = 0;

	for (i = 0; i < pd_src_pdo_cnt; i += 4) {
		if (PDO_TYPE_FIXED == (((pd_src_pdo[i+3] & 0xC)<<24)))
			if (PDO_FIXED_DUAL_ROLE & ((pd_src_pdo[i+3]<<24)))
				drp_role_support = 1;
	}
	return drp_role_support;
}

u8 pd_check_power_swap(void)
{
	u8 i = 0;
	u8 drp_role_support = 0;

	for (i = 0; i < pd_src_pdo_cnt; i += 4) {
		if (PDO_TYPE_FIXED == (((pd_src_pdo[i+3] & 0xC)<<24)))
			if (PDO_FIXED_DUAL_ROLE & ((pd_src_pdo[i+3]<<24)))
				drp_role_support = 1;
	}
	return drp_role_support;
}


/* Receive Data Role Swap message's callback function.
  * it can be rewritten by customer just reimmplement this function,
  * through init_pd_msg_callback, it it pd_callback is not 0, using the default
  *  void *para : in this function it means PDO pointer
  *   para_len : means PDO length
  * return:  0, fail;   1, success
  */
u8 recv_pd_dswap_default_callback(void *para, u8 para_len)
{
	if (pd_check_data_swap())
		interface_send_accept();
	else
		interface_send_reject();

	return 1;
}


/* Receive Power Role Swap message's callback function.
  * it can be rewritten by customer just reimmplement this function,
  * through init_pd_msg_callback, it it pd_callback is not 0, using the default
  *  void *para : in this function it means PDO pointer
  *   para_len : means PDO length
  * return:  0, fail;   1, success
  */
u8 recv_pd_pswap_default_callback(void *para, u8 para_len)
{
	if (pd_check_power_swap())
		interface_send_accept();
	else
		interface_send_reject();

	return 1;
}

static pd_callback_t pd_callback_array[256] = { 0 };

pd_callback_t get_pd_callback_fnc(PD_MSG_TYPE type)
{
	pd_callback_t fnc = 0;

	if (type < 256)
		fnc = pd_callback_array[type];

	return fnc;

}

void set_pd_callback_fnc(PD_MSG_TYPE type, pd_callback_t fnc)
{
	pd_callback_array[type] = fnc;
}

void init_pd_msg_callback(void)
{
	u8 i = 0;

	for (i = 0; i < 256; i++)
		pd_callback_array[i] = 0x0;
}


