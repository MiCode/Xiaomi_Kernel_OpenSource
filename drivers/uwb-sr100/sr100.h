/*====================================================================================*/
/*                                                                                    */
/*                        Copyright 2018-2019 NXP                                     */
/*                                                                                    */
/* This program is free software; you can redistribute it and/or modify               */
/* it under the terms of the GNU General Public License as published by               */
/* the Free Software Foundation; either version 2 of the License, or                  */
/* (at your option) any later version.                                                */
/*                                                                                    */
/* This program is distributed in the hope that it will be useful,                    */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of                     */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                      */
/* GNU General Public License for more details.                                       */
/*                                                                                    */
/* You should have received a copy of the GNU General Public License                  */
/* along with this program; if not, write to the Free Software                        */
/* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA          */
/*                                                                                    */
/*====================================================================================*/

#define SR100_MAGIC 0xEA
#define SR100_SET_PWR _IOW(SR100_MAGIC, 0x01, long)
#define SR100_SET_DBG _IOW(SR100_MAGIC, 0x02, long)
#define SR100_SET_POLL _IOW(SR100_MAGIC, 0x03, long)
#define SR100_SET_FWD _IOW(SR100_MAGIC, 0x04, long)
#define SR100_GET_THROUGHPUT _IOW(SR100_MAGIC, 0x05, long)
#define SR100_ESE_RESET _IOW(SR100_MAGIC, 0x06, long)

#define NORMAL_MODE_HEADER_LEN 4
#define HBCI_MODE_HEADER_LEN 4
#define NORMAL_MODE_LEN_OFFSET 3
#define UCI_NORMAL_PKT_SIZE 0

#define EXTND_LEN_INDICATOR_OFFSET 1
#define EXTND_LEN_INDICATOR_OFFSET_MASK 0x80
#define EXTENDED_LENGTH_OFFSET 2

struct sr100_spi_platform_data {
  unsigned int irq_gpio;
  unsigned int ce_gpio;
  unsigned int spi_handshake_gpio;
  unsigned int rtc_sync_gpio;
  unsigned int vdd_1v8_gpio; /* to control VDD for super interposer board */
  unsigned int vdd_1v8_rf_gpio;
  unsigned int vbat_3v6_gpio;
};
enum {
  PWR_DISABLE = 0,
  PWR_ENABLE,
  ABORT_READ_PENDING
};
