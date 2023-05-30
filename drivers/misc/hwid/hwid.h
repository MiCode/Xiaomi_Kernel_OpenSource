// SPDX-License-Identifier: GPL-2.0
/*
 * HWID external interface declaration
 *
 * Copyright (C) 2021-2022 XiaoMi, Inc.
 */

#ifndef __HWID_H__
#define __HWID_H__

extern const char *get_hw_sku(void);
extern const char *get_hw_country(void);
extern const char *get_hw_level(void);
extern const char *get_hw_version(void);

#endif
