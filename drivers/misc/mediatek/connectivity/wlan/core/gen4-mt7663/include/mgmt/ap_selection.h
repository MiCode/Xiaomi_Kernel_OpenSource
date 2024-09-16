/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef _AP_SELECTION_H
#define _AP_SELECTION_H

/* Support AP Selection */
struct BSS_DESC *scanSearchBssDescByScoreForAis(struct ADAPTER *prAdapter);
void scanGetCurrentEssChnlList(struct ADAPTER *prAdapter);
/* end Support AP Selection */

#endif
