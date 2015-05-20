/*
  Copyright (C) 2014 Intel Corporation.  All Rights Reserved.

  This file is part of SEP Development Kit

  SEP Development Kit is free software; you can redistribute it
  and/or modify it under the terms of the GNU General Public License
  version 2 as published by the Free Software Foundation.

  SEP Development Kit is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with SEP Development Kit; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

  As a special exception, you may use this file as part of a free software
  library without restriction.  Specifically, if other files instantiate
  templates or use macros or inline functions from this file, or you compile
  this file and link it with other files to produce an executable, this
  file does not by itself cause the resulting executable to be covered by
  the GNU General Public License.  This exception does not however
  invalidate any other reasons why the executable file might be covered by
  the GNU General Public License.
*/
#ifndef _VTSS_NMIWD_H_
#define _VTSS_NMIWD_H_

#include "vtss_autoconf.h"

//Disable watchdog
//mode: 0 - disable watchdog
//      1 - disable watchdog and programme it enabling by calling vtss_nmi_watchdog_enable(1). 
//          If watchdog is disabled successfully memorise the state to be able enable watchdog  back after collection stops.
//returned values: 0 - watchdog was disabled successfully
//                 1 - watchdog is already disabled
//                <0 - error. cannot disable watchdog
int vtss_nmi_watchdog_disable (int mode);


//Enable watchdog
//mode: 0 - enable watchdog
//      1 - enable watchdog only if vtss_nmi_watchdog_disable(1) was called previously.
//          If watchdog is disabled successfully memorise the state to be able enable watchdog  back after collection stops.
//returned values: 0 - watchdog was enabled successfully
//                 1 - watchdog is already enabled
//                <0 - error. cannot enable watchdog
int vtss_nmi_watchdog_enable (int mode);

#endif /* _VTSS_NMIWD_H_ */
