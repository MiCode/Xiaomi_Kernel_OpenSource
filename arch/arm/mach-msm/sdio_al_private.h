/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*
 * SDIO-Abstraction-Layer internal interface.
 */

#ifndef __SDIO_AL_PRIVATE__
#define __SDIO_AL_PRIVATE__

#include <linux/mmc/card.h>
#include <linux/platform_device.h>
#include <mach/sdio_al.h>

#define DRV_VERSION "1.30"
#define MODULE_NAME "sdio_al"
#define SDIOC_CHAN_TO_FUNC_NUM(x)	((x)+2)
#define REAL_FUNC_TO_FUNC_IN_ARRAY(x)	((x)-1)
#define SDIO_PREFIX "SDIO_"
#define PEER_CHANNEL_NAME_SIZE		4
#define CHANNEL_NAME_SIZE (sizeof(SDIO_PREFIX) + PEER_CHANNEL_NAME_SIZE)
#define SDIO_TEST_POSTFIX_SIZE 5
#define MAX_NUM_OF_SDIO_DEVICES	2
#define TEST_CH_NAME_SIZE (CHANNEL_NAME_SIZE + SDIO_TEST_POSTFIX_SIZE)

struct sdio_al_device; /* Forward Declaration */

enum sdio_channel_state {
	SDIO_CHANNEL_STATE_INVALID,	 /* before reading software header */
	SDIO_CHANNEL_STATE_IDLE,         /* channel valid, not opened    */
	SDIO_CHANNEL_STATE_CLOSED,       /* was closed */
	SDIO_CHANNEL_STATE_OPEN,	 /* opened */
	SDIO_CHANNEL_STATE_CLOSING,      /* during flush, when closing */
};
/**
 * Peer SDIO-Client channel configuration.
 *
 *  @is_ready - channel is ready and the data is valid.
 *
 *  @max_rx_threshold - maximum rx threshold, according to the
 *		total buffers size on the peer pipe.
 *  @max_tx_threshold - maximum tx threshold, according to the
 *		total buffers size on the peer pipe.
 *  @tx_buf_size - size of a single buffer on the peer pipe; a
 *		transfer smaller than the buffer size still
 *		make the buffer unusable for the next transfer.
 * @max_packet_size
 * @is_host_ok_to_sleep - Host marks this bit when it's okay to
 *		sleep (no pending transactions)
 */
struct peer_sdioc_channel_config {
	u32 is_ready;
	u32 max_rx_threshold; /* Downlink */
	u32 max_tx_threshold; /* Uplink */
	u32 tx_buf_size;
	u32 max_packet_size;
	u32 is_host_ok_to_sleep;
	u32 is_packet_mode;
	u32 peer_operation;
	u32 reserved[24];
};


/**
 * Peer SDIO-Client channel statsitics.
 *
 * @last_any_read_avail - the last read avail in all the
 *		 channels including this channel.
 * @last_read_avail - the last read_avail that was read from HW
 *	    mailbox.
 * @last_old_read_avail - the last read_avail channel shadow.
 * @total_notifs - the total number of read notifications sent
 *	 to this channel client
 * @total_read_times - the total number of successful sdio_read
 *	     calls for this channel
 */
struct sdio_channel_statistics {
	int last_any_read_avail;
	int last_read_avail;
	int last_old_read_avail;
	int total_notifs;
	int total_read_times;
};

/**
 *  SDIO Channel context.
 *
 *  @name - channel name. Used by the caller to open the
 *	  channel.
 *
 *  @read_threshold - Threshold on SDIO-Client mailbox for Rx
 *				Data available bytes. When the limit exceed
 *				the SDIO-Client generates an interrupt to the
 *				host.
 *
 *  @write_threshold - Threshold on SDIO-Client mailbox for Tx
 *				Data available bytes. When the limit exceed
 *				the SDIO-Client generates an interrupt to the
 *				host.
 *
 *  @def_read_threshold - Default theshold on SDIO-Client for Rx
 *
 *  @min_write_avail - Threshold of minimal available bytes
 *					 to write. Below that threshold the host
 *					 will initiate reading the mailbox.
 *
 *  @poll_delay_msec - Delay between polling the mailbox. When
 *				 the SDIO-Client doesn't generates EOT
 *				 interrupt for Rx Available bytes, the host
 *				 should poll the SDIO-Client mailbox.
 *
 *  @is_packet_mode - The host get interrupt when a packet is
 *				available at the SDIO-client (pipe EOT
 *				indication).
 *
 *  @num - channel number.
 *
 *  @notify - Client's callback. Should not call sdio read/write.
 *
 *  @priv - Client's private context, provided to callback.
 *
 *  @is_valid - Channel is used (we have a list of
 *		SDIO_AL_MAX_CHANNELS and not all of them are in
 *		use).
 *
 *  @is_open - Channel is open.
 *
 *  @func - SDIO Function handle.
 *
 *  @rx_pipe_index - SDIO-Client Pipe Index for Rx Data.
 *
 *  @tx_pipe_index - SDIO-Client Pipe Index for Tx Data.
 *
 *  @ch_lock - Channel lock to protect channel specific Data
 *
 *  @rx_pending_bytes - Total number of Rx pending bytes, at Rx
 *				  packet list. Maximum of 16KB-1 limited by
 *				  SDIO-Client specification.
 *
 *  @read_avail - Available bytes to read.
 *
 *  @write_avail - Available bytes to write.
 *
 *  @rx_size_list_head - The head of Rx Pending Packets List.
 *
 *  @pdev - platform device - clients to probe for the sdio-al.
 *
 *  @signature - Context Validity check.
 *
 *  @sdio_al_dev - a pointer to the sdio_al_device instance of
 *   this channel
 *
 *   @statistics - channel statistics
 *
 */
struct sdio_channel {
	/* Channel Configuration Parameters*/
	char name[CHANNEL_NAME_SIZE];
	char ch_test_name[TEST_CH_NAME_SIZE];
	int read_threshold;
	int write_threshold;
	int def_read_threshold;
	int threshold_change_cnt;
	int min_write_avail;
	int poll_delay_msec;
	int is_packet_mode;

	struct peer_sdioc_channel_config ch_config;

	/* Channel Info */
	int num;

	void (*notify)(void *priv, unsigned channel_event);
	void *priv;

	int state;

	struct sdio_func *func;

	int rx_pipe_index;
	int tx_pipe_index;

	struct mutex ch_lock;

	u32 read_avail;
	u32 write_avail;

	u32 peer_tx_buf_size;

	u16 rx_pending_bytes;

	struct list_head rx_size_list_head;

	struct platform_device *pdev;

	u32 total_rx_bytes;
	u32 total_tx_bytes;

	u32 signature;

	struct sdio_al_device *sdio_al_dev;

	struct sdio_channel_statistics statistics;
};

/**
 * sdio_downloader_setup
 * initializes the TTY driver
 *
 * @card: a pointer to mmc_card.
 * @num_of_devices: number of devices.
 * @channel_number: channel number.
 * @return 0 on success or negative value on error.
 *
 * The TTY stack needs to know in advance how many devices it should
 * plan to manage. Use this call to set up the ports that will
 * be exported through SDIO.
 */
int sdio_downloader_setup(struct mmc_card *card,
			  unsigned int num_of_devices,
			  int func_number,
			  int(*func)(void));

/**
 * test_channel_init
 * initializes a test channel
 *
 * @name: the channel name.
 * @return 0 on success or negative value on error.
 *
 */
int test_channel_init(char *name);

/**
 * sdio_al_register_lpm_cb
 * Allow the sdio_al test to register for lpm voting
 * notifications
 *
 * @device_handle: the device handle.
 * @wakeup_callback: callback function to be called when voting.
 *
 */
void sdio_al_register_lpm_cb(void *device_handle,
				       int(*lpm_callback)(void *, int));

/**
 * sdio_al_unregister_lpm_cb
 * Allow the sdio_al test to unregister for lpm voting
 * notifications
 *
 * @device_handle: the device handle.
 *
 */
void sdio_al_unregister_lpm_cb(void *device_handle);

#endif /* __SDIO_AL_PRIVATE__ */
