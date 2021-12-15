/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2018 MediaTek Inc.
 */

#ifndef AUDIO_IPI_CLIENT_ULTRA_H
#define AUDIO_IPI_CLIENT_ULTRA_H

#include <linux/fs.h>           /* needed by file_operations* */


void audio_ipi_client_ultra_init(void);
void audio_ipi_client_ultra_deinit(void);
void ultra_dump_message(void *msg_data);
int ultra_open_dump_file(void);
void ultra_close_dump_file(void);
void ultra_pcm_dump_split_task_enable(void);
void ultra_pcm_dump_split_task_disable(void);
int ultra_start_engine_thread(void);
void ultra_stop_engine_thread(void);

#endif /* end of AUDIO_IPI_CLIENT_ULTRA_H */

