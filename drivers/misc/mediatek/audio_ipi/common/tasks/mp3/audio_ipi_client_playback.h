/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 */

#ifndef AUDIO_IPI_CLIENT_PLAYBACK_H
#define AUDIO_IPI_CLIENT_PLAYBACK_H

#include <linux/fs.h>           /* needed by file_operations* */
#include "audio_messenger_ipi.h"

void audio_ipi_client_playback_init(void);
void audio_ipi_client_playback_deinit(void);
void playback_dump_message(struct ipi_msg_t *ipi_msg);
void playback_open_dump_file(void);
void playback_close_dump_file(void);


#endif /* end of AUDIO_IPI_CLIENT_PLAYBACK_H */

