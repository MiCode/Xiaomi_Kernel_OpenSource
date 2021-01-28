/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2017 MediaTek Inc.
 */

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

#ifndef _MT_GPUFREQ_CORE_H_
#define _MT_GPUFREQ_CORE_H_

/**************************************************
 * GPU DVFS OPP table Setting
 **************************************************/
#define ENABLE_BUCK_CONTROL 1
#define ENABLE_MTCMOS_CONTROL 1
#define USE_FINE_GRAIN_OPP_TABLE
#define USE_COMPLETE_VOLT_SWITCH_SOLUTION
/*************************************************
 * MT6779 segment_1 :
 *************************************************/
#define SEG1_GPU_DVFS_FREQ0		(970000)/* KHz */
#define SEG1_GPU_DVFS_FREQ1		(944000)/* KHz */
#define SEG1_GPU_DVFS_FREQ2		(918000)/* KHz */
#define SEG1_GPU_DVFS_FREQ3		(892000)/* KHz */
#define SEG1_GPU_DVFS_FREQ4		(866000)/* KHz */
#define SEG1_GPU_DVFS_FREQ5		(841000)/* KHz */
#define SEG1_GPU_DVFS_FREQ6		(815000)/* KHz */
#define SEG1_GPU_DVFS_FREQ7		(800000)/* KHz */
#define SEG1_GPU_DVFS_FREQ8		(776000)/* KHz */
#define SEG1_GPU_DVFS_FREQ9		(763000)/* KHz */
#define SEG1_GPU_DVFS_FREQ10	(743000)/* KHz */
#define SEG1_GPU_DVFS_FREQ11	(722000)/* KHz */
#define SEG1_GPU_DVFS_FREQ12	(702000)/* KHz */
#define SEG1_GPU_DVFS_FREQ13	(681000)/* KHz */
#define SEG1_GPU_DVFS_FREQ14	(661000)/* KHz */
#define SEG1_GPU_DVFS_FREQ15	(640000)/* KHz */
#define SEG1_GPU_DVFS_FREQ16	(618000)/* KHz */
#define SEG1_GPU_DVFS_FREQ17	(596000)/* KHz */
#define SEG1_GPU_DVFS_FREQ18	(575000)/* KHz */
#define SEG1_GPU_DVFS_FREQ19	(553000)/* KHz */
#define SEG1_GPU_DVFS_FREQ20	(532000)/* KHz */
#define SEG1_GPU_DVFS_FREQ21	(510000)/* KHz */
#define SEG1_GPU_DVFS_FREQ22	(489000)/* KHz */
#define SEG1_GPU_DVFS_FREQ23	(467000)/* KHz */
#define SEG1_GPU_DVFS_FREQ24	(446000)/* KHz */
#define SEG1_GPU_DVFS_FREQ25	(424000)/* KHz */
#define SEG1_GPU_DVFS_FREQ26	(403000)/* KHz */
#define SEG1_GPU_DVFS_FREQ27	(381000)/* KHz */
#define SEG1_GPU_DVFS_FREQ28	(360000)/* KHz */
#define SEG1_GPU_DVFS_FREQ29	(338000)/* KHz */
#define SEG1_GPU_DVFS_FREQ30	(317000)/* KHz */
#define SEG1_GPU_DVFS_FREQ31	(295000)/* KHz */

#define SEG1_GPU_DVFS_VOLT0		(95000)	/* mV x 100 */
#define SEG1_GPU_DVFS_VOLT1		(93750)	/* mV x 100 */
#define SEG1_GPU_DVFS_VOLT2		(91875)	/* mV x 100 */
#define SEG1_GPU_DVFS_VOLT3		(90625)	/* mV x 100 */
#define SEG1_GPU_DVFS_VOLT4		(88750)	/* mV x 100 */
#define SEG1_GPU_DVFS_VOLT5		(87500)	/* mV x 100 */
#define SEG1_GPU_DVFS_VOLT6		(85625)	/* mV x 100 */
#define SEG1_GPU_DVFS_VOLT7		(85000)	/* mV x 100 */
#define SEG1_GPU_DVFS_VOLT8		(83750)	/* mV x 100 */
#define SEG1_GPU_DVFS_VOLT9		(82500)	/* mV x 100 */
#define SEG1_GPU_DVFS_VOLT10	(81250)	/* mV x 100 */
#define SEG1_GPU_DVFS_VOLT11	(80000)	/* mV x 100 */
#define SEG1_GPU_DVFS_VOLT12	(78750)	/* mV x 100 */
#define SEG1_GPU_DVFS_VOLT13	(77500)	/* mV x 100 */
#define SEG1_GPU_DVFS_VOLT14	(76250)	/* mV x 100 */
#define SEG1_GPU_DVFS_VOLT15	(75000)	/* mV x 100 */
#define SEG1_GPU_DVFS_VOLT16	(74375)	/* mV x 100 */
#define SEG1_GPU_DVFS_VOLT17	(73125)	/* mV x 100 */
#define SEG1_GPU_DVFS_VOLT18	(72500)	/* mV x 100 */
#define SEG1_GPU_DVFS_VOLT19	(71250)	/* mV x 100 */
#define SEG1_GPU_DVFS_VOLT20	(70625)	/* mV x 100 */
#define SEG1_GPU_DVFS_VOLT21	(69375)	/* mV x 100 */
#define SEG1_GPU_DVFS_VOLT22	(68750)	/* mV x 100 */
#define SEG1_GPU_DVFS_VOLT23	(67500)	/* mV x 100 */
#define SEG1_GPU_DVFS_VOLT24	(66875)	/* mV x 100 */
#define SEG1_GPU_DVFS_VOLT25	(65625)	/* mV x 100 */
#define SEG1_GPU_DVFS_VOLT26	(65000)	/* mV x 100 */
#define SEG1_GPU_DVFS_VOLT27	(63750)	/* mV x 100 */
#define SEG1_GPU_DVFS_VOLT28	(63125)	/* mV x 100 */
#define SEG1_GPU_DVFS_VOLT29	(61875)	/* mV x 100 */
#define SEG1_GPU_DVFS_VOLT30	(61250)	/* mV x 100 */
#define SEG1_GPU_DVFS_VOLT31	(60000)	/* mV x 100 */

#define SEG1_GPU_DVFS_VSRAM0	(95000)	/* mV x 100 */
#define SEG1_GPU_DVFS_VSRAM1	(93750)	/* mV x 100 */
#define SEG1_GPU_DVFS_VSRAM2	(91875)	/* mV x 100 */
#define SEG1_GPU_DVFS_VSRAM3	(90625)	/* mV x 100 */
#define SEG1_GPU_DVFS_VSRAM4	(88750)	/* mV x 100 */
#define SEG1_GPU_DVFS_VSRAM5	(87500)	/* mV x 100 */
/*************************************************
 * MT6779T segment_2 :
 *************************************************/
#define SEG2_GPU_DVFS_FREQ0		(1050000)/* KHz */
#define SEG2_GPU_DVFS_FREQ1		(1010000)/* KHz */
#define SEG2_GPU_DVFS_FREQ2		(970000)/* KHz */
#define SEG2_GPU_DVFS_FREQ3		(944000)/* KHz */
#define SEG2_GPU_DVFS_FREQ4		(918000)/* KHz */
#define SEG2_GPU_DVFS_FREQ5		(892000)/* KHz */
#define SEG2_GPU_DVFS_FREQ6		(866000)/* KHz */
#define SEG2_GPU_DVFS_FREQ7		(841000)/* KHz */
#define SEG2_GPU_DVFS_FREQ8		(815000)/* KHz */
#define SEG2_GPU_DVFS_FREQ9		(800000)/* KHz */
#define SEG2_GPU_DVFS_FREQ10	(776000)/* KHz */
#define SEG2_GPU_DVFS_FREQ11	(763000)/* KHz */
#define SEG2_GPU_DVFS_FREQ12	(743000)/* KHz */
#define SEG2_GPU_DVFS_FREQ13	(722000)/* KHz */
#define SEG2_GPU_DVFS_FREQ14	(702000)/* KHz */
#define SEG2_GPU_DVFS_FREQ15	(681000)/* KHz */
#define SEG2_GPU_DVFS_FREQ16	(661000)/* KHz */
#define SEG2_GPU_DVFS_FREQ17	(640000)/* KHz */
#define SEG2_GPU_DVFS_FREQ18	(611000)/* KHz */
#define SEG2_GPU_DVFS_FREQ19	(582000)/* KHz */
#define SEG2_GPU_DVFS_FREQ20	(554000)/* KHz */
#define SEG2_GPU_DVFS_FREQ21	(525000)/* KHz */
#define SEG2_GPU_DVFS_FREQ22	(496000)/* KHz */
#define SEG2_GPU_DVFS_FREQ23	(467000)/* KHz */
#define SEG2_GPU_DVFS_FREQ24	(446000)/* KHz */
#define SEG2_GPU_DVFS_FREQ25	(424000)/* KHz */
#define SEG2_GPU_DVFS_FREQ26	(403000)/* KHz */
#define SEG2_GPU_DVFS_FREQ27	(381000)/* KHz */
#define SEG2_GPU_DVFS_FREQ28	(360000)/* KHz */
#define SEG2_GPU_DVFS_FREQ29	(338000)/* KHz */
#define SEG2_GPU_DVFS_FREQ30	(317000)/* KHz */
#define SEG2_GPU_DVFS_FREQ31	(295000)/* KHz */

#define SEG2_GPU_DVFS_VOLT0		(105000)	/* mV x 100 */
#define SEG2_GPU_DVFS_VOLT1		(100000)	/* mV x 100 */
#define SEG2_GPU_DVFS_VOLT2		(95000)	/* mV x 100 */
#define SEG2_GPU_DVFS_VOLT3		(93750)	/* mV x 100 */
#define SEG2_GPU_DVFS_VOLT4		(91875)	/* mV x 100 */
#define SEG2_GPU_DVFS_VOLT5		(90625)	/* mV x 100 */
#define SEG2_GPU_DVFS_VOLT6		(88750)	/* mV x 100 */
#define SEG2_GPU_DVFS_VOLT7		(87500)	/* mV x 100 */
#define SEG2_GPU_DVFS_VOLT8		(85625)	/* mV x 100 */
#define SEG2_GPU_DVFS_VOLT9		(85000)	/* mV x 100 */
#define SEG2_GPU_DVFS_VOLT10	(83750)	/* mV x 100 */
#define SEG2_GPU_DVFS_VOLT11	(82500)	/* mV x 100 */
#define SEG2_GPU_DVFS_VOLT12	(81250)	/* mV x 100 */
#define SEG2_GPU_DVFS_VOLT13	(80000)	/* mV x 100 */
#define SEG2_GPU_DVFS_VOLT14	(78750)	/* mV x 100 */
#define SEG2_GPU_DVFS_VOLT15	(77500)	/* mV x 100 */
#define SEG2_GPU_DVFS_VOLT16	(76250)	/* mV x 100 */
#define SEG2_GPU_DVFS_VOLT17	(75000)	/* mV x 100 */
#define SEG2_GPU_DVFS_VOLT18	(73750)	/* mV x 100 */
#define SEG2_GPU_DVFS_VOLT19	(72500)	/* mV x 100 */
#define SEG2_GPU_DVFS_VOLT20	(71250)	/* mV x 100 */
#define SEG2_GPU_DVFS_VOLT21	(70000)	/* mV x 100 */
#define SEG2_GPU_DVFS_VOLT22	(68750)	/* mV x 100 */
#define SEG2_GPU_DVFS_VOLT23	(67500)	/* mV x 100 */
#define SEG2_GPU_DVFS_VOLT24	(66875)	/* mV x 100 */
#define SEG2_GPU_DVFS_VOLT25	(65625)	/* mV x 100 */
#define SEG2_GPU_DVFS_VOLT26	(65000)	/* mV x 100 */
#define SEG2_GPU_DVFS_VOLT27	(63750)	/* mV x 100 */
#define SEG2_GPU_DVFS_VOLT28	(63125)	/* mV x 100 */
#define SEG2_GPU_DVFS_VOLT29	(61875)	/* mV x 100 */
#define SEG2_GPU_DVFS_VOLT30	(61250)	/* mV x 100 */
#define SEG2_GPU_DVFS_VOLT31	(60000)	/* mV x 100 */

#define SEG2_GPU_DVFS_VSRAM0	(105000)	/* mV x 100 */
#define SEG2_GPU_DVFS_VSRAM1	(100000)	/* mV x 100 */
#define SEG2_GPU_DVFS_VSRAM2	(95000)	/* mV x 100 */
#define SEG2_GPU_DVFS_VSRAM3	(93750)	/* mV x 100 */
#define SEG2_GPU_DVFS_VSRAM4	(91875)	/* mV x 100 */
#define SEG2_GPU_DVFS_VSRAM5	(90625)	/* mV x 100 */
#define SEG2_GPU_DVFS_VSRAM6	(88750)	/* mV x 100 */
#define SEG2_GPU_DVFS_VSRAM7	(87500)	/* mV x 100 */
/*************************************************
 * P90M segment_3(lite) :
 *************************************************/
#define SEG3_GPU_DVFS_FREQ0	 (897000)/* KHz */
#define SEG3_GPU_DVFS_FREQ1	 (882000)/* KHz */
#define SEG3_GPU_DVFS_FREQ2	 (866000)/* KHz */
#define SEG3_GPU_DVFS_FREQ3	 (851000)/* KHz */
#define SEG3_GPU_DVFS_FREQ4	 (835000)/* KHz */
#define SEG3_GPU_DVFS_FREQ5	 (820000)/* KHz */
#define SEG3_GPU_DVFS_FREQ6	 (805000)/* KHz */
#define SEG3_GPU_DVFS_FREQ7	 (790000)/* KHz */
#define SEG3_GPU_DVFS_FREQ8	 (774000)/* KHz */
#define SEG3_GPU_DVFS_FREQ9	 (760000)/* KHz */
#define SEG3_GPU_DVFS_FREQ10	 (746000)/* KHz */
#define SEG3_GPU_DVFS_FREQ11	 (732000)/* KHz */
#define SEG3_GPU_DVFS_FREQ12	 (717000)/* KHz */
#define SEG3_GPU_DVFS_FREQ13	 (701000)/* KHz */
#define SEG3_GPU_DVFS_FREQ14	 (686000)/* KHz */
#define SEG3_GPU_DVFS_FREQ15	 (670000)/* KHz */
#define SEG3_GPU_DVFS_FREQ16	 (655000)/* KHz */
#define SEG3_GPU_DVFS_FREQ17	 (640000)/* KHz */
#define SEG3_GPU_DVFS_FREQ18	 (611000)/* KHz */
#define SEG3_GPU_DVFS_FREQ19	 (582000)/* KHz */
#define SEG3_GPU_DVFS_FREQ20	 (554000)/* KHz */
#define SEG3_GPU_DVFS_FREQ21	 (525000)/* KHz */
#define SEG3_GPU_DVFS_FREQ22	 (496000)/* KHz */
#define SEG3_GPU_DVFS_FREQ23	 (467000)/* KHz */
#define SEG3_GPU_DVFS_FREQ24	 (446000)/* KHz */
#define SEG3_GPU_DVFS_FREQ25	 (424000)/* KHz */
#define SEG3_GPU_DVFS_FREQ26	 (403000)/* KHz */
#define SEG3_GPU_DVFS_FREQ27	 (381000)/* KHz */
#define SEG3_GPU_DVFS_FREQ28	 (360000)/* KHz */
#define SEG3_GPU_DVFS_FREQ29	 (338000)/* KHz */
#define SEG3_GPU_DVFS_FREQ30	 (317000)/* KHz */
#define SEG3_GPU_DVFS_FREQ31	 (295000)/* KHz */

#define SEG3_GPU_DVFS_VOLT0	(90625)	/* mV x 100 */
#define SEG3_GPU_DVFS_VOLT1	(90000)	/* mV x 100 */
#define SEG3_GPU_DVFS_VOLT2	(88750)	/* mV x 100 */
#define SEG3_GPU_DVFS_VOLT3	(88125)	/* mV x 100 */
#define SEG3_GPU_DVFS_VOLT4	(86875)	/* mV x 100 */
#define SEG3_GPU_DVFS_VOLT5	(86250)	/* mV x 100 */
#define SEG3_GPU_DVFS_VOLT6	(85000)	/* mV x 100 */
#define SEG3_GPU_DVFS_VOLT7	(84375)	/* mV x 100 */
#define SEG3_GPU_DVFS_VOLT8	(83125)	/* mV x 100 */
#define SEG3_GPU_DVFS_VOLT9	(82500)	/* mV x 100 */
#define SEG3_GPU_DVFS_VOLT10	(81875)	/* mV x 100 */
#define SEG3_GPU_DVFS_VOLT11	(80625)	/* mV x 100 */
#define SEG3_GPU_DVFS_VOLT12	(80000)	/* mV x 100 */
#define SEG3_GPU_DVFS_VOLT13	(78750)	/* mV x 100 */
#define SEG3_GPU_DVFS_VOLT14	(78125)	/* mV x 100 */
#define SEG3_GPU_DVFS_VOLT15	(76875)	/* mV x 100 */
#define SEG3_GPU_DVFS_VOLT16	(76250)	/* mV x 100 */
#define SEG3_GPU_DVFS_VOLT17	(75000)	/* mV x 100 */
#define SEG3_GPU_DVFS_VOLT18	(73750)	/* mV x 100 */
#define SEG3_GPU_DVFS_VOLT19	(72500)	/* mV x 100 */
#define SEG3_GPU_DVFS_VOLT20	(71250)	/* mV x 100 */
#define SEG3_GPU_DVFS_VOLT21	(70000)	/* mV x 100 */
#define SEG3_GPU_DVFS_VOLT22	(68750)	/* mV x 100 */
#define SEG3_GPU_DVFS_VOLT23	(67500)	/* mV x 100 */
#define SEG3_GPU_DVFS_VOLT24	(66875)	/* mV x 100 */
#define SEG3_GPU_DVFS_VOLT25	(65625)	/* mV x 100 */
#define SEG3_GPU_DVFS_VOLT26	(65000)	/* mV x 100 */
#define SEG3_GPU_DVFS_VOLT27	(63750)	/* mV x 100 */
#define SEG3_GPU_DVFS_VOLT28	(63125)	/* mV x 100 */
#define SEG3_GPU_DVFS_VOLT29	(61875)	/* mV x 100 */
#define SEG3_GPU_DVFS_VOLT30	(61250)	/* mV x 100 */
#define SEG3_GPU_DVFS_VOLT31	(60000)	/* mV x 100 */

#define SEG3_GPU_DVFS_VSRAM0	(90625)	/* mV x 100 */
#define SEG3_GPU_DVFS_VSRAM1	(90000)	/* mV x 100 */
#define SEG3_GPU_DVFS_VSRAM2	(88750)	/* mV x 100 */
#define SEG3_GPU_DVFS_VSRAM3	(88125)	/* mV x 100 */
#define SEG3_GPU_DVFS_VSRAM4	(87500)	/* mV x 100 */
/*************************************************
 * MT6779T segment_4(0.625) :
 *************************************************/
#define SEG4_GPU_DVFS_FREQ0		(1050000)/* KHz */
#define SEG4_GPU_DVFS_FREQ1		(1010000)/* KHz */
#define SEG4_GPU_DVFS_FREQ2		(970000)/* KHz */
#define SEG4_GPU_DVFS_FREQ3		(944000)/* KHz */
#define SEG4_GPU_DVFS_FREQ4		(918000)/* KHz */
#define SEG4_GPU_DVFS_FREQ5		(892000)/* KHz */
#define SEG4_GPU_DVFS_FREQ6		(866000)/* KHz */
#define SEG4_GPU_DVFS_FREQ7		(841000)/* KHz */
#define SEG4_GPU_DVFS_FREQ8		(815000)/* KHz */
#define SEG4_GPU_DVFS_FREQ9		(800000)/* KHz */
#define SEG4_GPU_DVFS_FREQ10	(776000)/* KHz */
#define SEG4_GPU_DVFS_FREQ11	(763000)/* KHz */
#define SEG4_GPU_DVFS_FREQ12	(743000)/* KHz */
#define SEG4_GPU_DVFS_FREQ13	(722000)/* KHz */
#define SEG4_GPU_DVFS_FREQ14	(702000)/* KHz */
#define SEG4_GPU_DVFS_FREQ15	(681000)/* KHz */
#define SEG4_GPU_DVFS_FREQ16	(661000)/* KHz */
#define SEG4_GPU_DVFS_FREQ17	(640000)/* KHz */
#define SEG4_GPU_DVFS_FREQ18	(606000)/* KHz */
#define SEG4_GPU_DVFS_FREQ19	(571000)/* KHz */
#define SEG4_GPU_DVFS_FREQ20	(537000)/* KHz */
#define SEG4_GPU_DVFS_FREQ21	(502000)/* KHz */
#define SEG4_GPU_DVFS_FREQ22	(468000)/* KHz */
#define SEG4_GPU_DVFS_FREQ23	(433000)/* KHz */
#define SEG4_GPU_DVFS_FREQ24	(416000)/* KHz */
#define SEG4_GPU_DVFS_FREQ25	(398000)/* KHz */
#define SEG4_GPU_DVFS_FREQ26	(381000)/* KHz */
#define SEG4_GPU_DVFS_FREQ27	(364000)/* KHz */
#define SEG4_GPU_DVFS_FREQ28	(346000)/* KHz */
#define SEG4_GPU_DVFS_FREQ29	(329000)/* KHz */
#define SEG4_GPU_DVFS_FREQ30	(312000)/* KHz */
#define SEG4_GPU_DVFS_FREQ31	(295000)/* KHz */

#define SEG4_GPU_DVFS_VOLT0	(105000)	/* mV x 100 */
#define SEG4_GPU_DVFS_VOLT1	(100000)	/* mV x 100 */
#define SEG4_GPU_DVFS_VOLT2	(95000)	/* mV x 100 */
#define SEG4_GPU_DVFS_VOLT3	(93750)	/* mV x 100 */
#define SEG4_GPU_DVFS_VOLT4	(91875)	/* mV x 100 */
#define SEG4_GPU_DVFS_VOLT5	(90625)	/* mV x 100 */
#define SEG4_GPU_DVFS_VOLT6	(88750)	/* mV x 100 */
#define SEG4_GPU_DVFS_VOLT7	(87500)	/* mV x 100 */
#define SEG4_GPU_DVFS_VOLT8	(85625)	/* mV x 100 */
#define SEG4_GPU_DVFS_VOLT9	(85000)	/* mV x 100 */
#define SEG4_GPU_DVFS_VOLT10	(83750)	/* mV x 100 */
#define SEG4_GPU_DVFS_VOLT11	(82500)	/* mV x 100 */
#define SEG4_GPU_DVFS_VOLT12	(81250)	/* mV x 100 */
#define SEG4_GPU_DVFS_VOLT13	(80000)	/* mV x 100 */
#define SEG4_GPU_DVFS_VOLT14	(78750)	/* mV x 100 */
#define SEG4_GPU_DVFS_VOLT15	(77500)	/* mV x 100 */
#define SEG4_GPU_DVFS_VOLT16	(76250)	/* mV x 100 */
#define SEG4_GPU_DVFS_VOLT17	(75000)	/* mV x 100 */
#define SEG4_GPU_DVFS_VOLT18	(73750)	/* mV x 100 */
#define SEG4_GPU_DVFS_VOLT19	(72500)	/* mV x 100 */
#define SEG4_GPU_DVFS_VOLT20	(71250)	/* mV x 100 */
#define SEG4_GPU_DVFS_VOLT21	(70000)	/* mV x 100 */
#define SEG4_GPU_DVFS_VOLT22	(68750)	/* mV x 100 */
#define SEG4_GPU_DVFS_VOLT23	(67500)	/* mV x 100 */
#define SEG4_GPU_DVFS_VOLT24	(66875)	/* mV x 100 */
#define SEG4_GPU_DVFS_VOLT25	(66250)	/* mV x 100 */
#define SEG4_GPU_DVFS_VOLT26	(65625)	/* mV x 100 */
#define SEG4_GPU_DVFS_VOLT27	(65000)	/* mV x 100 */
#define SEG4_GPU_DVFS_VOLT28	(64375)	/* mV x 100 */
#define SEG4_GPU_DVFS_VOLT29	(63750)	/* mV x 100 */
#define SEG4_GPU_DVFS_VOLT30	(63125)	/* mV x 100 */
#define SEG4_GPU_DVFS_VOLT31	(62500)	/* mV x 100 */

#define SEG4_GPU_DVFS_VSRAM0	(105000)	/* mV x 100 */
#define SEG4_GPU_DVFS_VSRAM1	(100000)	/* mV x 100 */
#define SEG4_GPU_DVFS_VSRAM2	(95000)	/* mV x 100 */
#define SEG4_GPU_DVFS_VSRAM3	(93750)	/* mV x 100 */
#define SEG4_GPU_DVFS_VSRAM4	(91875)	/* mV x 100 */
#define SEG4_GPU_DVFS_VSRAM5	(90625)	/* mV x 100 */
#define SEG4_GPU_DVFS_VSRAM6	(88750)	/* mV x 100 */
#define SEG4_GPU_DVFS_VSRAM7	(87500)	/* mV x 100 */
/*************************************************
 * P90M segment_5(lite, 0.625) :
 *************************************************/
#define SEG5_GPU_DVFS_FREQ0	 (897000)/* KHz */
#define SEG5_GPU_DVFS_FREQ1	 (882000)/* KHz */
#define SEG5_GPU_DVFS_FREQ2	 (866000)/* KHz */
#define SEG5_GPU_DVFS_FREQ3	 (851000)/* KHz */
#define SEG5_GPU_DVFS_FREQ4	 (835000)/* KHz */
#define SEG5_GPU_DVFS_FREQ5	 (820000)/* KHz */
#define SEG5_GPU_DVFS_FREQ6	 (805000)/* KHz */
#define SEG5_GPU_DVFS_FREQ7	 (790000)/* KHz */
#define SEG5_GPU_DVFS_FREQ8	 (774000)/* KHz */
#define SEG5_GPU_DVFS_FREQ9	 (760000)/* KHz */
#define SEG5_GPU_DVFS_FREQ10	 (746000)/* KHz */
#define SEG5_GPU_DVFS_FREQ11	 (732000)/* KHz */
#define SEG5_GPU_DVFS_FREQ12	 (717000)/* KHz */
#define SEG5_GPU_DVFS_FREQ13	 (701000)/* KHz */
#define SEG5_GPU_DVFS_FREQ14	 (686000)/* KHz */
#define SEG5_GPU_DVFS_FREQ15	 (670000)/* KHz */
#define SEG5_GPU_DVFS_FREQ16	 (655000)/* KHz */
#define SEG5_GPU_DVFS_FREQ17	 (640000)/* KHz */
#define SEG5_GPU_DVFS_FREQ18	 (606000)/* KHz */
#define SEG5_GPU_DVFS_FREQ19	 (571000)/* KHz */
#define SEG5_GPU_DVFS_FREQ20	 (537000)/* KHz */
#define SEG5_GPU_DVFS_FREQ21	 (502000)/* KHz */
#define SEG5_GPU_DVFS_FREQ22	 (468000)/* KHz */
#define SEG5_GPU_DVFS_FREQ23	 (433000)/* KHz */
#define SEG5_GPU_DVFS_FREQ24	 (416000)/* KHz */
#define SEG5_GPU_DVFS_FREQ25	 (398000)/* KHz */
#define SEG5_GPU_DVFS_FREQ26	 (381000)/* KHz */
#define SEG5_GPU_DVFS_FREQ27	 (364000)/* KHz */
#define SEG5_GPU_DVFS_FREQ28	 (346000)/* KHz */
#define SEG5_GPU_DVFS_FREQ29	 (329000)/* KHz */
#define SEG5_GPU_DVFS_FREQ30	 (312000)/* KHz */
#define SEG5_GPU_DVFS_FREQ31	 (295000)/* KHz */

#define SEG5_GPU_DVFS_VOLT0	(90625)	/* mV x 100 */
#define SEG5_GPU_DVFS_VOLT1	(90000)	/* mV x 100 */
#define SEG5_GPU_DVFS_VOLT2	(88750)	/* mV x 100 */
#define SEG5_GPU_DVFS_VOLT3	(88125)	/* mV x 100 */
#define SEG5_GPU_DVFS_VOLT4	(86875)	/* mV x 100 */
#define SEG5_GPU_DVFS_VOLT5	(86250)	/* mV x 100 */
#define SEG5_GPU_DVFS_VOLT6	(85000)	/* mV x 100 */
#define SEG5_GPU_DVFS_VOLT7	(84375)	/* mV x 100 */
#define SEG5_GPU_DVFS_VOLT8	(83125)	/* mV x 100 */
#define SEG5_GPU_DVFS_VOLT9	(82500)	/* mV x 100 */
#define SEG5_GPU_DVFS_VOLT10	(81875)	/* mV x 100 */
#define SEG5_GPU_DVFS_VOLT11	(80625)	/* mV x 100 */
#define SEG5_GPU_DVFS_VOLT12	(80000)	/* mV x 100 */
#define SEG5_GPU_DVFS_VOLT13	(78750)	/* mV x 100 */
#define SEG5_GPU_DVFS_VOLT14	(78125)	/* mV x 100 */
#define SEG5_GPU_DVFS_VOLT15	(76875)	/* mV x 100 */
#define SEG5_GPU_DVFS_VOLT16	(76250)	/* mV x 100 */
#define SEG5_GPU_DVFS_VOLT17	(75000)	/* mV x 100 */
#define SEG5_GPU_DVFS_VOLT18	(73750)	/* mV x 100 */
#define SEG5_GPU_DVFS_VOLT19	(72500)	/* mV x 100 */
#define SEG5_GPU_DVFS_VOLT20	(71250)	/* mV x 100 */
#define SEG5_GPU_DVFS_VOLT21	(70000)	/* mV x 100 */
#define SEG5_GPU_DVFS_VOLT22	(68750)	/* mV x 100 */
#define SEG5_GPU_DVFS_VOLT23	(67500)	/* mV x 100 */
#define SEG5_GPU_DVFS_VOLT24	(66875)	/* mV x 100 */
#define SEG5_GPU_DVFS_VOLT25	(66250)	/* mV x 100 */
#define SEG5_GPU_DVFS_VOLT26	(65625)	/* mV x 100 */
#define SEG5_GPU_DVFS_VOLT27	(65000)	/* mV x 100 */
#define SEG5_GPU_DVFS_VOLT28	(64375)	/* mV x 100 */
#define SEG5_GPU_DVFS_VOLT29	(63750)	/* mV x 100 */
#define SEG5_GPU_DVFS_VOLT30	(63125)	/* mV x 100 */
#define SEG5_GPU_DVFS_VOLT31	(62500)	/* mV x 100 */

#define SEG5_GPU_DVFS_VSRAM0	(90625)	/* mV x 100 */
#define SEG5_GPU_DVFS_VSRAM1	(90000)	/* mV x 100 */
#define SEG5_GPU_DVFS_VSRAM2	(88750)	/* mV x 100 */
#define SEG5_GPU_DVFS_VSRAM3	(88125)	/* mV x 100 */
#define SEG5_GPU_DVFS_VSRAM4	(87500)	/* mV x 100 */
/**************************************************
 * PMIC Setting
 **************************************************/
#define VGPU_MAX_VOLT	(SEG2_GPU_DVFS_VOLT0)
#define VSRAM_MAX_VOLT	(SEG2_GPU_DVFS_VSRAM0)
#define VMDLA_MIN_VOLT	(55000) /* mV x 100 */
#define VAPU_MIN_VOLT	(55000) /* mV x 100 */
#define SLEW_RATE_UP	(1000) /* 1 us for 10 mV */
#define SLEW_RATE_DOWN	(500) /* 1 us for 5 mV */
#define VSRAM_RELAY_POINT	(87500)/* mV x 100 */
#define VSRAM_RELAY_MAX_POINT	(100000) /* mV x 100 */
#define PMIC_VCORE_ADDR	PMIC_RG_BUCK_VCORE_VOSEL
#define VCORE_BASE	40000
#define VCORE_STEP	625
#define VCORE_OPP_0	825000
#define VCORE_OPP_1	725000
#define VCORE_OPP_UNREQ	650000

/**************************************************
 * efuse Setting
 **************************************************/
#define GPUFREQ_EFUSE_INDEX	(8)
#define EFUSE_MFG_SPD_BOND_SHIFT	(8)
#define EFUSE_MFG_SPD_BOND_MASK	(0xF)
#define FUNC_CODE_EFUSE_INDEX	(22)

/**************************************************
 * Clock Setting
 **************************************************/
#define POST_DIV_2_MAX_FREQ	(1900000)
#define POST_DIV_2_MIN_FREQ	(750000)
#define POST_DIV_4_MAX_FREQ	(950000)
#define POST_DIV_4_MIN_FREQ	(375000)
#define POST_DIV_8_MAX_FREQ	(475000)
#define POST_DIV_8_MIN_FREQ	(187500)
#define POST_DIV_16_MAX_FREQ	(237500)
#define POST_DIV_16_MIN_FREQ	(93750)
#define POST_DIV_MASK	(0x07000000)
#define POST_DIV_SHIFT	(24)
#define TO_MHz_HEAD	(100)
#define TO_MHz_TAIL	(10)
#define ROUNDING_VALUE	(5)
#define DDS_SHIFT	(14)
#define GPUPLL_FIN	(26)
#define GPUPLL_CON0	(g_apmixed_base + 0x250)
#define GPUPLL_CON1	(g_apmixed_base + 0x254)


/**************************************************
 * Reference Power Setting
 **************************************************/
#define GPU_ACT_REF_POWER	(1285) /* mW  */
#define GPU_ACT_REF_FREQ	(900000) /* KHz */
#define GPU_ACT_REF_VOLT	(90000) /* mV x 100 */
#define GPU_DVFS_PTPOD_DISABLE_VOLT	(80000) /* mV x 100 */

/**************************************************
 * Log Setting
 **************************************************/
#define GPUFERQ_TAG "[GPU/FREQ]"
#define gpufreq_perr(fmt, args...)\
	pr_err(GPUFERQ_TAG"[ERROR]"fmt, ##args)
#define gpufreq_pwarn(fmt, args...)\
	pr_debug(GPUFERQ_TAG"[WARNING]"fmt, ##args)
#define gpufreq_pr_info(fmt, args...)\
	pr_info(GPUFERQ_TAG"[INFO]"fmt, ##args)
#define gpufreq_pr_debug(fmt, args...)\
	pr_debug(GPUFERQ_TAG"[DEBUG]"fmt, ##args)

#define	GPUFREQ_UNREFERENCED(param) ((void)(param))

/**************************************************
 * Condition Setting
 **************************************************/
#define MT_GPUFREQ_OPP_STRESS_TEST
#define MT_GPUFREQ_LOW_BATT_VOLT_PROTECT
// #define MT_GPUFREQ_BATT_PERCENT_PROTECT /* todo: disable it */
#define MT_GPUFREQ_BATT_OC_PROTECT
// #define MT_GPUFREQ_DYNAMIC_POWER_TABLE_UPDATE

/**************************************************
 * Battery Over Current Protect
 **************************************************/
#ifdef MT_GPUFREQ_BATT_OC_PROTECT
#define MT_GPUFREQ_BATT_OC_LIMIT_FREQ			(510000)/* KHz */
#endif

/**************************************************
 * Battery Percentage Protect
 **************************************************/
#ifdef MT_GPUFREQ_BATT_PERCENT_PROTECT
#define MT_GPUFREQ_BATT_PERCENT_LIMIT_FREQ		(510000)/* KHz */
#endif

/**************************************************
 * Low Battery Volume Protect
 **************************************************/
#ifdef MT_GPUFREQ_LOW_BATT_VOLT_PROTECT
#define MT_GPUFREQ_LOW_BATT_VOLT_LIMIT_FREQ		(510000)/* KHz */
#endif

/**************************************************
 * Proc Node Definition
 **************************************************/
#ifdef CONFIG_PROC_FS
#define PROC_FOPS_RW(name)	\
static int mt_ ## name ## _proc_open(struct inode *inode, struct file *file)\
{	\
	return single_open(file, mt_ ## name ## _proc_show, PDE_DATA(inode));\
}	\
	static const struct file_operations mt_ ## name ## _proc_fops = {\
		.owner = THIS_MODULE,	\
		.open = mt_ ## name ## _proc_open,	\
		.read = seq_read,	\
		.llseek = seq_lseek,	\
		.release = single_release,	\
		.write = mt_ ## name ## _proc_write,	\
	}
#define PROC_FOPS_RO(name)	\
static int mt_ ## name ## _proc_open(struct inode *inode, struct file *file)\
{	\
	return single_open(file, mt_ ## name ## _proc_show, PDE_DATA(inode));\
}	\
	static const struct file_operations mt_ ## name ## _proc_fops =	\
	{	\
		.owner = THIS_MODULE,	\
		.open = mt_ ## name ## _proc_open,	\
		.read = seq_read,	\
		.llseek = seq_lseek,	\
		.release = single_release,	\
	}
#define PROC_ENTRY(name) \
	{__stringify(name), &mt_ ## name ## _proc_fops}
#endif

/**************************************************
 * Operation Definition
 **************************************************/
#define VOLT_NORMALIZATION(volt)\
((volt % 625) ? (volt - (volt % 625) + 625) : volt)
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define GPUOP(khz, volt, vsram, idx)	\
	{	\
		.gpufreq_khz = khz,	\
		.gpufreq_volt = volt,	\
		.gpufreq_vsram = vsram,	\
		.gpufreq_idx = idx,	\
	}

#define vcore_pmic_to_uv(pmic)  \
(((pmic) * VCORE_STEP) + VCORE_BASE)

/**************************************************
 * Enumerations
 **************************************************/
enum g_segment_id_enum {
	MT6779_SEGMENT = 1,
};
enum g_post_divider_power_enum  {
	POST_DIV2 = 1,
	POST_DIV4,
	POST_DIV8,
	POST_DIV16,
};
enum g_clock_source_enum  {
	CLOCK_MAIN = 0,
	CLOCK_SUB,
};
enum g_limited_idx_enum {
	IDX_THERMAL_PROTECT_LIMITED = 0,
	IDX_LOW_BATT_LIMITED,
	IDX_BATT_PERCENT_LIMITED,
	IDX_BATT_OC_LIMITED,
	IDX_PBM_LIMITED,
	NUMBER_OF_LIMITED_IDX,
};

/**************************************************
 * Structures
 **************************************************/
struct g_opp_table_info {
	unsigned int gpufreq_khz;
	unsigned int gpufreq_volt;
	unsigned int gpufreq_vsram;
	unsigned int gpufreq_idx;
};
struct g_clk_info {
	struct clk *clk_mux;/* main clock for mfg setting*/
	struct clk *clk_main_parent;/* sub clock for mfg trans mux setting*/
	struct clk *clk_sub_parent; /* sub clock for mfg trans parent setting*/
	struct clk *mtcmos_mfg_async;
	struct clk *mtcmos_mfg;/* dependent on mtcmos_mfg_async */
	struct clk *mtcmos_mfg_core0;/* dependent on mtcmos_mfg */
	struct clk *mtcmos_mfg_core1;/* dependent on mtcmos_mfg */
};
struct g_pmic_info {
	struct regulator *reg_vgpu;
	struct regulator *reg_vsram;
	struct regulator *reg_vmdla;
	struct regulator *reg_vapu;
	struct regulator *reg_vcore;
	struct mtk_pm_qos_request pm_vcore;
};

/**************************************************
 * External functions declaration
 **************************************************/
extern bool mtk_get_gpu_loading(unsigned int *pLoading);
extern u32 get_devinfo_with_index(u32 index);
extern int mt_dfs_general_pll(unsigned int pll_id, unsigned int dds);

#endif /* _MT_GPUFREQ_CORE_H_ */
