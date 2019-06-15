/* Copyright (c) 2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _NVME_QCOM_H_
#define _NVME_QCOM_H_

#ifdef CONFIG_NVME_QCOM

int nvme_qcom_parse_dt(struct device *dev);
void nvme_qcom_release_mapping(struct device *dev);

#else

static inline int nvme_qcom_parse_dt(struct device *dev)
{
	return 0;
}

static inline void nvme_qcom_release_mapping(struct device *dev)
{

}

#endif // CONFIG_NVME_QCOM

#endif // _NVME_QCOM_H_
