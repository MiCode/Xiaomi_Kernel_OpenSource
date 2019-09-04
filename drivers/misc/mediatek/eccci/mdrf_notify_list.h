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
 */


/* MD_RF_NOTIFY(0, LCM_NOTFY1, "LCM")
 * para. 0: bit in parameter md send;
 * para. 1: function name;
 * para. 2: module name;
 */
MD_RF_NOTIFY(0, primary_display_ccci_mipi_callback, "MIPI_CLK")
MD_RF_NOTIFY(1, primary_display_ccci_osc_callback, "LCM_OSC")
