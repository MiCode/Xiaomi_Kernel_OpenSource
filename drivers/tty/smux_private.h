/* drivers/tty/smux_private.h
 *
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

struct tty_struct;

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
	SMUX_CMD_PWR_CTL_SLEEP_REQ =  1 << 1,
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

/* testing parameters */
extern int smux_byte_loopback;
extern int smux_simulate_wakeup_delay;

#endif /* SMUX_PRIVATE_H */
