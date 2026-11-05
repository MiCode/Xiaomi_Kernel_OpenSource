#ifndef RSMC_MSG_LOOP_X801_H
#define RSMC_MSG_LOOP_X801_H

#include <linux/types.h>

/* Notification request issued by the upper layer is defined as: */
struct msg_head {
	u16 type; // Event enumeration values
	u16 len; // The length behind this field, the limit lower 2048
	u32 module;
};

/*
 * This enumeration type is the netlink command types issued by the JNI.
 * Mainly to maintain the channel with JNI.
 */
enum nl_cmd_type {
	NL_MSG_REG = 0,
	NL_MSG_REQ,
	NL_MSG_HEARTBEAT,
	NB_MSG_REQ_BUTT
};

/* down enumeration message after this enumeration value. */
enum msg_type_dn {
	CMD_DN_INIT_REQ = 0,
	CMD_DN_ANY_REQ = 1,
	CMD_DN_DTX_REQ = 2,
	CMD_DN_MSG_NUM
};

/* kernel internal enumeration message after this enumeration value */
enum msg_type_inter {
	CMD_INTER_INIT_REQ = 2000,
	CMD_INTER_HB_TIMER_REQ = 2001,
	CMD_INTER_START_SPI_TIMER = 2002,
	CMD_INTER_STOP_SPI_TIMER = 2003,
	CMD_INTER_WAIT_STA_TIMEOUT = 2004,
	CMD_INT_MSG_NUM,
};

/* up enumeration message after this enumeration value. */
enum msg_type_up {
	CMD_UP_INIT_CNF = 1000,
	CMD_UP_ANY_IND,
	CMD_UP_SOC_ERR_IND,
	CMD_UP_MSG_NUM,
};

/* Message list structure */
struct msg_entity {
	struct list_head head;
	struct msg_head msg;
};

/* slow Message send function */
typedef void notify_event(struct msg_head *msg);
typedef void msg_process(struct msg_head *req);
typedef msg_process* model_reg(notify_event *fun);
typedef void model_unreg(int reason);
/* fast Message send function */
void rsmc_clear_dn_inter_msg_list(void);
void rsmc_clear_dn_spi_msg_list(void);
void rsmc_nl_dn_msg_notify_inter_nonblock(struct msg_head *msg);
struct msg_entity *rsmc_nl_dn_get_spi_msg(void);
void rsmc_process_cmd_x801(struct msg_head *cmd);
void rsmc_start_heartbeat_x801(void);
void rsmc_stop_heartbeat_x801(void);
void rsmc_update_soc_rx_jiffies(void);
void rsmc_update_soc_tx_jiffies(void);
int rsmc_init_x801(void);
void rsmc_exit_x801(void);
void send_up_soc_err(void);
u32 get_rsmc_spi_status(void);

#endif /* RSMC_MSG_LOOP_H */

