// SPDX-License-Identifier: GPL-2.0
/*
 * HWID external interface declaration
 *
 * Copyright (C) 2022 XiaoMi, Inc.
 */

#ifndef __HWID_H__
#define __HWID_H__

int get_hw_product(void);
int get_hw_version_major(void);
int get_hw_version_minor(void);

const char *get_hw_country(void);
const char *get_hw_level(void);
const char *get_hw_version(void);
const char *get_hw_sku(void);

#endif
