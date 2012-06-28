/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#ifndef __ARCH_ARM_MACH_MSM_IOMMU_HW_V2_H
#define __ARCH_ARM_MACH_MSM_IOMMU_HW_V2_H

#define CTX_SHIFT  12
#define CTX_OFFSET 0x8000

#define GET_GLOBAL_REG(reg, base) (readl_relaxed((base) + (reg)))
#define GET_CTX_REG(reg, base, ctx) \
	(readl_relaxed((base) + CTX_OFFSET + (reg) + ((ctx) << CTX_SHIFT)))

#define SET_GLOBAL_REG(reg, base, val)	writel_relaxed((val), ((base) + (reg)))

#define SET_CTX_REG(reg, base, ctx, val) \
	writel_relaxed((val), \
		((base) + CTX_OFFSET + (reg) + ((ctx) << CTX_SHIFT)))

/* Wrappers for numbered registers */
#define SET_GLOBAL_REG_N(b, n, r, v) SET_GLOBAL_REG((b), ((r) + (n << 2)), (v))
#define GET_GLOBAL_REG_N(b, n, r)    GET_GLOBAL_REG((b), ((r) + (n << 2)))

/* Field wrappers */
#define GET_GLOBAL_FIELD(b, r, F) \
	GET_FIELD(((b) + (r)), r##_##F##_MASK, r##_##F##_SHIFT)
#define GET_CONTEXT_FIELD(b, c, r, F) \
	GET_FIELD(((b) + CTX_OFFSET + (r) + ((c) << CTX_SHIFT)), \
			r##_##F##_MASK, r##_##F##_SHIFT)

#define SET_GLOBAL_FIELD(b, r, F, v) \
	SET_FIELD(((b) + (r)), r##_##F##_MASK, r##_##F##_SHIFT, (v))
#define SET_CONTEXT_FIELD(b, c, r, F, v) \
	SET_FIELD(((b) + CTX_OFFSET + (r) + ((c) << CTX_SHIFT)), \
			r##_##F##_MASK, r##_##F##_SHIFT, (v))

/* Wrappers for numbered field registers */
#define SET_GLOBAL_FIELD_N(b, n, r, F, v) \
	SET_FIELD(((b) + ((n) << 2) + (r)), r##_##F##_MASK, r##_##F##_SHIFT, v)
#define GET_GLOBAL_FIELD_N(b, n, r, F) \
	GET_FIELD(((b) + ((n) << 2) + (r)), r##_##F##_MASK, r##_##F##_SHIFT)

#define GET_FIELD(addr, mask, shift) ((readl_relaxed(addr) >> (shift)) & (mask))

#define SET_FIELD(addr, mask, shift, v) \
do { \
	int t = readl_relaxed(addr); \
	writel_relaxed((t & ~((mask) << (shift))) + (((v) & \
			(mask)) << (shift)), addr); \
} while (0)


/* Global register space 0 setters / getters */
#define SET_CR0(b, v)            SET_GLOBAL_REG(CR0, (b), (v))
#define SET_SCR1(b, v)           SET_GLOBAL_REG(SCR1, (b), (v))
#define SET_CR2(b, v)            SET_GLOBAL_REG(CR2, (b), (v))
#define SET_ACR(b, v)            SET_GLOBAL_REG(ACR, (b), (v))
#define SET_IDR0(b, N, v)        SET_GLOBAL_REG(IDR0, (b), (v))
#define SET_IDR1(b, N, v)        SET_GLOBAL_REG(IDR1, (b), (v))
#define SET_IDR2(b, N, v)        SET_GLOBAL_REG(IDR2, (b), (v))
#define SET_IDR7(b, N, v)        SET_GLOBAL_REG(IDR7, (b), (v))
#define SET_GFAR(b, v)           SET_GLOBAL_REG(GFAR, (b), (v))
#define SET_GFSR(b, v)           SET_GLOBAL_REG(GFSR, (b), (v))
#define SET_GFSRRESTORE(b, v)    SET_GLOBAL_REG(GFSRRESTORE, (b), (v))
#define SET_GFSYNR0(b, v)        SET_GLOBAL_REG(GFSYNR0, (b), (v))
#define SET_GFSYNR1(b, v)        SET_GLOBAL_REG(GFSYNR1, (b), (v))
#define SET_GFSYNR2(b, v)        SET_GLOBAL_REG(GFSYNR2, (b), (v))
#define SET_TLBIVMID(b, v)       SET_GLOBAL_REG(TLBIVMID, (b), (v))
#define SET_TLBIALLNSNH(b, v)    SET_GLOBAL_REG(TLBIALLNSNH, (b), (v))
#define SET_TLBIALLH(b, v)       SET_GLOBAL_REG(TLBIALLH, (b), (v))
#define SET_TLBGSYNC(b, v)       SET_GLOBAL_REG(TLBGSYNC, (b), (v))
#define SET_TLBGSTATUS(b, v)     SET_GLOBAL_REG(TLBSTATUS, (b), (v))
#define SET_TLBIVAH(b, v)        SET_GLOBAL_REG(TLBIVAH, (b), (v))
#define SET_GATS1UR(b, v)        SET_GLOBAL_REG(GATS1UR, (b), (v))
#define SET_GATS1UW(b, v)        SET_GLOBAL_REG(GATS1UW, (b), (v))
#define SET_GATS1PR(b, v)        SET_GLOBAL_REG(GATS1PR, (b), (v))
#define SET_GATS1PW(b, v)        SET_GLOBAL_REG(GATS1PW, (b), (v))
#define SET_GATS12UR(b, v)       SET_GLOBAL_REG(GATS12UR, (b), (v))
#define SET_GATS12UW(b, v)       SET_GLOBAL_REG(GATS12UW, (b), (v))
#define SET_GATS12PR(b, v)       SET_GLOBAL_REG(GATS12PR, (b), (v))
#define SET_GATS12PW(b, v)       SET_GLOBAL_REG(GATS12PW, (b), (v))
#define SET_GPAR(b, v)           SET_GLOBAL_REG(GPAR, (b), (v))
#define SET_GATSR(b, v)          SET_GLOBAL_REG(GATSR, (b), (v))
#define SET_NSCR0(b, v)          SET_GLOBAL_REG(NSCR0, (b), (v))
#define SET_NSCR2(b, v)          SET_GLOBAL_REG(NSCR2, (b), (v))
#define SET_NSACR(b, v)          SET_GLOBAL_REG(NSACR, (b), (v))
#define SET_PMCR(b, v)           SET_GLOBAL_REG(PMCR, (b), (v))
#define SET_SMR_N(b, N, v)       SET_GLOBAL_REG_N(SMR, N, (b), (v))
#define SET_S2CR_N(b, N, v)      SET_GLOBAL_REG_N(S2CR, N, (b), (v))

#define GET_CR0(b)               GET_GLOBAL_REG(CR0, (b))
#define GET_SCR1(b)              GET_GLOBAL_REG(SCR1, (b))
#define GET_CR2(b)               GET_GLOBAL_REG(CR2, (b))
#define GET_ACR(b)               GET_GLOBAL_REG(ACR, (b))
#define GET_IDR0(b, N)           GET_GLOBAL_REG(IDR0, (b))
#define GET_IDR1(b, N)           GET_GLOBAL_REG(IDR1, (b))
#define GET_IDR2(b, N)           GET_GLOBAL_REG(IDR2, (b))
#define GET_IDR7(b, N)           GET_GLOBAL_REG(IDR7, (b))
#define GET_GFAR(b)              GET_GLOBAL_REG(GFAR, (b))
#define GET_GFSR(b)              GET_GLOBAL_REG(GFSR, (b))
#define GET_GFSRRESTORE(b)       GET_GLOBAL_REG(GFSRRESTORE, (b))
#define GET_GFSYNR0(b)           GET_GLOBAL_REG(GFSYNR0, (b))
#define GET_GFSYNR1(b)           GET_GLOBAL_REG(GFSYNR1, (b))
#define GET_GFSYNR2(b)           GET_GLOBAL_REG(GFSYNR2, (b))
#define GET_TLBIVMID(b)          GET_GLOBAL_REG(TLBIVMID, (b))
#define GET_TLBIALLNSNH(b)       GET_GLOBAL_REG(TLBIALLNSNH, (b))
#define GET_TLBIALLH(b)          GET_GLOBAL_REG(TLBIALLH, (b))
#define GET_TLBGSYNC(b)          GET_GLOBAL_REG(TLBGSYNC, (b))
#define GET_TLBGSTATUS(b)        GET_GLOBAL_REG(TLBSTATUS, (b))
#define GET_TLBIVAH(b)           GET_GLOBAL_REG(TLBIVAH, (b))
#define GET_GATS1UR(b)           GET_GLOBAL_REG(GATS1UR, (b))
#define GET_GATS1UW(b)           GET_GLOBAL_REG(GATS1UW, (b))
#define GET_GATS1PR(b)           GET_GLOBAL_REG(GATS1PR, (b))
#define GET_GATS1PW(b)           GET_GLOBAL_REG(GATS1PW, (b))
#define GET_GATS12UR(b)          GET_GLOBAL_REG(GATS12UR, (b))
#define GET_GATS12UW(b)          GET_GLOBAL_REG(GATS12UW, (b))
#define GET_GATS12PR(b)          GET_GLOBAL_REG(GATS12PR, (b))
#define GET_GATS12PW(b)          GET_GLOBAL_REG(GATS12PW, (b))
#define GET_GPAR(b)              GET_GLOBAL_REG(GPAR, (b))
#define GET_GATSR(b)             GET_GLOBAL_REG(GATSR, (b))
#define GET_NSCR0(b)             GET_GLOBAL_REG(NSCR0, (b))
#define GET_NSCR2(b)             GET_GLOBAL_REG(NSCR2, (b))
#define GET_NSACR(b)             GET_GLOBAL_REG(NSACR, (b))
#define GET_PMCR(b, v)           GET_GLOBAL_REG(PMCR, (b))
#define GET_SMR_N(b, N)          GET_GLOBAL_REG_N(SMR, N, (b))
#define GET_S2CR_N(b, N)         GET_GLOBAL_REG_N(S2CR, N, (b))

/* Global register space 1 setters / getters */
#define SET_CBAR_N(b, N, v)      SET_GLOBAL_REG_N(CBAR, N, (b), (v))
#define SET_CBFRSYNRA_N(b, N, v) SET_GLOBAL_REG_N(CBFRSYNRA, N, (b), (v))

#define GET_CBAR_N(b, N)         GET_GLOBAL_REG_N(CBAR, N, (b))
#define GET_CBFRSYNRA_N(b, N)    GET_GLOBAL_REG_N(CBFRSYNRA, N, (b))

/* Implementation defined register setters/getters */
#define SET_PREDICTIONDIS0(b, v) SET_GLOBAL_REG(PREDICTIONDIS0, (b), (v))
#define SET_PREDICTIONDIS1(b, v) SET_GLOBAL_REG(PREDICTIONDIS1, (b), (v))
#define SET_S1L1BFBLP0(b, v)     SET_GLOBAL_REG(S1L1BFBLP0, (b), (v))

/* SSD register setters/getters */
#define SET_SSDR_N(b, N, v)      SET_GLOBAL_REG_N(SSDR_N, N, (b), (v))

#define GET_SSDR_N(b, N)         GET_GLOBAL_REG_N(SSDR_N, N, (b))

/* Context bank register setters/getters */
#define SET_SCTLR(b, c, v)       SET_CTX_REG(CB_SCTLR, (b), (c), (v))
#define SET_ACTLR(b, c, v)       SET_CTX_REG(CB_ACTLR, (b), (c), (v))
#define SET_RESUME(b, c, v)      SET_CTX_REG(CB_RESUME, (b), (c), (v))
#define SET_TTBR0(b, c, v)       SET_CTX_REG(CB_TTBR0, (b), (c), (v))
#define SET_TTBR1(b, c, v)       SET_CTX_REG(CB_TTBR1, (b), (c), (v))
#define SET_TTBCR(b, c, v)       SET_CTX_REG(CB_TTBCR, (b), (c), (v))
#define SET_CONTEXTIDR(b, c, v)  SET_CTX_REG(CB_CONTEXTIDR, (b), (c), (v))
#define SET_PRRR(b, c, v)        SET_CTX_REG(CB_PRRR, (b), (c), (v))
#define SET_NMRR(b, c, v)        SET_CTX_REG(CB_NMRR, (b), (c), (v))
#define SET_PAR(b, c, v)         SET_CTX_REG(CB_PAR, (b), (c), (v))
#define SET_FSR(b, c, v)         SET_CTX_REG(CB_FSR, (b), (c), (v))
#define SET_FSRRESTORE(b, c, v)  SET_CTX_REG(CB_FSRRESTORE, (b), (c), (v))
#define SET_FAR(b, c, v)         SET_CTX_REG(CB_FAR, (b), (c), (v))
#define SET_FSYNR0(b, c, v)      SET_CTX_REG(CB_FSYNR0, (b), (c), (v))
#define SET_FSYNR1(b, c, v)      SET_CTX_REG(CB_FSYNR1, (b), (c), (v))
#define SET_TLBIVA(b, c, v)      SET_CTX_REG(CB_TLBIVA, (b), (c), (v))
#define SET_TLBIVAA(b, c, v)     SET_CTX_REG(CB_TLBIVAA, (b), (c), (v))
#define SET_TLBIASID(b, c, v)    SET_CTX_REG(CB_TLBIASID, (b), (c), (v))
#define SET_TLBIALL(b, c, v)     SET_CTX_REG(CB_TLBIALL, (b), (c), (v))
#define SET_TLBIVAL(b, c, v)     SET_CTX_REG(CB_TLBIVAL, (b), (c), (v))
#define SET_TLBIVAAL(b, c, v)    SET_CTX_REG(CB_TLBIVAAL, (b), (c), (v))
#define SET_TLBSYNC(b, c, v)     SET_CTX_REG(CB_TLBSYNC, (b), (c), (v))
#define SET_TLBSTATUS(b, c, v)   SET_CTX_REG(CB_TLBSTATUS, (b), (c), (v))
#define SET_ATS1PR(b, c, v)      SET_CTX_REG(CB_ATS1PR, (b), (c), (v))
#define SET_ATS1PW(b, c, v)      SET_CTX_REG(CB_ATS1PW, (b), (c), (v))
#define SET_ATS1UR(b, c, v)      SET_CTX_REG(CB_ATS1UR, (b), (c), (v))
#define SET_ATS1UW(b, c, v)      SET_CTX_REG(CB_ATS1UW, (b), (c), (v))
#define SET_ATSR(b, c, v)        SET_CTX_REG(CB_ATSR, (b), (c), (v))

#define GET_SCTLR(b, c)          GET_CTX_REG(CB_SCTLR, (b), (c))
#define GET_ACTLR(b, c)          GET_CTX_REG(CB_ACTLR, (b), (c))
#define GET_RESUME(b, c)         GET_CTX_REG(CB_RESUME, (b), (c))
#define GET_TTBR0(b, c)          GET_CTX_REG(CB_TTBR0, (b), (c))
#define GET_TTBR1(b, c)          GET_CTX_REG(CB_TTBR1, (b), (c))
#define GET_TTBCR(b, c)          GET_CTX_REG(CB_TTBCR, (b), (c))
#define GET_CONTEXTIDR(b, c)     GET_CTX_REG(CB_CONTEXTIDR, (b), (c))
#define GET_PRRR(b, c)           GET_CTX_REG(CB_PRRR, (b), (c))
#define GET_NMRR(b, c)           GET_CTX_REG(CB_NMRR, (b), (c))
#define GET_PAR(b, c)            GET_CTX_REG(CB_PAR, (b), (c))
#define GET_FSR(b, c)            GET_CTX_REG(CB_FSR, (b), (c))
#define GET_FSRRESTORE(b, c)     GET_CTX_REG(CB_FSRRESTORE, (b), (c))
#define GET_FAR(b, c)            GET_CTX_REG(CB_FAR, (b), (c))
#define GET_FSYNR0(b, c)         GET_CTX_REG(CB_FSYNR0, (b), (c))
#define GET_FSYNR1(b, c)         GET_CTX_REG(CB_FSYNR1, (b), (c))
#define GET_TLBIVA(b, c)         GET_CTX_REG(CB_TLBIVA, (b), (c))
#define GET_TLBIVAA(b, c)        GET_CTX_REG(CB_TLBIVAA, (b), (c))
#define GET_TLBIASID(b, c)       GET_CTX_REG(CB_TLBIASID, (b), (c))
#define GET_TLBIALL(b, c)        GET_CTX_REG(CB_TLBIALL, (b), (c))
#define GET_TLBIVAL(b, c)        GET_CTX_REG(CB_TLBIVAL, (b), (c))
#define GET_TLBIVAAL(b, c)       GET_CTX_REG(CB_TLBIVAAL, (b), (c))
#define GET_TLBSYNC(b, c)        GET_CTX_REG(CB_TLBSYNC, (b), (c))
#define GET_TLBSTATUS(b, c)      GET_CTX_REG(CB_TLBSTATUS, (b), (c))
#define GET_ATS1PR(b, c)         GET_CTX_REG(CB_ATS1PR, (b), (c))
#define GET_ATS1PW(b, c)         GET_CTX_REG(CB_ATS1PW, (b), (c))
#define GET_ATS1UR(b, c)         GET_CTX_REG(CB_ATS1UR, (b), (c))
#define GET_ATS1UW(b, c)         GET_CTX_REG(CB_ATS1UW, (b), (c))
#define GET_ATSR(b, c)           GET_CTX_REG(CB_ATSR, (b), (c))

/* Global Register field setters / getters */
/* Configuration Register: CR0 */
#define SET_CR0_NSCFG(b, v)        SET_GLOBAL_FIELD(b, CR0, NSCFG, v)
#define SET_CR0_WACFG(b, v)        SET_GLOBAL_FIELD(b, CR0, WACFG, v)
#define SET_CR0_RACFG(b, v)        SET_GLOBAL_FIELD(b, CR0, RACFG, v)
#define SET_CR0_SHCFG(b, v)        SET_GLOBAL_FIELD(b, CR0, SHCFG, v)
#define SET_CR0_SMCFCFG(b, v)      SET_GLOBAL_FIELD(b, CR0, SMCFCFG, v)
#define SET_CR0_MTCFG(b, v)        SET_GLOBAL_FIELD(b, CR0, MTCFG, v)
#define SET_CR0_BSU(b, v)          SET_GLOBAL_FIELD(b, CR0, BSU, v)
#define SET_CR0_FB(b, v)           SET_GLOBAL_FIELD(b, CR0, FB, v)
#define SET_CR0_PTM(b, v)          SET_GLOBAL_FIELD(b, CR0, PTM, v)
#define SET_CR0_VMIDPNE(b, v)      SET_GLOBAL_FIELD(b, CR0, VMIDPNE, v)
#define SET_CR0_USFCFG(b, v)       SET_GLOBAL_FIELD(b, CR0, USFCFG, v)
#define SET_CR0_GSE(b, v)          SET_GLOBAL_FIELD(b, CR0, GSE, v)
#define SET_CR0_STALLD(b, v)       SET_GLOBAL_FIELD(b, CR0, STALLD, v)
#define SET_CR0_TRANSIENTCFG(b, v) SET_GLOBAL_FIELD(b, CR0, TRANSIENTCFG, v)
#define SET_CR0_GCFGFIE(b, v)      SET_GLOBAL_FIELD(b, CR0, GCFGFIE, v)
#define SET_CR0_GCFGFRE(b, v)      SET_GLOBAL_FIELD(b, CR0, GCFGFRE, v)
#define SET_CR0_GFIE(b, v)         SET_GLOBAL_FIELD(b, CR0, GFIE, v)
#define SET_CR0_GFRE(b, v)         SET_GLOBAL_FIELD(b, CR0, GFRE, v)
#define SET_CR0_CLIENTPD(b, v)     SET_GLOBAL_FIELD(b, CR0, CLIENTPD, v)

#define GET_CR0_NSCFG(b)           GET_GLOBAL_FIELD(b, CR0, NSCFG)
#define GET_CR0_WACFG(b)           GET_GLOBAL_FIELD(b, CR0, WACFG)
#define GET_CR0_RACFG(b)           GET_GLOBAL_FIELD(b, CR0, RACFG)
#define GET_CR0_SHCFG(b)           GET_GLOBAL_FIELD(b, CR0, SHCFG)
#define GET_CR0_SMCFCFG(b)         GET_GLOBAL_FIELD(b, CR0, SMCFCFG)
#define GET_CR0_MTCFG(b)           GET_GLOBAL_FIELD(b, CR0, MTCFG)
#define GET_CR0_BSU(b)             GET_GLOBAL_FIELD(b, CR0, BSU)
#define GET_CR0_FB(b)              GET_GLOBAL_FIELD(b, CR0, FB)
#define GET_CR0_PTM(b)             GET_GLOBAL_FIELD(b, CR0, PTM)
#define GET_CR0_VMIDPNE(b)         GET_GLOBAL_FIELD(b, CR0, VMIDPNE)
#define GET_CR0_USFCFG(b)          GET_GLOBAL_FIELD(b, CR0, USFCFG)
#define GET_CR0_GSE(b)             GET_GLOBAL_FIELD(b, CR0, GSE)
#define GET_CR0_STALLD(b)          GET_GLOBAL_FIELD(b, CR0, STALLD)
#define GET_CR0_TRANSIENTCFG(b)    GET_GLOBAL_FIELD(b, CR0, TRANSIENTCFG)
#define GET_CR0_GCFGFIE(b)         GET_GLOBAL_FIELD(b, CR0, GCFGFIE)
#define GET_CR0_GCFGFRE(b)         GET_GLOBAL_FIELD(b, CR0, GCFGFRE)
#define GET_CR0_GFIE(b)            GET_GLOBAL_FIELD(b, CR0, GFIE)
#define GET_CR0_GFRE(b)            GET_GLOBAL_FIELD(b, CR0, GFRE)
#define GET_CR0_CLIENTPD(b)        GET_GLOBAL_FIELD(b, CR0, CLIENTPD)

/* Configuration Register: CR2 */
#define SET_CR2_BPVMID(b, v)     SET_GLOBAL_FIELD(b, CR2, BPVMID, v)

#define GET_CR2_BPVMID(b)        GET_GLOBAL_FIELD(b, CR2, BPVMID)

/* Global Address Translation, Stage 1, Privileged Read: GATS1PR */
#define SET_GATS1PR_ADDR(b, v)   SET_GLOBAL_FIELD(b, GATS1PR, ADDR, v)
#define SET_GATS1PR_NDX(b, v)    SET_GLOBAL_FIELD(b, GATS1PR, NDX, v)

#define GET_GATS1PR_ADDR(b)      GET_GLOBAL_FIELD(b, GATS1PR, ADDR)
#define GET_GATS1PR_NDX(b)       GET_GLOBAL_FIELD(b, GATS1PR, NDX)

/* Global Address Translation, Stage 1, Privileged Write: GATS1PW */
#define SET_GATS1PW_ADDR(b, v)   SET_GLOBAL_FIELD(b, GATS1PW, ADDR, v)
#define SET_GATS1PW_NDX(b, v)    SET_GLOBAL_FIELD(b, GATS1PW, NDX, v)

#define GET_GATS1PW_ADDR(b)      GET_GLOBAL_FIELD(b, GATS1PW, ADDR)
#define GET_GATS1PW_NDX(b)       GET_GLOBAL_FIELD(b, GATS1PW, NDX)

/* Global Address Translation, Stage 1, User Read: GATS1UR */
#define SET_GATS1UR_ADDR(b, v)   SET_GLOBAL_FIELD(b, GATS1UR, ADDR, v)
#define SET_GATS1UR_NDX(b, v)    SET_GLOBAL_FIELD(b, GATS1UR, NDX, v)

#define GET_GATS1UR_ADDR(b)      GET_GLOBAL_FIELD(b, GATS1UR, ADDR)
#define GET_GATS1UR_NDX(b)       GET_GLOBAL_FIELD(b, GATS1UR, NDX)

/* Global Address Translation, Stage 1, User Read: GATS1UW */
#define SET_GATS1UW_ADDR(b, v)   SET_GLOBAL_FIELD(b, GATS1UW, ADDR, v)
#define SET_GATS1UW_NDX(b, v)    SET_GLOBAL_FIELD(b, GATS1UW, NDX, v)

#define GET_GATS1UW_ADDR(b)      GET_GLOBAL_FIELD(b, GATS1UW, ADDR)
#define GET_GATS1UW_NDX(b)       GET_GLOBAL_FIELD(b, GATS1UW, NDX)

/* Global Address Translation, Stage 1 and 2, Privileged Read: GATS12PR */
#define SET_GATS12PR_ADDR(b, v)  SET_GLOBAL_FIELD(b, GATS12PR, ADDR, v)
#define SET_GATS12PR_NDX(b, v)   SET_GLOBAL_FIELD(b, GATS12PR, NDX, v)

#define GET_GATS12PR_ADDR(b)     GET_GLOBAL_FIELD(b, GATS12PR, ADDR)
#define GET_GATS12PR_NDX(b)      GET_GLOBAL_FIELD(b, GATS12PR, NDX)

/* Global Address Translation, Stage 1, Privileged Write: GATS1PW */
#define SET_GATS12PW_ADDR(b, v)  SET_GLOBAL_FIELD(b, GATS12PW, ADDR, v)
#define SET_GATS12PW_NDX(b, v)   SET_GLOBAL_FIELD(b, GATS12PW, NDX, v)

#define GET_GATS12PW_ADDR(b)     GET_GLOBAL_FIELD(b, GATS12PW, ADDR)
#define GET_GATS12PW_NDX(b)      GET_GLOBAL_FIELD(b, GATS12PW, NDX)

/* Global Address Translation, Stage 1, User Read: GATS1UR */
#define SET_GATS12UR_ADDR(b, v)  SET_GLOBAL_FIELD(b, GATS12UR, ADDR, v)
#define SET_GATS12UR_NDX(b, v)   SET_GLOBAL_FIELD(b, GATS12UR, NDX, v)

#define GET_GATS12UR_ADDR(b)     GET_GLOBAL_FIELD(b, GATS12UR, ADDR)
#define GET_GATS12UR_NDX(b)      GET_GLOBAL_FIELD(b, GATS12UR, NDX)

/* Global Address Translation, Stage 1, User Read: GATS1UW */
#define SET_GATS12UW_ADDR(b, v)  SET_GLOBAL_FIELD(b, GATS12UW, ADDR, v)
#define SET_GATS12UW_NDX(b, v)   SET_GLOBAL_FIELD(b, GATS12UW, NDX, v)

#define GET_GATS12UW_ADDR(b)     GET_GLOBAL_FIELD(b, GATS12UW, ADDR)
#define GET_GATS12UW_NDX(b)      GET_GLOBAL_FIELD(b, GATS12UW, NDX)

/* Global Address Translation Status Register: GATSR */
#define SET_GATSR_ACTIVE(b, v)   SET_GLOBAL_FIELD(b, GATSR, ACTIVE, v)

#define GET_GATSR_ACTIVE(b)      GET_GLOBAL_FIELD(b, GATSR, ACTIVE)

/* Global Fault Address Register: GFAR */
#define SET_GFAR_FADDR(b, v)     SET_GLOBAL_FIELD(b, GFAR, FADDR, v)

#define GET_GFAR_FADDR(b)        GET_GLOBAL_FIELD(b, GFAR, FADDR)

/* Global Fault Status Register: GFSR */
#define SET_GFSR_ICF(b, v)        SET_GLOBAL_FIELD(b, GFSR, ICF, v)
#define SET_GFSR_USF(b, v)        SET_GLOBAL_FIELD(b, GFSR, USF, v)
#define SET_GFSR_SMCF(b, v)       SET_GLOBAL_FIELD(b, GFSR, SMCF, v)
#define SET_GFSR_UCBF(b, v)       SET_GLOBAL_FIELD(b, GFSR, UCBF, v)
#define SET_GFSR_UCIF(b, v)       SET_GLOBAL_FIELD(b, GFSR, UCIF, v)
#define SET_GFSR_CAF(b, v)        SET_GLOBAL_FIELD(b, GFSR, CAF, v)
#define SET_GFSR_EF(b, v)         SET_GLOBAL_FIELD(b, GFSR, EF, v)
#define SET_GFSR_PF(b, v)         SET_GLOBAL_FIELD(b, GFSR, PF, v)
#define SET_GFSR_MULTI(b, v)      SET_GLOBAL_FIELD(b, GFSR, MULTI, v)

#define GET_GFSR_ICF(b)           GET_GLOBAL_FIELD(b, GFSR, ICF)
#define GET_GFSR_USF(b)           GET_GLOBAL_FIELD(b, GFSR, USF)
#define GET_GFSR_SMCF(b)          GET_GLOBAL_FIELD(b, GFSR, SMCF)
#define GET_GFSR_UCBF(b)          GET_GLOBAL_FIELD(b, GFSR, UCBF)
#define GET_GFSR_UCIF(b)          GET_GLOBAL_FIELD(b, GFSR, UCIF)
#define GET_GFSR_CAF(b)           GET_GLOBAL_FIELD(b, GFSR, CAF)
#define GET_GFSR_EF(b)            GET_GLOBAL_FIELD(b, GFSR, EF)
#define GET_GFSR_PF(b)            GET_GLOBAL_FIELD(b, GFSR, PF)
#define GET_GFSR_MULTI(b)         GET_GLOBAL_FIELD(b, GFSR, MULTI)

/* Global Fault Syndrome Register 0: GFSYNR0 */
#define SET_GFSYNR0_NESTED(b, v)  SET_GLOBAL_FIELD(b, GFSYNR0, NESTED, v)
#define SET_GFSYNR0_WNR(b, v)     SET_GLOBAL_FIELD(b, GFSYNR0, WNR, v)
#define SET_GFSYNR0_PNU(b, v)     SET_GLOBAL_FIELD(b, GFSYNR0, PNU, v)
#define SET_GFSYNR0_IND(b, v)     SET_GLOBAL_FIELD(b, GFSYNR0, IND, v)
#define SET_GFSYNR0_NSSTATE(b, v) SET_GLOBAL_FIELD(b, GFSYNR0, NSSTATE, v)
#define SET_GFSYNR0_NSATTR(b, v)  SET_GLOBAL_FIELD(b, GFSYNR0, NSATTR, v)

#define GET_GFSYNR0_NESTED(b)     GET_GLOBAL_FIELD(b, GFSYNR0, NESTED)
#define GET_GFSYNR0_WNR(b)        GET_GLOBAL_FIELD(b, GFSYNR0, WNR)
#define GET_GFSYNR0_PNU(b)        GET_GLOBAL_FIELD(b, GFSYNR0, PNU)
#define GET_GFSYNR0_IND(b)        GET_GLOBAL_FIELD(b, GFSYNR0, IND)
#define GET_GFSYNR0_NSSTATE(b)    GET_GLOBAL_FIELD(b, GFSYNR0, NSSTATE)
#define GET_GFSYNR0_NSATTR(b)     GET_GLOBAL_FIELD(b, GFSYNR0, NSATTR)

/* Global Fault Syndrome Register 1: GFSYNR1 */
#define SET_GFSYNR1_SID(b, v)     SET_GLOBAL_FIELD(b, GFSYNR1, SID, v)

#define GET_GFSYNR1_SID(b)        GET_GLOBAL_FIELD(b, GFSYNR1, SID)

/* Global Physical Address Register: GPAR */
#define SET_GPAR_F(b, v)          SET_GLOBAL_FIELD(b, GPAR, F, v)
#define SET_GPAR_SS(b, v)         SET_GLOBAL_FIELD(b, GPAR, SS, v)
#define SET_GPAR_OUTER(b, v)      SET_GLOBAL_FIELD(b, GPAR, OUTER, v)
#define SET_GPAR_INNER(b, v)      SET_GLOBAL_FIELD(b, GPAR, INNER, v)
#define SET_GPAR_SH(b, v)         SET_GLOBAL_FIELD(b, GPAR, SH, v)
#define SET_GPAR_NS(b, v)         SET_GLOBAL_FIELD(b, GPAR, NS, v)
#define SET_GPAR_NOS(b, v)        SET_GLOBAL_FIELD(b, GPAR, NOS, v)
#define SET_GPAR_PA(b, v)         SET_GLOBAL_FIELD(b, GPAR, PA, v)
#define SET_GPAR_TF(b, v)         SET_GLOBAL_FIELD(b, GPAR, TF, v)
#define SET_GPAR_AFF(b, v)        SET_GLOBAL_FIELD(b, GPAR, AFF, v)
#define SET_GPAR_PF(b, v)         SET_GLOBAL_FIELD(b, GPAR, PF, v)
#define SET_GPAR_EF(b, v)         SET_GLOBAL_FIELD(b, GPAR, EF, v)
#define SET_GPAR_TLCMCF(b, v)     SET_GLOBAL_FIELD(b, GPAR, TLCMCF, v)
#define SET_GPAR_TLBLKF(b, v)     SET_GLOBAL_FIELD(b, GPAR, TLBLKF, v)
#define SET_GPAR_UCBF(b, v)       SET_GLOBAL_FIELD(b, GPAR, UCBF, v)

#define GET_GPAR_F(b)             GET_GLOBAL_FIELD(b, GPAR, F)
#define GET_GPAR_SS(b)            GET_GLOBAL_FIELD(b, GPAR, SS)
#define GET_GPAR_OUTER(b)         GET_GLOBAL_FIELD(b, GPAR, OUTER)
#define GET_GPAR_INNER(b)         GET_GLOBAL_FIELD(b, GPAR, INNER)
#define GET_GPAR_SH(b)            GET_GLOBAL_FIELD(b, GPAR, SH)
#define GET_GPAR_NS(b)            GET_GLOBAL_FIELD(b, GPAR, NS)
#define GET_GPAR_NOS(b)           GET_GLOBAL_FIELD(b, GPAR, NOS)
#define GET_GPAR_PA(b)            GET_GLOBAL_FIELD(b, GPAR, PA)
#define GET_GPAR_TF(b)            GET_GLOBAL_FIELD(b, GPAR, TF)
#define GET_GPAR_AFF(b)           GET_GLOBAL_FIELD(b, GPAR, AFF)
#define GET_GPAR_PF(b)            GET_GLOBAL_FIELD(b, GPAR, PF)
#define GET_GPAR_EF(b)            GET_GLOBAL_FIELD(b, GPAR, EF)
#define GET_GPAR_TLCMCF(b)        GET_GLOBAL_FIELD(b, GPAR, TLCMCF)
#define GET_GPAR_TLBLKF(b)        GET_GLOBAL_FIELD(b, GPAR, TLBLKF)
#define GET_GPAR_UCBF(b)          GET_GLOBAL_FIELD(b, GPAR, UCBF)

/* Identification Register: IDR0 */
#define SET_IDR0_NUMSMRG(b, v)    SET_GLOBAL_FIELD(b, IDR0, NUMSMRG, v)
#define SET_IDR0_NUMSIDB(b, v)    SET_GLOBAL_FIELD(b, IDR0, NUMSIDB, v)
#define SET_IDR0_BTM(b, v)        SET_GLOBAL_FIELD(b, IDR0, BTM, v)
#define SET_IDR0_CTTW(b, v)       SET_GLOBAL_FIELD(b, IDR0, CTTW, v)
#define SET_IDR0_NUMIRPT(b, v)    SET_GLOBAL_FIELD(b, IDR0, NUMIRPT, v)
#define SET_IDR0_PTFS(b, v)       SET_GLOBAL_FIELD(b, IDR0, PTFS, v)
#define SET_IDR0_SMS(b, v)        SET_GLOBAL_FIELD(b, IDR0, SMS, v)
#define SET_IDR0_NTS(b, v)        SET_GLOBAL_FIELD(b, IDR0, NTS, v)
#define SET_IDR0_S2TS(b, v)       SET_GLOBAL_FIELD(b, IDR0, S2TS, v)
#define SET_IDR0_S1TS(b, v)       SET_GLOBAL_FIELD(b, IDR0, S1TS, v)
#define SET_IDR0_SES(b, v)        SET_GLOBAL_FIELD(b, IDR0, SES, v)

#define GET_IDR0_NUMSMRG(b)       GET_GLOBAL_FIELD(b, IDR0, NUMSMRG)
#define GET_IDR0_NUMSIDB(b)       GET_GLOBAL_FIELD(b, IDR0, NUMSIDB)
#define GET_IDR0_BTM(b)           GET_GLOBAL_FIELD(b, IDR0, BTM)
#define GET_IDR0_CTTW(b)          GET_GLOBAL_FIELD(b, IDR0, CTTW)
#define GET_IDR0_NUMIRPT(b)       GET_GLOBAL_FIELD(b, IDR0, NUMIRPT)
#define GET_IDR0_PTFS(b)          GET_GLOBAL_FIELD(b, IDR0, PTFS)
#define GET_IDR0_SMS(b)           GET_GLOBAL_FIELD(b, IDR0, SMS)
#define GET_IDR0_NTS(b)           GET_GLOBAL_FIELD(b, IDR0, NTS)
#define GET_IDR0_S2TS(b)          GET_GLOBAL_FIELD(b, IDR0, S2TS)
#define GET_IDR0_S1TS(b)          GET_GLOBAL_FIELD(b, IDR0, S1TS)
#define GET_IDR0_SES(b)           GET_GLOBAL_FIELD(b, IDR0, SES)

/* Identification Register: IDR1 */
#define SET_IDR1_NUMCB(b, v)       SET_GLOBAL_FIELD(b, IDR1, NUMCB, v)
#define SET_IDR1_NUMSSDNDXB(b, v)  SET_GLOBAL_FIELD(b, IDR1, NUMSSDNDXB, v)
#define SET_IDR1_SSDTP(b, v)       SET_GLOBAL_FIELD(b, IDR1, SSDTP, v)
#define SET_IDR1_SMCD(b, v)        SET_GLOBAL_FIELD(b, IDR1, SMCD, v)
#define SET_IDR1_NUMS2CB(b, v)     SET_GLOBAL_FIELD(b, IDR1, NUMS2CB, v)
#define SET_IDR1_NUMPAGENDXB(b, v) SET_GLOBAL_FIELD(b, IDR1, NUMPAGENDXB, v)
#define SET_IDR1_PAGESIZE(b, v)    SET_GLOBAL_FIELD(b, IDR1, PAGESIZE, v)

#define GET_IDR1_NUMCB(b)          GET_GLOBAL_FIELD(b, IDR1, NUMCB)
#define GET_IDR1_NUMSSDNDXB(b)     GET_GLOBAL_FIELD(b, IDR1, NUMSSDNDXB)
#define GET_IDR1_SSDTP(b)          GET_GLOBAL_FIELD(b, IDR1, SSDTP)
#define GET_IDR1_SMCD(b)           GET_GLOBAL_FIELD(b, IDR1, SMCD)
#define GET_IDR1_NUMS2CB(b)        GET_GLOBAL_FIELD(b, IDR1, NUMS2CB)
#define GET_IDR1_NUMPAGENDXB(b)    GET_GLOBAL_FIELD(b, IDR1, NUMPAGENDXB)
#define GET_IDR1_PAGESIZE(b)       GET_GLOBAL_FIELD(b, IDR1, PAGESIZE)

/* Identification Register: IDR2 */
#define SET_IDR2_IAS(b, v)       SET_GLOBAL_FIELD(b, IDR2, IAS, v)
#define SET_IDR2_OAS(b, v)       SET_GLOBAL_FIELD(b, IDR2, OAS, v)

#define GET_IDR2_IAS(b)          GET_GLOBAL_FIELD(b, IDR2, IAS)
#define GET_IDR2_OAS(b)          GET_GLOBAL_FIELD(b, IDR2, OAS)

/* Identification Register: IDR7 */
#define SET_IDR7_MINOR(b, v)     SET_GLOBAL_FIELD(b, IDR7, MINOR, v)
#define SET_IDR7_MAJOR(b, v)     SET_GLOBAL_FIELD(b, IDR7, MAJOR, v)

#define GET_IDR7_MINOR(b)        GET_GLOBAL_FIELD(b, IDR7, MINOR)
#define GET_IDR7_MAJOR(b)        GET_GLOBAL_FIELD(b, IDR7, MAJOR)

/* Stream to Context Register: S2CR_N */
#define SET_S2CR_CBNDX(b, n, v)   SET_GLOBAL_FIELD_N(b, n, S2CR, CBNDX, v)
#define SET_S2CR_SHCFG(b, n, v)   SET_GLOBAL_FIELD_N(b, n, S2CR, SHCFG, v)
#define SET_S2CR_MTCFG(b, n, v)   SET_GLOBAL_FIELD_N(b, n, S2CR, MTCFG, v)
#define SET_S2CR_MEMATTR(b, n, v) SET_GLOBAL_FIELD_N(b, n, S2CR, MEMATTR, v)
#define SET_S2CR_TYPE(b, n, v)    SET_GLOBAL_FIELD_N(b, n, S2CR, TYPE, v)
#define SET_S2CR_NSCFG(b, n, v)   SET_GLOBAL_FIELD_N(b, n, S2CR, NSCFG, v)
#define SET_S2CR_RACFG(b, n, v)   SET_GLOBAL_FIELD_N(b, n, S2CR, RACFG, v)
#define SET_S2CR_WACFG(b, n, v)   SET_GLOBAL_FIELD_N(b, n, S2CR, WACFG, v)
#define SET_S2CR_PRIVCFG(b, n, v) SET_GLOBAL_FIELD_N(b, n, S2CR, PRIVCFG, v)
#define SET_S2CR_INSTCFG(b, n, v) SET_GLOBAL_FIELD_N(b, n, S2CR, INSTCFG, v)
#define SET_S2CR_TRANSIENTCFG(b, n, v) \
				SET_GLOBAL_FIELD_N(b, n, S2CR, TRANSIENTCFG, v)
#define SET_S2CR_VMID(b, n, v)    SET_GLOBAL_FIELD_N(b, n, S2CR, VMID, v)
#define SET_S2CR_BSU(b, n, v)     SET_GLOBAL_FIELD_N(b, n, S2CR, BSU, v)
#define SET_S2CR_FB(b, n, v)      SET_GLOBAL_FIELD_N(b, n, S2CR, FB, v)

#define GET_S2CR_CBNDX(b, n)      GET_GLOBAL_FIELD_N(b, n, S2CR, CBNDX)
#define GET_S2CR_SHCFG(b, n)      GET_GLOBAL_FIELD_N(b, n, S2CR, SHCFG)
#define GET_S2CR_MTCFG(b, n)      GET_GLOBAL_FIELD_N(b, n, S2CR, MTCFG)
#define GET_S2CR_MEMATTR(b, n)    GET_GLOBAL_FIELD_N(b, n, S2CR, MEMATTR)
#define GET_S2CR_TYPE(b, n)       GET_GLOBAL_FIELD_N(b, n, S2CR, TYPE)
#define GET_S2CR_NSCFG(b, n)      GET_GLOBAL_FIELD_N(b, n, S2CR, NSCFG)
#define GET_S2CR_RACFG(b, n)      GET_GLOBAL_FIELD_N(b, n, S2CR, RACFG)
#define GET_S2CR_WACFG(b, n)      GET_GLOBAL_FIELD_N(b, n, S2CR, WACFG)
#define GET_S2CR_PRIVCFG(b, n)    GET_GLOBAL_FIELD_N(b, n, S2CR, PRIVCFG)
#define GET_S2CR_INSTCFG(b, n)    GET_GLOBAL_FIELD_N(b, n, S2CR, INSTCFG)
#define GET_S2CR_TRANSIENTCFG(b, n) \
				GET_GLOBAL_FIELD_N(b, n, S2CR, TRANSIENTCFG)
#define GET_S2CR_VMID(b, n)       GET_GLOBAL_FIELD_N(b, n, S2CR, VMID)
#define GET_S2CR_BSU(b, n)        GET_GLOBAL_FIELD_N(b, n, S2CR, BSU)
#define GET_S2CR_FB(b, n)         GET_GLOBAL_FIELD_N(b, n, S2CR, FB)

/* Stream Match Register: SMR_N */
#define SET_SMR_ID(b, n, v)       SET_GLOBAL_FIELD_N(b, n, SMR, ID, v)
#define SET_SMR_MASK(b, n, v)     SET_GLOBAL_FIELD_N(b, n, SMR, MASK, v)
#define SET_SMR_VALID(b, n, v)    SET_GLOBAL_FIELD_N(b, n, SMR, VALID, v)

#define GET_SMR_ID(b, n)          GET_GLOBAL_FIELD_N(b, n, SMR, ID)
#define GET_SMR_MASK(b, n)        GET_GLOBAL_FIELD_N(b, n, SMR, MASK)
#define GET_SMR_VALID(b, n)       GET_GLOBAL_FIELD_N(b, n, SMR, VALID)

/* Global TLB Status: TLBGSTATUS */
#define SET_TLBGSTATUS_GSACTIVE(b, v) \
				SET_GLOBAL_FIELD(b, TLBGSTATUS, GSACTIVE, v)

#define GET_TLBGSTATUS_GSACTIVE(b)    \
				GET_GLOBAL_FIELD(b, TLBGSTATUS, GSACTIVE)

/* Invalidate Hyp TLB by VA: TLBIVAH */
#define SET_TLBIVAH_ADDR(b, v)  SET_GLOBAL_FIELD(b, TLBIVAH, ADDR, v)

#define GET_TLBIVAH_ADDR(b)     GET_GLOBAL_FIELD(b, TLBIVAH, ADDR)

/* Invalidate TLB by VMID: TLBIVMID */
#define SET_TLBIVMID_VMID(b, v) SET_GLOBAL_FIELD(b, TLBIVMID, VMID, v)

#define GET_TLBIVMID_VMID(b)    GET_GLOBAL_FIELD(b, TLBIVMID, VMID)

/* Global Register Space 1 Field setters/getters*/
/* Context Bank Attribute Register: CBAR_N */
#define SET_CBAR_VMID(b, n, v)     SET_GLOBAL_FIELD_N(b, n, CBAR, VMID, v)
#define SET_CBAR_CBNDX(b, n, v)    SET_GLOBAL_FIELD_N(b, n, CBAR, CBNDX, v)
#define SET_CBAR_BPSHCFG(b, n, v)  SET_GLOBAL_FIELD_N(b, n, CBAR, BPSHCFG, v)
#define SET_CBAR_HYPC(b, n, v)     SET_GLOBAL_FIELD_N(b, n, CBAR, HYPC, v)
#define SET_CBAR_FB(b, n, v)       SET_GLOBAL_FIELD_N(b, n, CBAR, FB, v)
#define SET_CBAR_MEMATTR(b, n, v)  SET_GLOBAL_FIELD_N(b, n, CBAR, MEMATTR, v)
#define SET_CBAR_TYPE(b, n, v)     SET_GLOBAL_FIELD_N(b, n, CBAR, TYPE, v)
#define SET_CBAR_BSU(b, n, v)      SET_GLOBAL_FIELD_N(b, n, CBAR, BSU, v)
#define SET_CBAR_RACFG(b, n, v)    SET_GLOBAL_FIELD_N(b, n, CBAR, RACFG, v)
#define SET_CBAR_WACFG(b, n, v)    SET_GLOBAL_FIELD_N(b, n, CBAR, WACFG, v)
#define SET_CBAR_IRPTNDX(b, n, v)  SET_GLOBAL_FIELD_N(b, n, CBAR, IRPTNDX, v)

#define GET_CBAR_VMID(b, n)        GET_GLOBAL_FIELD_N(b, n, CBAR, VMID)
#define GET_CBAR_CBNDX(b, n)       GET_GLOBAL_FIELD_N(b, n, CBAR, CBNDX)
#define GET_CBAR_BPSHCFG(b, n)     GET_GLOBAL_FIELD_N(b, n, CBAR, BPSHCFG)
#define GET_CBAR_HYPC(b, n)        GET_GLOBAL_FIELD_N(b, n, CBAR, HYPC)
#define GET_CBAR_FB(b, n)          GET_GLOBAL_FIELD_N(b, n, CBAR, FB)
#define GET_CBAR_MEMATTR(b, n)     GET_GLOBAL_FIELD_N(b, n, CBAR, MEMATTR)
#define GET_CBAR_TYPE(b, n)        GET_GLOBAL_FIELD_N(b, n, CBAR, TYPE)
#define GET_CBAR_BSU(b, n)         GET_GLOBAL_FIELD_N(b, n, CBAR, BSU)
#define GET_CBAR_RACFG(b, n)       GET_GLOBAL_FIELD_N(b, n, CBAR, RACFG)
#define GET_CBAR_WACFG(b, n)       GET_GLOBAL_FIELD_N(b, n, CBAR, WACFG)
#define GET_CBAR_IRPTNDX(b, n)     GET_GLOBAL_FIELD_N(b, n, CBAR, IRPTNDX)

/* Context Bank Fault Restricted Syndrome Register A: CBFRSYNRA_N */
#define SET_CBFRSYNRA_SID(b, n, v) SET_GLOBAL_FIELD_N(b, n, CBFRSYNRA, SID, v)

#define GET_CBFRSYNRA_SID(b, n)    GET_GLOBAL_FIELD_N(b, n, CBFRSYNRA, SID)

/* Stage 1 Context Bank Format Fields */
#define SET_CB_ACTLR_REQPRIORITY (b, c, v) \
		SET_CONTEXT_FIELD(b, c, CB_ACTLR, REQPRIORITY, v)
#define SET_CB_ACTLR_REQPRIORITYCFG(b, c, v) \
		SET_CONTEXT_FIELD(b, c, CB_ACTLR, REQPRIORITYCFG, v)
#define SET_CB_ACTLR_PRIVCFG(b, c, v) \
		SET_CONTEXT_FIELD(b, c, CB_ACTLR, PRIVCFG, v)
#define SET_CB_ACTLR_BPRCOSH(b, c, v) \
		SET_CONTEXT_FIELD(b, c, CB_ACTLR, BPRCOSH, v)
#define SET_CB_ACTLR_BPRCISH(b, c, v) \
		SET_CONTEXT_FIELD(b, c, CB_ACTLR, BPRCISH, v)
#define SET_CB_ACTLR_BPRCNSH(b, c, v) \
		SET_CONTEXT_FIELD(b, c, CB_ACTLR, BPRCNSH, v)

#define GET_CB_ACTLR_REQPRIORITY (b, c) \
		GET_CONTEXT_FIELD(b, c, CB_ACTLR, REQPRIORITY)
#define GET_CB_ACTLR_REQPRIORITYCFG(b, c) \
		GET_CONTEXT_FIELD(b, c, CB_ACTLR, REQPRIORITYCFG)
#define GET_CB_ACTLR_PRIVCFG(b, c)  GET_CONTEXT_FIELD(b, c, CB_ACTLR, PRIVCFG)
#define GET_CB_ACTLR_BPRCOSH(b, c)  GET_CONTEXT_FIELD(b, c, CB_ACTLR, BPRCOSH)
#define GET_CB_ACTLR_BPRCISH(b, c)  GET_CONTEXT_FIELD(b, c, CB_ACTLR, BPRCISH)
#define GET_CB_ACTLR_BPRCNSH(b, c)  GET_CONTEXT_FIELD(b, c, CB_ACTLR, BPRCNSH)

/* Address Translation, Stage 1, Privileged Read: CB_ATS1PR */
#define SET_CB_ATS1PR_ADDR(b, c, v) SET_CONTEXT_FIELD(b, c, CB_ATS1PR, ADDR, v)

#define GET_CB_ATS1PR_ADDR(b, c)    GET_CONTEXT_FIELD(b, c, CB_ATS1PR, ADDR)

/* Address Translation, Stage 1, Privileged Write: CB_ATS1PW */
#define SET_CB_ATS1PW_ADDR(b, c, v) SET_CONTEXT_FIELD(b, c, CB_ATS1PW, ADDR, v)

#define GET_CB_ATS1PW_ADDR(b, c)    GET_CONTEXT_FIELD(b, c, CB_ATS1PW, ADDR)

/* Address Translation, Stage 1, User Read: CB_ATS1UR */
#define SET_CB_ATS1UR_ADDR(b, c, v) SET_CONTEXT_FIELD(b, c, CB_ATS1UR, ADDR, v)

#define GET_CB_ATS1UR_ADDR(b, c)    GET_CONTEXT_FIELD(b, c, CB_ATS1UR, ADDR)

/* Address Translation, Stage 1, User Write: CB_ATS1UW */
#define SET_CB_ATS1UW_ADDR(b, c, v) SET_CONTEXT_FIELD(b, c, CB_ATS1UW, ADDR, v)

#define GET_CB_ATS1UW_ADDR(b, c)    GET_CONTEXT_FIELD(b, c, CB_ATS1UW, ADDR)

/* Address Translation Status Register: CB_ATSR */
#define SET_CB_ATSR_ACTIVE(b, c, v) SET_CONTEXT_FIELD(b, c, CB_ATSR, ACTIVE, v)

#define GET_CB_ATSR_ACTIVE(b, c)    GET_CONTEXT_FIELD(b, c, CB_ATSR, ACTIVE)

/* Context ID Register: CB_CONTEXTIDR */
#define SET_CB_CONTEXTIDR_ASID(b, c, v) \
			SET_CONTEXT_FIELD(b, c, CB_CONTEXTIDR, ASID, v)
#define SET_CB_CONTEXTIDR_PROCID(b, c, v) \
			SET_CONTEXT_FIELD(b, c, CB_CONTEXTIDR, PROCID, v)

#define GET_CB_CONTEXTIDR_ASID(b, c)    \
			GET_CONTEXT_FIELD(b, c, CB_CONTEXTIDR, ASID)
#define GET_CB_CONTEXTIDR_PROCID(b, c)    \
			GET_CONTEXT_FIELD(b, c, CB_CONTEXTIDR, PROCID)

/* Fault Address Register: CB_FAR */
#define SET_CB_FAR_FADDR(b, c, v) SET_CONTEXT_FIELD(b, c, CB_FAR, FADDR, v)

#define GET_CB_FAR_FADDR(b, c)    GET_CONTEXT_FIELD(b, c, CB_FAR, FADDR)

/* Fault Status Register: CB_FSR */
#define SET_CB_FSR_TF(b, c, v)     SET_CONTEXT_FIELD(b, c, CB_FSR, TF, v)
#define SET_CB_FSR_AFF(b, c, v)    SET_CONTEXT_FIELD(b, c, CB_FSR, AFF, v)
#define SET_CB_FSR_PF(b, c, v)     SET_CONTEXT_FIELD(b, c, CB_FSR, PF, v)
#define SET_CB_FSR_EF(b, c, v)     SET_CONTEXT_FIELD(b, c, CB_FSR, EF, v)
#define SET_CB_FSR_TLBMCF(b, c, v) SET_CONTEXT_FIELD(b, c, CB_FSR, TLBMCF, v)
#define SET_CB_FSR_TLBLKF(b, c, v) SET_CONTEXT_FIELD(b, c, CB_FSR, TLBLKF, v)
#define SET_CB_FSR_SS(b, c, v)     SET_CONTEXT_FIELD(b, c, CB_FSR, SS, v)
#define SET_CB_FSR_MULTI(b, c, v)  SET_CONTEXT_FIELD(b, c, CB_FSR, MULTI, v)

#define GET_CB_FSR_TF(b, c)        GET_CONTEXT_FIELD(b, c, CB_FSR, TF)
#define GET_CB_FSR_AFF(b, c)       GET_CONTEXT_FIELD(b, c, CB_FSR, AFF)
#define GET_CB_FSR_PF(b, c)        GET_CONTEXT_FIELD(b, c, CB_FSR, PF)
#define GET_CB_FSR_EF(b, c)        GET_CONTEXT_FIELD(b, c, CB_FSR, EF)
#define GET_CB_FSR_TLBMCF(b, c)    GET_CONTEXT_FIELD(b, c, CB_FSR, TLBMCF)
#define GET_CB_FSR_TLBLKF(b, c)    GET_CONTEXT_FIELD(b, c, CB_FSR, TLBLKF)
#define GET_CB_FSR_SS(b, c)        GET_CONTEXT_FIELD(b, c, CB_FSR, SS)
#define GET_CB_FSR_MULTI(b, c)     GET_CONTEXT_FIELD(b, c, CB_FSR, MULTI)

/* Fault Syndrome Register 0: CB_FSYNR0 */
#define SET_CB_FSYNR0_PLVL(b, c, v) SET_CONTEXT_FIELD(b, c, CB_FSYNR0, PLVL, v)
#define SET_CB_FSYNR0_S1PTWF(b, c, v) \
				SET_CONTEXT_FIELD(b, c, CB_FSYNR0, S1PTWF, v)
#define SET_CB_FSYNR0_WNR(b, c, v)  SET_CONTEXT_FIELD(b, c, CB_FSYNR0, WNR, v)
#define SET_CB_FSYNR0_PNU(b, c, v)  SET_CONTEXT_FIELD(b, c, CB_FSYNR0, PNU, v)
#define SET_CB_FSYNR0_IND(b, c, v)  SET_CONTEXT_FIELD(b, c, CB_FSYNR0, IND, v)
#define SET_CB_FSYNR0_NSSTATE(b, c, v) \
				SET_CONTEXT_FIELD(b, c, CB_FSYNR0, NSSTATE, v)
#define SET_CB_FSYNR0_NSATTR(b, c, v) \
				SET_CONTEXT_FIELD(b, c, CB_FSYNR0, NSATTR, v)
#define SET_CB_FSYNR0_ATOF(b, c, v)  SET_CONTEXT_FIELD(b, c, CB_FSYNR0, ATOF, v)
#define SET_CB_FSYNR0_PTWF(b, c, v)  SET_CONTEXT_FIELD(b, c, CB_FSYNR0, PTWF, v)
#define SET_CB_FSYNR0_AFR(b, c, v)   SET_CONTEXT_FIELD(b, c, CB_FSYNR0, AFR, v)
#define SET_CB_FSYNR0_S1CBNDX(b, c, v) \
				SET_CONTEXT_FIELD(b, c, CB_FSYNR0, S1CBNDX, v)

#define GET_CB_FSYNR0_PLVL(b, c)    GET_CONTEXT_FIELD(b, c, CB_FSYNR0, PLVL)
#define GET_CB_FSYNR0_S1PTWF(b, c)    \
				GET_CONTEXT_FIELD(b, c, CB_FSYNR0, S1PTWF)
#define GET_CB_FSYNR0_WNR(b, c)     GET_CONTEXT_FIELD(b, c, CB_FSYNR0, WNR)
#define GET_CB_FSYNR0_PNU(b, c)     GET_CONTEXT_FIELD(b, c, CB_FSYNR0, PNU)
#define GET_CB_FSYNR0_IND(b, c)     GET_CONTEXT_FIELD(b, c, CB_FSYNR0, IND)
#define GET_CB_FSYNR0_NSSTATE(b, c)    \
				GET_CONTEXT_FIELD(b, c, CB_FSYNR0, NSSTATE)
#define GET_CB_FSYNR0_NSATTR(b, c)    \
				GET_CONTEXT_FIELD(b, c, CB_FSYNR0, NSATTR)
#define GET_CB_FSYNR0_ATOF(b, c)     GET_CONTEXT_FIELD(b, c, CB_FSYNR0, ATOF)
#define GET_CB_FSYNR0_PTWF(b, c)     GET_CONTEXT_FIELD(b, c, CB_FSYNR0, PTWF)
#define GET_CB_FSYNR0_AFR(b, c)      GET_CONTEXT_FIELD(b, c, CB_FSYNR0, AFR)
#define GET_CB_FSYNR0_S1CBNDX(b, c)    \
				GET_CONTEXT_FIELD(b, c, CB_FSYNR0, S1CBNDX)

/* Normal Memory Remap Register: CB_NMRR */
#define SET_CB_NMRR_IR0(b, c, v)    SET_CONTEXT_FIELD(b, c, CB_NMRR, IR0, v)
#define SET_CB_NMRR_IR1(b, c, v)    SET_CONTEXT_FIELD(b, c, CB_NMRR, IR1, v)
#define SET_CB_NMRR_IR2(b, c, v)    SET_CONTEXT_FIELD(b, c, CB_NMRR, IR2, v)
#define SET_CB_NMRR_IR3(b, c, v)    SET_CONTEXT_FIELD(b, c, CB_NMRR, IR3, v)
#define SET_CB_NMRR_IR4(b, c, v)    SET_CONTEXT_FIELD(b, c, CB_NMRR, IR4, v)
#define SET_CB_NMRR_IR5(b, c, v)    SET_CONTEXT_FIELD(b, c, CB_NMRR, IR5, v)
#define SET_CB_NMRR_IR6(b, c, v)    SET_CONTEXT_FIELD(b, c, CB_NMRR, IR6, v)
#define SET_CB_NMRR_IR7(b, c, v)    SET_CONTEXT_FIELD(b, c, CB_NMRR, IR7, v)
#define SET_CB_NMRR_OR0(b, c, v)    SET_CONTEXT_FIELD(b, c, CB_NMRR, OR0, v)
#define SET_CB_NMRR_OR1(b, c, v)    SET_CONTEXT_FIELD(b, c, CB_NMRR, OR1, v)
#define SET_CB_NMRR_OR2(b, c, v)    SET_CONTEXT_FIELD(b, c, CB_NMRR, OR2, v)
#define SET_CB_NMRR_OR3(b, c, v)    SET_CONTEXT_FIELD(b, c, CB_NMRR, OR3, v)
#define SET_CB_NMRR_OR4(b, c, v)    SET_CONTEXT_FIELD(b, c, CB_NMRR, OR4, v)
#define SET_CB_NMRR_OR5(b, c, v)    SET_CONTEXT_FIELD(b, c, CB_NMRR, OR5, v)
#define SET_CB_NMRR_OR6(b, c, v)    SET_CONTEXT_FIELD(b, c, CB_NMRR, OR6, v)
#define SET_CB_NMRR_OR7(b, c, v)    SET_CONTEXT_FIELD(b, c, CB_NMRR, OR7, v)

#define GET_CB_NMRR_IR0(b, c)       GET_CONTEXT_FIELD(b, c, CB_NMRR, IR0)
#define GET_CB_NMRR_IR1(b, c)       GET_CONTEXT_FIELD(b, c, CB_NMRR, IR1)
#define GET_CB_NMRR_IR2(b, c)       GET_CONTEXT_FIELD(b, c, CB_NMRR, IR2)
#define GET_CB_NMRR_IR3(b, c)       GET_CONTEXT_FIELD(b, c, CB_NMRR, IR3)
#define GET_CB_NMRR_IR4(b, c)       GET_CONTEXT_FIELD(b, c, CB_NMRR, IR4)
#define GET_CB_NMRR_IR5(b, c)       GET_CONTEXT_FIELD(b, c, CB_NMRR, IR5)
#define GET_CB_NMRR_IR6(b, c)       GET_CONTEXT_FIELD(b, c, CB_NMRR, IR6)
#define GET_CB_NMRR_IR7(b, c)       GET_CONTEXT_FIELD(b, c, CB_NMRR, IR7)
#define GET_CB_NMRR_OR0(b, c)       GET_CONTEXT_FIELD(b, c, CB_NMRR, OR0)
#define GET_CB_NMRR_OR1(b, c)       GET_CONTEXT_FIELD(b, c, CB_NMRR, OR1)
#define GET_CB_NMRR_OR2(b, c)       GET_CONTEXT_FIELD(b, c, CB_NMRR, OR2)
#define GET_CB_NMRR_OR3(b, c)       GET_CONTEXT_FIELD(b, c, CB_NMRR, OR3)
#define GET_CB_NMRR_OR4(b, c)       GET_CONTEXT_FIELD(b, c, CB_NMRR, OR4)
#define GET_CB_NMRR_OR5(b, c)       GET_CONTEXT_FIELD(b, c, CB_NMRR, OR5)

/* Physical Address Register: CB_PAR */
#define SET_CB_PAR_F(b, c, v)       SET_CONTEXT_FIELD(b, c, CB_PAR, F, v)
#define SET_CB_PAR_SS(b, c, v)      SET_CONTEXT_FIELD(b, c, CB_PAR, SS, v)
#define SET_CB_PAR_OUTER(b, c, v)   SET_CONTEXT_FIELD(b, c, CB_PAR, OUTER, v)
#define SET_CB_PAR_INNER(b, c, v)   SET_CONTEXT_FIELD(b, c, CB_PAR, INNER, v)
#define SET_CB_PAR_SH(b, c, v)      SET_CONTEXT_FIELD(b, c, CB_PAR, SH, v)
#define SET_CB_PAR_NS(b, c, v)      SET_CONTEXT_FIELD(b, c, CB_PAR, NS, v)
#define SET_CB_PAR_NOS(b, c, v)     SET_CONTEXT_FIELD(b, c, CB_PAR, NOS, v)
#define SET_CB_PAR_PA(b, c, v)      SET_CONTEXT_FIELD(b, c, CB_PAR, PA, v)
#define SET_CB_PAR_TF(b, c, v)      SET_CONTEXT_FIELD(b, c, CB_PAR, TF, v)
#define SET_CB_PAR_AFF(b, c, v)     SET_CONTEXT_FIELD(b, c, CB_PAR, AFF, v)
#define SET_CB_PAR_PF(b, c, v)      SET_CONTEXT_FIELD(b, c, CB_PAR, PF, v)
#define SET_CB_PAR_TLBMCF(b, c, v)  SET_CONTEXT_FIELD(b, c, CB_PAR, TLBMCF, v)
#define SET_CB_PAR_TLBLKF(b, c, v)  SET_CONTEXT_FIELD(b, c, CB_PAR, TLBLKF, v)
#define SET_CB_PAR_ATOT(b, c, v)    SET_CONTEXT_FIELD(b, c, CB_PAR, ATOT, v)
#define SET_CB_PAR_PLVL(b, c, v)    SET_CONTEXT_FIELD(b, c, CB_PAR, PLVL, v)
#define SET_CB_PAR_STAGE(b, c, v)   SET_CONTEXT_FIELD(b, c, CB_PAR, STAGE, v)

#define GET_CB_PAR_F(b, c)          GET_CONTEXT_FIELD(b, c, CB_PAR, F)
#define GET_CB_PAR_SS(b, c)         GET_CONTEXT_FIELD(b, c, CB_PAR, SS)
#define GET_CB_PAR_OUTER(b, c)      GET_CONTEXT_FIELD(b, c, CB_PAR, OUTER)
#define GET_CB_PAR_INNER(b, c)      GET_CONTEXT_FIELD(b, c, CB_PAR, INNER)
#define GET_CB_PAR_SH(b, c)         GET_CONTEXT_FIELD(b, c, CB_PAR, SH)
#define GET_CB_PAR_NS(b, c)         GET_CONTEXT_FIELD(b, c, CB_PAR, NS)
#define GET_CB_PAR_NOS(b, c)        GET_CONTEXT_FIELD(b, c, CB_PAR, NOS)
#define GET_CB_PAR_PA(b, c)         GET_CONTEXT_FIELD(b, c, CB_PAR, PA)
#define GET_CB_PAR_TF(b, c)         GET_CONTEXT_FIELD(b, c, CB_PAR, TF)
#define GET_CB_PAR_AFF(b, c)        GET_CONTEXT_FIELD(b, c, CB_PAR, AFF)
#define GET_CB_PAR_PF(b, c)         GET_CONTEXT_FIELD(b, c, CB_PAR, PF)
#define GET_CB_PAR_TLBMCF(b, c)     GET_CONTEXT_FIELD(b, c, CB_PAR, TLBMCF)
#define GET_CB_PAR_TLBLKF(b, c)     GET_CONTEXT_FIELD(b, c, CB_PAR, TLBLKF)
#define GET_CB_PAR_ATOT(b, c)       GET_CONTEXT_FIELD(b, c, CB_PAR, ATOT)
#define GET_CB_PAR_PLVL(b, c)       GET_CONTEXT_FIELD(b, c, CB_PAR, PLVL)
#define GET_CB_PAR_STAGE(b, c)      GET_CONTEXT_FIELD(b, c, CB_PAR, STAGE)

/* Primary Region Remap Register: CB_PRRR */
#define SET_CB_PRRR_TR0(b, c, v)    SET_CONTEXT_FIELD(b, c, CB_PRRR, TR0, v)
#define SET_CB_PRRR_TR1(b, c, v)    SET_CONTEXT_FIELD(b, c, CB_PRRR, TR1, v)
#define SET_CB_PRRR_TR2(b, c, v)    SET_CONTEXT_FIELD(b, c, CB_PRRR, TR2, v)
#define SET_CB_PRRR_TR3(b, c, v)    SET_CONTEXT_FIELD(b, c, CB_PRRR, TR3, v)
#define SET_CB_PRRR_TR4(b, c, v)    SET_CONTEXT_FIELD(b, c, CB_PRRR, TR4, v)
#define SET_CB_PRRR_TR5(b, c, v)    SET_CONTEXT_FIELD(b, c, CB_PRRR, TR5, v)
#define SET_CB_PRRR_TR6(b, c, v)    SET_CONTEXT_FIELD(b, c, CB_PRRR, TR6, v)
#define SET_CB_PRRR_TR7(b, c, v)    SET_CONTEXT_FIELD(b, c, CB_PRRR, TR7, v)
#define SET_CB_PRRR_DS0(b, c, v)    SET_CONTEXT_FIELD(b, c, CB_PRRR, DS0, v)
#define SET_CB_PRRR_DS1(b, c, v)    SET_CONTEXT_FIELD(b, c, CB_PRRR, DS1, v)
#define SET_CB_PRRR_NS0(b, c, v)    SET_CONTEXT_FIELD(b, c, CB_PRRR, NS0, v)
#define SET_CB_PRRR_NS1(b, c, v)    SET_CONTEXT_FIELD(b, c, CB_PRRR, NS1, v)
#define SET_CB_PRRR_NOS0(b, c, v)   SET_CONTEXT_FIELD(b, c, CB_PRRR, NOS0, v)
#define SET_CB_PRRR_NOS1(b, c, v)   SET_CONTEXT_FIELD(b, c, CB_PRRR, NOS1, v)
#define SET_CB_PRRR_NOS2(b, c, v)   SET_CONTEXT_FIELD(b, c, CB_PRRR, NOS2, v)
#define SET_CB_PRRR_NOS3(b, c, v)   SET_CONTEXT_FIELD(b, c, CB_PRRR, NOS3, v)
#define SET_CB_PRRR_NOS4(b, c, v)   SET_CONTEXT_FIELD(b, c, CB_PRRR, NOS4, v)
#define SET_CB_PRRR_NOS5(b, c, v)   SET_CONTEXT_FIELD(b, c, CB_PRRR, NOS5, v)
#define SET_CB_PRRR_NOS6(b, c, v)   SET_CONTEXT_FIELD(b, c, CB_PRRR, NOS6, v)
#define SET_CB_PRRR_NOS7(b, c, v)   SET_CONTEXT_FIELD(b, c, CB_PRRR, NOS7, v)

#define GET_CB_PRRR_TR0(b, c)       GET_CONTEXT_FIELD(b, c, CB_PRRR, TR0)
#define GET_CB_PRRR_TR1(b, c)       GET_CONTEXT_FIELD(b, c, CB_PRRR, TR1)
#define GET_CB_PRRR_TR2(b, c)       GET_CONTEXT_FIELD(b, c, CB_PRRR, TR2)
#define GET_CB_PRRR_TR3(b, c)       GET_CONTEXT_FIELD(b, c, CB_PRRR, TR3)
#define GET_CB_PRRR_TR4(b, c)       GET_CONTEXT_FIELD(b, c, CB_PRRR, TR4)
#define GET_CB_PRRR_TR5(b, c)       GET_CONTEXT_FIELD(b, c, CB_PRRR, TR5)
#define GET_CB_PRRR_TR6(b, c)       GET_CONTEXT_FIELD(b, c, CB_PRRR, TR6)
#define GET_CB_PRRR_TR7(b, c)       GET_CONTEXT_FIELD(b, c, CB_PRRR, TR7)
#define GET_CB_PRRR_DS0(b, c)       GET_CONTEXT_FIELD(b, c, CB_PRRR, DS0)
#define GET_CB_PRRR_DS1(b, c)       GET_CONTEXT_FIELD(b, c, CB_PRRR, DS1)
#define GET_CB_PRRR_NS0(b, c)       GET_CONTEXT_FIELD(b, c, CB_PRRR, NS0)
#define GET_CB_PRRR_NS1(b, c)       GET_CONTEXT_FIELD(b, c, CB_PRRR, NS1)
#define GET_CB_PRRR_NOS0(b, c)      GET_CONTEXT_FIELD(b, c, CB_PRRR, NOS0)
#define GET_CB_PRRR_NOS1(b, c)      GET_CONTEXT_FIELD(b, c, CB_PRRR, NOS1)
#define GET_CB_PRRR_NOS2(b, c)      GET_CONTEXT_FIELD(b, c, CB_PRRR, NOS2)
#define GET_CB_PRRR_NOS3(b, c)      GET_CONTEXT_FIELD(b, c, CB_PRRR, NOS3)
#define GET_CB_PRRR_NOS4(b, c)      GET_CONTEXT_FIELD(b, c, CB_PRRR, NOS4)
#define GET_CB_PRRR_NOS5(b, c)      GET_CONTEXT_FIELD(b, c, CB_PRRR, NOS5)
#define GET_CB_PRRR_NOS6(b, c)      GET_CONTEXT_FIELD(b, c, CB_PRRR, NOS6)
#define GET_CB_PRRR_NOS7(b, c)      GET_CONTEXT_FIELD(b, c, CB_PRRR, NOS7)

/* Transaction Resume: CB_RESUME */
#define SET_CB_RESUME_TNR(b, c, v)  SET_CONTEXT_FIELD(b, c, CB_RESUME, TNR, v)

#define GET_CB_RESUME_TNR(b, c)     GET_CONTEXT_FIELD(b, c, CB_RESUME, TNR)

/* System Control Register: CB_SCTLR */
#define SET_CB_SCTLR_M(b, c, v)     SET_CONTEXT_FIELD(b, c, CB_SCTLR, M, v)
#define SET_CB_SCTLR_TRE(b, c, v)   SET_CONTEXT_FIELD(b, c, CB_SCTLR, TRE, v)
#define SET_CB_SCTLR_AFE(b, c, v)   SET_CONTEXT_FIELD(b, c, CB_SCTLR, AFE, v)
#define SET_CB_SCTLR_AFFD(b, c, v)  SET_CONTEXT_FIELD(b, c, CB_SCTLR, AFFD, v)
#define SET_CB_SCTLR_E(b, c, v)     SET_CONTEXT_FIELD(b, c, CB_SCTLR, E, v)
#define SET_CB_SCTLR_CFRE(b, c, v)  SET_CONTEXT_FIELD(b, c, CB_SCTLR, CFRE, v)
#define SET_CB_SCTLR_CFIE(b, c, v)  SET_CONTEXT_FIELD(b, c, CB_SCTLR, CFIE, v)
#define SET_CB_SCTLR_CFCFG(b, c, v) SET_CONTEXT_FIELD(b, c, CB_SCTLR, CFCFG, v)
#define SET_CB_SCTLR_HUPCF(b, c, v) SET_CONTEXT_FIELD(b, c, CB_SCTLR, HUPCF, v)
#define SET_CB_SCTLR_WXN(b, c, v)   SET_CONTEXT_FIELD(b, c, CB_SCTLR, WXN, v)
#define SET_CB_SCTLR_UWXN(b, c, v)  SET_CONTEXT_FIELD(b, c, CB_SCTLR, UWXN, v)
#define SET_CB_SCTLR_ASIDPNE(b, c, v) \
			SET_CONTEXT_FIELD(b, c, CB_SCTLR, ASIDPNE, v)
#define SET_CB_SCTLR_TRANSIENTCFG(b, c, v) \
			SET_CONTEXT_FIELD(b, c, CB_SCTLR, TRANSIENTCFG, v)
#define SET_CB_SCTLR_MEMATTR(b, c, v) \
			SET_CONTEXT_FIELD(b, c, CB_SCTLR, MEMATTR, v)
#define SET_CB_SCTLR_MTCFG(b, c, v) SET_CONTEXT_FIELD(b, c, CB_SCTLR, MTCFG, v)
#define SET_CB_SCTLR_SHCFG(b, c, v) SET_CONTEXT_FIELD(b, c, CB_SCTLR, SHCFG, v)
#define SET_CB_SCTLR_RACFG(b, c, v) SET_CONTEXT_FIELD(b, c, CB_SCTLR, RACFG, v)
#define SET_CB_SCTLR_WACFG(b, c, v) SET_CONTEXT_FIELD(b, c, CB_SCTLR, WACFG, v)
#define SET_CB_SCTLR_NSCFG(b, c, v) SET_CONTEXT_FIELD(b, c, CB_SCTLR, NSCFG, v)

#define GET_CB_SCTLR_M(b, c)        GET_CONTEXT_FIELD(b, c, CB_SCTLR, M)
#define GET_CB_SCTLR_TRE(b, c)      GET_CONTEXT_FIELD(b, c, CB_SCTLR, TRE)
#define GET_CB_SCTLR_AFE(b, c)      GET_CONTEXT_FIELD(b, c, CB_SCTLR, AFE)
#define GET_CB_SCTLR_AFFD(b, c)     GET_CONTEXT_FIELD(b, c, CB_SCTLR, AFFD)
#define GET_CB_SCTLR_E(b, c)        GET_CONTEXT_FIELD(b, c, CB_SCTLR, E)
#define GET_CB_SCTLR_CFRE(b, c)     GET_CONTEXT_FIELD(b, c, CB_SCTLR, CFRE)
#define GET_CB_SCTLR_CFIE(b, c)     GET_CONTEXT_FIELD(b, c, CB_SCTLR, CFIE)
#define GET_CB_SCTLR_CFCFG(b, c)    GET_CONTEXT_FIELD(b, c, CB_SCTLR, CFCFG)
#define GET_CB_SCTLR_HUPCF(b, c)    GET_CONTEXT_FIELD(b, c, CB_SCTLR, HUPCF)
#define GET_CB_SCTLR_WXN(b, c)      GET_CONTEXT_FIELD(b, c, CB_SCTLR, WXN)
#define GET_CB_SCTLR_UWXN(b, c)     GET_CONTEXT_FIELD(b, c, CB_SCTLR, UWXN)
#define GET_CB_SCTLR_ASIDPNE(b, c)    \
			GET_CONTEXT_FIELD(b, c, CB_SCTLR, ASIDPNE)
#define GET_CB_SCTLR_TRANSIENTCFG(b, c)    \
			GET_CONTEXT_FIELD(b, c, CB_SCTLR, TRANSIENTCFG)
#define GET_CB_SCTLR_MEMATTR(b, c)    \
			GET_CONTEXT_FIELD(b, c, CB_SCTLR, MEMATTR)
#define GET_CB_SCTLR_MTCFG(b, c)    GET_CONTEXT_FIELD(b, c, CB_SCTLR, MTCFG)
#define GET_CB_SCTLR_SHCFG(b, c)    GET_CONTEXT_FIELD(b, c, CB_SCTLR, SHCFG)
#define GET_CB_SCTLR_RACFG(b, c)    GET_CONTEXT_FIELD(b, c, CB_SCTLR, RACFG)
#define GET_CB_SCTLR_WACFG(b, c)    GET_CONTEXT_FIELD(b, c, CB_SCTLR, WACFG)
#define GET_CB_SCTLR_NSCFG(b, c)    GET_CONTEXT_FIELD(b, c, CB_SCTLR, NSCFG)

/* Invalidate TLB by ASID: CB_TLBIASID */
#define SET_CB_TLBIASID_ASID(b, c, v) \
				SET_CONTEXT_FIELD(b, c, CB_TLBIASID, ASID, v)

#define GET_CB_TLBIASID_ASID(b, c)    \
				GET_CONTEXT_FIELD(b, c, CB_TLBIASID, ASID)

/* Invalidate TLB by VA: CB_TLBIVA */
#define SET_CB_TLBIVA_ASID(b, c, v) SET_CONTEXT_FIELD(b, c, CB_TLBIVA, ASID, v)
#define SET_CB_TLBIVA_VA(b, c, v)   SET_CONTEXT_FIELD(b, c, CB_TLBIVA, VA, v)

#define GET_CB_TLBIVA_ASID(b, c)    GET_CONTEXT_FIELD(b, c, CB_TLBIVA, ASID)
#define GET_CB_TLBIVA_VA(b, c)      GET_CONTEXT_FIELD(b, c, CB_TLBIVA, VA)

/* Invalidate TLB by VA, All ASID: CB_TLBIVAA */
#define SET_CB_TLBIVAA_VA(b, c, v)  SET_CONTEXT_FIELD(b, c, CB_TLBIVAA, VA, v)

#define GET_CB_TLBIVAA_VA(b, c)     GET_CONTEXT_FIELD(b, c, CB_TLBIVAA, VA)

/* Invalidate TLB by VA, All ASID, Last Level: CB_TLBIVAAL */
#define SET_CB_TLBIVAAL_VA(b, c, v) SET_CONTEXT_FIELD(b, c, CB_TLBIVAAL, VA, v)

#define GET_CB_TLBIVAAL_VA(b, c)    GET_CONTEXT_FIELD(b, c, CB_TLBIVAAL, VA)

/* Invalidate TLB by VA, Last Level: CB_TLBIVAL */
#define SET_CB_TLBIVAL_ASID(b, c, v) \
			SET_CONTEXT_FIELD(b, c, CB_TLBIVAL, ASID, v)
#define SET_CB_TLBIVAL_VA(b, c, v)   SET_CONTEXT_FIELD(b, c, CB_TLBIVAL, VA, v)

#define GET_CB_TLBIVAL_ASID(b, c)    \
			GET_CONTEXT_FIELD(b, c, CB_TLBIVAL, ASID)
#define GET_CB_TLBIVAL_VA(b, c)      GET_CONTEXT_FIELD(b, c, CB_TLBIVAL, VA)

/* TLB Status: CB_TLBSTATUS */
#define SET_CB_TLBSTATUS_SACTIVE(b, c, v) \
			SET_CONTEXT_FIELD(b, c, CB_TLBSTATUS, SACTIVE, v)

#define GET_CB_TLBSTATUS_SACTIVE(b, c)    \
			GET_CONTEXT_FIELD(b, c, CB_TLBSTATUS, SACTIVE)

/* Translation Table Base Control Register: CB_TTBCR */
#define SET_CB_TTBCR_T0SZ(b, c, v)   SET_CONTEXT_FIELD(b, c, CB_TTBCR, T0SZ, v)
#define SET_CB_TTBCR_PD0(b, c, v)    SET_CONTEXT_FIELD(b, c, CB_TTBCR, PD0, v)
#define SET_CB_TTBCR_PD1(b, c, v)    SET_CONTEXT_FIELD(b, c, CB_TTBCR, PD1, v)
#define SET_CB_TTBCR_NSCFG0(b, c, v) \
			SET_CONTEXT_FIELD(b, c, CB_TTBCR, NSCFG0, v)
#define SET_CB_TTBCR_NSCFG1(b, c, v) \
			SET_CONTEXT_FIELD(b, c, CB_TTBCR, NSCFG1, v)
#define SET_CB_TTBCR_EAE(b, c, v)    SET_CONTEXT_FIELD(b, c, CB_TTBCR, EAE, v)

#define GET_CB_TTBCR_T0SZ(b, c)      GET_CONTEXT_FIELD(b, c, CB_TTBCR, T0SZ)
#define GET_CB_TTBCR_PD0(b, c)       GET_CONTEXT_FIELD(b, c, CB_TTBCR, PD0)
#define GET_CB_TTBCR_PD1(b, c)       GET_CONTEXT_FIELD(b, c, CB_TTBCR, PD1)
#define GET_CB_TTBCR_NSCFG0(b, c)    \
			GET_CONTEXT_FIELD(b, c, CB_TTBCR, NSCFG0)
#define GET_CB_TTBCR_NSCFG1(b, c)    \
			GET_CONTEXT_FIELD(b, c, CB_TTBCR, NSCFG1)
#define GET_CB_TTBCR_EAE(b, c)       GET_CONTEXT_FIELD(b, c, CB_TTBCR, EAE)

/* Translation Table Base Register 0: CB_TTBR */
#define SET_CB_TTBR0_IRGN1(b, c, v) SET_CONTEXT_FIELD(b, c, CB_TTBR0, IRGN1, v)
#define SET_CB_TTBR0_S(b, c, v)     SET_CONTEXT_FIELD(b, c, CB_TTBR0, S, v)
#define SET_CB_TTBR0_RGN(b, c, v)   SET_CONTEXT_FIELD(b, c, CB_TTBR0, RGN, v)
#define SET_CB_TTBR0_NOS(b, c, v)   SET_CONTEXT_FIELD(b, c, CB_TTBR0, NOS, v)
#define SET_CB_TTBR0_IRGN0(b, c, v) SET_CONTEXT_FIELD(b, c, CB_TTBR0, IRGN0, v)
#define SET_CB_TTBR0_ADDR(b, c, v)  SET_CONTEXT_FIELD(b, c, CB_TTBR0, ADDR, v)

#define GET_CB_TTBR0_IRGN1(b, c)    GET_CONTEXT_FIELD(b, c, CB_TTBR0, IRGN1)
#define GET_CB_TTBR0_S(b, c)        GET_CONTEXT_FIELD(b, c, CB_TTBR0, S)
#define GET_CB_TTBR0_RGN(b, c)      GET_CONTEXT_FIELD(b, c, CB_TTBR0, RGN)
#define GET_CB_TTBR0_NOS(b, c)      GET_CONTEXT_FIELD(b, c, CB_TTBR0, NOS)
#define GET_CB_TTBR0_IRGN0(b, c)    GET_CONTEXT_FIELD(b, c, CB_TTBR0, IRGN0)
#define GET_CB_TTBR0_ADDR(b, c)     GET_CONTEXT_FIELD(b, c, CB_TTBR0, ADDR)

/* Translation Table Base Register 1: CB_TTBR1 */
#define SET_CB_TTBR1_IRGN1(b, c, v) SET_CONTEXT_FIELD(b, c, CB_TTBR1, IRGN1, v)
#define SET_CB_TTBR1_0S(b, c, v)    SET_CONTEXT_FIELD(b, c, CB_TTBR1, S, v)
#define SET_CB_TTBR1_RGN(b, c, v)   SET_CONTEXT_FIELD(b, c, CB_TTBR1, RGN, v)
#define SET_CB_TTBR1_NOS(b, c, v)   SET_CONTEXT_FIELD(b, c, CB_TTBR1, NOS, v)
#define SET_CB_TTBR1_IRGN0(b, c, v) SET_CONTEXT_FIELD(b, c, CB_TTBR1, IRGN0, v)
#define SET_CB_TTBR1_ADDR(b, c, v)  SET_CONTEXT_FIELD(b, c, CB_TTBR1, ADDR, v)

#define GET_CB_TTBR1_IRGN1(b, c)    GET_CONTEXT_FIELD(b, c, CB_TTBR1, IRGN1)
#define GET_CB_TTBR1_0S(b, c)       GET_CONTEXT_FIELD(b, c, CB_TTBR1, S)
#define GET_CB_TTBR1_RGN(b, c)      GET_CONTEXT_FIELD(b, c, CB_TTBR1, RGN)
#define GET_CB_TTBR1_NOS(b, c)      GET_CONTEXT_FIELD(b, c, CB_TTBR1, NOS)
#define GET_CB_TTBR1_IRGN0(b, c)    GET_CONTEXT_FIELD(b, c, CB_TTBR1, IRGN0)
#define GET_CB_TTBR1_ADDR(b, c)     GET_CONTEXT_FIELD(b, c, CB_TTBR1, ADDR)

/* Global Register Space 0 */
#define CR0		(0x0000)
#define SCR1		(0x0004)
#define CR2		(0x0008)
#define ACR		(0x0010)
#define IDR0		(0x0020)
#define IDR1		(0x0024)
#define IDR2		(0x0028)
#define IDR7		(0x003C)
#define GFAR		(0x0040)
#define GFSR		(0x0044)
#define GFSRRESTORE	(0x004C)
#define GFSYNR0		(0x0050)
#define GFSYNR1		(0x0054)
#define GFSYNR2		(0x0058)
#define TLBIVMID	(0x0064)
#define TLBIALLNSNH	(0x0068)
#define TLBIALLH	(0x006C)
#define TLBGSYNC	(0x0070)
#define TLBGSTATUS	(0x0074)
#define TLBIVAH		(0x0078)
#define GATS1UR		(0x0100)
#define GATS1UW		(0x0108)
#define GATS1PR		(0x0110)
#define GATS1PW		(0x0118)
#define GATS12UR	(0x0120)
#define GATS12UW	(0x0128)
#define GATS12PR	(0x0130)
#define GATS12PW	(0x0138)
#define GPAR		(0x0180)
#define GATSR		(0x0188)
#define NSCR0		(0x0400)
#define NSCR2		(0x0408)
#define NSACR		(0x0410)
#define SMR		(0x0800)
#define S2CR		(0x0C00)

/* Global Register Space 1 */
#define CBAR		(0x1000)
#define CBFRSYNRA	(0x1400)

/* Implementation defined Register Space */
#define PREDICTIONDIS0	(0x204C)
#define PREDICTIONDIS1	(0x2050)
#define S1L1BFBLP0	(0x215C)

/* Performance Monitoring Register Space */
#define PMEVCNTR_N	(0x3000)
#define PMEVTYPER_N	(0x3400)
#define PMCGCR_N	(0x3800)
#define PMCGSMR_N	(0x3A00)
#define PMCNTENSET_N	(0x3C00)
#define PMCNTENCLR_N	(0x3C20)
#define PMINTENSET_N	(0x3C40)
#define PMINTENCLR_N	(0x3C60)
#define PMOVSCLR_N	(0x3C80)
#define PMOVSSET_N	(0x3CC0)
#define PMCFGR		(0x3E00)
#define PMCR		(0x3E04)
#define PMCEID0		(0x3E20)
#define PMCEID1		(0x3E24)
#define PMAUTHSTATUS	(0x3FB8)
#define PMDEVTYPE	(0x3FCC)

/* Secure Status Determination Address Space */
#define SSDR_N		(0x4000)

/* Stage 1 Context Bank Format */
#define CB_SCTLR	(0x000)
#define CB_ACTLR	(0x004)
#define CB_RESUME	(0x008)
#define CB_TTBR0	(0x020)
#define CB_TTBR1	(0x028)
#define CB_TTBCR	(0x030)
#define CB_CONTEXTIDR	(0x034)
#define CB_PRRR		(0x038)
#define CB_NMRR		(0x03C)
#define CB_PAR		(0x050)
#define CB_FSR		(0x058)
#define CB_FSRRESTORE	(0x05C)
#define CB_FAR		(0x060)
#define CB_FSYNR0	(0x068)
#define CB_FSYNR1	(0x06C)
#define CB_TLBIVA	(0x600)
#define CB_TLBIVAA	(0x608)
#define CB_TLBIASID	(0x610)
#define CB_TLBIALL	(0x618)
#define CB_TLBIVAL	(0x620)
#define CB_TLBIVAAL	(0x628)
#define CB_TLBSYNC	(0x7F0)
#define CB_TLBSTATUS	(0x7F4)
#define CB_ATS1PR	(0x800)
#define CB_ATS1PW	(0x808)
#define CB_ATS1UR	(0x810)
#define CB_ATS1UW	(0x818)
#define CB_ATSR		(0x8F0)
#define CB_PMXEVCNTR_N	(0xE00)
#define CB_PMXEVTYPER_N	(0xE80)
#define CB_PMCFGR	(0xF00)
#define CB_PMCR		(0xF04)
#define CB_PMCEID0	(0xF20)
#define CB_PMCEID1	(0xF24)
#define CB_PMCNTENSET	(0xF40)
#define CB_PMCNTENCLR	(0xF44)
#define CB_PMINTENSET	(0xF48)
#define CB_PMINTENCLR	(0xF4C)
#define CB_PMOVSCLR	(0xF50)
#define CB_PMOVSSET	(0xF58)
#define CB_PMAUTHSTATUS	(0xFB8)

/* Global Register Fields */
/* Configuration Register: CR0 */
#define CR0_NSCFG         (CR0_NSCFG_MASK         << CR0_NSCFG_SHIFT)
#define CR0_WACFG         (CR0_WACFG_MASK         << CR0_WACFG_SHIFT)
#define CR0_RACFG         (CR0_RACFG_MASK         << CR0_RACFG_SHIFT)
#define CR0_SHCFG         (CR0_SHCFG_MASK         << CR0_SHCFG_SHIFT)
#define CR0_SMCFCFG       (CR0_SMCFCFG_MASK       << CR0_SMCFCFG_SHIFT)
#define CR0_MTCFG         (CR0_MTCFG_MASK         << CR0_MTCFG_SHIFT)
#define CR0_MEMATTR       (CR0_MEMATTR_MASK       << CR0_MEMATTR_SHIFT)
#define CR0_BSU           (CR0_BSU_MASK           << CR0_BSU_SHIFT)
#define CR0_FB            (CR0_FB_MASK            << CR0_FB_SHIFT)
#define CR0_PTM           (CR0_PTM_MASK           << CR0_PTM_SHIFT)
#define CR0_VMIDPNE       (CR0_VMIDPNE_MASK       << CR0_VMIDPNE_SHIFT)
#define CR0_USFCFG        (CR0_USFCFG_MASK        << CR0_USFCFG_SHIFT)
#define CR0_GSE           (CR0_GSE_MASK           << CR0_GSE_SHIFT)
#define CR0_STALLD        (CR0_STALLD_MASK        << CR0_STALLD_SHIFT)
#define CR0_TRANSIENTCFG  (CR0_TRANSIENTCFG_MASK  << CR0_TRANSIENTCFG_SHIFT)
#define CR0_GCFGFIE       (CR0_GCFGFIE_MASK       << CR0_GCFGFIE_SHIFT)
#define CR0_GCFGFRE       (CR0_GCFGFRE_MASK       << CR0_GCFGFRE_SHIFT)
#define CR0_GFIE          (CR0_GFIE_MASK          << CR0_GFIE_SHIFT)
#define CR0_GFRE          (CR0_GFRE_MASK          << CR0_GFRE_SHIFT)
#define CR0_CLIENTPD      (CR0_CLIENTPD_MASK      << CR0_CLIENTPD_SHIFT)

/* Configuration Register: CR2 */
#define CR2_BPVMID        (CR2_BPVMID_MASK << CR2_BPVMID_SHIFT)

/* Global Address Translation, Stage 1, Privileged Read: GATS1PR */
#define GATS1PR_ADDR  (GATS1PR_ADDR_MASK  << GATS1PR_ADDR_SHIFT)
#define GATS1PR_NDX   (GATS1PR_NDX_MASK   << GATS1PR_NDX_SHIFT)

/* Global Address Translation, Stage 1, Privileged Write: GATS1PW */
#define GATS1PW_ADDR  (GATS1PW_ADDR_MASK  << GATS1PW_ADDR_SHIFT)
#define GATS1PW_NDX   (GATS1PW_NDX_MASK   << GATS1PW_NDX_SHIFT)

/* Global Address Translation, Stage 1, User Read: GATS1UR */
#define GATS1UR_ADDR  (GATS1UR_ADDR_MASK  << GATS1UR_ADDR_SHIFT)
#define GATS1UR_NDX   (GATS1UR_NDX_MASK   << GATS1UR_NDX_SHIFT)

/* Global Address Translation, Stage 1, User Write: GATS1UW */
#define GATS1UW_ADDR  (GATS1UW_ADDR_MASK  << GATS1UW_ADDR_SHIFT)
#define GATS1UW_NDX   (GATS1UW_NDX_MASK   << GATS1UW_NDX_SHIFT)

/* Global Address Translation, Stage 1 and 2, Privileged Read: GATS1PR */
#define GATS12PR_ADDR (GATS12PR_ADDR_MASK << GATS12PR_ADDR_SHIFT)
#define GATS12PR_NDX  (GATS12PR_NDX_MASK  << GATS12PR_NDX_SHIFT)

/* Global Address Translation, Stage 1 and 2, Privileged Write: GATS1PW */
#define GATS12PW_ADDR (GATS12PW_ADDR_MASK << GATS12PW_ADDR_SHIFT)
#define GATS12PW_NDX  (GATS12PW_NDX_MASK  << GATS12PW_NDX_SHIFT)

/* Global Address Translation, Stage 1 and 2, User Read: GATS1UR */
#define GATS12UR_ADDR (GATS12UR_ADDR_MASK << GATS12UR_ADDR_SHIFT)
#define GATS12UR_NDX  (GATS12UR_NDX_MASK  << GATS12UR_NDX_SHIFT)

/* Global Address Translation, Stage 1 and 2, User Write: GATS1UW */
#define GATS12UW_ADDR (GATS12UW_ADDR_MASK << GATS12UW_ADDR_SHIFT)
#define GATS12UW_NDX  (GATS12UW_NDX_MASK  << GATS12UW_NDX_SHIFT)

/* Global Address Translation Status Register: GATSR */
#define GATSR_ACTIVE  (GATSR_ACTIVE_MASK  << GATSR_ACTIVE_SHIFT)

/* Global Fault Address Register: GFAR */
#define GFAR_FADDR    (GFAR_FADDR_MASK << GFAR_FADDR_SHIFT)

/* Global Fault Status Register: GFSR */
#define GFSR_ICF      (GFSR_ICF_MASK   << GFSR_ICF_SHIFT)
#define GFSR_USF      (GFSR_USF_MASK   << GFSR_USF_SHIFT)
#define GFSR_SMCF     (GFSR_SMCF_MASK  << GFSR_SMCF_SHIFT)
#define GFSR_UCBF     (GFSR_UCBF_MASK  << GFSR_UCBF_SHIFT)
#define GFSR_UCIF     (GFSR_UCIF_MASK  << GFSR_UCIF_SHIFT)
#define GFSR_CAF      (GFSR_CAF_MASK   << GFSR_CAF_SHIFT)
#define GFSR_EF       (GFSR_EF_MASK    << GFSR_EF_SHIFT)
#define GFSR_PF       (GFSR_PF_MASK    << GFSR_PF_SHIFT)
#define GFSR_MULTI    (GFSR_MULTI_MASK << GFSR_MULTI_SHIFT)

/* Global Fault Syndrome Register 0: GFSYNR0 */
#define GFSYNR0_NESTED  (GFSYNR0_NESTED_MASK  << GFSYNR0_NESTED_SHIFT)
#define GFSYNR0_WNR     (GFSYNR0_WNR_MASK     << GFSYNR0_WNR_SHIFT)
#define GFSYNR0_PNU     (GFSYNR0_PNU_MASK     << GFSYNR0_PNU_SHIFT)
#define GFSYNR0_IND     (GFSYNR0_IND_MASK     << GFSYNR0_IND_SHIFT)
#define GFSYNR0_NSSTATE (GFSYNR0_NSSTATE_MASK << GFSYNR0_NSSTATE_SHIFT)
#define GFSYNR0_NSATTR  (GFSYNR0_NSATTR_MASK  << GFSYNR0_NSATTR_SHIFT)

/* Global Fault Syndrome Register 1: GFSYNR1 */
#define GFSYNR1_SID     (GFSYNR1_SID_MASK     << GFSYNR1_SID_SHIFT)

/* Global Physical Address Register: GPAR */
#define GPAR_F          (GPAR_F_MASK      << GPAR_F_SHIFT)
#define GPAR_SS         (GPAR_SS_MASK     << GPAR_SS_SHIFT)
#define GPAR_OUTER      (GPAR_OUTER_MASK  << GPAR_OUTER_SHIFT)
#define GPAR_INNER      (GPAR_INNER_MASK  << GPAR_INNER_SHIFT)
#define GPAR_SH         (GPAR_SH_MASK     << GPAR_SH_SHIFT)
#define GPAR_NS         (GPAR_NS_MASK     << GPAR_NS_SHIFT)
#define GPAR_NOS        (GPAR_NOS_MASK    << GPAR_NOS_SHIFT)
#define GPAR_PA         (GPAR_PA_MASK     << GPAR_PA_SHIFT)
#define GPAR_TF         (GPAR_TF_MASK     << GPAR_TF_SHIFT)
#define GPAR_AFF        (GPAR_AFF_MASK    << GPAR_AFF_SHIFT)
#define GPAR_PF         (GPAR_PF_MASK     << GPAR_PF_SHIFT)
#define GPAR_EF         (GPAR_EF_MASK     << GPAR_EF_SHIFT)
#define GPAR_TLCMCF     (GPAR_TLBMCF_MASK << GPAR_TLCMCF_SHIFT)
#define GPAR_TLBLKF     (GPAR_TLBLKF_MASK << GPAR_TLBLKF_SHIFT)
#define GPAR_UCBF       (GPAR_UCBF_MASK   << GFAR_UCBF_SHIFT)

/* Identification Register: IDR0 */
#define IDR0_NUMSMRG    (IDR0_NUMSMRG_MASK  << IDR0_NUMSMGR_SHIFT)
#define IDR0_NUMSIDB    (IDR0_NUMSIDB_MASK  << IDR0_NUMSIDB_SHIFT)
#define IDR0_BTM        (IDR0_BTM_MASK      << IDR0_BTM_SHIFT)
#define IDR0_CTTW       (IDR0_CTTW_MASK     << IDR0_CTTW_SHIFT)
#define IDR0_NUMIRPT    (IDR0_NUMIPRT_MASK  << IDR0_NUMIRPT_SHIFT)
#define IDR0_PTFS       (IDR0_PTFS_MASK     << IDR0_PTFS_SHIFT)
#define IDR0_SMS        (IDR0_SMS_MASK      << IDR0_SMS_SHIFT)
#define IDR0_NTS        (IDR0_NTS_MASK      << IDR0_NTS_SHIFT)
#define IDR0_S2TS       (IDR0_S2TS_MASK     << IDR0_S2TS_SHIFT)
#define IDR0_S1TS       (IDR0_S1TS_MASK     << IDR0_S1TS_SHIFT)
#define IDR0_SES        (IDR0_SES_MASK      << IDR0_SES_SHIFT)

/* Identification Register: IDR1 */
#define IDR1_NUMCB       (IDR1_NUMCB_MASK       << IDR1_NUMCB_SHIFT)
#define IDR1_NUMSSDNDXB  (IDR1_NUMSSDNDXB_MASK  << IDR1_NUMSSDNDXB_SHIFT)
#define IDR1_SSDTP       (IDR1_SSDTP_MASK       << IDR1_SSDTP_SHIFT)
#define IDR1_SMCD        (IDR1_SMCD_MASK        << IDR1_SMCD_SHIFT)
#define IDR1_NUMS2CB     (IDR1_NUMS2CB_MASK     << IDR1_NUMS2CB_SHIFT)
#define IDR1_NUMPAGENDXB (IDR1_NUMPAGENDXB_MASK << IDR1_NUMPAGENDXB_SHIFT)
#define IDR1_PAGESIZE    (IDR1_PAGESIZE_MASK    << IDR1_PAGESIZE_SHIFT)

/* Identification Register: IDR2 */
#define IDR2_IAS         (IDR2_IAS_MASK << IDR2_IAS_SHIFT)
#define IDR1_OAS         (IDR2_OAS_MASK << IDR2_OAS_SHIFT)

/* Identification Register: IDR7 */
#define IDR7_MINOR       (IDR7_MINOR_MASK << IDR7_MINOR_SHIFT)
#define IDR7_MAJOR       (IDR7_MAJOR_MASK << IDR7_MAJOR_SHIFT)

/* Stream to Context Register: S2CR */
#define S2CR_CBNDX        (S2CR_CBNDX_MASK         << S2cR_CBNDX_SHIFT)
#define S2CR_SHCFG        (S2CR_SHCFG_MASK         << s2CR_SHCFG_SHIFT)
#define S2CR_MTCFG        (S2CR_MTCFG_MASK         << S2CR_MTCFG_SHIFT)
#define S2CR_MEMATTR      (S2CR_MEMATTR_MASK       << S2CR_MEMATTR_SHIFT)
#define S2CR_TYPE         (S2CR_TYPE_MASK          << S2CR_TYPE_SHIFT)
#define S2CR_NSCFG        (S2CR_NSCFG_MASK         << S2CR_NSCFG_SHIFT)
#define S2CR_RACFG        (S2CR_RACFG_MASK         << S2CR_RACFG_SHIFT)
#define S2CR_WACFG        (S2CR_WACFG_MASK         << S2CR_WACFG_SHIFT)
#define S2CR_PRIVCFG      (S2CR_PRIVCFG_MASK       << S2CR_PRIVCFG_SHIFT)
#define S2CR_INSTCFG      (S2CR_INSTCFG_MASK       << S2CR_INSTCFG_SHIFT)
#define S2CR_TRANSIENTCFG (S2CR_TRANSIENTCFG_MASK  << S2CR_TRANSIENTCFG_SHIFT)
#define S2CR_VMID         (S2CR_VMID_MASK          << S2CR_VMID_SHIFT)
#define S2CR_BSU          (S2CR_BSU_MASK           << S2CR_BSU_SHIFT)
#define S2CR_FB           (S2CR_FB_MASK            << S2CR_FB_SHIFT)

/* Stream Match Register: SMR */
#define SMR_ID            (SMR_ID_MASK    << SMR_ID_SHIFT)
#define SMR_MASK          (SMR_MASK_MASK  << SMR_MASK_SHIFT)
#define SMR_VALID         (SMR_VALID_MASK << SMR_VALID_SHIFT)

/* Global TLB Status: TLBGSTATUS */
#define TLBGSTATUS_GSACTIVE (TLBGSTATUS_GSACTIVE_MASK << \
					TLBGSTATUS_GSACTIVE_SHIFT)
/* Invalidate Hyp TLB by VA: TLBIVAH */
#define TLBIVAH_ADDR  (TLBIVAH_ADDR_MASK << TLBIVAH_ADDR_SHIFT)

/* Invalidate TLB by VMID: TLBIVMID */
#define TLBIVMID_VMID (TLBIVMID_VMID_MASK << TLBIVMID_VMID_SHIFT)

/* Context Bank Attribute Register: CBAR */
#define CBAR_VMID       (CBAR_VMID_MASK    << CBAR_VMID_SHIFT)
#define CBAR_CBNDX      (CBAR_CBNDX_MASK   << CBAR_CBNDX_SHIFT)
#define CBAR_BPSHCFG    (CBAR_BPSHCFG_MASK << CBAR_BPSHCFG_SHIFT)
#define CBAR_HYPC       (CBAR_HYPC_MASK    << CBAR_HYPC_SHIFT)
#define CBAR_FB         (CBAR_FB_MASK      << CBAR_FB_SHIFT)
#define CBAR_MEMATTR    (CBAR_MEMATTR_MASK << CBAR_MEMATTR_SHIFT)
#define CBAR_TYPE       (CBAR_TYPE_MASK    << CBAR_TYPE_SHIFT)
#define CBAR_BSU        (CBAR_BSU_MASK     << CBAR_BSU_SHIFT)
#define CBAR_RACFG      (CBAR_RACFG_MASK   << CBAR_RACFG_SHIFT)
#define CBAR_WACFG      (CBAR_WACFG_MASK   << CBAR_WACFG_SHIFT)
#define CBAR_IRPTNDX    (CBAR_IRPTNDX_MASK << CBAR_IRPTNDX_SHIFT)

/* Context Bank Fault Restricted Syndrome Register A: CBFRSYNRA */
#define CBFRSYNRA_SID   (CBFRSYNRA_SID_MASK << CBFRSYNRA_SID_SHIFT)

/* Performance Monitoring Register Fields */

/* Stage 1 Context Bank Format Fields */
/* Auxiliary Control Register: CB_ACTLR */
#define CB_ACTLR_REQPRIORITY \
		(CB_ACTLR_REQPRIORITY_MASK << CB_ACTLR_REQPRIORITY_SHIFT)
#define CB_ACTLR_REQPRIORITYCFG \
		(CB_ACTLR_REQPRIORITYCFG_MASK << CB_ACTLR_REQPRIORITYCFG_SHIFT)
#define CB_ACTLR_PRIVCFG (CB_ACTLR_PRIVCFG_MASK << CB_ACTLR_PRIVCFG_SHIFT)
#define CB_ACTLR_BPRCOSH (CB_ACTLR_BPRCOSH_MASK << CB_ACTLR_BPRCOSH_SHIFT)
#define CB_ACTLR_BPRCISH (CB_ACTLR_BPRCISH_MASK << CB_ACTLR_BPRCISH_SHIFT)
#define CB_ACTLR_BPRCNSH (CB_ACTLR_BPRCNSH_MASK << CB_ACTLR_BPRCNSH_SHIFT)

/* Address Translation, Stage 1, Privileged Read: CB_ATS1PR */
#define CB_ATS1PR_ADDR  (CB_ATS1PR_ADDR_MASK << CB_ATS1PR_ADDR_SHIFT)

/* Address Translation, Stage 1, Privileged Write: CB_ATS1PW */
#define CB_ATS1PW_ADDR  (CB_ATS1PW_ADDR_MASK << CB_ATS1PW_ADDR_SHIFT)

/* Address Translation, Stage 1, User Read: CB_ATS1UR */
#define CB_ATS1UR_ADDR  (CB_ATS1UR_ADDR_MASK << CB_ATS1UR_ADDR_SHIFT)

/* Address Translation, Stage 1, User Write: CB_ATS1UW */
#define CB_ATS1UW_ADDR  (CB_ATS1UW_ADDR_MASK << CB_ATS1UW_ADDR_SHIFT)

/* Address Translation Status Register: CB_ATSR */
#define CB_ATSR_ACTIVE  (CB_ATSR_ACTIVE_MASK << CB_ATSR_ACTIVE_SHIFT)

/* Context ID Register: CB_CONTEXTIDR */
#define CB_CONTEXTIDR_ASID    (CB_CONTEXTIDR_ASID_MASK << \
				CB_CONTEXTIDR_ASID_SHIFT)
#define CB_CONTEXTIDR_PROCID  (CB_CONTEXTIDR_PROCID_MASK << \
				CB_CONTEXTIDR_PROCID_SHIFT)

/* Fault Address Register: CB_FAR */
#define CB_FAR_FADDR  (CB_FAR_FADDR_MASK << CB_FAR_FADDR_SHIFT)

/* Fault Status Register: CB_FSR */
#define CB_FSR_TF     (CB_FSR_TF_MASK     << CB_FSR_TF_SHIFT)
#define CB_FSR_AFF    (CB_FSR_AFF_MASK    << CB_FSR_AFF_SHIFT)
#define CB_FSR_PF     (CB_FSR_PF_MASK     << CB_FSR_PF_SHIFT)
#define CB_FSR_EF     (CB_FSR_EF_MASK     << CB_FSR_EF_SHIFT)
#define CB_FSR_TLBMCF (CB_FSR_TLBMCF_MASK << CB_FSR_TLBMCF_SHIFT)
#define CB_FSR_TLBLKF (CB_FSR_TLBLKF_MASK << CB_FSR_TLBLKF_SHIFT)
#define CB_FSR_SS     (CB_FSR_SS_MASK     << CB_FSR_SS_SHIFT)
#define CB_FSR_MULTI  (CB_FSR_MULTI_MASK  << CB_FSR_MULTI_SHIFT)

/* Fault Syndrome Register 0: CB_FSYNR0 */
#define CB_FSYNR0_PLVL     (CB_FSYNR0_PLVL_MASK    << CB_FSYNR0_PLVL_SHIFT)
#define CB_FSYNR0_S1PTWF   (CB_FSYNR0_S1PTWF_MASK  << CB_FSYNR0_S1PTWF_SHIFT)
#define CB_FSYNR0_WNR      (CB_FSYNR0_WNR_MASK     << CB_FSYNR0_WNR_SHIFT)
#define CB_FSYNR0_PNU      (CB_FSYNR0_PNU_MASK     << CB_FSYNR0_PNU_SHIFT)
#define CB_FSYNR0_IND      (CB_FSYNR0_IND_MASK     << CB_FSYNR0_IND_SHIFT)
#define CB_FSYNR0_NSSTATE  (CB_FSYNR0_NSSTATE_MASK << CB_FSYNR0_NSSTATE_SHIFT)
#define CB_FSYNR0_NSATTR   (CB_FSYNR0_NSATTR_MASK  << CB_FSYNR0_NSATTR_SHIFT)
#define CB_FSYNR0_ATOF     (CB_FSYNR0_ATOF_MASK    << CB_FSYNR0_ATOF_SHIFT)
#define CB_FSYNR0_PTWF     (CB_FSYNR0_PTWF_MASK    << CB_FSYNR0_PTWF_SHIFT)
#define CB_FSYNR0_AFR      (CB_FSYNR0_AFR_MASK     << CB_FSYNR0_AFR_SHIFT)
#define CB_FSYNR0_S1CBNDX  (CB_FSYNR0_S1CBNDX_MASK << CB_FSYNR0_S1CBNDX_SHIFT)

/* Normal Memory Remap Register: CB_NMRR */
#define CB_NMRR_IR0        (CB_NMRR_IR0_MASK   << CB_NMRR_IR0_SHIFT)
#define CB_NMRR_IR1        (CB_NMRR_IR1_MASK   << CB_NMRR_IR1_SHIFT)
#define CB_NMRR_IR2        (CB_NMRR_IR2_MASK   << CB_NMRR_IR2_SHIFT)
#define CB_NMRR_IR3        (CB_NMRR_IR3_MASK   << CB_NMRR_IR3_SHIFT)
#define CB_NMRR_IR4        (CB_NMRR_IR4_MASK   << CB_NMRR_IR4_SHIFT)
#define CB_NMRR_IR5        (CB_NMRR_IR5_MASK   << CB_NMRR_IR5_SHIFT)
#define CB_NMRR_IR6        (CB_NMRR_IR6_MASK   << CB_NMRR_IR6_SHIFT)
#define CB_NMRR_IR7        (CB_NMRR_IR7_MASK   << CB_NMRR_IR7_SHIFT)
#define CB_NMRR_OR0        (CB_NMRR_OR0_MASK   << CB_NMRR_OR0_SHIFT)
#define CB_NMRR_OR1        (CB_NMRR_OR1_MASK   << CB_NMRR_OR1_SHIFT)
#define CB_NMRR_OR2        (CB_NMRR_OR2_MASK   << CB_NMRR_OR2_SHIFT)
#define CB_NMRR_OR3        (CB_NMRR_OR3_MASK   << CB_NMRR_OR3_SHIFT)
#define CB_NMRR_OR4        (CB_NMRR_OR4_MASK   << CB_NMRR_OR4_SHIFT)
#define CB_NMRR_OR5        (CB_NMRR_OR5_MASK   << CB_NMRR_OR5_SHIFT)
#define CB_NMRR_OR6        (CB_NMRR_OR6_MASK   << CB_NMRR_OR6_SHIFT)
#define CB_NMRR_OR7        (CB_NMRR_OR7_MASK   << CB_NMRR_OR7_SHIFT)

/* Physical Address Register: CB_PAR */
#define CB_PAR_F           (CB_PAR_F_MASK      << CB_PAR_F_SHIFT)
#define CB_PAR_SS          (CB_PAR_SS_MASK     << CB_PAR_SS_SHIFT)
#define CB_PAR_OUTER       (CB_PAR_OUTER_MASK  << CB_PAR_OUTER_SHIFT)
#define CB_PAR_INNER       (CB_PAR_INNER_MASK  << CB_PAR_INNER_SHIFT)
#define CB_PAR_SH          (CB_PAR_SH_MASK     << CB_PAR_SH_SHIFT)
#define CB_PAR_NS          (CB_PAR_NS_MASK     << CB_PAR_NS_SHIFT)
#define CB_PAR_NOS         (CB_PAR_NOS_MASK    << CB_PAR_NOS_SHIFT)
#define CB_PAR_PA          (CB_PAR_PA_MASK     << CB_PAR_PA_SHIFT)
#define CB_PAR_TF          (CB_PAR_TF_MASK     << CB_PAR_TF_SHIFT)
#define CB_PAR_AFF         (CB_PAR_AFF_MASK    << CB_PAR_AFF_SHIFT)
#define CB_PAR_PF          (CB_PAR_PF_MASK     << CB_PAR_PF_SHIFT)
#define CB_PAR_TLBMCF      (CB_PAR_TLBMCF_MASK << CB_PAR_TLBMCF_SHIFT)
#define CB_PAR_TLBLKF      (CB_PAR_TLBLKF_MASK << CB_PAR_TLBLKF_SHIFT)
#define CB_PAR_ATOT        (CB_PAR_ATOT_MASK   << CB_PAR_ATOT_SHIFT)
#define CB_PAR_PLVL        (CB_PAR_PLVL_MASK   << CB_PAR_PLVL_SHIFT)
#define CB_PAR_STAGE       (CB_PAR_STAGE_MASK  << CB_PAR_STAGE_SHIFT)

/* Primary Region Remap Register: CB_PRRR */
#define CB_PRRR_TR0        (CB_PRRR_TR0_MASK   << CB_PRRR_TR0_SHIFT)
#define CB_PRRR_TR1        (CB_PRRR_TR1_MASK   << CB_PRRR_TR1_SHIFT)
#define CB_PRRR_TR2        (CB_PRRR_TR2_MASK   << CB_PRRR_TR2_SHIFT)
#define CB_PRRR_TR3        (CB_PRRR_TR3_MASK   << CB_PRRR_TR3_SHIFT)
#define CB_PRRR_TR4        (CB_PRRR_TR4_MASK   << CB_PRRR_TR4_SHIFT)
#define CB_PRRR_TR5        (CB_PRRR_TR5_MASK   << CB_PRRR_TR5_SHIFT)
#define CB_PRRR_TR6        (CB_PRRR_TR6_MASK   << CB_PRRR_TR6_SHIFT)
#define CB_PRRR_TR7        (CB_PRRR_TR7_MASK   << CB_PRRR_TR7_SHIFT)
#define CB_PRRR_DS0        (CB_PRRR_DS0_MASK   << CB_PRRR_DS0_SHIFT)
#define CB_PRRR_DS1        (CB_PRRR_DS1_MASK   << CB_PRRR_DS1_SHIFT)
#define CB_PRRR_NS0        (CB_PRRR_NS0_MASK   << CB_PRRR_NS0_SHIFT)
#define CB_PRRR_NS1        (CB_PRRR_NS1_MASK   << CB_PRRR_NS1_SHIFT)
#define CB_PRRR_NOS0       (CB_PRRR_NOS0_MASK  << CB_PRRR_NOS0_SHIFT)
#define CB_PRRR_NOS1       (CB_PRRR_NOS1_MASK  << CB_PRRR_NOS1_SHIFT)
#define CB_PRRR_NOS2       (CB_PRRR_NOS2_MASK  << CB_PRRR_NOS2_SHIFT)
#define CB_PRRR_NOS3       (CB_PRRR_NOS3_MASK  << CB_PRRR_NOS3_SHIFT)
#define CB_PRRR_NOS4       (CB_PRRR_NOS4_MASK  << CB_PRRR_NOS4_SHIFT)
#define CB_PRRR_NOS5       (CB_PRRR_NOS5_MASK  << CB_PRRR_NOS5_SHIFT)
#define CB_PRRR_NOS6       (CB_PRRR_NOS6_MASK  << CB_PRRR_NOS6_SHIFT)
#define CB_PRRR_NOS7       (CB_PRRR_NOS7_MASK  << CB_PRRR_NOS7_SHIFT)

/* Transaction Resume: CB_RESUME */
#define CB_RESUME_TNR      (CB_RESUME_TNR_MASK << CB_RESUME_TNR_SHIFT)

/* System Control Register: CB_SCTLR */
#define CB_SCTLR_M           (CB_SCTLR_M_MASK       << CB_SCTLR_M_SHIFT)
#define CB_SCTLR_TRE         (CB_SCTLR_TRE_MASK     << CB_SCTLR_TRE_SHIFT)
#define CB_SCTLR_AFE         (CB_SCTLR_AFE_MASK     << CB_SCTLR_AFE_SHIFT)
#define CB_SCTLR_AFFD        (CB_SCTLR_AFFD_MASK    << CB_SCTLR_AFFD_SHIFT)
#define CB_SCTLR_E           (CB_SCTLR_E_MASK       << CB_SCTLR_E_SHIFT)
#define CB_SCTLR_CFRE        (CB_SCTLR_CFRE_MASK    << CB_SCTLR_CFRE_SHIFT)
#define CB_SCTLR_CFIE        (CB_SCTLR_CFIE_MASK    << CB_SCTLR_CFIE_SHIFT)
#define CB_SCTLR_CFCFG       (CB_SCTLR_CFCFG_MASK   << CB_SCTLR_CFCFG_SHIFT)
#define CB_SCTLR_HUPCF       (CB_SCTLR_HUPCF_MASK   << CB_SCTLR_HUPCF_SHIFT)
#define CB_SCTLR_WXN         (CB_SCTLR_WXN_MASK     << CB_SCTLR_WXN_SHIFT)
#define CB_SCTLR_UWXN        (CB_SCTLR_UWXN_MASK    << CB_SCTLR_UWXN_SHIFT)
#define CB_SCTLR_ASIDPNE     (CB_SCTLR_ASIDPNE_MASK << CB_SCTLR_ASIDPNE_SHIFT)
#define CB_SCTLR_TRANSIENTCFG (CB_SCTLR_TRANSIENTCFG_MASK << \
						CB_SCTLR_TRANSIENTCFG_SHIFT)
#define CB_SCTLR_MEMATTR     (CB_SCTLR_MEMATTR_MASK << CB_SCTLR_MEMATTR_SHIFT)
#define CB_SCTLR_MTCFG       (CB_SCTLR_MTCFG_MASK   << CB_SCTLR_MTCFG_SHIFT)
#define CB_SCTLR_SHCFG       (CB_SCTLR_SHCFG_MASK   << CB_SCTLR_SHCFG_SHIFT)
#define CB_SCTLR_RACFG       (CB_SCTLR_RACFG_MASK   << CB_SCTLR_RACFG_SHIFT)
#define CB_SCTLR_WACFG       (CB_SCTLR_WACFG_MASK   << CB_SCTLR_WACFG_SHIFT)
#define CB_SCTLR_NSCFG       (CB_SCTLR_NSCFG_MASK   << CB_SCTLR_NSCFG_SHIFT)

/* Invalidate TLB by ASID: CB_TLBIASID */
#define CB_TLBIASID_ASID     (CB_TLBIASID_ASID_MASK << CB_TLBIASID_ASID_SHIFT)

/* Invalidate TLB by VA: CB_TLBIVA */
#define CB_TLBIVA_ASID       (CB_TLBIVA_ASID_MASK   << CB_TLBIVA_ASID_SHIFT)
#define CB_TLBIVA_VA         (CB_TLBIVA_VA_MASK     << CB_TLBIVA_VA_SHIFT)

/* Invalidate TLB by VA, All ASID: CB_TLBIVAA */
#define CB_TLBIVAA_VA        (CB_TLBIVAA_VA_MASK    << CB_TLBIVAA_VA_SHIFT)

/* Invalidate TLB by VA, All ASID, Last Level: CB_TLBIVAAL */
#define CB_TLBIVAAL_VA       (CB_TLBIVAAL_VA_MASK   << CB_TLBIVAAL_VA_SHIFT)

/* Invalidate TLB by VA, Last Level: CB_TLBIVAL */
#define CB_TLBIVAL_ASID      (CB_TLBIVAL_ASID_MASK  << CB_TLBIVAL_ASID_SHIFT)
#define CB_TLBIVAL_VA        (CB_TLBIVAL_VA_MASK    << CB_TLBIVAL_VA_SHIFT)

/* TLB Status: CB_TLBSTATUS */
#define CB_TLBSTATUS_SACTIVE (CB_TLBSTATUS_SACTIVE_MASK << \
						CB_TLBSTATUS_SACTIVE_SHIFT)

/* Translation Table Base Control Register: CB_TTBCR */
#define CB_TTBCR_T0SZ        (CB_TTBCR_T0SZ_MASK    << CB_TTBCR_T0SZ_SHIFT)
#define CB_TTBCR_PD0         (CB_TTBCR_PD0_MASK     << CB_TTBCR_PD0_SHIFT)
#define CB_TTBCR_PD1         (CB_TTBCR_PD1_MASK     << CB_TTBCR_PD1_SHIFT)
#define CB_TTBCR_NSCFG0      (CB_TTBCR_NSCFG0_MASK  << CB_TTBCR_NSCFG0_SHIFT)
#define CB_TTBCR_NSCFG1      (CB_TTBCR_NSCFG1_MASK  << CB_TTBCR_NSCFG1_SHIFT)
#define CB_TTBCR_EAE         (CB_TTBCR_EAE_MASK     << CB_TTBCR_EAE_SHIFT)

/* Translation Table Base Register 0: CB_TTBR0 */
#define CB_TTBR0_IRGN1       (CB_TTBR0_IRGN1_MASK   << CB_TTBR0_IRGN1_SHIFT)
#define CB_TTBR0_S           (CB_TTBR0_S_MASK       << CB_TTBR0_S_SHIFT)
#define CB_TTBR0_RGN         (CB_TTBR0_RGN_MASK     << CB_TTBR0_RGN_SHIFT)
#define CB_TTBR0_NOS         (CB_TTBR0_NOS_MASK     << CB_TTBR0_NOS_SHIFT)
#define CB_TTBR0_IRGN0       (CB_TTBR0_IRGN0_MASK   << CB_TTBR0_IRGN0_SHIFT)
#define CB_TTBR0_ADDR        (CB_TTBR0_ADDR_MASK    << CB_TTBR0_ADDR_SHIFT)

/* Translation Table Base Register 1: CB_TTBR1 */
#define CB_TTBR1_IRGN1       (CB_TTBR1_IRGN1_MASK   << CB_TTBR1_IRGN1_SHIFT)
#define CB_TTBR1_S           (CB_TTBR1_S_MASK       << CB_TTBR1_S_SHIFT)
#define CB_TTBR1_RGN         (CB_TTBR1_RGN_MASK     << CB_TTBR1_RGN_SHIFT)
#define CB_TTBR1_NOS         (CB_TTBR1_NOS_MASK     << CB_TTBR1_NOS_SHIFT)
#define CB_TTBR1_IRGN0       (CB_TTBR1_IRGN0_MASK   << CB_TTBR1_IRGN0_SHIFT)
#define CB_TTBR1_ADDR        (CB_TTBR1_ADDR_MASK    << CB_TTBR1_ADDR_SHIFT)

/* Global Register Masks */
/* Configuration Register 0 */
#define CR0_NSCFG_MASK          0x03
#define CR0_WACFG_MASK          0x03
#define CR0_RACFG_MASK          0x03
#define CR0_SHCFG_MASK          0x03
#define CR0_SMCFCFG_MASK        0x01
#define CR0_MTCFG_MASK          0x01
#define CR0_MEMATTR_MASK        0x0F
#define CR0_BSU_MASK            0x03
#define CR0_FB_MASK             0x01
#define CR0_PTM_MASK            0x01
#define CR0_VMIDPNE_MASK        0x01
#define CR0_USFCFG_MASK         0x01
#define CR0_GSE_MASK            0x01
#define CR0_STALLD_MASK         0x01
#define CR0_TRANSIENTCFG_MASK   0x03
#define CR0_GCFGFIE_MASK        0x01
#define CR0_GCFGFRE_MASK        0x01
#define CR0_GFIE_MASK           0x01
#define CR0_GFRE_MASK           0x01
#define CR0_CLIENTPD_MASK       0x01

/* Configuration Register 2 */
#define CR2_BPVMID_MASK         0xFF

/* Global Address Translation, Stage 1, Privileged Read: GATS1PR */
#define GATS1PR_ADDR_MASK       0xFFFFF
#define GATS1PR_NDX_MASK        0xFF

/* Global Address Translation, Stage 1, Privileged Write: GATS1PW */
#define GATS1PW_ADDR_MASK       0xFFFFF
#define GATS1PW_NDX_MASK        0xFF

/* Global Address Translation, Stage 1, User Read: GATS1UR */
#define GATS1UR_ADDR_MASK       0xFFFFF
#define GATS1UR_NDX_MASK        0xFF

/* Global Address Translation, Stage 1, User Write: GATS1UW */
#define GATS1UW_ADDR_MASK       0xFFFFF
#define GATS1UW_NDX_MASK        0xFF

/* Global Address Translation, Stage 1 and 2, Privileged Read: GATS1PR */
#define GATS12PR_ADDR_MASK      0xFFFFF
#define GATS12PR_NDX_MASK       0xFF

/* Global Address Translation, Stage 1 and 2, Privileged Write: GATS1PW */
#define GATS12PW_ADDR_MASK      0xFFFFF
#define GATS12PW_NDX_MASK       0xFF

/* Global Address Translation, Stage 1 and 2, User Read: GATS1UR */
#define GATS12UR_ADDR_MASK      0xFFFFF
#define GATS12UR_NDX_MASK       0xFF

/* Global Address Translation, Stage 1 and 2, User Write: GATS1UW */
#define GATS12UW_ADDR_MASK      0xFFFFF
#define GATS12UW_NDX_MASK       0xFF

/* Global Address Translation Status Register: GATSR */
#define GATSR_ACTIVE_MASK       0x01

/* Global Fault Address Register: GFAR */
#define GFAR_FADDR_MASK         0xFFFFFFFF

/* Global Fault Status Register: GFSR */
#define GFSR_ICF_MASK           0x01
#define GFSR_USF_MASK           0x01
#define GFSR_SMCF_MASK          0x01
#define GFSR_UCBF_MASK          0x01
#define GFSR_UCIF_MASK          0x01
#define GFSR_CAF_MASK           0x01
#define GFSR_EF_MASK            0x01
#define GFSR_PF_MASK            0x01
#define GFSR_MULTI_MASK         0x01

/* Global Fault Syndrome Register 0: GFSYNR0 */
#define GFSYNR0_NESTED_MASK     0x01
#define GFSYNR0_WNR_MASK        0x01
#define GFSYNR0_PNU_MASK        0x01
#define GFSYNR0_IND_MASK        0x01
#define GFSYNR0_NSSTATE_MASK    0x01
#define GFSYNR0_NSATTR_MASK     0x01

/* Global Fault Syndrome Register 1: GFSYNR1 */
#define GFSYNR1_SID_MASK        0x7FFF
#define GFSYNr1_SSD_IDX_MASK    0x7FFF

/* Global Physical Address Register: GPAR */
#define GPAR_F_MASK             0x01
#define GPAR_SS_MASK            0x01
#define GPAR_OUTER_MASK         0x03
#define GPAR_INNER_MASK         0x03
#define GPAR_SH_MASK            0x01
#define GPAR_NS_MASK            0x01
#define GPAR_NOS_MASK           0x01
#define GPAR_PA_MASK            0xFFFFF
#define GPAR_TF_MASK            0x01
#define GPAR_AFF_MASK           0x01
#define GPAR_PF_MASK            0x01
#define GPAR_EF_MASK            0x01
#define GPAR_TLBMCF_MASK        0x01
#define GPAR_TLBLKF_MASK        0x01
#define GPAR_UCBF_MASK          0x01

/* Identification Register: IDR0 */
#define IDR0_NUMSMRG_MASK       0xFF
#define IDR0_NUMSIDB_MASK       0x0F
#define IDR0_BTM_MASK           0x01
#define IDR0_CTTW_MASK          0x01
#define IDR0_NUMIPRT_MASK       0xFF
#define IDR0_PTFS_MASK          0x01
#define IDR0_SMS_MASK           0x01
#define IDR0_NTS_MASK           0x01
#define IDR0_S2TS_MASK          0x01
#define IDR0_S1TS_MASK          0x01
#define IDR0_SES_MASK           0x01

/* Identification Register: IDR1 */
#define IDR1_NUMCB_MASK         0xFF
#define IDR1_NUMSSDNDXB_MASK    0x0F
#define IDR1_SSDTP_MASK         0x01
#define IDR1_SMCD_MASK          0x01
#define IDR1_NUMS2CB_MASK       0xFF
#define IDR1_NUMPAGENDXB_MASK   0x07
#define IDR1_PAGESIZE_MASK      0x01

/* Identification Register: IDR2 */
#define IDR2_IAS_MASK           0x0F
#define IDR2_OAS_MASK           0x0F

/* Identification Register: IDR7 */
#define IDR7_MINOR_MASK         0x0F
#define IDR7_MAJOR_MASK         0x0F

/* Stream to Context Register: S2CR */
#define S2CR_CBNDX_MASK         0xFF
#define S2CR_SHCFG_MASK         0x03
#define S2CR_MTCFG_MASK         0x01
#define S2CR_MEMATTR_MASK       0x0F
#define S2CR_TYPE_MASK          0x03
#define S2CR_NSCFG_MASK         0x03
#define S2CR_RACFG_MASK         0x03
#define S2CR_WACFG_MASK         0x03
#define S2CR_PRIVCFG_MASK       0x03
#define S2CR_INSTCFG_MASK       0x03
#define S2CR_TRANSIENTCFG_MASK  0x03
#define S2CR_VMID_MASK          0xFF
#define S2CR_BSU_MASK           0x03
#define S2CR_FB_MASK            0x01

/* Stream Match Register: SMR */
#define SMR_ID_MASK             0x7FFF
#define SMR_MASK_MASK           0x7FFF
#define SMR_VALID_MASK          0x01

/* Global TLB Status: TLBGSTATUS */
#define TLBGSTATUS_GSACTIVE_MASK 0x01

/* Invalidate Hyp TLB by VA: TLBIVAH */
#define TLBIVAH_ADDR_MASK       0xFFFFF

/* Invalidate TLB by VMID: TLBIVMID */
#define TLBIVMID_VMID_MASK      0xFF

/* Global Register Space 1 Mask */
/* Context Bank Attribute Register: CBAR */
#define CBAR_VMID_MASK          0xFF
#define CBAR_CBNDX_MASK         0x03
#define CBAR_BPSHCFG_MASK       0x03
#define CBAR_HYPC_MASK          0x01
#define CBAR_FB_MASK            0x01
#define CBAR_MEMATTR_MASK       0x0F
#define CBAR_TYPE_MASK          0x03
#define CBAR_BSU_MASK           0x03
#define CBAR_RACFG_MASK         0x03
#define CBAR_WACFG_MASK         0x03
#define CBAR_IRPTNDX_MASK       0xFF

/* Context Bank Fault Restricted Syndrome Register A: CBFRSYNRA */
#define CBFRSYNRA_SID_MASK      0x7FFF

/* Stage 1 Context Bank Format Masks */
/* Auxiliary Control Register: CB_ACTLR */
#define CB_ACTLR_REQPRIORITY_MASK    0x3
#define CB_ACTLR_REQPRIORITYCFG_MASK 0x1
#define CB_ACTLR_PRIVCFG_MASK        0x3
#define CB_ACTLR_BPRCOSH_MASK        0x1
#define CB_ACTLR_BPRCISH_MASK        0x1
#define CB_ACTLR_BPRCNSH_MASK        0x1

/* Address Translation, Stage 1, Privileged Read: CB_ATS1PR */
#define CB_ATS1PR_ADDR_MASK     0xFFFFF

/* Address Translation, Stage 1, Privileged Write: CB_ATS1PW */
#define CB_ATS1PW_ADDR_MASK     0xFFFFF

/* Address Translation, Stage 1, User Read: CB_ATS1UR */
#define CB_ATS1UR_ADDR_MASK     0xFFFFF

/* Address Translation, Stage 1, User Write: CB_ATS1UW */
#define CB_ATS1UW_ADDR_MASK     0xFFFFF

/* Address Translation Status Register: CB_ATSR */
#define CB_ATSR_ACTIVE_MASK     0x01

/* Context ID Register: CB_CONTEXTIDR */
#define CB_CONTEXTIDR_ASID_MASK   0xFF
#define CB_CONTEXTIDR_PROCID_MASK 0xFFFFFF

/* Fault Address Register: CB_FAR */
#define CB_FAR_FADDR_MASK       0xFFFFFFFF

/* Fault Status Register: CB_FSR */
#define CB_FSR_TF_MASK          0x01
#define CB_FSR_AFF_MASK         0x01
#define CB_FSR_PF_MASK          0x01
#define CB_FSR_EF_MASK          0x01
#define CB_FSR_TLBMCF_MASK      0x01
#define CB_FSR_TLBLKF_MASK      0x01
#define CB_FSR_SS_MASK          0x01
#define CB_FSR_MULTI_MASK       0x01

/* Fault Syndrome Register 0: CB_FSYNR0 */
#define CB_FSYNR0_PLVL_MASK     0x03
#define CB_FSYNR0_S1PTWF_MASK   0x01
#define CB_FSYNR0_WNR_MASK      0x01
#define CB_FSYNR0_PNU_MASK      0x01
#define CB_FSYNR0_IND_MASK      0x01
#define CB_FSYNR0_NSSTATE_MASK  0x01
#define CB_FSYNR0_NSATTR_MASK   0x01
#define CB_FSYNR0_ATOF_MASK     0x01
#define CB_FSYNR0_PTWF_MASK     0x01
#define CB_FSYNR0_AFR_MASK      0x01
#define CB_FSYNR0_S1CBNDX_MASK  0xFF

/* Normal Memory Remap Register: CB_NMRR */
#define CB_NMRR_IR0_MASK        0x03
#define CB_NMRR_IR1_MASK        0x03
#define CB_NMRR_IR2_MASK        0x03
#define CB_NMRR_IR3_MASK        0x03
#define CB_NMRR_IR4_MASK        0x03
#define CB_NMRR_IR5_MASK        0x03
#define CB_NMRR_IR6_MASK        0x03
#define CB_NMRR_IR7_MASK        0x03
#define CB_NMRR_OR0_MASK        0x03
#define CB_NMRR_OR1_MASK        0x03
#define CB_NMRR_OR2_MASK        0x03
#define CB_NMRR_OR3_MASK        0x03
#define CB_NMRR_OR4_MASK        0x03
#define CB_NMRR_OR5_MASK        0x03
#define CB_NMRR_OR6_MASK        0x03
#define CB_NMRR_OR7_MASK        0x03

/* Physical Address Register: CB_PAR */
#define CB_PAR_F_MASK           0x01
#define CB_PAR_SS_MASK          0x01
#define CB_PAR_OUTER_MASK       0x03
#define CB_PAR_INNER_MASK       0x07
#define CB_PAR_SH_MASK          0x01
#define CB_PAR_NS_MASK          0x01
#define CB_PAR_NOS_MASK         0x01
#define CB_PAR_PA_MASK          0xFFFFF
#define CB_PAR_TF_MASK          0x01
#define CB_PAR_AFF_MASK         0x01
#define CB_PAR_PF_MASK          0x01
#define CB_PAR_TLBMCF_MASK      0x01
#define CB_PAR_TLBLKF_MASK      0x01
#define CB_PAR_ATOT_MASK        0x01
#define CB_PAR_PLVL_MASK        0x03
#define CB_PAR_STAGE_MASK       0x01

/* Primary Region Remap Register: CB_PRRR */
#define CB_PRRR_TR0_MASK        0x03
#define CB_PRRR_TR1_MASK        0x03
#define CB_PRRR_TR2_MASK        0x03
#define CB_PRRR_TR3_MASK        0x03
#define CB_PRRR_TR4_MASK        0x03
#define CB_PRRR_TR5_MASK        0x03
#define CB_PRRR_TR6_MASK        0x03
#define CB_PRRR_TR7_MASK        0x03
#define CB_PRRR_DS0_MASK        0x01
#define CB_PRRR_DS1_MASK        0x01
#define CB_PRRR_NS0_MASK        0x01
#define CB_PRRR_NS1_MASK        0x01
#define CB_PRRR_NOS0_MASK       0x01
#define CB_PRRR_NOS1_MASK       0x01
#define CB_PRRR_NOS2_MASK       0x01
#define CB_PRRR_NOS3_MASK       0x01
#define CB_PRRR_NOS4_MASK       0x01
#define CB_PRRR_NOS5_MASK       0x01
#define CB_PRRR_NOS6_MASK       0x01
#define CB_PRRR_NOS7_MASK       0x01

/* Transaction Resume: CB_RESUME */
#define CB_RESUME_TNR_MASK      0x01

/* System Control Register: CB_SCTLR */
#define CB_SCTLR_M_MASK            0x01
#define CB_SCTLR_TRE_MASK          0x01
#define CB_SCTLR_AFE_MASK          0x01
#define CB_SCTLR_AFFD_MASK         0x01
#define CB_SCTLR_E_MASK            0x01
#define CB_SCTLR_CFRE_MASK         0x01
#define CB_SCTLR_CFIE_MASK         0x01
#define CB_SCTLR_CFCFG_MASK        0x01
#define CB_SCTLR_HUPCF_MASK        0x01
#define CB_SCTLR_WXN_MASK          0x01
#define CB_SCTLR_UWXN_MASK         0x01
#define CB_SCTLR_ASIDPNE_MASK      0x01
#define CB_SCTLR_TRANSIENTCFG_MASK 0x03
#define CB_SCTLR_MEMATTR_MASK      0x0F
#define CB_SCTLR_MTCFG_MASK        0x01
#define CB_SCTLR_SHCFG_MASK        0x03
#define CB_SCTLR_RACFG_MASK        0x03
#define CB_SCTLR_WACFG_MASK        0x03
#define CB_SCTLR_NSCFG_MASK        0x03

/* Invalidate TLB by ASID: CB_TLBIASID */
#define CB_TLBIASID_ASID_MASK      0xFF

/* Invalidate TLB by VA: CB_TLBIVA */
#define CB_TLBIVA_ASID_MASK        0xFF
#define CB_TLBIVA_VA_MASK          0xFFFFF

/* Invalidate TLB by VA, All ASID: CB_TLBIVAA */
#define CB_TLBIVAA_VA_MASK         0xFFFFF

/* Invalidate TLB by VA, All ASID, Last Level: CB_TLBIVAAL */
#define CB_TLBIVAAL_VA_MASK        0xFFFFF

/* Invalidate TLB by VA, Last Level: CB_TLBIVAL */
#define CB_TLBIVAL_ASID_MASK       0xFF
#define CB_TLBIVAL_VA_MASK         0xFFFFF

/* TLB Status: CB_TLBSTATUS */
#define CB_TLBSTATUS_SACTIVE_MASK  0x01

/* Translation Table Base Control Register: CB_TTBCR */
#define CB_TTBCR_T0SZ_MASK         0x07
#define CB_TTBCR_PD0_MASK          0x01
#define CB_TTBCR_PD1_MASK          0x01
#define CB_TTBCR_NSCFG0_MASK       0x01
#define CB_TTBCR_NSCFG1_MASK       0x01
#define CB_TTBCR_EAE_MASK          0x01

/* Translation Table Base Register 0/1: CB_TTBR */
#define CB_TTBR0_IRGN1_MASK        0x01
#define CB_TTBR0_S_MASK            0x01
#define CB_TTBR0_RGN_MASK          0x01
#define CB_TTBR0_NOS_MASK          0x01
#define CB_TTBR0_IRGN0_MASK        0x01
#define CB_TTBR0_ADDR_MASK         0xFFFFFF

#define CB_TTBR1_IRGN1_MASK        0x1
#define CB_TTBR1_S_MASK            0x1
#define CB_TTBR1_RGN_MASK          0x1
#define CB_TTBR1_NOS_MASK          0X1
#define CB_TTBR1_IRGN0_MASK        0X1
#define CB_TTBR1_ADDR_MASK         0xFFFFFF

/* Global Register Shifts */
/* Configuration Register: CR0 */
#define CR0_NSCFG_SHIFT            28
#define CR0_WACFG_SHIFT            26
#define CR0_RACFG_SHIFT            24
#define CR0_SHCFG_SHIFT            22
#define CR0_SMCFCFG_SHIFT          21
#define CR0_MTCFG_SHIFT            20
#define CR0_MEMATTR_SHIFT          16
#define CR0_BSU_SHIFT              14
#define CR0_FB_SHIFT               13
#define CR0_PTM_SHIFT              12
#define CR0_VMIDPNE_SHIFT          11
#define CR0_USFCFG_SHIFT           10
#define CR0_GSE_SHIFT              9
#define CR0_STALLD_SHIFT           8
#define CR0_TRANSIENTCFG_SHIFT     6
#define CR0_GCFGFIE_SHIFT          5
#define CR0_GCFGFRE_SHIFT          4
#define CR0_GFIE_SHIFT             2
#define CR0_GFRE_SHIFT             1
#define CR0_CLIENTPD_SHIFT         0

/* Configuration Register: CR2 */
#define CR2_BPVMID_SHIFT           0

/* Global Address Translation, Stage 1, Privileged Read: GATS1PR */
#define GATS1PR_ADDR_SHIFT         12
#define GATS1PR_NDX_SHIFT          0

/* Global Address Translation, Stage 1, Privileged Write: GATS1PW */
#define GATS1PW_ADDR_SHIFT         12
#define GATS1PW_NDX_SHIFT          0

/* Global Address Translation, Stage 1, User Read: GATS1UR */
#define GATS1UR_ADDR_SHIFT         12
#define GATS1UR_NDX_SHIFT          0

/* Global Address Translation, Stage 1, User Write: GATS1UW */
#define GATS1UW_ADDR_SHIFT         12
#define GATS1UW_NDX_SHIFT          0

/* Global Address Translation, Stage 1 and 2, Privileged Read: GATS12PR */
#define GATS12PR_ADDR_SHIFT        12
#define GATS12PR_NDX_SHIFT         0

/* Global Address Translation, Stage 1 and 2, Privileged Write: GATS12PW */
#define GATS12PW_ADDR_SHIFT        12
#define GATS12PW_NDX_SHIFT         0

/* Global Address Translation, Stage 1 and 2, User Read: GATS12UR */
#define GATS12UR_ADDR_SHIFT        12
#define GATS12UR_NDX_SHIFT         0

/* Global Address Translation, Stage 1 and 2, User Write: GATS12UW */
#define GATS12UW_ADDR_SHIFT        12
#define GATS12UW_NDX_SHIFT         0

/* Global Address Translation Status Register: GATSR */
#define GATSR_ACTIVE_SHIFT         0

/* Global Fault Address Register: GFAR */
#define GFAR_FADDR_SHIFT           0

/* Global Fault Status Register: GFSR */
#define GFSR_ICF_SHIFT             0
#define GFSR_USF_SHIFT             1
#define GFSR_SMCF_SHIFT            2
#define GFSR_UCBF_SHIFT            3
#define GFSR_UCIF_SHIFT            4
#define GFSR_CAF_SHIFT             5
#define GFSR_EF_SHIFT              6
#define GFSR_PF_SHIFT              7
#define GFSR_MULTI_SHIFT           31

/* Global Fault Syndrome Register 0: GFSYNR0 */
#define GFSYNR0_NESTED_SHIFT       0
#define GFSYNR0_WNR_SHIFT          1
#define GFSYNR0_PNU_SHIFT          2
#define GFSYNR0_IND_SHIFT          3
#define GFSYNR0_NSSTATE_SHIFT      4
#define GFSYNR0_NSATTR_SHIFT       5

/* Global Fault Syndrome Register 1: GFSYNR1 */
#define GFSYNR1_SID_SHIFT          0

/* Global Physical Address Register: GPAR */
#define GPAR_F_SHIFT               0
#define GPAR_SS_SHIFT              1
#define GPAR_OUTER_SHIFT           2
#define GPAR_INNER_SHIFT           4
#define GPAR_SH_SHIFT              7
#define GPAR_NS_SHIFT              9
#define GPAR_NOS_SHIFT             10
#define GPAR_PA_SHIFT              12
#define GPAR_TF_SHIFT              1
#define GPAR_AFF_SHIFT             2
#define GPAR_PF_SHIFT              3
#define GPAR_EF_SHIFT              4
#define GPAR_TLCMCF_SHIFT          5
#define GPAR_TLBLKF_SHIFT          6
#define GFAR_UCBF_SHIFT            30

/* Identification Register: IDR0 */
#define IDR0_NUMSMRG_SHIFT         0
#define IDR0_NUMSIDB_SHIFT         9
#define IDR0_BTM_SHIFT             13
#define IDR0_CTTW_SHIFT            14
#define IDR0_NUMIRPT_SHIFT         16
#define IDR0_PTFS_SHIFT            24
#define IDR0_SMS_SHIFT             27
#define IDR0_NTS_SHIFT             28
#define IDR0_S2TS_SHIFT            29
#define IDR0_S1TS_SHIFT            30
#define IDR0_SES_SHIFT             31

/* Identification Register: IDR1 */
#define IDR1_NUMCB_SHIFT           0
#define IDR1_NUMSSDNDXB_SHIFT      8
#define IDR1_SSDTP_SHIFT           12
#define IDR1_SMCD_SHIFT            15
#define IDR1_NUMS2CB_SHIFT         16
#define IDR1_NUMPAGENDXB_SHIFT     28
#define IDR1_PAGESIZE_SHIFT        31

/* Identification Register: IDR2 */
#define IDR2_IAS_SHIFT             0
#define IDR2_OAS_SHIFT             4

/* Identification Register: IDR7 */
#define IDR7_MINOR_SHIFT           0
#define IDR7_MAJOR_SHIFT           4

/* Stream to Context Register: S2CR */
#define S2CR_CBNDX_SHIFT           0
#define s2CR_SHCFG_SHIFT           8
#define S2CR_MTCFG_SHIFT           11
#define S2CR_MEMATTR_SHIFT         12
#define S2CR_TYPE_SHIFT            16
#define S2CR_NSCFG_SHIFT           18
#define S2CR_RACFG_SHIFT           20
#define S2CR_WACFG_SHIFT           22
#define S2CR_PRIVCFG_SHIFT         24
#define S2CR_INSTCFG_SHIFT         26
#define S2CR_TRANSIENTCFG_SHIFT    28
#define S2CR_VMID_SHIFT            0
#define S2CR_BSU_SHIFT             24
#define S2CR_FB_SHIFT              26

/* Stream Match Register: SMR */
#define SMR_ID_SHIFT               0
#define SMR_MASK_SHIFT             16
#define SMR_VALID_SHIFT            31

/* Global TLB Status: TLBGSTATUS */
#define TLBGSTATUS_GSACTIVE_SHIFT  0

/* Invalidate Hyp TLB by VA: TLBIVAH */
#define TLBIVAH_ADDR_SHIFT         12

/* Invalidate TLB by VMID: TLBIVMID */
#define TLBIVMID_VMID_SHIFT        0

/* Context Bank Attribute Register: CBAR */
#define CBAR_VMID_SHIFT            0
#define CBAR_CBNDX_SHIFT           8
#define CBAR_BPSHCFG_SHIFT         8
#define CBAR_HYPC_SHIFT            10
#define CBAR_FB_SHIFT              11
#define CBAR_MEMATTR_SHIFT         12
#define CBAR_TYPE_SHIFT            16
#define CBAR_BSU_SHIFT             18
#define CBAR_RACFG_SHIFT           20
#define CBAR_WACFG_SHIFT           22
#define CBAR_IRPTNDX_SHIFT         24

/* Context Bank Fault Restricted Syndrome Register A: CBFRSYNRA */
#define CBFRSYNRA_SID_SHIFT        0

/* Stage 1 Context Bank Format Shifts */
/* Auxiliary Control Register: CB_ACTLR */
#define CB_ACTLR_REQPRIORITY_SHIFT     0
#define CB_ACTLR_REQPRIORITYCFG_SHIFT  4
#define CB_ACTLR_PRIVCFG_SHIFT         8
#define CB_ACTLR_BPRCOSH_SHIFT         28
#define CB_ACTLR_BPRCISH_SHIFT         29
#define CB_ACTLR_BPRCNSH_SHIFT         30

/* Address Translation, Stage 1, Privileged Read: CB_ATS1PR */
#define CB_ATS1PR_ADDR_SHIFT       12

/* Address Translation, Stage 1, Privileged Write: CB_ATS1PW */
#define CB_ATS1PW_ADDR_SHIFT       12

/* Address Translation, Stage 1, User Read: CB_ATS1UR */
#define CB_ATS1UR_ADDR_SHIFT       12

/* Address Translation, Stage 1, User Write: CB_ATS1UW */
#define CB_ATS1UW_ADDR_SHIFT       12

/* Address Translation Status Register: CB_ATSR */
#define CB_ATSR_ACTIVE_SHIFT       0

/* Context ID Register: CB_CONTEXTIDR */
#define CB_CONTEXTIDR_ASID_SHIFT   0
#define CB_CONTEXTIDR_PROCID_SHIFT 8

/* Fault Address Register: CB_FAR */
#define CB_FAR_FADDR_SHIFT         0

/* Fault Status Register: CB_FSR */
#define CB_FSR_TF_SHIFT            1
#define CB_FSR_AFF_SHIFT           2
#define CB_FSR_PF_SHIFT            3
#define CB_FSR_EF_SHIFT            4
#define CB_FSR_TLBMCF_SHIFT        5
#define CB_FSR_TLBLKF_SHIFT        6
#define CB_FSR_SS_SHIFT            30
#define CB_FSR_MULTI_SHIFT         31

/* Fault Syndrome Register 0: CB_FSYNR0 */
#define CB_FSYNR0_PLVL_SHIFT       0
#define CB_FSYNR0_S1PTWF_SHIFT     3
#define CB_FSYNR0_WNR_SHIFT        4
#define CB_FSYNR0_PNU_SHIFT        5
#define CB_FSYNR0_IND_SHIFT        6
#define CB_FSYNR0_NSSTATE_SHIFT    7
#define CB_FSYNR0_NSATTR_SHIFT     8
#define CB_FSYNR0_ATOF_SHIFT       9
#define CB_FSYNR0_PTWF_SHIFT       10
#define CB_FSYNR0_AFR_SHIFT        11
#define CB_FSYNR0_S1CBNDX_SHIFT    16

/* Normal Memory Remap Register: CB_NMRR */
#define CB_NMRR_IR0_SHIFT          0
#define CB_NMRR_IR1_SHIFT          2
#define CB_NMRR_IR2_SHIFT          4
#define CB_NMRR_IR3_SHIFT          6
#define CB_NMRR_IR4_SHIFT          8
#define CB_NMRR_IR5_SHIFT          10
#define CB_NMRR_IR6_SHIFT          12
#define CB_NMRR_IR7_SHIFT          14
#define CB_NMRR_OR0_SHIFT          16
#define CB_NMRR_OR1_SHIFT          18
#define CB_NMRR_OR2_SHIFT          20
#define CB_NMRR_OR3_SHIFT          22
#define CB_NMRR_OR4_SHIFT          24
#define CB_NMRR_OR5_SHIFT          26
#define CB_NMRR_OR6_SHIFT          28
#define CB_NMRR_OR7_SHIFT          30

/* Physical Address Register: CB_PAR */
#define CB_PAR_F_SHIFT             0
#define CB_PAR_SS_SHIFT            1
#define CB_PAR_OUTER_SHIFT         2
#define CB_PAR_INNER_SHIFT         4
#define CB_PAR_SH_SHIFT            7
#define CB_PAR_NS_SHIFT            9
#define CB_PAR_NOS_SHIFT           10
#define CB_PAR_PA_SHIFT            12
#define CB_PAR_TF_SHIFT            1
#define CB_PAR_AFF_SHIFT           2
#define CB_PAR_PF_SHIFT            3
#define CB_PAR_TLBMCF_SHIFT        5
#define CB_PAR_TLBLKF_SHIFT        6
#define CB_PAR_ATOT_SHIFT          31
#define CB_PAR_PLVL_SHIFT          0
#define CB_PAR_STAGE_SHIFT         3

/* Primary Region Remap Register: CB_PRRR */
#define CB_PRRR_TR0_SHIFT          0
#define CB_PRRR_TR1_SHIFT          2
#define CB_PRRR_TR2_SHIFT          4
#define CB_PRRR_TR3_SHIFT          6
#define CB_PRRR_TR4_SHIFT          8
#define CB_PRRR_TR5_SHIFT          10
#define CB_PRRR_TR6_SHIFT          12
#define CB_PRRR_TR7_SHIFT          14
#define CB_PRRR_DS0_SHIFT          16
#define CB_PRRR_DS1_SHIFT          17
#define CB_PRRR_NS0_SHIFT          18
#define CB_PRRR_NS1_SHIFT          19
#define CB_PRRR_NOS0_SHIFT         24
#define CB_PRRR_NOS1_SHIFT         25
#define CB_PRRR_NOS2_SHIFT         26
#define CB_PRRR_NOS3_SHIFT         27
#define CB_PRRR_NOS4_SHIFT         28
#define CB_PRRR_NOS5_SHIFT         29
#define CB_PRRR_NOS6_SHIFT         30
#define CB_PRRR_NOS7_SHIFT         31

/* Transaction Resume: CB_RESUME */
#define CB_RESUME_TNR_SHIFT        0

/* System Control Register: CB_SCTLR */
#define CB_SCTLR_M_SHIFT            0
#define CB_SCTLR_TRE_SHIFT          1
#define CB_SCTLR_AFE_SHIFT          2
#define CB_SCTLR_AFFD_SHIFT         3
#define CB_SCTLR_E_SHIFT            4
#define CB_SCTLR_CFRE_SHIFT         5
#define CB_SCTLR_CFIE_SHIFT         6
#define CB_SCTLR_CFCFG_SHIFT        7
#define CB_SCTLR_HUPCF_SHIFT        8
#define CB_SCTLR_WXN_SHIFT          9
#define CB_SCTLR_UWXN_SHIFT         10
#define CB_SCTLR_ASIDPNE_SHIFT      12
#define CB_SCTLR_TRANSIENTCFG_SHIFT 14
#define CB_SCTLR_MEMATTR_SHIFT      16
#define CB_SCTLR_MTCFG_SHIFT        20
#define CB_SCTLR_SHCFG_SHIFT        22
#define CB_SCTLR_RACFG_SHIFT        24
#define CB_SCTLR_WACFG_SHIFT        26
#define CB_SCTLR_NSCFG_SHIFT        28

/* Invalidate TLB by ASID: CB_TLBIASID */
#define CB_TLBIASID_ASID_SHIFT      0

/* Invalidate TLB by VA: CB_TLBIVA */
#define CB_TLBIVA_ASID_SHIFT        0
#define CB_TLBIVA_VA_SHIFT          12

/* Invalidate TLB by VA, All ASID: CB_TLBIVAA */
#define CB_TLBIVAA_VA_SHIFT         12

/* Invalidate TLB by VA, All ASID, Last Level: CB_TLBIVAAL */
#define CB_TLBIVAAL_VA_SHIFT        12

/* Invalidate TLB by VA, Last Level: CB_TLBIVAL */
#define CB_TLBIVAL_ASID_SHIFT       0
#define CB_TLBIVAL_VA_SHIFT         12

/* TLB Status: CB_TLBSTATUS */
#define CB_TLBSTATUS_SACTIVE_SHIFT  0

/* Translation Table Base Control Register: CB_TTBCR */
#define CB_TTBCR_T0SZ_SHIFT         0
#define CB_TTBCR_PD0_SHIFT          4
#define CB_TTBCR_PD1_SHIFT          5
#define CB_TTBCR_NSCFG0_SHIFT       14
#define CB_TTBCR_NSCFG1_SHIFT       30
#define CB_TTBCR_EAE_SHIFT          31

/* Translation Table Base Register 0/1: CB_TTBR */
#define CB_TTBR0_IRGN1_SHIFT        0
#define CB_TTBR0_S_SHIFT            1
#define CB_TTBR0_RGN_SHIFT          3
#define CB_TTBR0_NOS_SHIFT          5
#define CB_TTBR0_IRGN0_SHIFT        6
#define CB_TTBR0_ADDR_SHIFT         14

#define CB_TTBR1_IRGN1_SHIFT        0
#define CB_TTBR1_S_SHIFT            1
#define CB_TTBR1_RGN_SHIFT          3
#define CB_TTBR1_NOS_SHIFT          5
#define CB_TTBR1_IRGN0_SHIFT        6
#define CB_TTBR1_ADDR_SHIFT         14

#endif
