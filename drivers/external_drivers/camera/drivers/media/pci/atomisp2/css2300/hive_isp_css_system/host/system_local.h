#ifndef __SYSTEM_LOCAL_H_INCLUDED__
#define __SYSTEM_LOCAL_H_INCLUDED__

#ifdef HRT_ISP_CSS_CUSTOM_HOST
#ifndef HRT_USE_VIR_ADDRS
#define HRT_USE_VIR_ADDRS
#endif
#ifdef __KERNEL__
#include <hive_isp_css_custom_host_hrt.h>
#endif
#endif

#include "system_global.h"

/* Surprise, this is a local property */
#define HRT_ADDRESS_WIDTH	32

#define GHANIUS 0

#if !defined(__KERNEL__) && !GHANIUS
#include <hrt/hive_types.h>
#else  /* __KERNEL__ */
/*
 * leaks through to the ISP code generation
 *
#include <stdint.h>

typedef uint32_t			hrt_address;
typedef uint32_t			hrt_vaddress;
typedef uint32_t			hrt_data;
*/
#define hrt_address			unsigned
#define hrt_vaddress		unsigned
#define hrt_data			unsigned
#endif /* __KERNEL__ */

/*
 * Cell specific address maps
 */
/* This is NOT a base address */
#define GP_FIFO_BASE   ((hrt_address)0x10200304)

/* DDR */
static const hrt_address DDR_BASE[N_DDR_ID] = {
	0x00000000UL};

/* ISP */
static const hrt_address ISP_CTRL_BASE[N_ISP_ID] = {
	0x10020000UL};

static const hrt_address ISP_DMEM_BASE[N_ISP_ID] = {
	0xffffffffUL};

static const hrt_address ISP_BAMEM_BASE[N_BAMEM_ID] = {
	0xffffffffUL};

static const hrt_address ISP_VAMEM_BASE[N_VAMEM_ID] = {
	0xffffffffUL,
	0xffffffffUL};

/* SP */
static const hrt_address SP_CTRL_BASE[N_SP_ID] = {
	0x10104000UL};

static const hrt_address SP_DMEM_BASE[N_SP_ID] = {
	0x10100000UL};

/* MMU */
static const hrt_address MMU_BASE[N_MMU_ID] = {
	0x10250000UL};

/* DMA */
static const hrt_address DMA_BASE[N_DMA_ID] = {
	0x10240000UL};

/* IRQ */
static const hrt_address IRQ_BASE[N_IRQ_ID] = {
	0x10200500UL};

/* GDC */
static const hrt_address GDC_BASE[N_GDC_ID] = {
	0x10180000UL};

/* FIFO_MONITOR (subset of GP_DEVICE) */
static const hrt_address FIFO_MONITOR_BASE[N_FIFO_MONITOR_ID] = {
	0x10200000UL};

/* GP_DEVICE */
static const hrt_address GP_DEVICE_BASE[N_GP_DEVICE_ID] = {
	0x10200000UL};

/* GPIO */
static const hrt_address GPIO_BASE[N_GPIO_ID] = {
	0x10200400UL};

/* TIMED_CTRL */
static const hrt_address TIMED_CTRL_BASE[N_TIMED_CTRL_ID] = {
	0x10200100UL};

/* INPUT_FORMATTER */
static const hrt_address INPUT_FORMATTER_BASE[N_INPUT_FORMATTER_ID] = {
	0x10210000UL,
	0x10270000UL};
/*	0x10220000UL, */ /* sec */
/*	0x10230000UL, */ /* memcpy() */

/* INPUT_SYSTEM (subset of GP_DEVICE) */
static const hrt_address INPUT_SYSTEM_BASE[N_INPUT_SYSTEM_ID] = {
	0x10200000UL};

/* RX */
static const hrt_address RX_BASE[N_RX_ID] = {
	0x10260000UL};

#endif /* __SYSTEM_LOCAL_H_INCLUDED__ */
