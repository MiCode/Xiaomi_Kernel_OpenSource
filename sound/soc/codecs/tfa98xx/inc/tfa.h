/* 
 * Copyright (C) 2014-2020 NXP Semiconductors, All Rights Reserved.
 * Copyright 2020 GOODIX 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */


#ifndef TFA_H_
#define TFA_H_

/* set the limit for the container file length */
#define TFA_MAX_CNT_LENGTH (256*1024)

extern struct tfa_device **devs;

/**
 * tfa error return codes
 */
enum tfa_error {
        tfa_error_ok,       /**< no error */
        tfa_error_device,   /**< no response from device */
        tfa_error_bad_param,/**< parameter no accepted */
        tfa_error_noclock,  /**< required clock not present */
        tfa_error_timeout,  /**< a timeout occurred */
        tfa_error_dsp,      /**< a DSP error was returned */
        tfa_error_container,/**< no or wrong container file */
        tfa_error_max       /**< impossible value, max enum */
};

enum Tfa98xx_Error tfa_write_filters(struct tfa_device *tfa, int prof_idx);

struct tfa_device ** tfa_devs_create(int count);
void tfa_devs_destroy(int count);

struct tfa_device ** tfa_get_device_struct(void);

int tfa_plop_noise_interrupt(struct tfa_device *tfa, int profile, int vstep);
void tfa_lp_mode_interrupt(struct tfa_device *tfa);
void tfa_adapt_noisemode(struct tfa_device *tfa);

#endif /* TFA_H_ */
