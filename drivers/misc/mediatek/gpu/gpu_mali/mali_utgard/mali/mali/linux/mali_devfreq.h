/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2011-2016 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#ifndef _MALI_DEVFREQ_H_
#define _MALI_DEVFREQ_H_

int mali_devfreq_init(struct mali_device *mdev);

void mali_devfreq_term(struct mali_device *mdev);

#endif
