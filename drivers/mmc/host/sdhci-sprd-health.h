/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _SDHCI_SPRD_HEALTH_H
#define _SDHCI_SPRD_HEALTH_H

#define LITTLE_ENDIAN 0

#define TOTAL_HEALTH_BYTE 512
#define HEALTH_FILED_NUM 19
#define PROC_MODE 0440
#define HEALTH_CMD_ARG1 0x4B534BFB
#define HEALTH_CMD_ARG2 0x0D

int sprd_mmc_health_init(struct mmc_card *card);

#endif
