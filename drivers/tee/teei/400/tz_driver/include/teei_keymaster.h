/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
 */

#ifndef TEEI_KEYMASTER_H
#define TEEI_KEYMASTER_H

extern struct semaphore keymaster_api_lock;

int send_keymaster_command(void *buffer, unsigned long size);
#endif /* end of TEEI_KEYMASTER_H */
