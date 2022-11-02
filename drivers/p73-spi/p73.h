/******************************************************************************
 *
 *  Copyright 2012-2020 NXP
 *   *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 ******************************************************************************/

#define P61_MAGIC 0xEA
#define P61_SET_PWR _IOW(P61_MAGIC, 0x01, long)
#define P61_SET_DBG _IOW(P61_MAGIC, 0x02, long)
#define P61_SET_POLL _IOW(P61_MAGIC, 0x03, long)
/*
 * SPI Request NFCC to enable p61 power, only in param
 * Only for SPI
 * level 1 = Enable power
 * level 0 = Disable power
 */
#define P61_SET_SPM_PWR    _IOW(P61_MAGIC, 0x04, long)

/* SPI or DWP can call this ioctl to get the current
 * power state of P61
 *
*/
#define P61_GET_SPM_STATUS    _IOR(P61_MAGIC, 0x05, long)

#define P61_SET_THROUGHPUT    _IOW(P61_MAGIC, 0x06, long)
#define P61_GET_ESE_ACCESS    _IOW(P61_MAGIC, 0x07, long)

#define P61_SET_POWER_SCHEME  _IOW(P61_MAGIC, 0x08, long)

#define P61_SET_DWNLD_STATUS    _IOW(P61_MAGIC, 0x09, long)

#define P61_INHIBIT_PWR_CNTRL  _IOW(P61_MAGIC, 0x0A, long)

/* SPI can call this IOCTL to perform the eSE COLD_RESET
 * via NFC driver.
 */
#define ESE_PERFORM_COLD_RESET  _IOW(P61_MAGIC, 0x0C, long)

#define ENBLE_SPI_CLK     _IOW(P61_MAGIC, 0x0D, long)
#define DISABLE_SPI_CLK     _IOW(P61_MAGIC, 0x0E, long)

#define PERFORM_RESET_PROTECTION  _IOW(P61_MAGIC, 0x0F, long)

struct p61_spi_platform_data {
	unsigned int irq_gpio;
	unsigned int rst_gpio;
};
