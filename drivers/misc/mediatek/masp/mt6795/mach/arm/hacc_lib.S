	.arch armv7-a
	.fpu softvfp
	.eabi_attribute 20, 1
	.eabi_attribute 21, 1
	.eabi_attribute 23, 3
	.eabi_attribute 24, 1
	.eabi_attribute 25, 1
	.eabi_attribute 26, 2
	.eabi_attribute 30, 2
	.eabi_attribute 18, 4
	.file	"hacc_lib.c"
	.text
.Ltext0:
	.align	2
	.global	masp_hal_secure_algo_init
	.type	masp_hal_secure_algo_init, %function
masp_hal_secure_algo_init:
.LFB199:
	.file 1 "/alps/kernel/mediatek/platform/mt6582/kernel/drivers/masp/mach/hacc_lib.c"
	.loc 1 60 0
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
.LCFI0:
	stmfd	sp!, {fp, ip, lr, pc}
.LCFI1:
	sub	fp, ip, #4
.LCFI2:
.LVL0:
	.loc 1 64 0
	mov	r0, #1
	ldmfd	sp, {fp, sp, pc}
.LFE199:
	.size	masp_hal_secure_algo_init, .-masp_hal_secure_algo_init
	.align	2
	.global	masp_hal_secure_algo_deinit
	.type	masp_hal_secure_algo_deinit, %function
masp_hal_secure_algo_deinit:
.LFB200:
	.loc 1 70 0
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
.LCFI3:
	stmfd	sp!, {fp, ip, lr, pc}
.LCFI4:
	sub	fp, ip, #4
.LCFI5:
.LVL1:
	.loc 1 74 0
	mov	r0, #1
	ldmfd	sp, {fp, sp, pc}
.LFE200:
	.size	masp_hal_secure_algo_deinit, .-masp_hal_secure_algo_deinit
	.align	2
	.global	masp_hal_secure_algo
	.type	masp_hal_secure_algo, %function
masp_hal_secure_algo:
.LFB201:
	.loc 1 87 0
	@ args = 4, pretend = 0, frame = 0
	@ frame_needed = 1, uses_anonymous_args = 0
.LVL2:
	mov	ip, sp
.LCFI6:
	stmfd	sp!, {r4, r5, r6, r7, r8, fp, ip, lr, pc}
.LCFI7:
	sub	fp, ip, #4
.LCFI8:
	sub	sp, sp, #12
.LCFI9:
	ldr	r5, [fp, #4]
.LVL3:
	mov	r7, r0
	mov	r8, r1
	mov	r4, r2
	mov	r6, r3
.LVL4:
.L4:
	.loc 1 96 0
	bl	osal_hacc_lock
.LVL5:
	.loc 1 97 0 discriminator 1
	cmp	r0, #0
	bne	.L4
.LVL6:
	.loc 1 101 0
	mov	r0, r6
.LVL7:
	mov	r1, #16
	bl	masp_hal_sp_hacc_init
.LVL8:
	.loc 1 103 0
	subs	r6, r0, #0
.LVL9:
	bne	.L5
.LVL10:
	.loc 1 113 0
	cmp	r7, #0
	bne	.L16
	.loc 1 126 0
	mov	r0, r8
.LVL11:
	mov	r1, r4
	mov	r2, #1
	mov	r3, #2
	str	r7, [sp, #0]
	bl	masp_hal_sp_hacc_dec
.LVL12:
.L8:
	.loc 1 136 0 discriminator 1
	cmp	r4, #0
	beq	.L9
	mov	r3, #0
.LVL13:
.L10:
	.loc 1 138 0
	ldrb	r2, [r0, r3]	@ zero_extendqisi2
	strb	r2, [r5, r3]
	.loc 1 136 0 discriminator 2
	add	r3, r3, #1
.LVL14:
	.loc 1 136 0 is_stmt 0 discriminator 1
	cmp	r3, r4
	bne	.L10
.LVL15:
.L9:
	.loc 1 142 0 is_stmt 1
	bl	osal_hacc_unlock
.LVL16:
	.loc 1 153 0
	sub	sp, fp, #32
	ldmfd	sp, {r4, r5, r6, r7, r8, fp, sp, pc}
.LVL17:
.L16:
	.loc 1 113 0
	cmp	r7, #1
	.loc 1 130 0
	movne	r6, #24576
	.loc 1 113 0
	beq	.L17
.LVL18:
.L5:
	.loc 1 148 0
	bl	osal_hacc_unlock
	.loc 1 150 0 discriminator 1
	ldr	r1, .L18
	mov	r2, r6
	ldr	r0, .L18+4
	bl	printk
	.loc 1 151 0 discriminator 1
#APP
@ 151 "/alps/kernel/mediatek/platform/mt6582/kernel/drivers/masp/mach/hacc_lib.c" 1
	.word 0xe7f001f2
@ 0 "" 2
.LVL19:
.L17:
	.loc 1 119 0
	mov	r0, r8
.LVL20:
	mov	r1, r4
	mov	r2, r7
	mov	r3, #2
	str	r6, [sp, #0]
	bl	masp_hal_sp_hacc_enc
.LVL21:
	.loc 1 120 0
	b	.L8
.L19:
	.align	2
.L18:
	.word	.LC1
	.word	.LC0
.LFE201:
	.size	masp_hal_secure_algo, .-masp_hal_secure_algo
	.section	.rodata.str1.4,"aMS",%progbits,1
	.align	2
.LC0:
	.ascii	"[%s] masp_hal_secure_algo error (0x%x)\012\000"
.LC1:
	.ascii	"CE\000"
	.section	.debug_frame,"",%progbits
.Lframe0:
	.4byte	.LECIE0-.LSCIE0
.LSCIE0:
	.4byte	0xffffffff
	.byte	0x1
	.ascii	"\000"
	.uleb128 0x1
	.sleb128 -4
	.byte	0xe
	.byte	0xc
	.uleb128 0xd
	.uleb128 0
	.align	2
.LECIE0:
.LSFDE0:
	.4byte	.LEFDE0-.LASFDE0
.LASFDE0:
	.4byte	.Lframe0
	.4byte	.LFB199
	.4byte	.LFE199-.LFB199
	.byte	0x4
	.4byte	.LCFI0-.LFB199
	.byte	0xd
	.uleb128 0xc
	.byte	0x4
	.4byte	.LCFI1-.LCFI0
	.byte	0x8e
	.uleb128 0x2
	.byte	0x8d
	.uleb128 0x3
	.byte	0x8b
	.uleb128 0x4
	.byte	0x4
	.4byte	.LCFI2-.LCFI1
	.byte	0xc
	.uleb128 0xb
	.uleb128 0x4
	.align	2
.LEFDE0:
.LSFDE2:
	.4byte	.LEFDE2-.LASFDE2
.LASFDE2:
	.4byte	.Lframe0
	.4byte	.LFB200
	.4byte	.LFE200-.LFB200
	.byte	0x4
	.4byte	.LCFI3-.LFB200
	.byte	0xd
	.uleb128 0xc
	.byte	0x4
	.4byte	.LCFI4-.LCFI3
	.byte	0x8e
	.uleb128 0x2
	.byte	0x8d
	.uleb128 0x3
	.byte	0x8b
	.uleb128 0x4
	.byte	0x4
	.4byte	.LCFI5-.LCFI4
	.byte	0xc
	.uleb128 0xb
	.uleb128 0x4
	.align	2
.LEFDE2:
.LSFDE4:
	.4byte	.LEFDE4-.LASFDE4
.LASFDE4:
	.4byte	.Lframe0
	.4byte	.LFB201
	.4byte	.LFE201-.LFB201
	.byte	0x4
	.4byte	.LCFI6-.LFB201
	.byte	0xd
	.uleb128 0xc
	.byte	0x4
	.4byte	.LCFI7-.LCFI6
	.byte	0x8e
	.uleb128 0x2
	.byte	0x8d
	.uleb128 0x3
	.byte	0x8b
	.uleb128 0x4
	.byte	0x88
	.uleb128 0x5
	.byte	0x87
	.uleb128 0x6
	.byte	0x86
	.uleb128 0x7
	.byte	0x85
	.uleb128 0x8
	.byte	0x84
	.uleb128 0x9
	.byte	0x4
	.4byte	.LCFI8-.LCFI7
	.byte	0xc
	.uleb128 0xb
	.uleb128 0x4
	.align	2
.LEFDE4:
	.text
.Letext0:
	.file 2 "/alps/kernel/mediatek/platform/mt6582/kernel/drivers/masp/asf/asf_inc/sec_typedef.h"
	.file 3 "/alps/kernel/arch/arm/include/asm/hwcap.h"
	.file 4 "/alps/kernel/include/linux/printk.h"
	.file 5 "/alps/kernel/include/linux/kernel.h"
	.file 6 "/alps/kernel/../mediatek/kernel/include/mach/mt_sec_hal.h"
	.section	.debug_info,"",%progbits
.Ldebug_info0:
	.4byte	0x211
	.2byte	0x2
	.4byte	.Ldebug_abbrev0
	.byte	0x4
	.uleb128 0x1
	.4byte	.LASF28
	.byte	0x1
	.4byte	.LASF29
	.4byte	.LASF30
	.4byte	.Ltext0
	.4byte	.Letext0
	.4byte	.Ldebug_line0
	.4byte	.Ldebug_pubnames0
	.4byte	.Ldebug_pubtypes0
	.uleb128 0x2
	.byte	0x4
	.byte	0x7
	.4byte	.LASF0
	.uleb128 0x2
	.byte	0x4
	.byte	0x7
	.4byte	.LASF1
	.uleb128 0x2
	.byte	0x1
	.byte	0x8
	.4byte	.LASF2
	.uleb128 0x2
	.byte	0x1
	.byte	0x6
	.4byte	.LASF3
	.uleb128 0x2
	.byte	0x1
	.byte	0x8
	.4byte	.LASF4
	.uleb128 0x2
	.byte	0x2
	.byte	0x5
	.4byte	.LASF5
	.uleb128 0x2
	.byte	0x2
	.byte	0x7
	.4byte	.LASF6
	.uleb128 0x3
	.byte	0x4
	.byte	0x5
	.ascii	"int\000"
	.uleb128 0x2
	.byte	0x8
	.byte	0x5
	.4byte	.LASF7
	.uleb128 0x2
	.byte	0x8
	.byte	0x7
	.4byte	.LASF8
	.uleb128 0x2
	.byte	0x4
	.byte	0x5
	.4byte	.LASF9
	.uleb128 0x2
	.byte	0x1
	.byte	0x2
	.4byte	.LASF10
	.uleb128 0x4
	.byte	0x4
	.4byte	0x49
	.uleb128 0x2
	.byte	0x4
	.byte	0x4
	.4byte	.LASF11
	.uleb128 0x5
	.byte	0x4
	.byte	0x6
	.byte	0x12
	.4byte	0xa9
	.uleb128 0x6
	.4byte	.LASF12
	.sleb128 0
	.uleb128 0x6
	.4byte	.LASF13
	.sleb128 1
	.uleb128 0x6
	.4byte	.LASF14
	.sleb128 2
	.byte	0
	.uleb128 0x7
	.4byte	.LASF15
	.byte	0x2
	.byte	0x32
	.4byte	0x34
	.uleb128 0x7
	.4byte	.LASF16
	.byte	0x2
	.byte	0x33
	.4byte	0x49
	.uleb128 0x8
	.byte	0x1
	.4byte	.LASF17
	.byte	0x1
	.byte	0x3b
	.byte	0x1
	.4byte	0x49
	.4byte	.LFB199
	.4byte	.LFE199
	.4byte	.LLST0
	.4byte	0xe9
	.uleb128 0x9
	.ascii	"ret\000"
	.byte	0x1
	.byte	0x3d
	.4byte	0x7a
	.byte	0x1
	.byte	0
	.uleb128 0x8
	.byte	0x1
	.4byte	.LASF18
	.byte	0x1
	.byte	0x45
	.byte	0x1
	.4byte	0x49
	.4byte	.LFB200
	.4byte	.LFE200
	.4byte	.LLST1
	.4byte	0x113
	.uleb128 0x9
	.ascii	"ret\000"
	.byte	0x1
	.byte	0x47
	.4byte	0x7a
	.byte	0x1
	.byte	0
	.uleb128 0xa
	.byte	0x1
	.4byte	.LASF31
	.byte	0x1
	.byte	0x56
	.byte	0x1
	.4byte	.LFB201
	.4byte	.LFE201
	.4byte	.LLST2
	.4byte	0x1cb
	.uleb128 0xb
	.4byte	.LASF19
	.byte	0x1
	.byte	0x56
	.4byte	0x49
	.4byte	.LLST3
	.uleb128 0xb
	.4byte	.LASF20
	.byte	0x1
	.byte	0x56
	.4byte	0x34
	.4byte	.LLST4
	.uleb128 0xb
	.4byte	.LASF21
	.byte	0x1
	.byte	0x56
	.4byte	0x34
	.4byte	.LLST5
	.uleb128 0xb
	.4byte	.LASF22
	.byte	0x1
	.byte	0x56
	.4byte	0x81
	.4byte	.LLST6
	.uleb128 0xc
	.4byte	.LASF23
	.byte	0x1
	.byte	0x56
	.4byte	0x81
	.byte	0x2
	.byte	0x91
	.sleb128 0
	.uleb128 0xd
	.ascii	"err\000"
	.byte	0x1
	.byte	0x58
	.4byte	0xa9
	.4byte	.LLST7
	.uleb128 0xd
	.ascii	"src\000"
	.byte	0x1
	.byte	0x59
	.4byte	0x1cb
	.4byte	.LLST8
	.uleb128 0xd
	.ascii	"dst\000"
	.byte	0x1
	.byte	0x59
	.4byte	0x1cb
	.4byte	.LLST9
	.uleb128 0xe
	.4byte	.LASF24
	.byte	0x1
	.byte	0x59
	.4byte	0x1cb
	.4byte	.LLST10
	.uleb128 0xd
	.ascii	"i\000"
	.byte	0x1
	.byte	0x5a
	.4byte	0xa9
	.4byte	.LLST11
	.uleb128 0xf
	.4byte	.LASF32
	.byte	0x1
	.byte	0x92
	.4byte	.L5
	.byte	0
	.uleb128 0x4
	.byte	0x4
	.4byte	0xb4
	.uleb128 0x10
	.4byte	.LASF25
	.byte	0x3
	.byte	0xc
	.4byte	0x34
	.byte	0x1
	.byte	0x1
	.uleb128 0x11
	.4byte	0x5e
	.4byte	0x1e9
	.uleb128 0x12
	.byte	0
	.uleb128 0x10
	.4byte	.LASF26
	.byte	0x4
	.byte	0x22
	.4byte	0x1de
	.byte	0x1
	.byte	0x1
	.uleb128 0x11
	.4byte	0x3b
	.4byte	0x201
	.uleb128 0x12
	.byte	0
	.uleb128 0x13
	.4byte	.LASF27
	.byte	0x5
	.2byte	0x1af
	.4byte	0x20f
	.byte	0x1
	.byte	0x1
	.uleb128 0x14
	.4byte	0x1f6
	.byte	0
	.section	.debug_abbrev,"",%progbits
.Ldebug_abbrev0:
	.uleb128 0x1
	.uleb128 0x11
	.byte	0x1
	.uleb128 0x25
	.uleb128 0xe
	.uleb128 0x13
	.uleb128 0xb
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x1b
	.uleb128 0xe
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x1
	.uleb128 0x10
	.uleb128 0x6
	.uleb128 0x2134
	.uleb128 0x6
	.uleb128 0x2135
	.uleb128 0x6
	.byte	0
	.byte	0
	.uleb128 0x2
	.uleb128 0x24
	.byte	0
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3e
	.uleb128 0xb
	.uleb128 0x3
	.uleb128 0xe
	.byte	0
	.byte	0
	.uleb128 0x3
	.uleb128 0x24
	.byte	0
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3e
	.uleb128 0xb
	.uleb128 0x3
	.uleb128 0x8
	.byte	0
	.byte	0
	.uleb128 0x4
	.uleb128 0xf
	.byte	0
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x5
	.uleb128 0x4
	.byte	0x1
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x6
	.uleb128 0x28
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x1c
	.uleb128 0xd
	.byte	0
	.byte	0
	.uleb128 0x7
	.uleb128 0x16
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x8
	.uleb128 0x2e
	.byte	0x1
	.uleb128 0x3f
	.uleb128 0xc
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x27
	.uleb128 0xc
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x1
	.uleb128 0x40
	.uleb128 0x6
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x9
	.uleb128 0x34
	.byte	0
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x1c
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0xa
	.uleb128 0x2e
	.byte	0x1
	.uleb128 0x3f
	.uleb128 0xc
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x27
	.uleb128 0xc
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x1
	.uleb128 0x40
	.uleb128 0x6
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0xb
	.uleb128 0x5
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2
	.uleb128 0x6
	.byte	0
	.byte	0
	.uleb128 0xc
	.uleb128 0x5
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2
	.uleb128 0xa
	.byte	0
	.byte	0
	.uleb128 0xd
	.uleb128 0x34
	.byte	0
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2
	.uleb128 0x6
	.byte	0
	.byte	0
	.uleb128 0xe
	.uleb128 0x34
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2
	.uleb128 0x6
	.byte	0
	.byte	0
	.uleb128 0xf
	.uleb128 0xa
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x11
	.uleb128 0x1
	.byte	0
	.byte	0
	.uleb128 0x10
	.uleb128 0x34
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x3f
	.uleb128 0xc
	.uleb128 0x3c
	.uleb128 0xc
	.byte	0
	.byte	0
	.uleb128 0x11
	.uleb128 0x1
	.byte	0x1
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x12
	.uleb128 0x21
	.byte	0
	.byte	0
	.byte	0
	.uleb128 0x13
	.uleb128 0x34
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0x5
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x3f
	.uleb128 0xc
	.uleb128 0x3c
	.uleb128 0xc
	.byte	0
	.byte	0
	.uleb128 0x14
	.uleb128 0x26
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.byte	0
	.section	.debug_loc,"",%progbits
.Ldebug_loc0:
.LLST0:
	.4byte	.LFB199-.Ltext0
	.4byte	.LCFI0-.Ltext0
	.2byte	0x2
	.byte	0x7d
	.sleb128 0
	.4byte	.LCFI0-.Ltext0
	.4byte	.LCFI2-.Ltext0
	.2byte	0x2
	.byte	0x7c
	.sleb128 0
	.4byte	.LCFI2-.Ltext0
	.4byte	.LFE199-.Ltext0
	.2byte	0x2
	.byte	0x7b
	.sleb128 4
	.4byte	0
	.4byte	0
.LLST1:
	.4byte	.LFB200-.Ltext0
	.4byte	.LCFI3-.Ltext0
	.2byte	0x2
	.byte	0x7d
	.sleb128 0
	.4byte	.LCFI3-.Ltext0
	.4byte	.LCFI5-.Ltext0
	.2byte	0x2
	.byte	0x7c
	.sleb128 0
	.4byte	.LCFI5-.Ltext0
	.4byte	.LFE200-.Ltext0
	.2byte	0x2
	.byte	0x7b
	.sleb128 4
	.4byte	0
	.4byte	0
.LLST2:
	.4byte	.LFB201-.Ltext0
	.4byte	.LCFI6-.Ltext0
	.2byte	0x2
	.byte	0x7d
	.sleb128 0
	.4byte	.LCFI6-.Ltext0
	.4byte	.LCFI8-.Ltext0
	.2byte	0x2
	.byte	0x7c
	.sleb128 0
	.4byte	.LCFI8-.Ltext0
	.4byte	.LFE201-.Ltext0
	.2byte	0x2
	.byte	0x7b
	.sleb128 4
	.4byte	0
	.4byte	0
.LLST3:
	.4byte	.LVL2-.Ltext0
	.4byte	.LVL4-.Ltext0
	.2byte	0x1
	.byte	0x50
	.4byte	0
	.4byte	0
.LLST4:
	.4byte	.LVL2-.Ltext0
	.4byte	.LVL4-.Ltext0
	.2byte	0x1
	.byte	0x51
	.4byte	.LVL4-.Ltext0
	.4byte	.LFE201-.Ltext0
	.2byte	0x1
	.byte	0x58
	.4byte	0
	.4byte	0
.LLST5:
	.4byte	.LVL2-.Ltext0
	.4byte	.LVL4-.Ltext0
	.2byte	0x1
	.byte	0x52
	.4byte	.LVL4-.Ltext0
	.4byte	.LFE201-.Ltext0
	.2byte	0x1
	.byte	0x54
	.4byte	0
	.4byte	0
.LLST6:
	.4byte	.LVL2-.Ltext0
	.4byte	.LVL4-.Ltext0
	.2byte	0x1
	.byte	0x53
	.4byte	.LVL4-.Ltext0
	.4byte	.LVL9-.Ltext0
	.2byte	0x1
	.byte	0x56
	.4byte	0
	.4byte	0
.LLST7:
	.4byte	.LVL5-.Ltext0
	.4byte	.LVL7-.Ltext0
	.2byte	0x1
	.byte	0x50
	.4byte	.LVL8-.Ltext0
	.4byte	.LVL11-.Ltext0
	.2byte	0x1
	.byte	0x50
	.4byte	.LVL11-.Ltext0
	.4byte	.LVL12-.Ltext0
	.2byte	0x1
	.byte	0x56
	.4byte	.LVL17-.Ltext0
	.4byte	.LVL18-.Ltext0
	.2byte	0x1
	.byte	0x50
	.4byte	.LVL18-.Ltext0
	.4byte	.LVL19-.Ltext0
	.2byte	0x1
	.byte	0x56
	.4byte	.LVL19-.Ltext0
	.4byte	.LVL20-.Ltext0
	.2byte	0x1
	.byte	0x50
	.4byte	0
	.4byte	0
.LLST8:
	.4byte	.LVL10-.Ltext0
	.4byte	.LVL18-.Ltext0
	.2byte	0x1
	.byte	0x58
	.4byte	.LVL19-.Ltext0
	.4byte	.LFE201-.Ltext0
	.2byte	0x1
	.byte	0x58
	.4byte	0
	.4byte	0
.LLST9:
	.4byte	.LVL10-.Ltext0
	.4byte	.LVL12-.Ltext0
	.2byte	0x2
	.byte	0x91
	.sleb128 0
	.4byte	.LVL12-.Ltext0
	.4byte	.LVL16-1-.Ltext0
	.2byte	0x1
	.byte	0x50
	.4byte	.LVL17-.Ltext0
	.4byte	.LVL18-.Ltext0
	.2byte	0x2
	.byte	0x91
	.sleb128 0
	.4byte	.LVL19-.Ltext0
	.4byte	.LVL21-.Ltext0
	.2byte	0x2
	.byte	0x91
	.sleb128 0
	.4byte	.LVL21-.Ltext0
	.4byte	.LFE201-.Ltext0
	.2byte	0x1
	.byte	0x50
	.4byte	0
	.4byte	0
.LLST10:
	.4byte	.LVL6-.Ltext0
	.4byte	.LVL9-.Ltext0
	.2byte	0x1
	.byte	0x56
	.4byte	0
	.4byte	0
.LLST11:
	.4byte	.LVL3-.Ltext0
	.4byte	.LVL13-.Ltext0
	.2byte	0x2
	.byte	0x30
	.byte	0x9f
	.4byte	.LVL14-.Ltext0
	.4byte	.LVL15-.Ltext0
	.2byte	0x1
	.byte	0x53
	.4byte	.LVL17-.Ltext0
	.4byte	.LFE201-.Ltext0
	.2byte	0x2
	.byte	0x30
	.byte	0x9f
	.4byte	0
	.4byte	0
	.section	.debug_pubnames,"",%progbits
.Ldebug_pubnames0:
	.4byte	0x65
	.2byte	0x2
	.4byte	.Ldebug_info0
	.4byte	0x215
	.4byte	0xbf
	.ascii	"masp_hal_secure_algo_init\000"
	.4byte	0xe9
	.ascii	"masp_hal_secure_algo_deinit\000"
	.4byte	0x113
	.ascii	"masp_hal_secure_algo\000"
	.4byte	0
	.section	.debug_pubtypes,"",%progbits
.Ldebug_pubtypes0:
	.4byte	0xf0
	.2byte	0x2
	.4byte	.Ldebug_info0
	.4byte	0x215
	.4byte	0x2d
	.ascii	"long unsigned int\000"
	.4byte	0x34
	.ascii	"unsigned int\000"
	.4byte	0x3b
	.ascii	"char\000"
	.4byte	0x42
	.ascii	"signed char\000"
	.4byte	0x49
	.ascii	"unsigned char\000"
	.4byte	0x50
	.ascii	"short int\000"
	.4byte	0x57
	.ascii	"short unsigned int\000"
	.4byte	0x5e
	.ascii	"int\000"
	.4byte	0x65
	.ascii	"long long int\000"
	.4byte	0x6c
	.ascii	"long long unsigned int\000"
	.4byte	0x73
	.ascii	"long int\000"
	.4byte	0x7a
	.ascii	"_Bool\000"
	.4byte	0x87
	.ascii	"float\000"
	.4byte	0xa9
	.ascii	"uint32\000"
	.4byte	0xb4
	.ascii	"uchar\000"
	.4byte	0
	.section	.debug_aranges,"",%progbits
	.4byte	0x1c
	.2byte	0x2
	.4byte	.Ldebug_info0
	.byte	0x4
	.byte	0
	.2byte	0
	.2byte	0
	.4byte	.Ltext0
	.4byte	.Letext0-.Ltext0
	.4byte	0
	.4byte	0
	.section	.debug_line,"",%progbits
.Ldebug_line0:
	.section	.debug_str,"MS",%progbits,1
.LASF29:
	.ascii	"ALPS_SW/T"
	.ascii	"RUNK/ALPS.JB2/alps/kernel/mediatek/platform/mt6582/"
	.ascii	"kernel/drivers/masp/mach/hacc_lib.c\000"
.LASF26:
	.ascii	"console_printk\000"
.LASF11:
	.ascii	"float\000"
.LASF4:
	.ascii	"unsigned char\000"
.LASF22:
	.ascii	"CustomSeed\000"
.LASF0:
	.ascii	"long unsigned int\000"
.LASF31:
	.ascii	"masp_hal_secure_algo\000"
.LASF6:
	.ascii	"short unsigned int\000"
.LASF28:
	.ascii	"GNU C 4.6.x-google 20120106 (prerelease)\000"
.LASF30:
	.ascii	"ALPS_SW/T"
	.ascii	"RUNK/ALPS.JB2/alps/kernel/out\000"
.LASF16:
	.ascii	"uchar\000"
.LASF23:
	.ascii	"ResText\000"
.LASF25:
	.ascii	"elf_hwcap\000"
.LASF21:
	.ascii	"ContentLen\000"
.LASF15:
	.ascii	"uint32\000"
.LASF17:
	.ascii	"masp_hal_secure_algo_init\000"
.LASF1:
	.ascii	"unsigned int\000"
.LASF8:
	.ascii	"long long unsigned int\000"
.LASF7:
	.ascii	"long long int\000"
.LASF2:
	.ascii	"char\000"
.LASF18:
	.ascii	"masp_hal_secure_algo_deinit\000"
.LASF5:
	.ascii	"short int\000"
.LASF27:
	.ascii	"hex_asc\000"
.LASF24:
	.ascii	"seed\000"
.LASF19:
	.ascii	"Direction\000"
.LASF9:
	.ascii	"long int\000"
.LASF20:
	.ascii	"ContentAddr\000"
.LASF3:
	.ascii	"signed char\000"
.LASF12:
	.ascii	"HACC_USER1\000"
.LASF13:
	.ascii	"HACC_USER2\000"
.LASF14:
	.ascii	"HACC_USER3\000"
.LASF10:
	.ascii	"_Bool\000"
.LASF32:
	.ascii	"_error\000"
	.ident	"GCC: (GNU) 4.6.x-google 20120106 (prerelease)"
	.section	.note.GNU-stack,"",%progbits
