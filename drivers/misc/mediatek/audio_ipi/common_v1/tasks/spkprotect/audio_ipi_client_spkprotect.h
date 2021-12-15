/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 */

#ifndef AUDIO_IPI_CLIENT_SPKPRORECT_H
#define AUDIO_IPI_CLIENT_SPKPRORECT_H

#include <linux/fs.h>           /* needed by file_operations* */
#include "audio_messenger_ipi.h"


void audio_ipi_client_spkprotect_init(void);
void audio_ipi_client_spkprotect_deinit(void);
void spkprotect_dump_message(struct ipi_msg_t *ipi_msg);
int spkprotect_open_dump_file(void);
void spkprotect_close_dump_file(void);
void spk_pcm_dump_split_task_enable(void);
void spk_pcm_dump_split_task_disable(void);

#endif /* end of AUDIO_IPI_CLIENT_PLAYBACK_H */

