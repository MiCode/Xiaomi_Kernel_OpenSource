/*
	All files except if stated otherwise in the begining of the file
	are under the ISC license:
	----------------------------------------------------------------------

	Copyright (c) 2010-2012 Design Art Networks Ltd.

	Permission to use, copy, modify, and/or distribute this software for any
	purpose with or without fee is hereby granted, provided that the above
	copyright notice and this permission notice appear in all copies.

	THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
	WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
	MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
	ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
	WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
	ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
	OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/


#ifndef IPC_CONFIG_H_
#define IPC_CONFIG_H_

/*****************************************************************************
*                    TYPES
****************************************************************************
*/
/* Node type Enumeration */
enum ipc_node_type {
	undef_e,
	dan3400_e,	/* Node is located on the DAN3400 based board */
	dan3400_eth_e,	/* eth. proxy */
	ext_eth_e,	/* Node: on External board; Connected: Eth */
	ext_fser_e,	/* Node: on External board; Connected: Fast Serial */
	ext_uart_e,	/* Node: on External board; Connected: Uart */
};

#endif /* IPC_CONFIG_H_ */
