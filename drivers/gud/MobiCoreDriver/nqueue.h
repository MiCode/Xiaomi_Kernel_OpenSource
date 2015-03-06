/*
 * Copyright (c) 2013-2015 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __NQUEUE_H__
#define __NQUEUE_H__

int nqueue_init(uint16_t txq_length, uint16_t rxq_length);
void nqueue_cleanup(void);
int irq_handler_init(void);
void irq_handler_exit(void);

#endif /* __NQUEUE_H__ */
