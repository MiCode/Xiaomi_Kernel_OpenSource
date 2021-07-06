/*
 * Copyright (C) 2012-2019 NXP Semiconductors
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define P61_MAGIC 0xEA
#define P61_SET_PWR _IOW(P61_MAGIC, 0x01, long)
#define P61_SET_DBG _IOW(P61_MAGIC, 0x02, long)
#define P61_SET_POLL _IOW(P61_MAGIC, 0x03, long)
/* SPI can call this IOCTL to perform the eSE COLD_RESET
 * via NFC driver.
*/
#define ESE_PERFORM_COLD_RESET  _IOW(P61_MAGIC, 0x0C, long)
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
#define ENBLE_SPI_CLK     _IOW(P61_MAGIC, 0x0D, long)
#define DISABLE_SPI_CLK     _IOW(P61_MAGIC, 0x0E, long)
struct p61_spi_platform_data {
	unsigned int irq_gpio;
	unsigned int rst_gpio;
};
