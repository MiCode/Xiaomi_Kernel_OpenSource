/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
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

#ifndef MSM_PFT_H_
#define MSM_PFT_H_

#include <linux/types.h>

/**
 *  enum pft_command_opcode - PFT driver command ID
 *
 *  @PFT_CMD_OPCODE_SET_STATE -
 *      command ID to set PFT driver state
 *  @PFT_CMD_OPCODE_UPDATE_REG_APP_UID -
 *      command ID to update the list of registered application
 *      UID
 *  @PFT_CMD_OPCODE_PERFORM_IN_PLACE_FILE_ENC -
 *      command ID to perfrom in-place file encryption
 */
enum pft_command_opcode {
	PFT_CMD_OPCODE_SET_STATE,
	PFT_CMD_OPCODE_UPDATE_REG_APP_UID,
	PFT_CMD_OPCODE_PERFORM_IN_PLACE_FILE_ENC,
	/* */
	PFT_CMD_OPCODE_MAX_COMMAND_INDEX
};

/**
 * enum pft_state - PFT driver operational states
 *
 * @PFT_STATE_DEACTIVATED - driver is deativated.
 * @PFT_STATE_DEACTIVATING - driver is in the process of being deativated.
 * @PFT_STATE_KEY_REMOVED - driver is active but no encryption key is loaded.
 * @PFT_STATE_REMOVING_KEY - driver is active, but the encryption key is being
 *      removed.
 * @PFT_STATE_KEY_LOADED - driver is active, and the encryption key is loaded
 *      to encryption block, hence registered apps can perform file operations
 *      on encrypted files.
 */
enum pft_state {
	PFT_STATE_DEACTIVATED,
	PFT_STATE_DEACTIVATING,
	PFT_STATE_KEY_REMOVED,
	PFT_STATE_REMOVING_KEY,
	PFT_STATE_KEY_LOADED,
	/* Internal */
	PFT_STATE_MAX_INDEX
};

/**
 * enum pft_command_response_code - PFT response on the previous
 * command
 *
 * @PFT_CMD_RESP_SUCCESS - The command was properly processed
 *      without an error.
 * @PFT_CMD_RESP_GENERAL_ERROR -
 *      Indicates an error that cannot be better described by a
 *      more specific errors below.
 * @PFT_CMD_RESP_INVALID_COMMAND - Invalid or unsupported
 *      command id.
 * @PFT_CMD_RESP_INVALID_CMD_PARAMS - Invalid command
 *	parameters.
 * @PFT_CMD_RESP_INVALID_STATE - Invalid state
 * @PFT_CMD_RESP_ALREADY_IN_STATE - Used to indicates that
 *      the new state is equal to the existing one.
 * @PFT_CMD_RESP_INPLACE_FILE_IS_OPEN - Used to indicates
 *      that the file that should be encrypted is already open
 *      and can be encrypted.
 * @PFT_CMD_RESP_ENT_FILES_CLOSING_FAILURE
 *	Indicates about failure of the PFT to close Enterprise files
 * @PFT_CMD_RESP_MAX_INDEX
 */
enum pft_command_response_code {
	PFT_CMD_RESP_SUCCESS,
	PFT_CMD_RESP_GENERAL_ERROR,
	PFT_CMD_RESP_INVALID_COMMAND,
	PFT_CMD_RESP_INVALID_CMD_PARAMS,
	PFT_CMD_RESP_INVALID_STATE,
	PFT_CMD_RESP_ALREADY_IN_STATE,
	PFT_CMD_RESP_INPLACE_FILE_IS_OPEN,
	PFT_CMD_RESP_ENT_FILES_CLOSING_FAILURE,
	/* Internal */
	PFT_CMD_RESP_MAX_INDEX
};

/**
 * struct pft_command_response - response structure
 *
 * @command_id - see enum pft_command_response_code
 * @error_codee - see enum pft_command_response_code
 */
struct pft_command_response {
	__u32 command_id;
	__u32 error_code;
};

/**
 * struct pft_command - pft command
 *
 * @opcode - see enum pft_command_opcode.
 * @set_state.state - see enum pft_state.
 * @update_app_list.count - number of items in the
 *      registered applications list.
 * @update_app_list.table - registered applications array
 * @preform_in_place_file_enc.file_descriptor - file descriptor
 *      of the opened file to be in-placed encrypted.
 */
struct pft_command {
	__u32 opcode;
	union {
		struct {
			/* @see pft_state */
			__u32 state;
		} set_state;
		struct {
			__u32 items_count; /* number of items */
			uid_t table[0]; /* array of UIDs */
		} update_app_list;
		struct {
			__u32 file_descriptor;
		} preform_in_place_file_enc;
	};
};

#endif /* MSM_PFT_H_ */
