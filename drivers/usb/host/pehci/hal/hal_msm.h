/* 
* Copyright (C) ST-Ericsson AP Pte Ltd 2010 
*
* ISP1763 Linux OTG Controller driver : hal
* 
* This program is free software; you can redistribute it and/or modify it under the terms of 
* the GNU General Public License as published by the Free Software Foundation; version 
* 2 of the License. 
* 
* This program is distributed in the hope that it will be useful, but WITHOUT ANY  
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS  
* FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more  
* details. 
* 
* You should have received a copy of the GNU General Public License 
* along with this program; if not, write to the Free Software 
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA. 
* 
* This is a hardware abstraction layer header file.
* 
* Author : wired support <wired.support@stericsson.com>
*
*/

#ifndef	HAL_X86_H
#define	HAL_X86_H

#define	DRIVER_AUTHOR	"ST-ERICSSON	  "
#define	DRIVER_DESC	"ISP1763 bus driver"

/* Driver tuning, per ST-ERICSSON requirements:	*/

#define	MEM_TO_CHECK		4096	/*bytes, must be multiple of 2 */

/* BIT defines */
#define	BIT0	(1 << 0)
#define	BIT1	(1 << 1)
#define	BIT2	(1 << 2)
#define	BIT3	(1 << 3)
#define	BIT4	(1 << 4)
#define	BIT5	(1 << 5)
#define	BIT6	(1 << 6)
#define	BIT7	(1 << 7)
#define	BIT8	(1 << 8)
#define	BIT9	(1 << 9)
#define	BIT10	(1 << 10)
#define	BIT11	(1 << 11)
#define	BIT12	(1 << 12)
#define	BIT13	(1 << 13)
#define	BIT14	(1 << 14)
#define	BIT15	(1 << 15)
#define	BIT16	(1 << 16)
#define	BIT17	(1 << 17)
#define	BIT18	(1 << 18)
#define	BIT19	(1 << 19)
#define	BIT20	(1 << 20)
#define	BIT21	(1 << 21)
#define	BIT22	(1 << 22)
#define	BIT23	(1 << 23)
#define	BIT24	(1 << 24)
#define	BIT25	(1 << 26)
#define	BIT27	(1 << 27)
#define	BIT28	(1 << 28)
#define	BIT29	(1 << 29)
#define	BIT30	(1 << 30)
#define	BIT31	(1 << 31)

/* Definitions Related to Chip Address and CPU Physical	Address
 * cpu_phy_add:	CPU Physical Address , it uses 32 bit data per address
 * chip_add   :	Chip Address, it uses double word(64) bit data per address
 */
#define	chip_add(cpu_phy_add)		(((cpu_phy_add)	- 0x400) / 8)
#define	cpu_phy_add(chip_add)		((8 * (chip_add)) + 0x400)

/* for getting end add,	and start add, provided	we have	one address with us */
/* IMPORTANT length  hex(base16) and dec(base10) works fine*/
#define	end_add(start_add, length)	(start_add + (length - 4))
#define	start_add(end_add, length)	(end_add - (length - 4))

/* Device Registers*/
#define	DEV_UNLOCK_REGISTER		0x7C
#define	DEV_INTERRUPT_REGISTER		0x18
#define	INT_ENABLE_REGISTER		0x14

#endif /*_HAL_X86_H_ */
