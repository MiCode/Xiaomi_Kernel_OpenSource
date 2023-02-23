// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2014-2020, Elliptic Laboratories AS. All rights reserved.
 * Elliptic Labs Linux driver
 */

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <linux/atomic.h>
#include <linux/uaccess.h>
#include <linux/errno.h>

//#include <stdint.h>
//#include <stddef.h>
//#include <string.h>

#include "scp_ipi_pin.h"
#include <audio_task_manager.h>
#include <audio_ipi_queue.h>

#include "elliptic_data_io.h"
#include "elliptic_device.h"
#include "elliptic_mixer_controls.h"
#include "scp_helper.h"
#include "scp_ipi.h"
#include "scp_ipi_wrapper.h"
// #define USND_IPI_SEND_BUFFER_LENGTH 128
// #define USND_IPI_RECEIVE_LENGTH 128
// #define SHARE_BUF_DATA_SIZE 128 * sizeof(int)
#include "hf_sensor_type.h"
#include "elliptic_data_mtk_io.h"

// #define USND_IPI_SEND_BUFFER_LENGTH 62
// #define PIN_OUT_SIZE_AUDIO_USND_1 64
// #define USND_IPI_RECEIVE_LENGTH 62
// #define SHARE_BUF_DATA_SIZE 62 * sizeof(int)
// 248 bytes, so that send/receive info structs are 256 bytes

//#define PIN_OUT_SIZE_AUDIO_USND_0 (ELLIPTIC_IPI_AP_TO_SCP_DATA_SIZE/4)
#define ELLIPTIC_SHARED_MEMORY_MSG_ID 0xFACE0000
#define ELLIPTIC_DRAM_PAYLOAD_MAX_OFFSET ((uint16_t) 8)
#define ELLIPTIC_SCP_IPI_RETRY_TIMES 1000
#define ELLIPTIC_SCP_IPI_MSG_FIFO_NUM 8

//static elliptic_ipi_scp_to_host_message_t usnd_ipi_receive;
static struct scp_elliptic_reserved_mem_t debug_segment;
static struct elliptic_ipi_handler_data_t elliptic_ipi_handler_data;

int32_t elliptic_debug_io_open(void)
{

	pr_info("[ELUS] %s()", __func__);
	if (debug_segment.reserved == 0) {
		debug_segment.phys =
			scp_get_reserve_mem_phys(SCP_ELLIPTIC_DEBUG_MEM);
		debug_segment.virt =
			scp_get_reserve_mem_virt(SCP_ELLIPTIC_DEBUG_MEM);
		debug_segment.size =
			scp_get_reserve_mem_size(SCP_ELLIPTIC_DEBUG_MEM);
		debug_segment.reserved = 1;
	}
	return elliptic_data_io_write(ELLIPTIC_INIT_DEBUG_SEGMENT,
				(const char *)&debug_segment,
				sizeof(debug_segment));
}

int32_t elliptic_debug_io_close(void)
{
	pr_info("[ELUS] %s()", __func__);
	return 0;
}

static void copy_to_local_ap_cache(
	const char *name, uint32_t shared_object_id, size_t shared_object_size,
	elliptic_scp_to_host_message_header_t *msg_header, void *payload)
{
	if (msg_header->data_size == shared_object_size) {
		struct elliptic_shared_data_block *data_block =
		    elliptic_get_shared_obj(shared_object_id);
		memcpy(data_block->buffer, payload, shared_object_size);
		pr_info("[ELUS] %s copied to local AP cache, size: %u",
			       name, msg_header->data_size);
	} else {
		pr_debug("[ELUS] %s - illegal size: %u",
			name, msg_header->data_size);
	}
}
static elliptic_scp_to_host_message_header_t *get_header(
	elliptic_dram_payload_t *dram_payload, uint16_t header_id)
{
	size_t i;

	if (dram_payload == NULL)
		return NULL;
	for (i = 0; i < ELLIPTIC_DRAM_PAYLOAD_MAX_OFFSET; ++i) {
		elliptic_scp_to_host_message_header_t *header =
			(elliptic_scp_to_host_message_header_t *)
			(dram_payload + i);
		if (header->dram_payload_offset == header_id)
			return header;
	}
	return NULL;
}

static elliptic_dram_payload_t temp_buffer[ELLIPTIC_DRAM_PAYLOAD_MAX_OFFSET];
static void elliptic_data_io_ipi_handle_task(unsigned long data)
{
    elliptic_ipi_scp_to_host_message_t ipi_msg; 
    static uint16_t current_ipi_counter = 0;
    int32_t ret = -1;
    elliptic_dram_payload_t *dram_payload = (elliptic_dram_payload_t *)debug_segment.virt;
    unsigned int msg_size;
    uint16_t target_ipi_message_count;
    elliptic_scp_to_host_message_header_t *header;
    void *payload = NULL;
    const char *via_str = "";

    elliptic_ipi_handler_data.task_running = 1;
    while (kfifo_is_empty(&elliptic_ipi_handler_data.fifo) == 0) { // Read all ipi messages in fifo
        msg_size = kfifo_out(&elliptic_ipi_handler_data.fifo, &ipi_msg, sizeof(elliptic_ipi_scp_to_host_message_t));
        if (msg_size != sizeof(elliptic_ipi_scp_to_host_message_t)) {
            pr_err("[ELUS] fifo buffer element has unexpected size (corrupt?)");
            continue;
        }
        target_ipi_message_count = ipi_msg.header.dram_payload_offset;
        if ( (uint16_t) (target_ipi_message_count - current_ipi_counter) > ELLIPTIC_DRAM_PAYLOAD_MAX_OFFSET) {
            pr_info("[ELUS] Offset mismatch between SCP and AP kernel, current_ipi_counter: %u, target_ipi_message_count: %u!\n",
                    current_ipi_counter, target_ipi_message_count);
            current_ipi_counter = target_ipi_message_count - ELLIPTIC_DRAM_PAYLOAD_MAX_OFFSET;
        }
        if (debug_segment.reserved == 0) {
            dram_payload = NULL;
        }
        else {
            dram_payload = temp_buffer;
            memcpy(dram_payload, (void *)debug_segment.virt, 
                sizeof(elliptic_dram_payload_t) * ELLIPTIC_DRAM_PAYLOAD_MAX_OFFSET);
        }
        while( current_ipi_counter != target_ipi_message_count ) {
            ++current_ipi_counter;
            // check if payload is in dram buffer
            if( NULL != ( header = get_header( dram_payload, current_ipi_counter ) ) ) 
            {
                payload = header + 1;
                via_str = "dram";
            }
            // if not in dram buffer, it might be a small message in the ipi buffer
            else if( current_ipi_counter == target_ipi_message_count )
            {
                header = &ipi_msg.header;
                payload = ipi_msg.data;
                via_str = "ipi";
            }
            else if( header == NULL )
            {
                // message seems to be lost
                pr_err( "[ELUS] did not find payload with id %u", (unsigned int)current_ipi_counter );
                continue;
            }
            pr_info("[ELUS] Got data via %s, counter:%u parameter_id:%u len:%u",
				via_str,
				(unsigned int)current_ipi_counter,
				(unsigned int)header->parameter_id,
				(unsigned int)header->data_size);

            switch (header->parameter_id) {
            case ELLIPTIC_ULTRASOUND_PARAM_ID_ENGINE_VERSION:
                copy_to_local_ap_cache("engine_version",
                            ELLIPTIC_OBJ_ID_VERSION_INFO,
                            ELLIPTIC_VERSION_INFO_SIZE,
                            header, payload);
                break;
            case ELLIPTIC_ULTRASOUND_PARAM_ID_BUILD_BRANCH:
                copy_to_local_ap_cache("build_branch",
                            ELLIPTIC_OBJ_ID_BRANCH_INFO,
                            ELLIPTIC_BRANCH_INFO_SIZE,
                            header, payload);
                break;
            case ELLIPTIC_ULTRASOUND_PARAM_ID_TAG:
                copy_to_local_ap_cache("tag", ELLIPTIC_OBJ_ID_TAG_INFO,
                            ELLIPTIC_TAG_INFO_SIZE, header,
                            payload);
                break;
            case ELLIPTIC_ULTRASOUND_PARAM_ID_CALIBRATION_DATA:
                copy_to_local_ap_cache("calib_data",
                            ELLIPTIC_OBJ_ID_CALIBRATION_DATA,
                            ELLIPTIC_CALIBRATION_DATA_SIZE,
                            header, payload);
                break;
            case ELLIPTIC_ULTRASOUND_PARAM_ID_CALIBRATION_V2_DATA:
                copy_to_local_ap_cache("calib_v2_data",
                            ELLIPTIC_OBJ_ID_CALIBRATION_V2_DATA,
                            ELLIPTIC_CALIBRATION_V2_DATA_SIZE,
                            header, payload);
                break;
            case ELLIPTIC_ULTRASOUND_PARAM_ID_DIAGNOSTICS_DATA:
                copy_to_local_ap_cache("diag_data",
                            ELLIPTIC_OBJ_ID_DIAGNOSTICS_DATA,
                            ELLIPTIC_DIAGNOSTICS_DATA_SIZE,
                            header, payload);
                break;
            case ELLIPTIC_ULTRASOUND_PARAM_ID_ENGINE_DATA:
                ret = elliptic_data_push(
                    ELLIPTIC_ALL_DEVICES,
                    payload,
                    header->data_size,
                    ELLIPTIC_DATA_PUSH_FROM_KERNEL);

                if (ret != 0)
                    pr_err("[ELUS] failed to push payload to elliptic device");
                break;
            default:
                pr_warn("[ELUS] illegal param id: %u",
                    header->parameter_id);
                break;
            }
        }
    }
    elliptic_ipi_handler_data.task_running = 0;
}

////
/* Will be called from MTK SCP IPI driver when data arrives from DSP */
void elliptic_data_io_ipi_handler(
    int id, /*void *prdata,*/ void *data, unsigned int len)
{
    unsigned int ret;
    pr_info("[ELUS] %s() enter", __func__);
    ret = kfifo_in(&elliptic_ipi_handler_data.fifo, data, sizeof(elliptic_ipi_scp_to_host_message_t));
    if (ret != sizeof(elliptic_ipi_scp_to_host_message_t)) {
        pr_err("[ELUS] IPI handler push message error, size=%u", ret);
    }
    if (elliptic_ipi_handler_data.task_running == 0) {
        tasklet_hi_schedule(&elliptic_ipi_handler_data.handle_task);
    }
    else {
        pr_err("[ELUS] %s() tasklet already running", __func__);
    }
}

int elliptic_data_io_initialize(void)
{
	// mtk_ipi_register(&scp_ipidev, IPI_IN_AUDIO_USND_0,
		// (void *)elliptic_data_io_ipi_handler, NULL,
		// &usnd_ipi_receive);
	scp_ipi_registration(IPI_ELLIPTIC,
		(void *)elliptic_data_io_ipi_handler, "elliptic_data_io");
	if (kfifo_alloc(&elliptic_ipi_handler_data.fifo, 
        sizeof(elliptic_ipi_scp_to_host_message_t) * ELLIPTIC_SCP_IPI_MSG_FIFO_NUM, GFP_KERNEL) != 0) {
        pr_err("[ELUS] failed to allocate ipi msg fifo");
        return -EINVAL;
    }
    tasklet_init(&elliptic_ipi_handler_data.handle_task, elliptic_data_io_ipi_handle_task, 0);
    elliptic_ipi_handler_data.task_running = 0;
	return 0;
}

int32_t elliptic_data_io_write(uint32_t message_id, const char *data,
	size_t data_size)
{
	static elliptic_ipi_host_to_scp_message_t host_message /*= {0}*/;
	int ipi_result;
	int retry = 0;

	pr_debug("[ELUS] %s,enter", __func__);

	/* clear send buffer */
	memset(&host_message, 0, sizeof(host_message));

	host_message.header.elliptic_ipi_message_id = message_id;

	host_message.header.data_size =
		min_t(size_t, data_size, sizeof(host_message.data));
	memcpy(host_message.data, data, host_message.header.data_size);

	if (data_size > sizeof(host_message.data)) {
		// pr_err("[ELUS] %s() failed to send (%zu > %zu)\n",
			// __func__, data_size, sizeof(host_message.data));
		pr_err("[ELUS] %s(%d) failed to send (%zu > %zu)\n",
		       __func__, message_id, data_size, sizeof(host_message.data));
		return -1;
	}
	// ipi_result = mtk_ipi_send(&scp_ipidev,
		// IPI_OUT_AUDIO_USND_0,
		// 0,
		// &host_message,
		// PIN_OUT_SIZE_AUDIO_USND_0,
		// 0);

	do {
		ipi_result = scp_ipi_send(IPI_ELLIPTIC, &host_message,
					  sizeof(host_message), 0, SCP_A_ID);
		if (ipi_result == SCP_IPI_BUSY) {
			if (retry++ == ELLIPTIC_SCP_IPI_RETRY_TIMES) {
				pr_err("%s: retry fail\n", __func__);
				break;
			}
			if (retry % 100 == 0)
				usleep_range(1000, 2000);
		}
	} while (ipi_result == SCP_IPI_BUSY);

	if (ipi_result != IPI_ACTION_DONE) {
		pr_err("[ELUS] %s failed to send\n", __func__);
		return 0;
	}

	pr_debug("[ELUS] %s success\n", __func__);
	return ipi_result != IPI_ACTION_DONE;
}


int elliptic_data_io_cleanup(void)
{
	pr_info("[ELUS] Unimplemented");
    tasklet_kill(&elliptic_ipi_handler_data.handle_task);
    kfifo_free(&elliptic_ipi_handler_data.fifo);
	return 0;
}
MODULE_AUTHOR("Elliptic Labs");
MODULE_DESCRIPTION("Providing Interface to UPS data");
MODULE_LICENSE("GPL");

