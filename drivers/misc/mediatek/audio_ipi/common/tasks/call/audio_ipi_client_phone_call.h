/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 */

#ifndef AUDIO_IPI_CLIENT_PHONE_CALL_H
#define AUDIO_IPI_CLIENT_PHONE_CALL_H

#include <linux/fs.h>           /* needed by file_operations* */

void audio_ipi_client_phone_call_init(void);
void audio_ipi_client_phone_call_deinit(void);

void open_dump_file(void);
void close_dump_file(void);


#endif /* end of AUDIO_IPI_CLIENT_PHONE_CALL_H */

