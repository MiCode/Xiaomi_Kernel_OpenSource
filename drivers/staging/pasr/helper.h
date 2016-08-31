/*
 * Copyright (C) ST-Ericsson SA 2011
 * Author: Maxime Coquelin <maxime.coquelin@stericsson.com> for ST-Ericsson.
 * License terms:  GNU General Public License (GPL), version 2
 */

#ifndef _PASR_HELPER_H
#define _PASR_HELPER_H

#include <linux/pasr.h>

struct pasr_die *pasr_addr2die(struct pasr_map *map, phys_addr_t addr);
struct pasr_section *pasr_addr2section(struct pasr_map *map, phys_addr_t addr);
phys_addr_t pasr_section2addr(struct pasr_section *s);

#endif /* _PASR_HELPER_H */
