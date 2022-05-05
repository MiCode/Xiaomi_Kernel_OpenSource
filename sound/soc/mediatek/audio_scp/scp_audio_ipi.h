/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __SCP_AUDIO_IPI_H__
#define __SCP_AUDIO_IPI_H__

enum scp_audio_ipi_id {
	SCP_AUDIO_IPI_WDT = 0,
	SCP_AUDIO_IPI_TEST1 = 1,
	SCP_AUDIO_IPI_AUDIO = 9,
	SCP_AUDIO_NR_IPI,
};

typedef void (*scp_audio_ipi_handler_t)(int id, void *data, unsigned int len);
typedef int (*recv_queue_handler_t)(unsigned int cid, unsigned int ipi_id, void *buf,
				    unsigned int len, scp_audio_ipi_handler_t handler);

int scp_send_message(unsigned int id, void *buf, unsigned int len, unsigned int wait,
		     unsigned int cid);
bool is_scp_audio_ready(void);
bool is_audio_mbox_init_done(void);

int scp_audio_ipi_registration(unsigned int id, scp_audio_ipi_handler_t ipi_handler,
			       const char *name);
int scp_audio_ipi_unregistration(unsigned int id);

void hook_scp_ipi_queue_recv_msg_handler(recv_queue_handler_t queue_handler);
void unhook_scp_ipi_queue_recv_msg_handler(void);

#endif  /* __SCP_AUDIO_IPI_H__ */
