/*
 * Copyright (c) 2018 Cog Systems Pty Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Header file for communication with resource manager.
 *
 */

/*
 * Core API
 */
#define ERROR_REPLY 0x8000ffff

/*
 * Boot manager API
 */
#define BOOT_MGR_PROTOCOL_ID 'B'

/* start_client: Unmap the client (ML VM) memory and start Linux */
#define BOOT_MGR_START_CLIENT 0x00420001
/* msg_payload: struct boot_mgr_start_params */

struct boot_mgr_start_params {
    uint64_t entry_addr; /* Physical load address / entry point of Linux */
    uint64_t dtb_addr; /* Physical address of DTB */
    bool is_64bit; /* True to reset VM to AArch64 mode, false for AArch32 */
};

/* start_client_reply: Response to BOOT_MGR_START_CLIENT */
#define BOOT_MGR_START_CLIENT_REPLY 0x80420001
/* msg_payload: bool success */

/* start_self: Reset the caller and start the loaded HLOS image */
#define BOOT_MGR_START_SELF 0x00420002
/* msg_payload: struct boot_mgr_start_params */

/*
 * start_self_reply: Response to BOOT_MGR_START_CLIENT; sent only on
 * failure as the caller will be reset if this call succeeds
 */
#define BOOT_MGR_START_SELF_REPLY 0x80420002
/* msg_payload: bool success */


/*
 * Secure Camera Server API (for HLOS)
 */
#define RES_MGR_SECURECAM_SERVER_PROTOCOL_ID 'q'

/*
 * get_handle: Given a buffer sg list, return an SC handle.
 *
 * This is sent by the HLOS to the resource manager to obtain the SC handle
 * to be used to refer to a specific camera buffer.
 *
 * The message payload is a list of IPA ranges in the HLOS VM's stage 2
 * address space. These ranges must have previously been passed to a TZ secure
 * camera map call that has been intercepted by the hypervisor and forwarded
 * to both TZ and the resource manager.
 *
 * Payload: struct res_mgr_sglist securecam.sglist
 * Note: The payload ends with a variable-length array.
 */
#define RES_MGR_SECURECAM_GET_HANDLE 0x00710001

struct res_mgr_region {
    uint64_t address_ipa;
    uint64_t size;
};

struct res_mgr_sglist {
    uint32_t region_count;
    struct res_mgr_region regions[];
};

/*
 * get_handle_reply: Response to a get_handle request.
 *
 * This is sent by the resource manager to the HLOS to return the SC handle to
 * be used to refer to the specified buffer.
 *
 * If the specified sglist did not match a secure camera buffer known to the
 * resource manager, the value 0xffffffff is returned. This value is never
 * a valid SC handle.
 *
 * Payload: uint32_t securecam.handle
 */
#define RES_MGR_SECURECAM_GET_HANDLE_REPLY 0x80710001

/*
 * destroy_handles: Destroy all SC handles and unmap their buffers.
 *
 * This is sent by the HLOS to the resource manager to ask it to unmap all
 * secure camera buffers from the ML VM and return the memory to the HLOS.
 *
 * Under normal operation, this message will be received by the resource
 * manager after the ML VM has indicated that its application is complete by
 * sending a DONE message. If this is not the case, the resource manager will
 * wait until both this message and the DONE message have been received before
 * destroying the buffers.
 *
 * Payload: void
 */
#define RES_MGR_SECURECAM_DESTROY_HANDLES 0x00710002

/*
 * destroy_handles_reply: Indicate that all SC handles have been destroyed.
 *
 * This is sent by the resource manager to the HLOS to inform it that all
 * secure camera buffers have been unmapped from the ML VM and returned to the
 * HLOS.
 *
 * Payload: void
 */
#define RES_MGR_SECURECAM_DESTROY_HANDLES_REPLY 0x80710002


/*
 * Secure Camera Client API (for ML VM)
 */
#define RES_MGR_SECURECAM_CLIENT_PROTOCOL_ID 'Q'

/*
 * notify_start: Tell the client that the first camera buffer has been mapped.
 *
 * This is sent by the resource manager to the ML VM after the first instance
 * of a TZ map call for a secure camera buffer being intercepted.
 *
 * Payload: void
 */
#define RES_MGR_SECURECAM_NOTIFY_START 0x80510001

/*
 * ack_start: Acknowledge a notify_start message
 *
 * This is sent by the ML VM to the resource manager to acknowledge receipt
 * of a notify_start message.
 *
 * Payload: void
 */
#define RES_MGR_SECURECAM_ACK_START 0x00510001

/*
 * done: Indicate that the secure camera application has terminated.
 *
 * This is sent by the ML VM when access to the secure camera buffers is no
 * longer required. The resource manager will delay unmapping the buffers
 * until this message is received.
 *
 * Payload: void
 */
#define RES_MGR_SECURECAM_DONE 0x00510002

/*
 * lookup_handle: Request physical addresses for a secure camera handle.
 *
 * This is sent by the ML VM when userspace code attempts to register a secure
 * camera buffer handle.
 *
 * Payload: uint32_t securecam.handle
 */
#define RES_MGR_LOOKUP_HANDLE 0x00510003

/*
 * lookup_handle_reply: Response to lookup_handle.
 *
 * When the resource manager receives a lookup_handle message containing a
 * handle that is valid and has already been mapped into the ML VM stage 2,
 * this message is returned containing the list of IPA ranges that have been
 * assigned to the buffer in the ML VM's address space.
 *
 * If the handle is unknown, or corresponds to a buffer that is not currently
 * mapped into the ML VM stage 2, the region_count field of the result will be
 * set to 0.
 *
 * Payload: struct res_mgr_sglist securecam.sglist
 * Note: The payload ends with a variable-length array.
 */
#define RES_MGR_LOOKUP_HANDLE_REPLY 0x80510003

/*
 * notify_start: Tell the client that the camera buffers will be unmapped.
 *
 * This is sent by the resource manager to the ML VM after the first instance
 * of a TZ unprotect call for a secure camera buffer being intercepted.
 *
 * Payload: void
 */
#define RES_MGR_SECURECAM_NOTIFY_STOP 0x80510004

/*
 * ack_start: Acknowledge a notify_stop message
 *
 * This is sent by the ML VM to the resource manager to acknowledge receipt
 * of a notify_stop message.
 *
 * Payload: void
 */
#define RES_MGR_SECURECAM_ACK_STOP 0x00510004

/*
 * Top-level message structure
 */
struct res_mgr_msg {
    uint32_t msg_id;
    union {
        bool success;
        struct {
            struct boot_mgr_start_params start_params;
        } boot_mgr;
        struct {
            uint32_t handle;
            struct res_mgr_sglist sglist;
        } securecam;
    };
};
