/* include/linux/smux.h
 *
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
#ifndef SMUX_H
#define SMUX_H

/**
 * Logical Channel IDs
 *
 * This must be identical between local and remote clients.
 */
enum {
	/* Data Ports */
	SMUX_DATA_0,
	SMUX_DATA_1,
	SMUX_DATA_2,
	SMUX_DATA_3,
	SMUX_DATA_4,
	SMUX_DATA_5,
	SMUX_DATA_6,
	SMUX_DATA_7,
	SMUX_DATA_8,
	SMUX_DATA_9,
	SMUX_USB_RMNET_DATA_0,
	SMUX_USB_DUN_0,
	SMUX_USB_DIAG_0,
	SMUX_SYS_MONITOR_0,
	SMUX_CSVT_0,
	/* add new data ports here */

	/* Control Ports */
	SMUX_DATA_CTL_0 = 32,
	SMUX_DATA_CTL_1,
	SMUX_DATA_CTL_2,
	SMUX_DATA_CTL_3,
	SMUX_DATA_CTL_4,
	SMUX_DATA_CTL_5,
	SMUX_DATA_CTL_6,
	SMUX_DATA_CTL_7,
	SMUX_DATA_CTL_8,
	SMUX_DATA_CTL_9,
	SMUX_USB_RMNET_CTL_0,
	SMUX_USB_DUN_CTL_0_UNUSED,
	SMUX_USB_DIAG_CTL_0,
	SMUX_SYS_MONITOR_CTL_0,
	SMUX_CSVT_CTL_0,
	/* add new control ports here */

	SMUX_TEST_LCID,
	SMUX_NUM_LOGICAL_CHANNELS,
};

/**
 * Notification events that are passed to the notify() function.
 *
 * If the @metadata argument in the notifier is non-null, then it will
 * point to the associated struct smux_meta_* structure.
 */
enum {
	SMUX_CONNECTED,       /* @metadata is null */
	SMUX_DISCONNECTED,
	SMUX_READ_DONE,
	SMUX_READ_FAIL,
	SMUX_WRITE_DONE,
	SMUX_WRITE_FAIL,
	SMUX_TIOCM_UPDATE,
	SMUX_LOW_WM_HIT,      /* @metadata is NULL */
	SMUX_HIGH_WM_HIT,     /* @metadata is NULL */
	SMUX_RX_RETRY_HIGH_WM_HIT,  /* @metadata is NULL */
	SMUX_RX_RETRY_LOW_WM_HIT,   /* @metadata is NULL */
};

/**
 * Channel options used to modify channel behavior.
 */
enum {
	SMUX_CH_OPTION_LOCAL_LOOPBACK = 1 << 0,
	SMUX_CH_OPTION_REMOTE_LOOPBACK = 1 << 1,
	SMUX_CH_OPTION_REMOTE_TX_STOP = 1 << 2,
	SMUX_CH_OPTION_AUTO_REMOTE_TX_STOP = 1 << 3,
};

/**
 * Metadata for SMUX_DISCONNECTED notification
 *
 * @is_ssr:  Disconnect caused by subsystem restart
 */
struct smux_meta_disconnected {
	int is_ssr;
};

/**
 * Metadata for SMUX_READ_DONE/SMUX_READ_FAIL notification
 *
 * @pkt_priv: Packet-specific private data
 * @buffer:   Buffer pointer passed into msm_smux_write
 * @len:      Buffer length passed into  msm_smux_write
 */
struct smux_meta_read {
	void *pkt_priv;
	void *buffer;
	int len;
};

/**
 * Metadata for SMUX_WRITE_DONE/SMUX_WRITE_FAIL notification
 *
 * @pkt_priv: Packet-specific private data
 * @buffer:  Buffer pointer returned by get_rx_buffer()
 * @len:     Buffer length returned by get_rx_buffer()
 */
struct smux_meta_write {
	void *pkt_priv;
	void *buffer;
	int len;
};

/**
 * Metadata for SMUX_TIOCM_UPDATE notification
 *
 * @tiocm_old:  Previous TIOCM state
 * @tiocm_new:   Current TIOCM state
 */
struct smux_meta_tiocm {
	uint32_t tiocm_old;
	uint32_t tiocm_new;
};


#ifdef CONFIG_N_SMUX
/**
 * Starts the opening sequence for a logical channel.
 *
 * @lcid          Logical channel ID
 * @priv          Free for client usage
 * @notify        Event notification function
 * @get_rx_buffer Function used to provide a receive buffer to SMUX
 *
 * @returns 0 for success, <0 otherwise
 *
 * A channel must be fully closed (either not previously opened or
 * msm_smux_close() has been called and the SMUX_DISCONNECTED has been
 * recevied.
 *
 * One the remote side is opened, the client will receive a SMUX_CONNECTED
 * event.
 */
int msm_smux_open(uint8_t lcid, void *priv,
	void (*notify)(void *priv, int event_type, const void *metadata),
	int (*get_rx_buffer)(void *priv, void **pkt_priv,
					void **buffer, int size));

/**
 * Starts the closing sequence for a logical channel.
 *
 * @lcid    Logical channel ID
 * @returns 0 for success, <0 otherwise
 *
 * Once the close event has been acknowledge by the remote side, the client
 * will receive a SMUX_DISCONNECTED notification.
 */
int msm_smux_close(uint8_t lcid);

/**
 * Write data to a logical channel.
 *
 * @lcid      Logical channel ID
 * @pkt_priv  Client data that will be returned with the SMUX_WRITE_DONE or
 *            SMUX_WRITE_FAIL notification.
 * @data      Data to write
 * @len       Length of @data
 *
 * @returns   0 for success, <0 otherwise
 *
 * Data may be written immediately after msm_smux_open() is called, but
 * the data will wait in the transmit queue until the channel has been
 * fully opened.
 *
 * Once the data has been written, the client will receive either a completion
 * (SMUX_WRITE_DONE) or a failure notice (SMUX_WRITE_FAIL).
 */
int msm_smux_write(uint8_t lcid, void *pkt_priv, const void *data, int len);

/**
 * Returns true if the TX queue is currently full (high water mark).
 *
 * @lcid      Logical channel ID
 *
 * @returns   0 if channel is not full; 1 if it is full; < 0 for error
 */
int msm_smux_is_ch_full(uint8_t lcid);

/**
 * Returns true if the TX queue has space for more packets it is at or
 * below the low water mark).
 *
 * @lcid      Logical channel ID
 *
 * @returns   0 if channel is above low watermark
 *            1 if it's at or below the low watermark
 *            < 0 for error
 */
int msm_smux_is_ch_low(uint8_t lcid);

/**
 * Get the TIOCM status bits.
 *
 * @lcid      Logical channel ID
 *
 * @returns   >= 0 TIOCM status bits
 *            < 0  Error condition
 */
long msm_smux_tiocm_get(uint8_t lcid);

/**
 * Set/clear the TIOCM status bits.
 *
 * @lcid      Logical channel ID
 * @set       Bits to set
 * @clear     Bits to clear
 *
 * @returns   0 for success; < 0 for failure
 *
 * If a bit is specified in both the @set and @clear masks, then the clear bit
 * definition will dominate and the bit will be cleared.
 */
int msm_smux_tiocm_set(uint8_t lcid, uint32_t set, uint32_t clear);

/**
 * Set or clear channel option using the SMUX_CH_OPTION_* channel
 * flags.
 *
 * @lcid   Logical channel ID
 * @set    Options to set
 * @clear  Options to clear
 *
 * @returns 0 for success, < 0 for failure
 */
int msm_smux_set_ch_option(uint8_t lcid, uint32_t set, uint32_t clear);

#else
static inline int msm_smux_open(uint8_t lcid, void *priv,
	void (*notify)(void *priv, int event_type, const void *metadata),
	int (*get_rx_buffer)(void *priv, void **pkt_priv,
					void **buffer, int size))
{
	return -ENODEV;
}

static inline int msm_smux_close(uint8_t lcid)
{
	return -ENODEV;
}

static inline int msm_smux_write(uint8_t lcid, void *pkt_priv,
				const void *data, int len)
{
	return -ENODEV;
}

static inline int msm_smux_is_ch_full(uint8_t lcid)
{
	return -ENODEV;
}

static inline int msm_smux_is_ch_low(uint8_t lcid)
{
	return -ENODEV;
}

static inline long msm_smux_tiocm_get(uint8_t lcid)
{
	return 0;
}

static inline int msm_smux_tiocm_set(uint8_t lcid, uint32_t set, uint32_t clear)
{
	return -ENODEV;
}

static inline int msm_smux_set_ch_option(uint8_t lcid, uint32_t set,
					uint32_t clear)
{
	return -ENODEV;
}

#endif /* CONFIG_N_SMUX */

#endif /* SMUX_H */
