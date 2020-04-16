/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _RS_LOG_H_
#define _RS_LOG_H_

#ifdef RS_DEBUG
#define RS_LOGI(...)	pr_debug("RS:" __VA_ARGS__)
#else
#define RS_LOGI(...)
#endif
#define RS_LOGE(...)	pr_debug("RS:" __VA_ARGS__)

#endif
