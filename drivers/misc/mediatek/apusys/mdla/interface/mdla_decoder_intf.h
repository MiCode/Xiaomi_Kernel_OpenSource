/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MDLA_DECODER_INTF_H__
#define __MDLA_DECODER_INTF_H__

void mdla_decode_v1_0(const char *cmd, char *str, int size);
void mdla_decode_v1_x(const char *cmd, char *str, int size);
void mdla_decode_v2_0(const char *cmd, char *str, int size);

#endif /* __MDLA_DECODER_INTF_H__ */

