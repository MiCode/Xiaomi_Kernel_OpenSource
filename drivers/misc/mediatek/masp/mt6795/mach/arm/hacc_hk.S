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
	.file	"hacc_hk.c"
	.text
.Ltext0:
	.align	2
	.global	HACC_V3_Init
	.type	HACC_V3_Init, %function
HACC_V3_Init:
.LFB199:
	.file 1 "/alps/kernel/mediatek/platform/mt6582/kernel/drivers/masp/mach/hacc_hk.c"
	.loc 1 63 0
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 1, uses_anonymous_args = 0
.LVL0:
	mov	ip, sp
.LCFI0:
	stmfd	sp!, {r3, r4, r5, r6, fp, ip, lr, pc}
.LCFI1:
	sub	fp, ip, #4
.LCFI2:
	.loc 1 63 0
	mov	r4, r1
	mov	r5, r0
	.loc 1 73 0
	bl	masp_hal_secure_algo_init
.LVL1:
	.loc 1 105 0
	mov	r3, #40960
	movt	r3, 61440
	mov	r2, #0
	.loc 1 111 0
	mov	r6, #2
	.loc 1 91 0
	cmp	r5, r2
	.loc 1 105 0
	str	r2, [r3, #32]
	.loc 1 136 0
	mov	r1, r3
	.loc 1 105 0
	str	r2, [r3, #36]
	.loc 1 91 0
	moveq	r5, r6
	.loc 1 105 0
	str	r2, [r3, #40]
	.loc 1 91 0
	movne	r5, #3
.LVL2:
	.loc 1 105 0
	str	r2, [r3, #44]
.LVL3:
	.loc 1 139 0
	mov	ip, #1
	.loc 1 105 0
	str	r2, [r3, #48]
.LVL4:
	str	r2, [r3, #52]
.LVL5:
	str	r2, [r3, #56]
.LVL6:
	str	r2, [r3, #60]
.LVL7:
	.loc 1 115 0
	mov	r2, #16
	.loc 1 111 0
	str	r6, [r3, #4]
	.loc 1 115 0
	str	r2, [r3, #12]
	.loc 1 117 0
	ldr	r2, [r3, #12]
	.loc 1 133 0
	ldr	r0, .L9
	.loc 1 117 0
	orr	r2, r2, #256
	str	r2, [r3, #12]
	.loc 1 120 0
	str	r6, [r3, #8]
.LVL8:
	.loc 1 62 0
	add	r6, r0, #48
	.loc 1 126 0
	ldr	r2, [r4, #0]
	str	r2, [r3, #64]
.LVL9:
	ldr	r2, [r4, #4]
	str	r2, [r3, #68]
.LVL10:
	ldr	r2, [r4, #8]
	str	r2, [r3, #72]
.LVL11:
	ldr	r2, [r4, #12]
	str	r2, [r3, #76]
.LVL12:
.L4:
	.loc 1 136 0
	ldr	r3, [r0, #0]
	str	r3, [r1, #16]
.LVL13:
	ldr	r3, [r0, #4]
	str	r3, [r1, #20]
.LVL14:
	ldr	r3, [r0, #8]
	str	r3, [r1, #24]
.LVL15:
	ldr	r3, [r0, #12]
	str	r3, [r1, #28]
.LVL16:
	.loc 1 139 0
	str	ip, [r1, #8]
.L3:
	.loc 1 141 0 discriminator 1
	ldr	r2, [r1, #8]
	mov	r3, #40960
	movt	r3, 61440
	tst	r2, #32768
	beq	.L3
	add	r0, r0, #16
	.loc 1 130 0 discriminator 1
	cmp	r0, r6
	bne	.L4
	.loc 1 150 0
	mov	r2, #2
	str	r2, [r3, #8]
.LVL17:
	.loc 1 156 0
	ldr	r2, [r4, #0]
	str	r2, [r3, #64]
.LVL18:
	ldr	r2, [r4, #4]
	str	r2, [r3, #68]
.LVL19:
	ldr	r2, [r4, #8]
	str	r2, [r3, #72]
.LVL20:
	ldr	r2, [r4, #12]
	str	r2, [r3, #76]
.LVL21:
	.loc 1 161 0
	mov	r2, #0
	.loc 1 160 0
	str	r5, [r3, #4]
	.loc 1 161 0
	str	r2, [r3, #12]
	.loc 1 162 0
	ldmfd	sp, {r3, r4, r5, r6, fp, sp, pc}
.L10:
	.align	2
.L9:
	.word	.LANCHOR0
.LFE199:
	.size	HACC_V3_Init, .-HACC_V3_Init
	.align	2
	.global	HACC_V3_Run
	.type	HACC_V3_Run, %function
HACC_V3_Run:
.LFB200:
	.loc 1 165 0
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 1, uses_anonymous_args = 0
.LVL22:
	mov	ip, sp
.LCFI3:
	stmfd	sp!, {r4, r5, r6, r7, r8, fp, ip, lr, pc}
.LCFI4:
	sub	fp, ip, #4
.LCFI5:
.LVL23:
	.loc 1 172 0 discriminator 1
	cmp	r1, #0
.LVL24:
	ldmeqfd	sp, {r4, r5, r6, r7, r8, fp, sp, pc}
	.loc 1 177 0
	mov	ip, #40960
	movt	ip, 61440
	.loc 1 172 0 discriminator 1
	mov	r4, #0
	.loc 1 180 0
	mov	r7, #1
	.loc 1 182 0 discriminator 1
	mov	r6, ip
.LVL25:
.L12:
	.loc 1 177 0
	ldr	r8, [r0, r4]
	.loc 1 164 0
	add	r3, r0, r4
.LVL26:
	add	r5, r2, r4
	.loc 1 177 0
	str	r8, [ip, #16]
.LVL27:
	ldr	r8, [r3, #4]
	str	r8, [ip, #20]
.LVL28:
	ldr	r8, [r3, #8]
	str	r8, [ip, #24]
.LVL29:
	ldr	r3, [r3, #12]
	str	r3, [ip, #28]
.LVL30:
	.loc 1 180 0
	str	r7, [ip, #8]
.L14:
	.loc 1 182 0 discriminator 1
	ldr	r3, [ip, #8]
	tst	r3, #32768
	beq	.L14
.LVL31:
	.loc 1 186 0
	ldr	r3, [r6, #80]
	.loc 1 172 0
	add	r4, r4, #16
	.loc 1 172 0 is_stmt 0 discriminator 1
	cmp	r1, r4
	.loc 1 186 0 is_stmt 1
	str	r3, [r5, #0]
.LVL32:
	ldr	r3, [r6, #84]
	str	r3, [r5, #4]
.LVL33:
	ldr	r3, [r6, #88]
	str	r3, [r5, #8]
.LVL34:
	ldr	r3, [r6, #92]
	str	r3, [r5, #12]
.LVL35:
	.loc 1 172 0 discriminator 1
	bhi	.L12
	ldmfd	sp, {r4, r5, r6, r7, r8, fp, sp, pc}
.LFE200:
	.size	HACC_V3_Run, .-HACC_V3_Run
	.align	2
	.global	HACC_V3_Terminate
	.type	HACC_V3_Terminate, %function
HACC_V3_Terminate:
.LFB201:
	.loc 1 192 0
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
.LCFI6:
	stmfd	sp!, {fp, ip, lr, pc}
.LCFI7:
	sub	fp, ip, #4
.LCFI8:
	.loc 1 196 0
	mov	r3, #40960
	movt	r3, 61440
	.loc 1 204 0
	mov	r2, #0
	.loc 1 196 0
	mov	r1, #2
	str	r1, [r3, #8]
.LVL36:
	.loc 1 204 0
	str	r2, [r3, #32]
.LVL37:
	str	r2, [r3, #36]
.LVL38:
	str	r2, [r3, #40]
.LVL39:
	str	r2, [r3, #44]
.LVL40:
	str	r2, [r3, #48]
.LVL41:
	str	r2, [r3, #52]
.LVL42:
	str	r2, [r3, #56]
.LVL43:
	str	r2, [r3, #60]
.LVL44:
	.loc 1 207 0
	bl	masp_hal_secure_algo_deinit
	.loc 1 208 0
	ldmfd	sp, {fp, sp, pc}
.LFE201:
	.size	HACC_V3_Terminate, .-HACC_V3_Terminate
	.section	.rodata
	.align	2
.LANCHOR0 = . + 0
	.type	g_CFG_RANDOM_PATTERN, %object
	.size	g_CFG_RANDOM_PATTERN, 48
g_CFG_RANDOM_PATTERN:
	.word	759479152
	.word	-1488661977
	.word	-794196405
	.word	-2080390588
	.word	2127046251
	.word	1139281842
	.word	27669642
	.word	543685024
	.word	1414751237
	.word	-716790095
	.word	482602042
	.word	-566789501
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
	.byte	0x86
	.uleb128 0x5
	.byte	0x85
	.uleb128 0x6
	.byte	0x84
	.uleb128 0x7
	.byte	0x83
	.uleb128 0x8
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
	.byte	0x4
	.4byte	.LCFI8-.LCFI7
	.byte	0xc
	.uleb128 0xb
	.uleb128 0x4
	.align	2
.LEFDE4:
	.text
.Letext0:
	.file 2 "/alps/kernel/arch/arm/include/asm/hwcap.h"
	.file 3 "/alps/kernel/include/linux/printk.h"
	.file 4 "/alps/kernel/include/linux/kernel.h"
	.file 5 "/alps/kernel/mediatek/platform/mt6582/kernel/drivers/masp/asf/asf_inc/sec_typedef.h"
	.section	.debug_info,"",%progbits
.Ldebug_info0:
	.4byte	0x219
	.2byte	0x2
	.4byte	.Ldebug_abbrev0
	.byte	0x4
	.uleb128 0x1
	.4byte	.LASF25
	.byte	0x1
	.4byte	.LASF26
	.4byte	.LASF27
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
	.4byte	.LASF28
	.byte	0x5
	.byte	0x32
	.4byte	0x34
	.uleb128 0x2
	.byte	0x4
	.byte	0x4
	.4byte	.LASF11
	.uleb128 0x5
	.byte	0x1
	.4byte	.LASF15
	.byte	0x1
	.byte	0x3e
	.byte	0x1
	.4byte	.LFB199
	.4byte	.LFE199
	.4byte	.LLST0
	.4byte	0x102
	.uleb128 0x6
	.4byte	.LASF12
	.byte	0x1
	.byte	0x3e
	.4byte	0x7a
	.4byte	.LLST1
	.uleb128 0x6
	.4byte	.LASF13
	.byte	0x1
	.byte	0x3e
	.4byte	0x102
	.4byte	.LLST2
	.uleb128 0x7
	.ascii	"i\000"
	.byte	0x1
	.byte	0x41
	.4byte	0x81
	.4byte	.LLST3
	.uleb128 0x7
	.ascii	"j\000"
	.byte	0x1
	.byte	0x41
	.4byte	0x81
	.4byte	.LLST4
	.uleb128 0x7
	.ascii	"p1\000"
	.byte	0x1
	.byte	0x42
	.4byte	0x102
	.4byte	.LLST5
	.uleb128 0x8
	.4byte	.LASF14
	.byte	0x1
	.byte	0x44
	.4byte	0x81
	.4byte	.LLST6
	.byte	0
	.uleb128 0x9
	.byte	0x4
	.4byte	0x108
	.uleb128 0xa
	.4byte	0x81
	.uleb128 0x5
	.byte	0x1
	.4byte	.LASF16
	.byte	0x1
	.byte	0xa4
	.byte	0x1
	.4byte	.LFB200
	.4byte	.LFE200
	.4byte	.LLST7
	.4byte	0x17b
	.uleb128 0x6
	.4byte	.LASF17
	.byte	0x1
	.byte	0xa4
	.4byte	0x17b
	.4byte	.LLST8
	.uleb128 0x6
	.4byte	.LASF18
	.byte	0x1
	.byte	0xa4
	.4byte	0x81
	.4byte	.LLST9
	.uleb128 0x6
	.4byte	.LASF19
	.byte	0x1
	.byte	0xa4
	.4byte	0x17b
	.4byte	.LLST10
	.uleb128 0x7
	.ascii	"i\000"
	.byte	0x1
	.byte	0xa6
	.4byte	0x81
	.4byte	.LLST11
	.uleb128 0x7
	.ascii	"j\000"
	.byte	0x1
	.byte	0xa6
	.4byte	0x81
	.4byte	.LLST12
	.uleb128 0xb
	.ascii	"len\000"
	.byte	0x1
	.byte	0xa9
	.4byte	0x81
	.byte	0x1
	.byte	0x51
	.byte	0
	.uleb128 0x9
	.byte	0x4
	.4byte	0x181
	.uleb128 0xc
	.4byte	0x81
	.uleb128 0x5
	.byte	0x1
	.4byte	.LASF20
	.byte	0x1
	.byte	0xbf
	.byte	0x1
	.4byte	.LFB201
	.4byte	.LFE201
	.4byte	.LLST13
	.4byte	0x1ad
	.uleb128 0x7
	.ascii	"i\000"
	.byte	0x1
	.byte	0xc1
	.4byte	0x81
	.4byte	.LLST14
	.byte	0
	.uleb128 0xd
	.4byte	0x81
	.4byte	0x1c3
	.uleb128 0xe
	.4byte	0x34
	.byte	0x2
	.uleb128 0xe
	.4byte	0x34
	.byte	0x3
	.byte	0
	.uleb128 0xf
	.4byte	.LASF21
	.byte	0x1
	.byte	0x34
	.4byte	0x1d4
	.byte	0x5
	.byte	0x3
	.4byte	g_CFG_RANDOM_PATTERN
	.uleb128 0xa
	.4byte	0x1ad
	.uleb128 0x10
	.4byte	.LASF22
	.byte	0x2
	.byte	0xc
	.4byte	0x34
	.byte	0x1
	.byte	0x1
	.uleb128 0xd
	.4byte	0x5e
	.4byte	0x1f1
	.uleb128 0x11
	.byte	0
	.uleb128 0x10
	.4byte	.LASF23
	.byte	0x3
	.byte	0x22
	.4byte	0x1e6
	.byte	0x1
	.byte	0x1
	.uleb128 0xd
	.4byte	0x3b
	.4byte	0x209
	.uleb128 0x11
	.byte	0
	.uleb128 0x12
	.4byte	.LASF24
	.byte	0x4
	.2byte	0x1af
	.4byte	0x217
	.byte	0x1
	.byte	0x1
	.uleb128 0xa
	.4byte	0x1fe
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
	.uleb128 0x5
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
	.uleb128 0x6
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
	.uleb128 0x7
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
	.uleb128 0x8
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
	.uleb128 0x9
	.uleb128 0xf
	.byte	0
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0xa
	.uleb128 0x26
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0xb
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
	.uleb128 0xa
	.byte	0
	.byte	0
	.uleb128 0xc
	.uleb128 0x35
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0xd
	.uleb128 0x1
	.byte	0x1
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0xe
	.uleb128 0x21
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2f
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0xf
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
	.uleb128 0xa
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
	.uleb128 0x21
	.byte	0
	.byte	0
	.byte	0
	.uleb128 0x12
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
	.4byte	.LVL0-.Ltext0
	.4byte	.LVL1-1-.Ltext0
	.2byte	0x1
	.byte	0x50
	.4byte	0
	.4byte	0
.LLST2:
	.4byte	.LVL0-.Ltext0
	.4byte	.LVL1-1-.Ltext0
	.2byte	0x1
	.byte	0x51
	.4byte	.LVL1-1-.Ltext0
	.4byte	.LFE199-.Ltext0
	.2byte	0x1
	.byte	0x54
	.4byte	0
	.4byte	0
.LLST3:
	.4byte	.LVL2-.Ltext0
	.4byte	.LVL3-.Ltext0
	.2byte	0x2
	.byte	0x33
	.byte	0x9f
	.4byte	.LVL3-.Ltext0
	.4byte	.LVL4-.Ltext0
	.2byte	0x2
	.byte	0x34
	.byte	0x9f
	.4byte	.LVL4-.Ltext0
	.4byte	.LVL5-.Ltext0
	.2byte	0x2
	.byte	0x35
	.byte	0x9f
	.4byte	.LVL5-.Ltext0
	.4byte	.LVL6-.Ltext0
	.2byte	0x2
	.byte	0x36
	.byte	0x9f
	.4byte	.LVL6-.Ltext0
	.4byte	.LVL7-.Ltext0
	.2byte	0x2
	.byte	0x37
	.byte	0x9f
	.4byte	.LVL7-.Ltext0
	.4byte	.LVL8-.Ltext0
	.2byte	0x2
	.byte	0x38
	.byte	0x9f
	.4byte	.LVL8-.Ltext0
	.4byte	.LVL9-.Ltext0
	.2byte	0x2
	.byte	0x30
	.byte	0x9f
	.4byte	.LVL9-.Ltext0
	.4byte	.LVL10-.Ltext0
	.2byte	0x2
	.byte	0x31
	.byte	0x9f
	.4byte	.LVL10-.Ltext0
	.4byte	.LVL11-.Ltext0
	.2byte	0x2
	.byte	0x32
	.byte	0x9f
	.4byte	.LVL11-.Ltext0
	.4byte	.LVL12-.Ltext0
	.2byte	0x2
	.byte	0x33
	.byte	0x9f
	.4byte	.LVL17-.Ltext0
	.4byte	.LVL18-.Ltext0
	.2byte	0x2
	.byte	0x30
	.byte	0x9f
	.4byte	.LVL18-.Ltext0
	.4byte	.LVL19-.Ltext0
	.2byte	0x2
	.byte	0x31
	.byte	0x9f
	.4byte	.LVL19-.Ltext0
	.4byte	.LVL20-.Ltext0
	.2byte	0x2
	.byte	0x32
	.byte	0x9f
	.4byte	.LVL20-.Ltext0
	.4byte	.LVL21-.Ltext0
	.2byte	0x2
	.byte	0x33
	.byte	0x9f
	.4byte	.LVL21-.Ltext0
	.4byte	.LFE199-.Ltext0
	.2byte	0x2
	.byte	0x34
	.byte	0x9f
	.4byte	0
	.4byte	0
.LLST4:
	.4byte	.LVL12-.Ltext0
	.4byte	.LVL13-.Ltext0
	.2byte	0x2
	.byte	0x30
	.byte	0x9f
	.4byte	.LVL13-.Ltext0
	.4byte	.LVL14-.Ltext0
	.2byte	0x2
	.byte	0x31
	.byte	0x9f
	.4byte	.LVL14-.Ltext0
	.4byte	.LVL15-.Ltext0
	.2byte	0x2
	.byte	0x32
	.byte	0x9f
	.4byte	.LVL15-.Ltext0
	.4byte	.LVL16-.Ltext0
	.2byte	0x2
	.byte	0x33
	.byte	0x9f
	.4byte	.LVL16-.Ltext0
	.4byte	.LFE199-.Ltext0
	.2byte	0x2
	.byte	0x34
	.byte	0x9f
	.4byte	0
	.4byte	0
.LLST5:
	.4byte	.LVL8-.Ltext0
	.4byte	.LVL12-.Ltext0
	.2byte	0x1
	.byte	0x54
	.4byte	.LVL17-.Ltext0
	.4byte	.LFE199-.Ltext0
	.2byte	0x1
	.byte	0x54
	.4byte	0
	.4byte	0
.LLST6:
	.4byte	.LVL1-.Ltext0
	.4byte	.LVL2-.Ltext0
	.2byte	0x2
	.byte	0x32
	.byte	0x9f
	.4byte	.LVL2-.Ltext0
	.4byte	.LFE199-.Ltext0
	.2byte	0x1
	.byte	0x55
	.4byte	0
	.4byte	0
.LLST7:
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
.LLST8:
	.4byte	.LVL22-.Ltext0
	.4byte	.LVL25-.Ltext0
	.2byte	0x1
	.byte	0x50
	.4byte	0
	.4byte	0
.LLST9:
	.4byte	.LVL22-.Ltext0
	.4byte	.LVL24-.Ltext0
	.2byte	0x1
	.byte	0x51
	.4byte	.LVL25-.Ltext0
	.4byte	.LFE200-.Ltext0
	.2byte	0x1
	.byte	0x51
	.4byte	0
	.4byte	0
.LLST10:
	.4byte	.LVL22-.Ltext0
	.4byte	.LVL25-.Ltext0
	.2byte	0x1
	.byte	0x52
	.4byte	.LVL35-.Ltext0
	.4byte	.LFE200-.Ltext0
	.2byte	0x3
	.byte	0x75
	.sleb128 16
	.byte	0x9f
	.4byte	0
	.4byte	0
.LLST11:
	.4byte	.LVL23-.Ltext0
	.4byte	.LVL25-.Ltext0
	.2byte	0x2
	.byte	0x30
	.byte	0x9f
	.4byte	.LVL35-.Ltext0
	.4byte	.LFE200-.Ltext0
	.2byte	0x1
	.byte	0x54
	.4byte	0
	.4byte	0
.LLST12:
	.4byte	.LVL26-.Ltext0
	.4byte	.LVL27-.Ltext0
	.2byte	0x2
	.byte	0x30
	.byte	0x9f
	.4byte	.LVL27-.Ltext0
	.4byte	.LVL28-.Ltext0
	.2byte	0x2
	.byte	0x31
	.byte	0x9f
	.4byte	.LVL28-.Ltext0
	.4byte	.LVL29-.Ltext0
	.2byte	0x2
	.byte	0x32
	.byte	0x9f
	.4byte	.LVL29-.Ltext0
	.4byte	.LVL30-.Ltext0
	.2byte	0x2
	.byte	0x33
	.byte	0x9f
	.4byte	.LVL30-.Ltext0
	.4byte	.LVL31-.Ltext0
	.2byte	0x2
	.byte	0x34
	.byte	0x9f
	.4byte	.LVL31-.Ltext0
	.4byte	.LVL32-.Ltext0
	.2byte	0x2
	.byte	0x30
	.byte	0x9f
	.4byte	.LVL32-.Ltext0
	.4byte	.LVL33-.Ltext0
	.2byte	0x2
	.byte	0x31
	.byte	0x9f
	.4byte	.LVL33-.Ltext0
	.4byte	.LVL34-.Ltext0
	.2byte	0x2
	.byte	0x32
	.byte	0x9f
	.4byte	.LVL34-.Ltext0
	.4byte	.LVL35-.Ltext0
	.2byte	0x2
	.byte	0x33
	.byte	0x9f
	.4byte	.LVL35-.Ltext0
	.4byte	.LFE200-.Ltext0
	.2byte	0x2
	.byte	0x34
	.byte	0x9f
	.4byte	0
	.4byte	0
.LLST13:
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
.LLST14:
	.4byte	.LVL36-.Ltext0
	.4byte	.LVL37-.Ltext0
	.2byte	0x2
	.byte	0x30
	.byte	0x9f
	.4byte	.LVL37-.Ltext0
	.4byte	.LVL38-.Ltext0
	.2byte	0x2
	.byte	0x31
	.byte	0x9f
	.4byte	.LVL38-.Ltext0
	.4byte	.LVL39-.Ltext0
	.2byte	0x2
	.byte	0x32
	.byte	0x9f
	.4byte	.LVL39-.Ltext0
	.4byte	.LVL40-.Ltext0
	.2byte	0x2
	.byte	0x33
	.byte	0x9f
	.4byte	.LVL40-.Ltext0
	.4byte	.LVL41-.Ltext0
	.2byte	0x2
	.byte	0x34
	.byte	0x9f
	.4byte	.LVL41-.Ltext0
	.4byte	.LVL42-.Ltext0
	.2byte	0x2
	.byte	0x35
	.byte	0x9f
	.4byte	.LVL42-.Ltext0
	.4byte	.LVL43-.Ltext0
	.2byte	0x2
	.byte	0x36
	.byte	0x9f
	.4byte	.LVL43-.Ltext0
	.4byte	.LVL44-.Ltext0
	.2byte	0x2
	.byte	0x37
	.byte	0x9f
	.4byte	.LVL44-.Ltext0
	.4byte	.LFE201-.Ltext0
	.2byte	0x2
	.byte	0x38
	.byte	0x9f
	.4byte	0
	.4byte	0
	.section	.debug_pubnames,"",%progbits
.Ldebug_pubnames0:
	.4byte	0x5e
	.2byte	0x2
	.4byte	.Ldebug_info0
	.4byte	0x21d
	.4byte	0x93
	.ascii	"HACC_V3_Init\000"
	.4byte	0x10d
	.ascii	"HACC_V3_Run\000"
	.4byte	0x186
	.ascii	"HACC_V3_Terminate\000"
	.4byte	0x1c3
	.ascii	"g_CFG_RANDOM_PATTERN\000"
	.4byte	0
	.section	.debug_pubtypes,"",%progbits
.Ldebug_pubtypes0:
	.4byte	0xe6
	.2byte	0x2
	.4byte	.Ldebug_info0
	.4byte	0x21d
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
	.4byte	0x81
	.ascii	"uint32\000"
	.4byte	0x8c
	.ascii	"float\000"
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
.LASF23:
	.ascii	"console_printk\000"
.LASF14:
	.ascii	"acon_setting\000"
.LASF17:
	.ascii	"p_src\000"
.LASF18:
	.ascii	"src_len\000"
.LASF11:
	.ascii	"float\000"
.LASF13:
	.ascii	"g_AC_CFG\000"
.LASF4:
	.ascii	"unsigned char\000"
.LASF0:
	.ascii	"long unsigned int\000"
.LASF6:
	.ascii	"short unsigned int\000"
.LASF25:
	.ascii	"GNU C 4.6.x-google 20120106 (prerelease)\000"
.LASF20:
	.ascii	"HACC_V3_Terminate\000"
.LASF22:
	.ascii	"elf_hwcap\000"
.LASF16:
	.ascii	"HACC_V3_Run\000"
.LASF28:
	.ascii	"uint32\000"
.LASF1:
	.ascii	"unsigned int\000"
.LASF8:
	.ascii	"long long unsigned int\000"
.LASF15:
	.ascii	"HACC_V3_Init\000"
.LASF19:
	.ascii	"p_dst\000"
.LASF26:
	.ascii	"ALPS_SW/T"
	.ascii	"RUNK/ALPS.JB2/alps/kernel/mediatek/platform/mt6582/"
	.ascii	"kernel/drivers/masp/mach/hacc_hk.c\000"
.LASF7:
	.ascii	"long long int\000"
.LASF2:
	.ascii	"char\000"
.LASF21:
	.ascii	"g_CFG_RANDOM_PATTERN\000"
.LASF5:
	.ascii	"short int\000"
.LASF24:
	.ascii	"hex_asc\000"
.LASF9:
	.ascii	"long int\000"
.LASF12:
	.ascii	"encode\000"
.LASF3:
	.ascii	"signed char\000"
.LASF27:
	.ascii	"ALPS_SW/T"
	.ascii	"RUNK/ALPS.JB2/alps/kernel/out\000"
.LASF10:
	.ascii	"_Bool\000"
	.ident	"GCC: (GNU) 4.6.x-google 20120106 (prerelease)"
	.section	.note.GNU-stack,"",%progbits
