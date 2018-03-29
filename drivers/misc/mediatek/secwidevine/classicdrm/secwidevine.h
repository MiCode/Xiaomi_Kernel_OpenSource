/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef SECWIDEVINE_H
#define SECWIDEVINE_H


/*
struct secwidevine_param {
    u32 refcount;    INOUT
    u32 session_handle;  OUT - A pointer to mcSessionHandle_t
};
*/

#define CMD_SEC_WIDEVINE_INIT              1
#define CMD_SEC_WIDEVINE_TERMINATE         2
#define CMD_SEC_WIDEVINE_SETENTITLMENTKEY  3
#define CMD_SEC_WIDEVINE_DERIVECONTROLWORD 4
#define CMD_SEC_WIDEVINE_ISKEYBOXVALID     5
#define CMD_SEC_WIDEVINE_GETDEVICEID       6
#define CMD_SEC_WIDEVINE_GETKEYDATA        7
#define CMD_SEC_WIDEVINE_GETRANDOM         8

#define SECWIDEVINE_IOC_MAGIC      'T'
#define SECWIDEVINE_INIT               _IOWR(SECWIDEVINE_IOC_MAGIC, 1, struct secwidevine_param)
#define SECWIDEVINE_TERMINATE          _IOWR(SECWIDEVINE_IOC_MAGIC, 2, struct secwidevine_param)
#define SECWIDEVINE_SETENTITLMENTKEY   _IOWR(SECWIDEVINE_IOC_MAGIC, 3, struct secwidevine_param)
#define SECWIDEVINE_DERIVECONTROLWORD  _IOWR(SECWIDEVINE_IOC_MAGIC, 4, struct secwidevine_param)
#define SECWIDEVINE_ISKEYBOXVALID      _IOWR(SECWIDEVINE_IOC_MAGIC, 5, struct secwidevine_param)
#define SECWIDEVINE_GETDEVICEID        _IOWR(SECWIDEVINE_IOC_MAGIC, 6, struct secwidevine_param)
#define SECWIDEVINE_GETKEYDATA         _IOWR(SECWIDEVINE_IOC_MAGIC, 7, struct secwidevine_param)
#define SECWIDEVINE_GETRANDOM          _IOWR(SECWIDEVINE_IOC_MAGIC, 8, struct secwidevine_param)

#define SECWIDEVINE_IOC_MAXNR      (10)

#endif /* end of SECWIDEVINE_H */
