/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#ifndef _GPS_DL_HW_UTIL_H
#define _GPS_DL_HW_UTIL_H

#include "gps_dl_config.h"
#include "gps_dl_hw_type.h"
#include "gps_dl_time_tick.h"

enum GPS_DL_BUS_ENUM {
	GPS_DL_GPS_BUS,
	GPS_DL_BGF_BUS,
	GPS_DL_CONN_INFRA_BUS,
	GPS_DL_BUS_NUM
};

enum gps_dl_bus_rw_opt_enum {
	WR_NO_READ_BACK,
	RW_DO_CHECK,
	RW_FORCE_PRINT,
	RW_FULL_PRINT,
	RW_OPT_MAX = 32
};
#define BMASK_WR_NO_READ_BACK (1UL << WR_NO_READ_BACK)
#define BMASK_RW_DO_CHECK     (1UL << RW_DO_CHECK)
#define BMASK_RW_FORCE_PRINT  (1UL << RW_FORCE_PRINT)
#define BMASK_RW_FULL_PRINT   (1UL << RW_FULL_PRINT)

void gps_dl_bus_wr_opt(enum GPS_DL_BUS_ENUM bus_id, unsigned int bus_addr, unsigned int val,
	unsigned int opt_bitmask);
unsigned int gps_dl_bus_rd_opt(enum GPS_DL_BUS_ENUM bus_id, unsigned int bus_addr,
	unsigned int opt_bitmask);


/* the function will convert bus addr to host-view addr by remaping */
#if 0
void gps_dl_bus_wr32(u32 bus_addr, u32 val);
u32  gps_dl_bus_rd32(u32 bus_addr);
#endif
void gps_dl_bus_write(enum GPS_DL_BUS_ENUM bus_id, unsigned int bus_addr, unsigned int val);
void gps_dl_bus_write_no_rb(enum GPS_DL_BUS_ENUM bus_id, unsigned int bus_addr, unsigned int val);
unsigned int gps_dl_bus_read(enum GPS_DL_BUS_ENUM bus_id, unsigned int bus_addr);
void gps_dl_bus_check_and_print(unsigned int host_addr);


/* provide function/macro declaration for c files under hal folder */

/* todo: writel, mb */
#define ADDR(Field) (Field##_ADDR)
#define MASK(Field) (Field##_MASK)
#define SHFT(Field) (Field##_SHFT)

/* TODO: using volatile in kernel is almost an error */
#if 0
#define DL_SET_ENTRY(Field, Value) \
	   { conn_reg * addr = ADDR(Field); \
		 *addr = ((((conn_reg)(Value) << SHFT(Field)) \
					& MASK(Field)) | (*addr & ~MASK(Field))) ; }

#define DL_GET_ENTRY(Field) \
	  ((*ADDR(Field) & (MASK(Field))) >> SHFT(Field))
#endif

/* todo
 *	1. iomap the region to access or just writel can work?
 *      2. using volatile in kernel is almost an error. Replace it with accessor functions
 */
#if GPS_DL_ON_CTP
#define GPS_DL_HOST_REG_WR(host_addr, val)  ((*(conn_reg *)host_addr) = (val))
#define GPS_DL_HOST_REG_RD(host_addr)       (*(conn_reg *)host_addr)
#else
#define GPS_DL_HOST_REG_WR(host_addr, val)  do {} while (0)
#define GPS_DL_HOST_REG_RD(host_addr)       (0)
#endif

#define GDL_HW_WR_CONN_INFRA_REG(Addr, Value) gps_dl_bus_write(GPS_DL_CONN_INFRA_BUS, Addr, Value)
#define GDL_HW_RD_CONN_INFRA_REG(Addr)        gps_dl_bus_read(GPS_DL_CONN_INFRA_BUS, Addr)

#define GDL_HW_WR_BGF_REG(Addr, Value) gps_dl_bus_write(GPS_DL_BGF_BUS, Addr, Value)
#define GDL_HW_RD_BGF_REG(Addr)	       gps_dl_bus_read(GPS_DL_BGF_BUS, Addr)

#define GDL_HW_WR_GPS_REG(Addr, Value) gps_dl_bus_write(GPS_DL_GPS_BUS, Addr, Value)
#define GDL_HW_RD_GPS_REG(Addr)        gps_dl_bus_read(GPS_DL_GPS_BUS, Addr)


#define GDL_HW_SET_ENTRY(Bus_ID, Field, Value) do {     \
		conn_reg val;                                   \
		val = gps_dl_bus_read(Bus_ID, ADDR(Field));     \
		val &= (~MASK(Field));                          \
		val |= ((Value << SHFT(Field)) & MASK(Field));  \
		gps_dl_bus_write(Bus_ID, ADDR(Field), val);     \
	} while (0)

#define GDL_HW_GET_ENTRY(Bus_ID, Field)                 \
	((gps_dl_bus_read(Bus_ID, ADDR(Field)) & (MASK(Field))) >> SHFT(Field))

#define GDL_HW_EXTRACT_ENTRY(Field, Val)                \
	((Val & (MASK(Field))) >> SHFT(Field))

#define POLL_INTERVAL_US (100)
#define POLL_US      (1)
#define POLL_1_TIME  (0)
#define POLL_FOREVER (-1)
#define POLL_DEFAULT (1000 * POLL_US)
#if (GPS_DL_ON_CTP || GPS_DL_ON_LINUX)
#define GDL_HW_POLL_ENTRY_VERBOSE(Bus_ID, Field, pIsOkay, pLastValue, TimeoutUsec, condExpected) \
	do {                                                                       \
		if (pIsOkay != NULL) {                                             \
			*pIsOkay = false;                                          \
		}                                                                  \
		if (POLL_1_TIME == TimeoutUsec) {                                  \
			if (pLastValue != NULL) {                                  \
				*pLastValue = GDL_HW_GET_ENTRY(Bus_ID, Field);     \
			}                                                          \
			if ((condExpected)) {                                      \
				if (pIsOkay != NULL) {                             \
					*pIsOkay = true;                           \
				}                                                  \
			}                                                          \
		} else if (TimeoutUsec > 0) {                                      \
			unsigned int poll_wait_cnt = 0;                            \
			while (true) {                                             \
				if (pLastValue != NULL) {                          \
					*pLastValue = GDL_HW_GET_ENTRY(Bus_ID, Field); \
				}                                                  \
				if ((condExpected)) {                              \
					if (pIsOkay != NULL) {                     \
						*pIsOkay = true;                   \
					}                                          \
					break;                                     \
				}                                                  \
				if (poll_wait_cnt >= TimeoutUsec) {                \
					break;                                     \
				}                                                  \
				gps_dl_wait_us(POLL_INTERVAL_US);                  \
				poll_wait_cnt += POLL_INTERVAL_US;                 \
			}                                                          \
		} else if (TimeoutUsec <= POLL_FOREVER) {                          \
			while (true) {                                             \
				if (pLastValue != NULL) {                          \
					*pLastValue = GDL_HW_GET_ENTRY(Bus_ID, Field); \
				}                                                  \
				if ((condExpected)) {                              \
					if (pIsOkay != NULL) {                     \
						*pIsOkay = true;                   \
					}                                          \
					break;                                     \
				}                                                  \
				gps_dl_wait_us(POLL_INTERVAL_US);                  \
			}                                                          \
		}                                                                  \
	} while (0)

#define GDL_HW_POLL_ENTRY(Bus_ID, Field, ValueExpected, TimeoutUsec, pIsOkay)      \
	do {                                                                       \
		unsigned int gdl_hw_poll_value;                                    \
		GDL_HW_POLL_ENTRY_VERBOSE(Bus_ID, Field,                           \
			pIsOkay, &gdl_hw_poll_value,                               \
			TimeoutUsec, (gdl_hw_poll_value == ValueExpected));        \
	} while (0)
#else
#define GDL_HW_POLL_ENTRY(Bus_ID, Field, ValueExpected, TimeoutUsec, pIsOkay)      \
	do {                                                                       \
		if (ValueExpected == GDL_HW_GET_ENTRY(Bus_ID, Field)) {            \
			;                                                          \
		}                                                                  \
	} while (0)
#endif



#define GDL_HW_SET_CONN_INFRA_ENTRY(Field, Value) GDL_HW_SET_ENTRY(GPS_DL_CONN_INFRA_BUS, Field, Value)
#define GDL_HW_GET_CONN_INFRA_ENTRY(Field)        GDL_HW_GET_ENTRY(GPS_DL_CONN_INFRA_BUS, Field)
#define GDL_HW_POLL_CONN_INFRA_ENTRY(Field, ValueExpected, TimeoutUsec, pIsOkay) \
	GDL_HW_POLL_ENTRY(GPS_DL_CONN_INFRA_BUS, Field, ValueExpected, TimeoutUsec, pIsOkay)

#define GDL_HW_SET_BGF_ENTRY(Field, Value) GDL_HW_SET_ENTRY(GPS_DL_BGF_BUS, Field, Value)
#define GDL_HW_GET_BGF_ENTRY(Field)        GDL_HW_GET_ENTRY(GPS_DL_BGF_BUS, Field)
#define GDL_HW_POLL_BGF_ENTRY(Field, ValueExpected, TimeoutUsec, pIsOkay) \
	GDL_HW_POLL_ENTRY(GPS_DL_BGF_BUS, Field, ValueExpected, TimeoutUsec, pIsOkay)

#define GDL_HW_SET_GPS_ENTRY(Field, Value) GDL_HW_SET_ENTRY(GPS_DL_GPS_BUS, Field, Value)
#define GDL_HW_SET_GPS_ENTRY2(LinkID, Value, Field1, Field2) do {         \
		if (GPS_DATA_LINK_ID0 == LinkID)                          \
			GDL_HW_SET_GPS_ENTRY(Field1, Value);              \
		else if (GPS_DATA_LINK_ID1 == LinkID)                     \
			GDL_HW_SET_GPS_ENTRY(Field2, Value);              \
	} while (0)

#define GDL_HW_GET_GPS_ENTRY(Field)        GDL_HW_GET_ENTRY(GPS_DL_GPS_BUS, Field)
#define GDL_HW_GET_GPS_ENTRY2(LinkID, Field1, Field2) (                   \
		(GPS_DATA_LINK_ID0 == LinkID) ?                           \
			GDL_HW_GET_GPS_ENTRY(Field1) :                    \
		((GPS_DATA_LINK_ID1 == LinkID) ?                          \
			GDL_HW_GET_GPS_ENTRY(Field2) : 0)                 \
	)

#define GDL_HW_POLL_GPS_ENTRY(Field, ValueExpected, TimeoutUsec, pIsOkay) \
	GDL_HW_POLL_ENTRY(GPS_DL_GPS_BUS, Field, ValueExpected, TimeoutUsec, pIsOkay)

struct gps_dl_addr_map_entry {
	unsigned int host_addr;
	unsigned int bus_addr;
	unsigned int length;
};

unsigned int bgf_bus_to_host(unsigned int bgf_addr);
unsigned int gps_bus_to_host(unsigned int gps_addr);


#endif /* _GPS_DL_HW_UTIL_H */
