/*
 * Copyright (c) 2013-2014 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __DCITUI_H__
#define __DCITUI_H__

/**< Responses have bit 31 set */
#define RSP_ID_MASK (1U << 31)
#define RSP_ID(cmd_id) (((uint32_t)(cmd_id)) | RSP_ID_MASK)
#define IS_CMD(cmd_id) ((((uint32_t)(cmd_id)) & RSP_ID_MASK) == 0)
#define IS_RSP(cmd_id) ((((uint32_t)(cmd_id)) & RSP_ID_MASK) == RSP_ID_MASK)
#define CMD_ID_FROM_RSP(rsp_id) (rsp_id & (~RSP_ID_MASK))

/**
 * Return codes of driver commands.
 */
#define TUI_DCI_OK                      0x00030000
#define TUI_DCI_ERR_UNKNOWN_CMD         0x00030001
#define TUI_DCI_ERR_NOT_SUPPORTED       0x00030002
#define TUI_DCI_ERR_INTERNAL_ERROR      0x00030003
#define TUI_DCI_ERR_NO_RESPONSE         0x00030004
#define TUI_DCI_ERR_BAD_PARAMETERS      0x00030005
#define TUI_DCI_ERR_NO_EVENT            0x00030006
#define TUI_DCI_ERR_OUT_OF_DISPLAY      0x00030007
/* ... add more error codes when needed */


/**
 * Notification ID's for communication Trustlet Connector -> Driver.
 */
#define NOT_TUI_NONE                0
/* NWd system event that closes the current TUI session*/
#define NOT_TUI_CANCEL_EVENT        1


/**
 * Command ID's for communication Driver -> Trustlet Connector.
 */
#define CMD_TUI_SW_NONE             0
/* SWd request to NWd to start the TUI session */
#define CMD_TUI_SW_OPEN_SESSION     1
/* SWd request to NWd to close the TUI session */
#define CMD_TUI_SW_CLOSE_SESSION    2
/* SWd request to NWd stop accessing display controller */
#define CMD_TUI_SW_STOP_DISPLAY     3


/**
 * Maximum data length.
 */
#define MAX_DCI_DATA_LEN (1024*100)

/* Command payload */
struct tui_alloc_data_t {
	uint32_t alloc_size;
	uint32_t num_of_buff;
};

union dci_cmd_payload_t {
	struct tui_alloc_data_t alloc_data;
};

/* Command */
struct dci_command_t {
	volatile uint32_t id;
	union dci_cmd_payload_t payload;
};

/* TUI frame buffer (output from NWd) */
typedef struct {
	uint64_t    pa;
} tuiAllocBuffer_t;

#define MAX_DCI_BUFFER_NUMBER 4

/* Response */
struct dci_response_t {
	volatile uint32_t	id; /* must be command ID | RSP_ID_MASK */
	uint32_t		return_code;
	union {
		tuiAllocBuffer_t alloc_buffer[MAX_DCI_BUFFER_NUMBER];
	};
};

/* DCI buffer */
struct tui_dci_msg_t {
	volatile uint32_t     nwd_notif; /* Notification from TlcTui to DrTui */
	struct dci_command_t  cmd_nwd;   /* Command from DrTui to TlcTui */
	struct dci_response_t nwd_rsp;   /* Response from TlcTui to DrTui */
};

/**
 * Driver UUID. Update accordingly after reserving UUID
 */
#define DR_TUI_UUID { { 7, 0xC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } }

#endif /* __DCITUI_H__ */
