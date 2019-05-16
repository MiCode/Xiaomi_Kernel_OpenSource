/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 TRUSTONIC LIMITED
 */

#ifndef _MC_LOGGING_H_
#define _MC_LOGGING_H_

void logging_run(void);
int logging_init(phys_addr_t *buffer, u32 *size);
void logging_exit(bool buffer_busy);

#endif /* _MC_LOGGING_H_ */
