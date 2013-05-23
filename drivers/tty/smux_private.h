/* drivers/tty/smux_private.h
 *
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef SMUX_PRIVATE_H
#define SMUX_PRIVATE_H

#define SMUX_MAX_PKT_SIZE   8192
#define SMUX_BROADCAST_LCID 0xFF

/* SMUX Protocol Characters */
#define SMUX_MAGIC          0x33FC
#define SMUX_MAGIC_WORD1    0xFC
#define SMUX_MAGIC_WORD2    0x33
#define SMUX_WAKEUP_REQ     0xFD
#define SMUX_WAKEUP_ACK     0xFE

/* Unit testing characters */
#define SMUX_UT_ECHO_REQ    0xF0
#define SMUX_UT_ECHO_ACK_OK 0xF1
#define SMUX_UT_ECHO_ACK_FAIL 0xF2

/* Maximum number of packets in retry queue */
#define SMUX_RX_RETRY_MAX_PKTS 128
#define SMUX_RX_WM_HIGH          4
#define SMUX_RX_WM_LOW           0
#define SMUX_TX_WM_LOW           2
#define SMUX_TX_WM_HIGH          4

struct tty_struct;

/**
 * Logical Channel Structure.  One instance per channel.
 *
 * Locking Hierarchy
 * Each lock has a postfix that describes the locking level.  If multiple locks
 * are required, only increasing lock hierarchy numbers may be locked which
 * ensures avoiding a deadlock.
 *
 * Locking Example
 * If state_lock_lhb1 is currently held and the TX list needs to be
 * manipulated, then tx_lock_lhb2 may be locked since it's locking hierarchy
 * is greater.  However, if tx_lock_lhb2 is held, then state_lock_lhb1 may
 * not be acquired since it would result in a deadlock.
 *
 * Note that the Line Discipline locks (*_lha) should always be acquired
 * before the logical channel locks.
 */
struct smux_lch_t {
	/* channel state */
	spinlock_t state_lock_lhb1;
	uint8_t lcid;
	unsigned local_state;
	unsigned local_mode;
	uint8_t local_tiocm;
	unsigned options;

	unsigned remote_state;
	unsigned remote_mode;
	uint8_t remote_tiocm;

	int tx_flow_control;
	int rx_flow_control_auto;
	int rx_flow_control_client;

	/* client callbacks and private data */
	void *priv;
	void (*notify)(void *priv, int event_type, const void *metadata);
	int (*get_rx_buffer)(void *priv, void **pkt_priv, void **buffer,
								int size);

	/* RX Info */
	struct list_head rx_retry_queue;
	unsigned rx_retry_queue_cnt;
	struct delayed_work rx_retry_work;

	/* TX Info */
	spinlock_t tx_lock_lhb2;
	struct list_head tx_queue;
	struct list_head tx_ready_list;
	unsigned tx_pending_data_cnt;
	unsigned notify_lwm;
};

/* Each instance of smux_lch_t */
extern struct smux_lch_t smux_lch[SMUX_NUM_LOGICAL_CHANNELS];

/* Packet header. */
struct smux_hdr_t {
	uint16_t magic;
	uint8_t flags;
	uint8_t cmd;
	uint8_t pad_len;
	uint8_t lcid;
	uint16_t payload_len;
};

/* Internal packet structure. */
struct smux_pkt_t {
	struct smux_hdr_t hdr;
	int allocated;
	unsigned char *payload;
	int free_payload;
	struct list_head list;
	void *priv;
};

/* SMUX Packet Commands */
enum {
	SMUX_CMD_DATA = 0x0,
	SMUX_CMD_OPEN_LCH = 0x1,
	SMUX_CMD_CLOSE_LCH = 0x2,
	SMUX_CMD_STATUS = 0x3,
	SMUX_CMD_PWR_CTL = 0x4,
	SMUX_CMD_DELAY = 0x5,

	SMUX_CMD_BYTE, /* for internal usage */
	SMUX_NUM_COMMANDS
};

/* Open command flags */
enum {
	SMUX_CMD_OPEN_ACK = 1 << 0,
	SMUX_CMD_OPEN_POWER_COLLAPSE = 1 << 1,
	SMUX_CMD_OPEN_REMOTE_LOOPBACK = 1 << 2,
};

/* Close command flags */
enum {
	SMUX_CMD_CLOSE_ACK = 1 << 0,
};

/* Power command flags */
enum {
	SMUX_CMD_PWR_CTL_ACK =  1 << 0,
};

/* Local logical channel states */
enum {
	SMUX_LCH_LOCAL_CLOSED,
	SMUX_LCH_LOCAL_OPENING,
	SMUX_LCH_LOCAL_OPENED,
	SMUX_LCH_LOCAL_CLOSING,
};

/* Remote logical channel states */
enum {
	SMUX_LCH_REMOTE_CLOSED,
	SMUX_LCH_REMOTE_OPENED,
};

/* Enum used to report various undefined actions */
enum {
	SMUX_UNDEF_LONG,
	SMUX_UNDEF_SHORT,
};

long msm_smux_tiocm_get_atomic(struct smux_lch_t *ch);
const char *local_lch_state(unsigned state);
const char *remote_lch_state(unsigned state);
const char *lch_mode(unsigned mode);

int smux_assert_lch_id(uint32_t lcid);
void smux_init_pkt(struct smux_pkt_t *pkt);
struct smux_pkt_t *smux_alloc_pkt(void);
int smux_alloc_pkt_payload(struct smux_pkt_t *pkt);
void smux_free_pkt(struct smux_pkt_t *pkt);
int smux_serialize(struct smux_pkt_t *pkt, char *out,
					unsigned int *out_len);

void smux_rx_state_machine(const unsigned char *data, int len, int flag);
void smuxld_receive_buf(struct tty_struct *tty, const unsigned char *cp,
			   char *fp, int count);
bool smux_remote_is_active(void);
void smux_set_loopback_data_reply_delay(uint32_t ms);
void smux_get_wakeup_counts(int *local_cnt, int *remote_cnt);

/* testing parameters */
extern int smux_byte_loopback;
extern int smux_simulate_wakeup_delay;

#endif /* SMUX_PRIVATE_H */
