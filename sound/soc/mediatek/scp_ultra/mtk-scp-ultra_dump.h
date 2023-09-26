/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
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

