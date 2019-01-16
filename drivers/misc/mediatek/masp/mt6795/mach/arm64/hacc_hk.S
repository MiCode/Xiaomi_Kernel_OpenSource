	.cpu generic+fp+simd
	.file	"hacc_hk.c"
	.text
.Ltext0:
	.cfi_sections	.debug_frame
	.align	2
	.global	HACC_V3_Init
	.type	HACC_V3_Init, %function
HACC_V3_Init:
.LFB444:
	.file 1 "dummy"
	.loc 1 63 0
	.cfi_startproc
.LVL0:
	stp	x29, x30, [sp, -48]!
	.cfi_def_cfa_offset 48
	.cfi_offset 29, -48
	.cfi_offset 30, -40
	add	x29, sp, 0
	.cfi_def_cfa_register 29
	str	x19, [sp,16]
	.cfi_offset 19, -32
	.loc 1 63 0
	and	w19, w0, (1 << 8) - 1
.LVL1:
	str	x1, [x29,40]
	.loc 1 73 0
	bl	masp_hal_secure_algo_init
.LVL2:
	.loc 1 105 0
	adrp	x2, hacc_base
	.loc 1 95 0
	mov	w3, 2
	.loc 1 126 0
	ldr	x1, [x29,40]
.LVL3:
	.loc 1 95 0
	cmp	w19, wzr
	mov	w6, 3
	adrp	x4, .LANCHOR0
	ldr	x5, [x2,#:lo12:hacc_base]
	csel	w6, w6, w3, ne
.LVL4:
	add	x4, x4, :lo12:.LANCHOR0
	.loc 1 139 0
	mov	w7, 1
	add	x8, x4, 48
	.loc 1 105 0
	str	wzr, [x5,32]
	.loc 1 115 0
	mov	w5, 16
	.loc 1 105 0
	ldr	x0, [x2,#:lo12:hacc_base]
	str	wzr, [x0,36]
	ldr	x0, [x2,#:lo12:hacc_base]
	str	wzr, [x0,40]
	ldr	x0, [x2,#:lo12:hacc_base]
	str	wzr, [x0,44]
	ldr	x0, [x2,#:lo12:hacc_base]
	str	wzr, [x0,48]
	ldr	x0, [x2,#:lo12:hacc_base]
	str	wzr, [x0,52]
	ldr	x0, [x2,#:lo12:hacc_base]
	str	wzr, [x0,56]
	ldr	x0, [x2,#:lo12:hacc_base]
	str	wzr, [x0,60]
	.loc 1 111 0
	ldr	x0, [x2,#:lo12:hacc_base]
	str	w3, [x0,4]
	.loc 1 115 0
	ldr	x0, [x2,#:lo12:hacc_base]
	str	w5, [x0,12]
	.loc 1 117 0
	ldr	x5, [x2,#:lo12:hacc_base]
	ldr	w0, [x5,12]
	orr	w0, w0, 256
	str	w0, [x5,12]
	.loc 1 120 0
	ldr	x0, [x2,#:lo12:hacc_base]
	str	w3, [x0,8]
	ldr	x0, [x2,#:lo12:hacc_base]
	.loc 1 126 0
	ldr	w3, [x1]
	str	w3, [x0,64]
	ldr	x0, [x2,#:lo12:hacc_base]
	ldr	w3, [x1,4]
	str	w3, [x0,68]
	ldr	x0, [x2,#:lo12:hacc_base]
	ldr	w3, [x1,8]
	str	w3, [x0,72]
	ldr	x0, [x2,#:lo12:hacc_base]
	ldr	w3, [x1,12]
	str	w3, [x0,76]
	ldr	x5, [x2,#:lo12:hacc_base]
.L4:
	.loc 1 136 0
	ldr	w0, [x4]
	str	w0, [x5,16]
	ldr	x0, [x2,#:lo12:hacc_base]
	ldr	w3, [x4,4]
	str	w3, [x0,20]
	ldr	x0, [x2,#:lo12:hacc_base]
	ldr	w3, [x4,8]
	str	w3, [x0,24]
	ldr	x0, [x2,#:lo12:hacc_base]
	ldr	w3, [x4,12]
	str	w3, [x0,28]
	.loc 1 139 0
	ldr	x0, [x2,#:lo12:hacc_base]
	str	w7, [x0,8]
	ldr	x5, [x2,#:lo12:hacc_base]
	add	x3, x5, 8
.L3:
	.loc 1 141 0 discriminator 1
	ldr	w0, [x3]
	tbz	x0, 15, .L3
	add	x4, x4, 16
	.loc 1 130 0 discriminator 1
	cmp	x4, x8
	bne	.L4
	.loc 1 150 0
	mov	w0, 2
	str	w0, [x3]
	ldr	x0, [x2,#:lo12:hacc_base]
	.loc 1 156 0
	ldr	w3, [x1]
	str	w3, [x0,64]
	ldr	x0, [x2,#:lo12:hacc_base]
	ldr	w3, [x1,4]
	str	w3, [x0,68]
	ldr	x0, [x2,#:lo12:hacc_base]
	ldr	w3, [x1,8]
	str	w3, [x0,72]
	ldr	x0, [x2,#:lo12:hacc_base]
	ldr	w1, [x1,12]
.LVL5:
	str	w1, [x0,76]
	.loc 1 160 0
	ldr	x0, [x2,#:lo12:hacc_base]
	str	w6, [x0,4]
	.loc 1 161 0
	ldr	x0, [x2,#:lo12:hacc_base]
	str	wzr, [x0,12]
	.loc 1 162 0
	ldr	x19, [sp,16]
	.cfi_restore 19
.LVL6:
	ldp	x29, x30, [sp], 48
	.cfi_restore 30
	.cfi_restore 29
	.cfi_def_cfa 31, 0
	ret
	.cfi_endproc
.LFE444:
	.size	HACC_V3_Init, .-HACC_V3_Init
	.align	2
	.global	HACC_V3_Run
	.type	HACC_V3_Run, %function
HACC_V3_Run:
.LFB445:
	.loc 1 165 0
	.cfi_startproc
.LVL7:
	.loc 1 172 0 discriminator 1
	mov	x6, x2
	adrp	x5, hacc_base
	.loc 1 180 0
	mov	w8, 1
	.loc 1 172 0 discriminator 1
	cbz	w1, .L10
.LVL8:
.L16:
	ldr	x3, [x5,#:lo12:hacc_base]
.LVL9:
	.loc 1 177 0
	ldr	w4, [x0]
	str	w4, [x3,16]
	ldr	x3, [x5,#:lo12:hacc_base]
	ldr	w4, [x0,4]
	str	w4, [x3,20]
	ldr	x3, [x5,#:lo12:hacc_base]
	ldr	w4, [x0,8]
	str	w4, [x3,24]
	ldr	x3, [x5,#:lo12:hacc_base]
	ldr	w4, [x0,12]
	str	w4, [x3,28]
	.loc 1 180 0
	ldr	x3, [x5,#:lo12:hacc_base]
	str	w8, [x3,8]
	ldr	x7, [x5,#:lo12:hacc_base]
	add	x4, x7, 8
.L12:
	.loc 1 182 0 discriminator 1
	ldr	w3, [x4]
	tbz	x3, 15, .L12
	.loc 1 172 0 discriminator 2
	add	x6, x6, 16
	.loc 1 186 0
	ldr	w3, [x7,80]
	.loc 1 172 0 discriminator 2
	add	x0, x0, 16
	.loc 1 186 0
	str	w3, [x6,-16]
	ldr	x3, [x5,#:lo12:hacc_base]
	ldr	w3, [x3,84]
	str	w3, [x6,-12]
	ldr	x3, [x5,#:lo12:hacc_base]
	ldr	w4, [x3,88]
	str	w4, [x6,-8]
	ldr	w3, [x3,92]
	str	w3, [x6,-4]
.LVL10:
	sub	w3, w6, w2
.LVL11:
	.loc 1 172 0 discriminator 1
	cmp	w1, w3
	bhi	.L16
.L10:
	.loc 1 189 0
	ret
	.cfi_endproc
.LFE445:
	.size	HACC_V3_Run, .-HACC_V3_Run
	.align	2
	.global	HACC_V3_Terminate
	.type	HACC_V3_Terminate, %function
HACC_V3_Terminate:
.LFB446:
	.loc 1 192 0
	.cfi_startproc
	.loc 1 196 0
	adrp	x0, hacc_base
	mov	w2, 2
	.loc 1 192 0
	stp	x29, x30, [sp, -16]!
	.cfi_def_cfa_offset 16
	.cfi_offset 29, -16
	.cfi_offset 30, -8
	add	x29, sp, 0
	.cfi_def_cfa_register 29
	.loc 1 196 0
	ldr	x1, [x0,#:lo12:hacc_base]
	str	w2, [x1,8]
	ldr	x1, [x0,#:lo12:hacc_base]
	.loc 1 204 0
	str	wzr, [x1,32]
	ldr	x1, [x0,#:lo12:hacc_base]
	str	wzr, [x1,36]
	ldr	x1, [x0,#:lo12:hacc_base]
	str	wzr, [x1,40]
	ldr	x1, [x0,#:lo12:hacc_base]
	str	wzr, [x1,44]
	ldr	x1, [x0,#:lo12:hacc_base]
	str	wzr, [x1,48]
	ldr	x1, [x0,#:lo12:hacc_base]
	str	wzr, [x1,52]
	ldr	x1, [x0,#:lo12:hacc_base]
	str	wzr, [x1,56]
	ldr	x0, [x0,#:lo12:hacc_base]
	str	wzr, [x0,60]
	.loc 1 207 0
	bl	masp_hal_secure_algo_deinit
.LVL12:
	.loc 1 208 0
	ldp	x29, x30, [sp], 16
	.cfi_restore 30
	.cfi_restore 29
	.cfi_def_cfa 31, 0
	ret
	.cfi_endproc
.LFE446:
	.size	HACC_V3_Terminate, .-HACC_V3_Terminate
	.section	.rodata
	.align	3
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
	.text
.Letext0:
	.file 2 "dummy"
	.file 3 "dummy"
	.file 4 "dummy"
	.file 5 "dummy"
	.file 6 "dummy"
	.file 7 "dummy"
	.file 8 "dummy"
	.file 9 "dummy"
	.file 10 "dummy"
	.file 11 "dummy"
	.file 12 "dummy"
	.section	.debug_info,"",%progbits
.Ldebug_info0:
	.4byte	0x2bb
	.2byte	0x4
	.4byte	.Ldebug_abbrev0
	.byte	0x8
	.uleb128 0x1
	.4byte	.LASF38
	.byte	0x1
	.4byte	.LASF39
	.4byte	.LASF40
	.8byte	.Ltext0
	.8byte	.Letext0-.Ltext0
	.4byte	.Ldebug_line0
	.uleb128 0x2
	.byte	0x8
	.byte	0x7
	.4byte	.LASF0
	.uleb128 0x2
	.byte	0x8
	.byte	0x7
	.4byte	.LASF1
	.uleb128 0x2
	.byte	0x1
	.byte	0x8
	.4byte	.LASF2
	.uleb128 0x2
	.byte	0x4
	.byte	0x7
	.4byte	.LASF3
	.uleb128 0x2
	.byte	0x1
	.byte	0x6
	.4byte	.LASF4
	.uleb128 0x2
	.byte	0x1
	.byte	0x8
	.4byte	.LASF5
	.uleb128 0x2
	.byte	0x2
	.byte	0x5
	.4byte	.LASF6
	.uleb128 0x2
	.byte	0x2
	.byte	0x7
	.4byte	.LASF7
	.uleb128 0x3
	.byte	0x4
	.byte	0x5
	.string	"int"
	.uleb128 0x2
	.byte	0x8
	.byte	0x5
	.4byte	.LASF8
	.uleb128 0x2
	.byte	0x8
	.byte	0x7
	.4byte	.LASF9
	.uleb128 0x4
	.string	"u64"
	.byte	0xb
	.byte	0x19
	.4byte	0x73
	.uleb128 0x2
	.byte	0x8
	.byte	0x5
	.4byte	.LASF10
	.uleb128 0x2
	.byte	0x1
	.byte	0x2
	.4byte	.LASF11
	.uleb128 0x5
	.4byte	.LASF12
	.byte	0x2
	.byte	0xa2
	.4byte	0x7a
	.uleb128 0x2
	.byte	0x10
	.byte	0x7
	.4byte	.LASF13
	.uleb128 0x6
	.byte	0x4
	.byte	0xc
	.2byte	0x136
	.4byte	0xc7
	.uleb128 0x7
	.4byte	.LASF14
	.sleb128 0
	.uleb128 0x7
	.4byte	.LASF15
	.sleb128 1
	.uleb128 0x7
	.4byte	.LASF16
	.sleb128 2
	.uleb128 0x7
	.4byte	.LASF17
	.sleb128 3
	.byte	0
	.uleb128 0x5
	.4byte	.LASF18
	.byte	0x3
	.byte	0xe
	.4byte	0x42
	.uleb128 0x2
	.byte	0x4
	.byte	0x4
	.4byte	.LASF19
	.uleb128 0x8
	.4byte	.LASF22
	.byte	0x1
	.byte	0x3e
	.8byte	.LFB444
	.8byte	.LFE444-.LFB444
	.uleb128 0x1
	.byte	0x9c
	.4byte	0x14b
	.uleb128 0x9
	.4byte	.LASF20
	.byte	0x1
	.byte	0x3e
	.4byte	0x8c
	.4byte	.LLST0
	.uleb128 0x9
	.4byte	.LASF21
	.byte	0x1
	.byte	0x3e
	.4byte	0x14b
	.4byte	.LLST1
	.uleb128 0xa
	.string	"i"
	.byte	0x1
	.byte	0x41
	.4byte	0xc7
	.uleb128 0xa
	.string	"j"
	.byte	0x1
	.byte	0x41
	.4byte	0xc7
	.uleb128 0xa
	.string	"p1"
	.byte	0x1
	.byte	0x42
	.4byte	0x14b
	.uleb128 0xb
	.4byte	.LASF27
	.byte	0x1
	.byte	0x44
	.4byte	0xc7
	.uleb128 0x1
	.byte	0x56
	.uleb128 0xc
	.8byte	.LVL2
	.4byte	0x2a8
	.byte	0
	.uleb128 0xd
	.byte	0x8
	.4byte	0x151
	.uleb128 0xe
	.4byte	0xc7
	.uleb128 0x8
	.4byte	.LASF23
	.byte	0x1
	.byte	0xa4
	.8byte	.LFB445
	.8byte	.LFE445-.LFB445
	.uleb128 0x1
	.byte	0x9c
	.4byte	0x1be
	.uleb128 0xf
	.4byte	.LASF24
	.byte	0x1
	.byte	0xa4
	.4byte	0x1be
	.uleb128 0x1
	.byte	0x50
	.uleb128 0xf
	.4byte	.LASF25
	.byte	0x1
	.byte	0xa4
	.4byte	0xc7
	.uleb128 0x1
	.byte	0x51
	.uleb128 0x9
	.4byte	.LASF26
	.byte	0x1
	.byte	0xa4
	.4byte	0x1be
	.4byte	.LLST2
	.uleb128 0x10
	.string	"i"
	.byte	0x1
	.byte	0xa6
	.4byte	0xc7
	.4byte	.LLST3
	.uleb128 0xa
	.string	"j"
	.byte	0x1
	.byte	0xa6
	.4byte	0xc7
	.uleb128 0xa
	.string	"len"
	.byte	0x1
	.byte	0xa9
	.4byte	0xc7
	.byte	0
	.uleb128 0xd
	.byte	0x8
	.4byte	0x1c4
	.uleb128 0x11
	.4byte	0xc7
	.uleb128 0x8
	.4byte	.LASF28
	.byte	0x1
	.byte	0xbf
	.8byte	.LFB446
	.8byte	.LFE446-.LFB446
	.uleb128 0x1
	.byte	0x9c
	.4byte	0x1fd
	.uleb128 0xa
	.string	"i"
	.byte	0x1
	.byte	0xc1
	.4byte	0xc7
	.uleb128 0xc
	.8byte	.LVL12
	.4byte	0x2b3
	.byte	0
	.uleb128 0x12
	.4byte	0xc7
	.4byte	0x213
	.uleb128 0x13
	.4byte	0x34
	.byte	0x2
	.uleb128 0x13
	.4byte	0x34
	.byte	0x3
	.byte	0
	.uleb128 0xb
	.4byte	.LASF29
	.byte	0x1
	.byte	0x34
	.4byte	0x228
	.uleb128 0x9
	.byte	0x3
	.8byte	g_CFG_RANDOM_PATTERN
	.uleb128 0xe
	.4byte	0x1fd
	.uleb128 0x12
	.4byte	0x65
	.4byte	0x238
	.uleb128 0x14
	.byte	0
	.uleb128 0x15
	.4byte	.LASF30
	.byte	0x4
	.byte	0x24
	.4byte	0x22d
	.uleb128 0x12
	.4byte	0x3b
	.4byte	0x24e
	.uleb128 0x14
	.byte	0
	.uleb128 0x16
	.4byte	.LASF31
	.byte	0x5
	.2byte	0x1b2
	.4byte	0x25a
	.uleb128 0xe
	.4byte	0x243
	.uleb128 0x15
	.4byte	.LASF32
	.byte	0x6
	.byte	0x61
	.4byte	0x93
	.uleb128 0x12
	.4byte	0x2d
	.4byte	0x280
	.uleb128 0x13
	.4byte	0x34
	.byte	0x40
	.uleb128 0x13
	.4byte	0x34
	.byte	0
	.byte	0
	.uleb128 0x16
	.4byte	.LASF33
	.byte	0x7
	.2byte	0x2f9
	.4byte	0x28c
	.uleb128 0xe
	.4byte	0x26a
	.uleb128 0x16
	.4byte	.LASF34
	.byte	0x8
	.2byte	0x1db
	.4byte	0x2d
	.uleb128 0x15
	.4byte	.LASF35
	.byte	0x9
	.byte	0x34
	.4byte	0x73
	.uleb128 0x17
	.4byte	.LASF36
	.byte	0xa
	.byte	0x1c
	.4byte	0x50
	.uleb128 0x17
	.4byte	.LASF37
	.byte	0xa
	.byte	0x1d
	.4byte	0x50
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
	.uleb128 0x7
	.uleb128 0x10
	.uleb128 0x17
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
	.uleb128 0x8
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x5
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
	.uleb128 0x6
	.uleb128 0x4
	.byte	0x1
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0x5
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x7
	.uleb128 0x28
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x1c
	.uleb128 0xd
	.byte	0
	.byte	0
	.uleb128 0x8
	.uleb128 0x2e
	.byte	0x1
	.uleb128 0x3f
	.uleb128 0x19
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x27
	.uleb128 0x19
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x7
	.uleb128 0x40
	.uleb128 0x18
	.uleb128 0x2117
	.uleb128 0x19
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x9
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
	.uleb128 0x17
	.byte	0
	.byte	0
	.uleb128 0xa
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
	.byte	0
	.byte	0
	.uleb128 0xb
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
	.uleb128 0x18
	.byte	0
	.byte	0
	.uleb128 0xc
	.uleb128 0x4109
	.byte	0
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x31
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0xd
	.uleb128 0xf
	.byte	0
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0xe
	.uleb128 0x26
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0xf
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
	.uleb128 0x18
	.byte	0
	.byte	0
	.uleb128 0x10
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
	.uleb128 0x17
	.byte	0
	.byte	0
	.uleb128 0x11
	.uleb128 0x35
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x12
	.uleb128 0x1
	.byte	0x1
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x13
	.uleb128 0x21
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2f
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0x14
	.uleb128 0x21
	.byte	0
	.byte	0
	.byte	0
	.uleb128 0x15
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
	.uleb128 0x19
	.uleb128 0x3c
	.uleb128 0x19
	.byte	0
	.byte	0
	.uleb128 0x16
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
	.uleb128 0x19
	.uleb128 0x3c
	.uleb128 0x19
	.byte	0
	.byte	0
	.uleb128 0x17
	.uleb128 0x2e
	.byte	0
	.uleb128 0x3f
	.uleb128 0x19
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x27
	.uleb128 0x19
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x3c
	.uleb128 0x19
	.byte	0
	.byte	0
	.byte	0
	.section	.debug_loc,"",%progbits
.Ldebug_loc0:
.LLST0:
	.8byte	.LVL0-.Ltext0
	.8byte	.LVL1-.Ltext0
	.2byte	0x1
	.byte	0x50
	.8byte	.LVL1-.Ltext0
	.8byte	.LVL6-.Ltext0
	.2byte	0x1
	.byte	0x63
	.8byte	0
	.8byte	0
.LLST1:
	.8byte	.LVL0-.Ltext0
	.8byte	.LVL2-1-.Ltext0
	.2byte	0x1
	.byte	0x51
	.8byte	.LVL3-.Ltext0
	.8byte	.LVL5-.Ltext0
	.2byte	0x1
	.byte	0x51
	.8byte	0
	.8byte	0
.LLST2:
	.8byte	.LVL7-.Ltext0
	.8byte	.LVL10-.Ltext0
	.2byte	0x1
	.byte	0x52
	.8byte	0
	.8byte	0
.LLST3:
	.8byte	.LVL8-.Ltext0
	.8byte	.LVL9-.Ltext0
	.2byte	0x1
	.byte	0x53
	.8byte	.LVL11-.Ltext0
	.8byte	.LFE445-.Ltext0
	.2byte	0x1
	.byte	0x53
	.8byte	0
	.8byte	0
	.section	.debug_aranges,"",%progbits
	.4byte	0x2c
	.2byte	0x2
	.4byte	.Ldebug_info0
	.byte	0x8
	.byte	0
	.2byte	0
	.2byte	0
	.8byte	.Ltext0
	.8byte	.Letext0-.Ltext0
	.8byte	0
	.8byte	0
	.section	.debug_line,"",%progbits
.Ldebug_line0:
	.section	.debug_str,"MS",%progbits,1
.LASF13:
	.string	"__int128 unsigned"
.LASF30:
	.string	"console_printk"
.LASF32:
	.string	"memstart_addr"
.LASF27:
	.string	"acon_setting"
.LASF38:
	.ascii	"GNU C 4.9.x-google 20140827 (prerelease) -fpreprocessed -mbi"
	.ascii	"onic -mlittle-end"
	.string	"ian -mgeneral-regs-only -mabi=lp64 -g -O2 -fno-strict-aliasing -fno-common -fno-delete-null-pointer-checks -fno-pic -fno-stack-protector -fno-omit-frame-pointer -fno-optimize-sibling-calls -fno-var-tracking-assignments -fno-strict-overflow -fconserve-stack"
.LASF29:
	.string	"g_CFG_RANDOM_PATTERN"
.LASF40:
	.string	"dummy"
.LASF24:
	.string	"p_src"
.LASF34:
	.string	"zero_pfn"
.LASF33:
	.string	"cpu_bit_bitmap"
.LASF25:
	.string	"src_len"
.LASF19:
	.string	"float"
.LASF9:
	.string	"long long unsigned int"
.LASF5:
	.string	"unsigned char"
.LASF14:
	.string	"MM_FILEPAGES"
.LASF0:
	.string	"long unsigned int"
.LASF7:
	.string	"short unsigned int"
.LASF16:
	.string	"MM_SWAPENTS"
.LASF28:
	.string	"HACC_V3_Terminate"
.LASF21:
	.string	"g_AC_CFG"
.LASF23:
	.string	"HACC_V3_Run"
.LASF18:
	.string	"uint32"
.LASF36:
	.string	"masp_hal_secure_algo_init"
.LASF3:
	.string	"unsigned int"
.LASF2:
	.string	"char"
.LASF22:
	.string	"HACC_V3_Init"
.LASF26:
	.string	"p_dst"
.LASF17:
	.string	"NR_MM_COUNTERS"
.LASF12:
	.string	"phys_addr_t"
.LASF1:
	.string	"sizetype"
.LASF8:
	.string	"long long int"
.LASF39:
	.string	"dummy"
.LASF37:
	.string	"masp_hal_secure_algo_deinit"
.LASF6:
	.string	"short int"
.LASF31:
	.string	"hex_asc"
.LASF35:
	.string	"hacc_base"
.LASF10:
	.string	"long int"
.LASF20:
	.string	"encode"
.LASF4:
	.string	"signed char"
.LASF15:
	.string	"MM_ANONPAGES"
.LASF11:
	.string	"_Bool"
	.ident	"GCC: (GNU) 4.9.x-google 20140827 (prerelease)"
	.section	.note.GNU-stack,"",%progbits
