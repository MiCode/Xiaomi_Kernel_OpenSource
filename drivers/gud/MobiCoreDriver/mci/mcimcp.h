/*
 * Copyright (c) 2013-2015 TRUSTONIC LIMITED
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

#ifndef MCP_H_
#define MCP_H_

#include "mci/mcloadformat.h"

/** Indicates a response */
#define FLAG_RESPONSE       BIT(31)

/** Maximum number of buffers that can be mapped at once */
#define MCP_MAP_MAX_BUF        4

/** MobiCore Return Code Defines.
 * List of the possible MobiCore return codes.
 */
enum mcp_result {
	/** Memory has successfully been mapped */
	MC_MCP_RET_OK                                   =  0,
	/** The session ID is invalid */
	MC_MCP_RET_ERR_INVALID_SESSION                  =  1,
	/** The UUID of the Trustlet is unknown */
	MC_MCP_RET_ERR_UNKNOWN_UUID                     =  2,
	/** The ID of the driver is unknown */
	MC_MCP_RET_ERR_UNKNOWN_DRIVER_ID                =  3,
	/** No more session are allowed */
	MC_MCP_RET_ERR_NO_MORE_SESSIONS                 =  4,
	/** The container is invalid */
	MC_MCP_RET_ERR_CONTAINER_INVALID                =  5,
	/** The Trustlet is invalid */
	MC_MCP_RET_ERR_TRUSTLET_INVALID                 =  6,
	/** The memory block has already been mapped before */
	MC_MCP_RET_ERR_ALREADY_MAPPED                   =  7,
	/** Alignment or length error in the command parameters */
	MC_MCP_RET_ERR_INVALID_PARAM                    =  8,
	/** No space left in the virtual address space of the session */
	MC_MCP_RET_ERR_OUT_OF_RESOURCES                 =  9,
	/** WSM type unknown or broken WSM */
	MC_MCP_RET_ERR_INVALID_WSM                      = 10,
	/** unknown error */
	MC_MCP_RET_ERR_UNKNOWN                          = 11,
	/** Length of map invalid */
	MC_MCP_RET_ERR_INVALID_MAPPING_LENGTH           = 12,
	/** Map can only be applied to Trustlet session */
	MC_MCP_RET_ERR_MAPPING_TARGET                   = 13,
	/** Couldn't open crypto session */
	MC_MCP_RET_ERR_OUT_OF_CRYPTO_RESOURCES          = 14,
	/** System Trustlet signature verification failed */
	MC_MCP_RET_ERR_SIGNATURE_VERIFICATION_FAILED    = 15,
	/** System Trustlet public key is wrong */
	MC_MCP_RET_ERR_WRONG_PUBLIC_KEY                 = 16,
	/** Wrong containter type(s) */
	MC_MCP_RET_ERR_CONTAINER_TYPE_MISMATCH          = 17,
	/** Container is locked (or not activated) */
	MC_MCP_RET_ERR_CONTAINER_LOCKED                 = 18,
	/** SPID is not registered with root container */
	MC_MCP_RET_ERR_SP_NO_CHILD                      = 19,
	/** UUID is not registered with sp container */
	MC_MCP_RET_ERR_TL_NO_CHILD                      = 20,
	/** Unwrapping of root container failed */
	MC_MCP_RET_ERR_UNWRAP_ROOT_FAILED               = 21,
	/** Unwrapping of service provider container failed */
	MC_MCP_RET_ERR_UNWRAP_SP_FAILED                 = 22,
	/** Unwrapping of Trustlet container failed */
	MC_MCP_RET_ERR_UNWRAP_TRUSTLET_FAILED           = 23,
	/** Container version mismatch */
	MC_MCP_RET_ERR_CONTAINER_VERSION_MISMATCH       = 24,
	/** Decryption of service provider trustlet failed */
	MC_MCP_RET_ERR_SP_TL_DECRYPTION_FAILED          = 25,
	/** Hash check of service provider trustlet failed */
	MC_MCP_RET_ERR_SP_TL_HASH_CHECK_FAILED          = 26,
	/** Activation/starting of task failed */
	MC_MCP_RET_ERR_LAUNCH_TASK_FAILED               = 27,
	/** Closing of task not yet possible, try again later */
	MC_MCP_RET_ERR_CLOSE_TASK_FAILED                = 28,
	/**< Service is blocked and a session cannot be opened to it */
	MC_MCP_RET_ERR_SERVICE_BLOCKED                  = 29,
	/**< Service is locked and a session cannot be opened to it */
	MC_MCP_RET_ERR_SERVICE_LOCKED                   = 30,
	/**< Service was forcefully killed (due to an administrative command) */
	MC_MCP_RET_ERR_SERVICE_KILLED                   = 31,
	/** The command is unknown */
	MC_MCP_RET_ERR_UNKNOWN_COMMAND                  = 50,
	/** The command data is invalid */
	MC_MCP_RET_ERR_INVALID_DATA                     = 51
};

/** Possible MCP Command IDs
 * Command ID must be between 0 and 0x7FFFFFFF.
 */
enum cmd_id {
	/** Invalid command ID */
	MC_MCP_CMD_ID_INVALID		= 0x00,
	/** Open a session */
	MC_MCP_CMD_OPEN_SESSION		= 0x01,
	/** Close an existing session */
	MC_MCP_CMD_CLOSE_SESSION	= 0x03,
	/** Map WSM to session */
	MC_MCP_CMD_MAP			= 0x04,
	/** Unmap WSM from session */
	MC_MCP_CMD_UNMAP		= 0x05,
	/** Prepare for suspend */
	MC_MCP_CMD_SUSPEND		= 0x06,
	/** Resume from suspension */
	MC_MCP_CMD_RESUME		= 0x07,
	/** Get MobiCore version information */
	MC_MCP_CMD_GET_MOBICORE_VERSION	= 0x09,
	/** Close MCP and unmap MCI */
	MC_MCP_CMD_CLOSE_MCP		= 0x0A,
	/** Load token for device attestation */
	MC_MCP_CMD_LOAD_TOKEN		= 0x0B,
	/** Check that TA can be loaded */
	MC_MCP_CMD_CHECK_LOAD_TA	= 0x0C,
	/** Map multiple WSMs to session */
	MC_MCP_CMD_MULTIMAP		= 0x0D,
	/** Unmap multiple WSMs to session */
	MC_MCP_CMD_MULTIUNMAP		= 0x0E,
};

/*
 * Types of WSM known to the MobiCore.
 */
#define WSM_TYPE_MASK		0xFF
#define WSM_INVALID		0	/** Invalid memory type */
#define WSM_L2			2	/** Buffer mapping uses L2/L3 table */
#define WSM_L1			3	/** Buffer mapping uses fake L1 table */

/** Magic number used to identify if Open Command supports GP client
 * authentication.
 */
#define MC_GP_CLIENT_AUTH_MAGIC	0x47504131	/* "GPA1" */

/** Command header.
 * It just contains the command ID. Only values specified in cmd_id are
 * allowed as command IDs.  If the command ID is unspecified the MobiCore
 * returns an empty response with the result set to
 * MC_MCP_RET_ERR_UNKNOWN_COMMAND.
 */
struct cmd_header {
	enum cmd_id	cmd_id;	/** Command ID of the command */
};

/** Response header.
 * MobiCore will reply to every MCP command with an MCP response.  Like the MCP
 * command the response consists of a header followed by response data. The
 * response is written to the same memory location as the MCP command.
 */
struct rsp_header {
	uint32_t	rsp_id;	/** Command ID | FLAG_RESPONSE */
	enum mcp_result	result;		/** Result of the command execution */
};

/** @defgroup CMD MCP Commands
 */

/** @defgroup ASMCMD Administrative Commands
 */

/** @defgroup MCPGETMOBICOREVERSION GET_MOBICORE_VERSION
 * Get MobiCore version info.
 *
 */

/** Get MobiCore Version Command */
struct cmd_get_version {
	struct cmd_header	cmd_header;	/** Command header */
};

/** Get MobiCore Version Command Response */
struct rsp_get_version {
	struct rsp_header	rsp_header;	/** Response header */
	struct mc_version_info version_info;	/** MobiCore version info */
};

/** @defgroup POWERCMD Power Management Commands
 */

/** @defgroup MCPSUSPEND SUSPEND
 * Prepare MobiCore suspension.
 * This command allows MobiCore and MobiCore drivers to release or clean
 * resources and save device state.
 *
 */

/** Suspend Command */
struct cmd_suspend {
	struct cmd_header	cmd_header;	/** Command header */
};

/** Suspend Command Response */
struct rsp_suspend {
	struct rsp_header	rsp_header;	/** Response header */
};

/** @defgroup MCPRESUME RESUME
 * Resume MobiCore from suspension.
 * This command allows MobiCore and MobiCore drivers to reinitialize hardware
 * affected by suspension.
 *
 */

/** Resume Command */
struct cmd_resume {
	struct cmd_header	cmd_header;	/** Command header */
};

/** Resume Command Response */
struct rsp_resume {
	struct rsp_header	rsp_header;	/** Response header */
};

/** @defgroup SESSCMD Session Management Commands
 */

/** @defgroup MCPOPEN OPEN
 * Load and open a session to a Trustlet.
 * The OPEN command loads Trustlet data to the MobiCore context and opens a
 * session to the Trustlet.  If wsm_data_type is WSM_INVALID MobiCore tries to
 * start a pre-installed Trustlet associated with the uuid passed.  The uuid
 * passed must match the uuid contained in the load data (if available).
 * On success, MobiCore returns the session ID which can be used for further
 * communication.
 */

/** GP client authentication data */
struct cmd_open_data {
	uint32_t	mclf_magic;	/** ASCII "MCLF" on older versions */
	struct identity	identity;	/** Login method and data */
};

/** Open Command */
struct cmd_open {
	struct cmd_header cmd_header;	/** Command header */
	struct mc_uuid_t uuid;		/** Service UUID */
	uint8_t		unused[4];	/** Padding to be 64-bit aligned */
	uint64_t	adr_tci_buffer;	/** Physical address of the TCI MMU */
	uint64_t	adr_load_data;	/** Physical address of the data MMU */
	uint32_t	ofs_tci_buffer;	/** Offset to the data */
	uint32_t	len_tci_buffer;	/** Length of the TCI */
	uint32_t	wsmtype_tci;	/** Type of WSM used for the TCI */
	uint32_t	wsm_data_type;	/** Type of MMU */
	uint32_t	ofs_load_data;	/** Offset to the data */
	uint32_t	len_load_data;	/** Length of the data to load */
	union {
		struct cmd_open_data	cmd_open_data;	/** Client login data */
		union mclf_header	tl_header;	/** Service header */
	};
	uint32_t	is_gpta;	/** true if looking for an SD/GP-TA */
};

/** Open Command Response */
struct rsp_open {
	struct rsp_header	rsp_header;	/** Response header */
	uint32_t	session_id;	/** Session ID */
};

/** TA Load Check Command */
struct cmd_check_load {
	struct cmd_header cmd_header;	/** Command header */
	struct mc_uuid_t uuid;		/** Service UUID */
	uint64_t	adr_load_data;	/** Physical address of the data */
	uint32_t	wsm_data_type;	/** Type of MMU */
	uint32_t	ofs_load_data;	/** Offset to the data */
	uint32_t	len_load_data;	/** Length of the data to load */
	union mclf_header tl_header;	/** Service header */
};

/** TA Load Check Response */
struct rsp_check_load {
	struct rsp_header	rsp_header;	/** Response header */
};

/** @defgroup MCPCLOSE CLOSE
 * Close an existing session to a Trustlet.
 * The CLOSE command terminates a session and frees all resources in the
 * MobiCore system which are currently occupied by the session. Before closing
 * the session, the MobiCore runtime management waits until all pending
 * operations, like calls to drivers, invoked by the Trustlet have been
 * terminated.  Mapped memory will automatically be unmapped from the MobiCore
 * context. The NWd is responsible for processing the freed memory according to
 * the Rich-OS needs.
 *
 */

/** Close Command */
struct cmd_close {
	struct cmd_header	cmd_header;	/** Command header */
	uint32_t		session_id;	/** Session ID */
};

/** Close Command Response */
struct rsp_close {
	struct rsp_header	rsp_header;	/** Response header */
};

/** @defgroup MCPMAP MAP
 * Map a portion of memory to a session.
 * The MAP command provides a block of memory to the context of a service.
 * The memory then becomes world-shared memory (WSM).
 * The only allowed memory type here is WSM_L2.
 */

/** Map Command */
struct cmd_map {
	struct cmd_header cmd_header;	/** Command header */
	uint32_t	session_id;	/** Session ID */
	uint32_t	wsm_type;	/** Type of MMU */
	uint32_t	ofs_buffer;	/** Offset to the payload */
	uint64_t	adr_buffer;	/** Physical address of the MMU */
	uint32_t	len_buffer;	/** Length of the buffer */
};

#define MCP_MAP_MAX         0x100000    /** Maximum length for MCP map */

/** Map Command Response */
struct rsp_map {
	struct rsp_header	rsp_header;	/** Response header */
	/** Virtual address the WSM is mapped to, may include an offset! */
	uint32_t		secure_va;
};

/** @defgroup MCPUNMAP UNMAP
 * Unmap a portion of world-shared memory from a session.
 * The UNMAP command is used to unmap a previously mapped block of
 * world shared memory from the context of a session.
 *
 * Attention: The memory block will be immediately unmapped from the specified
 * session.  If the service is still accessing the memory, the service will
 * trigger a segmentation fault.
 */

/** Unmap Command */
struct cmd_unmap {
	struct cmd_header cmd_header;	/** Command header */
	uint32_t	session_id;	/** Session ID */
	uint32_t	wsm_type;	/** Type of WSM used of the memory */
	/** Virtual address the WSM is mapped to, may include an offset! */
	uint32_t	secure_va;
	uint32_t	virtual_buffer_len;  /** Length of virtual buffer */
};

/** Unmap Command Response */
struct rsp_unmap {
	struct rsp_header	rsp_header;	/** Response header */
};

/** @defgroup MCPLOADTOKEN
 * Load a token from the normal world and share it with <t-base
 * If something fails, the device attestation functionality will be disabled
 */

/** Load Token */
struct cmd_load_token {
	struct cmd_header cmd_header;	/** Command header */
	uint32_t	wsm_data_type;	/** Type of MMU */
	uint64_t	adr_load_data;	/** Physical address of the MMU */
	uint64_t	ofs_load_data;	/** Offset to the data */
	uint64_t	len_load_data;	/** Length of the data */
};

/** Load Token Command Response */
struct rsp_load_token {
	struct rsp_header	rsp_header;	/** Response header */
};

/** @defgroup MCPMULTIMAP MULTIMAP
 * Map up to MCP_MAP_MAX_BUF portions of memory to a session.
 * The MULTIMAP command provides MCP_MAP_MAX_BUF blocks of memory to the context
 * of a service.
 * The memory then becomes world-shared memory (WSM).
 * The only allowed memory type here is WSM_L2.
 * @{ */

/** NWd physical buffer description
 *
 * Note: Information is coming from NWd kernel. So it should not be trusted
 * more than NWd kernel is trusted.
 */
struct buffer_map {
	uint64_t		adr_buffer;	/**< Physical address */
	uint32_t		ofs_buffer;	/**< Offset of buffer */
	uint32_t		len_buffer;	/**< Length of buffer */
	uint32_t		wsm_type;	/**< Type of address */
};

/** MultiMap Command */
struct cmd_multimap {
	struct cmd_header	cmd_header;	/** Command header */
	uint32_t		session_id;	/** Session ID */
	struct buffer_map	bufs[MC_MAP_MAX]; /** NWd buffer info */
};

/** Multimap Command Response */
struct rsp_multimap {
	struct rsp_header	rsp_header;	/** Response header */
	/** Virtual address the WSM is mapped to, may include an offset! */
	uint64_t		secure_va[MC_MAP_MAX];
};

/** @defgroup MCPMULTIUNMAP MULTIUNMAP
 * Unmap up to MCP_MAP_MAX_BUF portions of world-shared memory from a session.
 * The MULTIUNMAP command is used to unmap MCP_MAP_MAX_BUF previously mapped
 * blocks of world shared memory from the context of a session.
 *
 * Attention: The memory blocks will be immediately unmapped from the specified
 * session. If the service is still accessing the memory, the service will
 * trigger a segmentation fault.
 * @{ */

/** NWd mapped buffer description
 *
 * Note: Information is coming from NWd kernel. So it should not be trusted more
 * than NWd kernel is trusted.
 */
struct buffer_unmap {
	uint64_t		secure_va;	/**< Secure virtual address */
	uint32_t		len_buffer;	/**< Length of buffer */
};

/** Multiunmap Command */
struct cmd_multiunmap {
	struct cmd_header	cmd_header;	/** Command header */
	uint32_t		session_id;	/** Session ID */
	struct buffer_unmap	bufs[MC_MAP_MAX]; /** NWd buffer info */
};

/** Multiunmap Command Response */
struct rsp_multiunmap {
	struct rsp_header	rsp_header;	/** Response header */
};

/** Structure of the MCP buffer */
union mcp_message {
	struct cmd_header	cmd_header;	/** Command header */
	struct rsp_header	rsp_header;
	struct cmd_open		cmd_open;	/** Load and open service */
	struct rsp_open		rsp_open;
	struct cmd_close	cmd_close;	/** Close command */
	struct rsp_close	rsp_close;
	struct cmd_map		cmd_map;	/** Map WSM to service */
	struct rsp_map		rsp_map;
	struct cmd_unmap	cmd_unmap;	/** Unmap WSM from service */
	struct rsp_unmap	rsp_unmap;
	struct cmd_suspend	cmd_suspend;	/** Suspend MobiCore */
	struct rsp_suspend	rsp_suspend;
	struct cmd_resume	cmd_resume;	/** Resume MobiCore */
	struct rsp_resume	rsp_resume;
	struct cmd_get_version	cmd_get_version; /** Get MobiCore Version */
	struct rsp_get_version	rsp_get_version;
	struct cmd_load_token	cmd_load_token;	/** Load token */
	struct rsp_load_token	rsp_load_token;
	struct cmd_check_load	cmd_check_load;	/** TA load check */
	struct rsp_check_load	rsp_check_load;
	struct cmd_multimap	cmd_multimap;	/** Map multiple WSMs */
	struct rsp_multimap	rsp_multimap;
	struct cmd_multiunmap	cmd_multiunmap;	/** Map multiple WSMs */
	struct rsp_multiunmap	rsp_multiunmap;
};

/** Minimum MCP buffer length (in bytes) */
#define MIN_MCP_LEN         sizeof(mcp_message_t)

#define MC_FLAG_NO_SLEEP_REQ   0
#define MC_FLAG_REQ_TO_SLEEP   1

#define MC_STATE_NORMAL_EXECUTION 0
#define MC_STATE_READY_TO_SLEEP   1

struct sleep_mode {
	uint16_t	sleep_req;	/** Ask SWd to get ready to sleep */
	uint16_t	ready_to_sleep;	/** SWd is now ready to sleep */
};

/** MobiCore status flags */
struct mcp_flags {
	/** If not MC_FLAG_SCHEDULE_IDLE, MobiCore needsscheduling */
	uint32_t	schedule;
	struct sleep_mode sleep_mode;
	/** Secure-world sleep timeout in milliseconds */
	int32_t		timeout_ms;
	/** Reserved for future use: Must not be interpreted */
	uint32_t	RFU3;
};

/** MobiCore is idle. No scheduling required */
#define MC_FLAG_SCHEDULE_IDLE      0
/** MobiCore is non idle, scheduling is required */
#define MC_FLAG_SCHEDULE_NON_IDLE  1

/** MCP buffer structure */
struct mcp_buffer {
	struct mcp_flags	mc_flags;	/** MobiCore Flags */
	union mcp_message	mcp_message;	/** MCP message buffer */
};

#endif /* MCP_H_ */
