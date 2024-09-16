/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/

#ifndef _GL_SEC_H
#define _GL_SEC_H

extern void handle_sec_msg_1(unsigned char *msg_in, int msg_in_len, unsigned char *msg_out, int *msg_out_len);
extern void handle_sec_msg_2(unsigned char *msg_in, int msg_in_len, unsigned char *msg_out, int *msg_out_len);
extern void handle_sec_msg_3(unsigned char *msg_in, int msg_in_len, unsigned char *msg_out, int *msg_out_len);
extern void handle_sec_msg_4(unsigned char *msg_in, int msg_in_len, unsigned char *msg_out, int *msg_out_len);
extern void handle_sec_msg_5(unsigned char *msg_in, int msg_in_len, unsigned char *msg_out, int *msg_out_len);
extern void handle_sec_msg_final(unsigned char *msg_in, int msg_in_len, unsigned char *msg_out, int *msg_out_len);

#endif /* _GL_SEC_H */
