/* Copyright (c) 2017-2020 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef BGCOM_INTERFACE_H
#define BGCOM_INTERFACE_H

/*
 * bg_soft_reset() - soft reset Blackghost
 * Return 0 on success or -Ve on error
 */
int bg_soft_reset(void);

/*
 * is_twm_exit()
 * Return true if device is booting up on TWM exit.
 * value is auto cleared once read.
 */
bool is_twm_exit(void);

/*
 * is_bg_running()
 * Return true if bg is running.
 * value is auto cleared once read.
 */
bool is_bg_running(void);

#endif /* BGCOM_INTERFACE_H */
