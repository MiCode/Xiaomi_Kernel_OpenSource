#ifndef RSMC_DEVICE_X801_H
#define RSMC_DEVICE_X801_H

#include <linux/mutex.h>
#include <linux/semaphore.h>

#include "rsmc_msg_loop_x801.h"

#define RSMC_IC_NAME "rsmc"
#define RSMC_DEV_NODE_NAME "rsmc-x801"
#define RSMC_SYNC_HEAD_LEN 40
#define RSMC_DATA_MAX_LEN 1024
#define RSMC_REG_NUM 20
#define RSMC_RF_NUM 20
#define RSMC_MAX_BIN_LEN 400
#define RSMC_MAX_BIN_LEN 400

enum RSMC_ANT_STATUS {
	RSMC_RNSS_RX = 0,
	RSMC_RDSS_RX = 1,
	RSMC_RDSS_TX = 2,
};

enum RSMC_TRK_STATUS {
	RSMC_TRK_TX = 0,
	RSMC_TRK_AGAIN = 1,
	RSMC_CAP_AGAIN = 2,
};

enum RSMC_DTX_STATUS {
	RSMC_RDSS_RX_AFTER_DTX = 0,
	RSMC_RNSS_RX_AFTER_DTX = 1,
	RSMC_HOLD_TX_AFTER_DTX = 2,
};

enum RSMC_SOC_CMD_RESULT {
	CMD_SUCC = 0,
	CMD_FAIL = 1,
	CMD_FAIL_FORMAT_ERR = 1,
	CMD_FAIL_CRC_ERR = 2,
	CMD_TX_CMD_ACK = 3,
	CMD_TX_FAIL_FIFO_FULL = 4,
	CMD_TX_FAIL_FIFO_EMPTY = 5,
	CMD_TX_FAIL_OTHER = 6
};

enum RSMC_RX_FREQ {
	RX_FREQ_L1CA = 0,
	RX_FREQ_B1I = 1,
	RX_FREQ_S2C = 2
};

struct enable_msg {
	struct msg_head head;
	u32 status;
	u32 file_len;
	u32 data_block_len;
	u32 data_block_idx;
};

enum RSMC_ENABLE_CAUSE {
	EN_SUCC = 0,
	EN_FAIL_FW_TRANS_FAIL,
	EN_FAIL_PLL_CFG,
	EN_FAIL_NOT_RECV_HT,
	EN_FAIL_HANDSHAKE_FAIL,
	EN_FAIL_FW_CRC_FAIL,
	EN_FAIL_NO_STA,
	EN_FAIL_NO_WMS,
	EN_FAIL_NO_WMS_ACK,
	EN_FAIL_NO_CFG,
	EN_FAIL_NO_CFG_ACK,
	EN_FAIL_OTHER = 0xFF
};

struct enable_cnf_msg {
	struct msg_head head;
	s32 result;
	u32 type; // 1: success 0: failure
	u32 cause;
};

struct any_msg {
	struct msg_head head;
	char data[RSMC_MAX_BIN_LEN];
};

struct dtx_msg {
	struct msg_head head;
	u32 status;
	char data[RSMC_MAX_BIN_LEN];
};

msg_process *rsmc_device_reg_x801(notify_event *fun);
void rsmc_device_unreg_x801(int reason);
void send_msg_to_ctrl_x801(struct msg_head *msg);
void rsmc_spi_recv_proc(void);
u32 get_spi_status(void);
void set_spi_status(u32 spi_status);
void up_spi_sema(void);
void rsmc_start_spi_recv_timer(void);
bool get_chip_on_status(void);

#endif /* RSMC_DEVICE_H */

