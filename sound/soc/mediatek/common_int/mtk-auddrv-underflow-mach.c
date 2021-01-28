// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Michael Hsiao <michael.hsiao@mediatek.com>
 */

/*******************************************************************************
 *
 * Filename:
 * ---------
 *   AudDrv_devtree_parser.c
 *
 * Project:
 * --------
 *
 *
 * Description:
 * ------------
 *   AudDrv_devtree_parser
 *
 * Author:
 * -------
 *   Chipeng Chang (mtk02308)
 *
 *------------------------------------------------------------------------------
 *
 *
 ******************************************************************************
 */

#include "mtk-auddrv-underflow-mach.h"
#include <mt-plat/aee.h>

#define UnderflowrecordNumber (20)

static bool bEnableDump;
/* default setting for samplerate and interrupt count */
static unsigned int mDlSamplerate = 44100;
static const unsigned int DlSampleRateUpperBound = 192000;
static unsigned int InterruptSample = 1024;
static const unsigned int InterruptSampleUpperBound = 192000;
/* static bool bDumpInit = false; */

static unsigned int mDL1InterruptInterval;
static unsigned int mDL1_Interrupt_Interval_Limit;
static unsigned int mDl1Numerator = 8; /* 1.6 */
static unsigned int mDl1denominator = 5;
static unsigned long long Irq_time_t1 = 0, Irq_time_t2;
static bool bAudioInterruptChange;

static unsigned long long UnderflowTime[UnderflowrecordNumber] = {0};

static unsigned int UnderflowCounter;
static unsigned int UnderflowThreshold = 3;
static void ClearInterruptTiming(void);
static void DumpUnderFlowTime(void);
static void ClearUnderFlowTime(void);

void SetUnderFlowThreshold(unsigned int Threshold)
{
	UnderflowThreshold = Threshold;
}

/* base on devtree name to pares dev tree. */
void Auddrv_Aee_Dump(void)
{
	pr_debug("+%s\n", __func__);
	if (bEnableDump == true) {
#ifdef CONFIG_MTK_AEE_FEATURE
		aee_kernel_exception_api(__FILE__, __LINE__, DB_OPT_FTRACE,
					 "Audio is blocked",
					 "audio blocked dump ftrace");
#endif
	}
	Auddrv_Reset_Dump_State();
	pr_debug("-%s\n", __func__);
}

/*
 *   dump underflow time in kernel
 */
static void DumpUnderFlowTime(void)
{
	int i = 0;

	pr_debug("%s\n", __func__);
	for (i = 0; i < UnderflowrecordNumber; i++)
		pr_debug("UnderflowTime[%d] = %llu\n", i, UnderflowTime[i]);
}

/*
 *    when pcm playback is close , need to call this function to clear record.
 */

void Auddrv_Set_UnderFlow(void)
{
	unsigned long long underflow_time = sched_clock(); /* in ns (10^9) */

	pr_debug("%s UnderflowCounter = %d\n", __func__, UnderflowCounter);
	UnderflowTime[UnderflowCounter] = underflow_time;
	UnderflowCounter++;
	UnderflowCounter %= UnderflowrecordNumber;
	if (UnderflowCounter > UnderflowThreshold) {
		DumpUnderFlowTime();
		Auddrv_Aee_Dump();
	}
}

/*
 * dump underflow time in kernel
 */

static void ClearUnderFlowTime(void)
{
	int i = 0;

	for (i = 0; i < UnderflowrecordNumber; i++)
		UnderflowTime[i] = 0;
	UnderflowCounter = 0;
}

/*
 * when InterruptSample or mDlSamplerate is change , nned to refine
 * mDL1InterruptInterval
 */

void Auddrv_Set_Interrupt_Changed(bool bChange)
{
	bAudioInterruptChange = bChange;
}

/*
 * when pcm playback is close , need to call this function to clear record.
 */

void Auddrv_Reset_Dump_State(void)
{
	ClearUnderFlowTime();
	Auddrv_Set_Interrupt_Changed(false);
	ClearInterruptTiming();
}

/*
 * when in interrupt , call this function to check irq timing
 */
void Auddrv_CheckInterruptTiming(void)
{
	if (Irq_time_t1 == 0) {
		Irq_time_t1 = sched_clock(); /* in ns (10^9) */
	} else {
		Irq_time_t2 = Irq_time_t1;
		Irq_time_t1 = sched_clock(); /* in ns (10^9) */
		if (bAudioInterruptChange == true) {
			/* for Audio Interrupt is change , so this interrupt
			 * interval may not clear.clearqueue
			 */
			ClearInterruptTiming();
			return;
		}
		if ((Irq_time_t1 > Irq_time_t2) &&
		    mDL1_Interrupt_Interval_Limit) {
			pr_debug(
				"%s  Irq_time_t2 = %llu Irq_time_t1 = %llu t1 - t2 = %llu Interval_Limit = %d\n",
				__func__, Irq_time_t2, Irq_time_t1,
				Irq_time_t1 - Irq_time_t2,
				mDL1_Interrupt_Interval_Limit);
			Irq_time_t2 = Irq_time_t1 - Irq_time_t2;
			if (Irq_time_t2 >
			    (unsigned long long)mDL1_Interrupt_Interval_Limit *
				    1000000) {
				pr_debug(
					"%s interrupt may be blocked Irq_time_t2 = %llu Interval_Limit = %d\n",
					__func__, Irq_time_t2,
					mDL1_Interrupt_Interval_Limit);
			}
		}
	}
}

static void ClearInterruptTiming(void)
{
	pr_debug("%s\n", __func__);
	Irq_time_t1 = 0;
	Irq_time_t2 = 0;
}

/*
 * when InterruptSample or mDlSamplerate is change , nned to refine
 * mDL1InterruptInterval
 */

void RefineInterrruptInterval(void)
{
	mDL1InterruptInterval = ((InterruptSample * 1000) / mDlSamplerate) + 1;
	mDL1_Interrupt_Interval_Limit =
		mDL1InterruptInterval * mDl1Numerator / mDl1denominator;
	pr_debug(
		"%s mDL1InterruptInterval = %d mDL1_Interrupt_Interval_Limit = %d\n",
		__func__, mDL1InterruptInterval, mDL1_Interrupt_Interval_Limit);
}

/*
 *    function to set DL sampleRate
 */

bool Auddrv_Set_DlSamplerate(unsigned int Samplerate)
{
	pr_debug("%s Samplerate = %d\n", __func__, Samplerate);
	if (Samplerate < DlSampleRateUpperBound) {
		mDlSamplerate = Samplerate;
		RefineInterrruptInterval();
	}
	return true;
}

bool Auddrv_Set_InterruptSample(unsigned int count)
{
	pr_debug("%s count = %d\n", __func__, count);
	if (count < InterruptSampleUpperBound) {
		InterruptSample = count;
		RefineInterrruptInterval();
		Auddrv_Set_Interrupt_Changed(true);
	}
	return true;
}

/*
 * function to enable / disable dump , only enable will arised aee
 */
bool Auddrv_Enable_dump(bool bEnable)
{
	pr_debug("%s bEnable = %d\n", __func__, bEnable);
	bEnableDump = bEnable;
	return true;
}
