/*
 * -------------------------------------------------------------------------
 * <linux_root>/arch/sh/kernel/cpu/sh4/suspend-core.S
 * -------------------------------------------------------------------------
 * Copyright (C) 2008  STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 *
 * ------------------------------------------------------------------------- */

#include <linux/linkage.h>
#include <asm/cpu-sh4/mmu_context.h>
#include <asm/cpu-sh4/suspend.h>

/*
 * Some register are dedicated for special purpose
 */
#define IOREGS_BASE		r14
#define ITABLE_ADDRESS		r13
#define DTABLE_ADDRESS		r12
#define DELAY_REG		r11

#define OFFSET_IOBASE		0x0
#define OFFSET_LPJ		0x4
#define OFFSET_DTABLE		0x8
#define OFFSET_DTABLE_SIZE	0xc
#define REG_INSTR		r5
#define REG_INSTR_END		r6


#define JUMPER()		bra 201f;	\
				 nop;		\
			200:	bra 200f;	\
				 nop;	;	\
			201:

#undef ENTRY
#define ENTRY(name, align)	\
  .balign align;		\
  .globl name;			\
  name:

.text
ENTRY(sh4_suspend, 32)		! to be icache aligned
	bra 200f		! start the jump sequence
	 nop
sh4_really_suspend:
	mov.l   r14, @-r15
	mov.l   r13, @-r15
	mov.l   r12, @-r15
	mov.l   r11, @-r15
	mov.l   r10, @-r15
	mov.l   r9,  @-r15
	mov.l   r8,  @-r15
	sts.l	pr,  @-r15	! save the pr (we can call other function)
	stc.l	sr,  @-r15
	stc	vbr, r0

	JUMPER()

	mov.l	r0,  @-r15	! save the original vbr on the stack

	mov.l	@(OFFSET_IOBASE, r4), IOREGS_BASE	! save ioregs address
	mov.l	@(OFFSET_LPJ, r4),    DELAY_REG
	mov.l	@(OFFSET_DTABLE, r4), DTABLE_ADDRESS
	mov	REG_INSTR,  	      ITABLE_ADDRESS	! the instruction table!

/*
 *	runs the suspend iteration tables
 */
	bsr	do_decode
	 nop

	mova	vbr_base_suspend, r0	! this mova isn't a problem
					! because vbr_base_suspend is
					! 4 bytes alligned
	ldc	r0, vbr			! install the wakeup_interrupt
	mov	#0x3c, r1

	JUMPER()

	shll2	r1
	not	r1,   r1
	stc	sr,   r0
	and	r1,   r0
	ldc	r0,   sr		! enable the interrups

	sleep				! SLEEP!!!

/*
 *	runs the resume instruction tables
 */
__resume:
	nop
	bsr     do_decode
	 nop
	mov.l	@r15+, r0
	ldc	r0,    vbr		! Restore the original vbr
	mov.l	@r15+, r0		! Original sr (on interrupts disabled)

	JUMPER()

	lds.l	@r15+, pr
	mov.l   @r15+, r8
	mov.l   @r15+, r9
	mov.l   @r15+, r10
	mov.l   @r15+, r11
	mov.l   @r15+, r12
	mov.l   @r15+, r13
	mov.l	@r15+, r14
	mov.l   1f,  r1
	mov.l   @r1, r1			! who waked up
	ldc	r0, sr			! Restore the original sr
	rts
	 mov	r1, r0			! who waked up
	JUMPER()

.balign 4
1:			.long	INTEVT


.balign       	1024,	0,	1024
vbr_base_suspend:
	.long   0
.balign         1024,   0,      1024

	.long 0
.balign         512,	0,	512
wakeup_interrupt:
	JUMPER()
	!	Disable the interrupts in the ssr
	!	and returns to the context (asap)....
	stc	ssr,   r0
	or	#0xf0, r0
	ldc	r0, ssr		! to avoit recursive irq...
				! this means the context will be resumed
				! with interrupt disabled!!!
/*
 * Here we could have a problem (a sleep with interrupt disables!!!)
 * It could happen if we detect an interrupt between
 * the enabled irq and the sleep!!!
 * Restoring the (raw) spc we will go to execute a sleep with the
 * interrupt disabled !!!!
 * To avoid that in any case we will return on the resume_address
 * label
 */
	mov.l	resume_address, r0
	ldc	r0, spc
	rte
	 nop

200:
/*
 *	load the instruction datas
 */
	mov.l   resume_address,	r0
	mov	REG_INSTR, r0		/* start address I-table */
	mov	REG_INSTR_END, r1	/* I-table size */
	tst	r1, r1
2:
	mov.l   @r0, r2			/* Load the I-tables in cache */
	add	#32, r0
        bf/s	2b
         dt	r1
/*
 *      load the writeable datas
 */
	mov.l	@(OFFSET_DTABLE, r4), r0
	mov.l	@(OFFSET_DTABLE_SIZE, r4),   r1
	tst	r1, r1
2:
	mov.l   @r0, r2			/* Load the d-tables in cache */
	add	#32, r0
	bf/s	2b
	 dt	r1
	bra	200f
	 nop

.balign 4
resume_address:		.long __resume
#define SRC0		r1
#define SRC1		r2
#define SRC2		r3
#define DEST		r4
#define TMP		r5

.balign 2
	JUMPER()
ENTRY(do_decode, 2)
	mov.l	@ITABLE_ADDRESS+, r0	! opcode
	cmp/eq	#OP_END, r0		! End marker ?
	bf	1f
	rts				! Return point
	 nop
1:	/* cmp/eq #OP_SOURCE */
	cmp/eq #OP_SOURCE, r0
	bf	1f
	mov.l	@ITABLE_ADDRESS+, r0	! load the source reg base
	mov.l	@(r0, IOREGS_BASE), TMP	! load ioreg in r5
	mov.l	@ITABLE_ADDRESS+, SRC0	! load the offset

	JUMPER()

	bra     do_decode
	 add	TMP, SRC0		! r2 = the iomem address of source

	/* Load a @SRC0 in Dest*/
1:	cmp/eq	#OP_LOAD, r0
	bf	1f
	bra	do_decode
	 mov.l	@SRC0, DEST		! load the value

	JUMPER()

1:	/* Load a value from table in SRC0 */
	cmp/eq #OP_ILOAD_SRC0, r0
	bf	1f
	bra	do_decode
	 mov.l	@ITABLE_ADDRESS+, SRC0	! the value is in SRC0 !!!

1:	/* Load a value from table in SRC1 */
	cmp/eq #OP_ILOAD_SRC1, r0
	bf	1f
	bra	do_decode
	 mov.l	@ITABLE_ADDRESS+, SRC1	! the value is in SRC1 !!!

1:	/* Load a value from table in SRC2 */
	cmp/eq	#OP_ILOAD_SRC2, r0
	bf	1f
	bra	do_decode
	 mov.l @ITABLE_ADDRESS+, SRC2	! the value is in SRC2 !!!

	JUMPER()

1:	/* Load a value from table in the DEST */
	cmp/eq #OP_ILOAD_DEST, r0
	bf	1f
	bra	do_decode
	 mov.l @ITABLE_ADDRESS+, DEST

1:	/* Store DEST value in @SRC0 */
	cmp/eq #OP_STORE, r0
	bf	1f
	bra	do_decode
	 mov.l DEST, @(0,SRC0)		! store the value

	/* Or operation: DEST |= SRC0 */
1:	cmp/eq #OP_OR,	r0
	bf	1f
	bra	do_decode
	 or	SRC0, DEST

	JUMPER()

	/* And operation: DEST &= SRC0 */
1:	cmp/eq #OP_AND, r0
	bf	1f
	bra	do_decode
	 and	SRC0, DEST

	/* Not operation: SRC0 = ~SRC1*/
	/* It's a bit dirty that the NOT operation works on SRC1 instead of DEST or SRC0*/
1:	cmp/eq #OP_NOT, r0
	bf	1f
	bra	do_decode
	 not	SRC1, SRC0

	/* While bits equal to value. This operation assumes:
		- SRC0: the iomemory address
		- SRC1: the bitmask
		- SRC2: the result
	*/
1:	cmp/eq #OP_WHILE_EQ, r0
	bf	1f
	mov.l	@SRC0, TMP
2:	and     SRC1, TMP

	JUMPER()

	cmp/eq	SRC2, TMP			! (@SRC0 and SRC1) ?!= SRC2)
	bt/s	2b
	 mov.l   @SRC0, TMP
	bra	do_decode
	 nop

	/* While bits not equal to value. This operation assumes:
		   - SRC0: the iomemory address
		   - SRC1: the bitmask
		   - SRC2: the result
	*/
1:	cmp/eq	#OP_WHILE_NEQ, r0
	bf	1f

	mov.l	@SRC0, TMP
2:	and	SRC1, TMP
	cmp/eq  SRC2, TMP		! (@SRC0 and SRC1) ?== SRC2)
	bf/s	2b
	 mov.l	@SRC0, TMP

	JUMPER()

	bra	do_decode
	 nop

	/* Delay operation */
1:	cmp/eq #OP_DELAY, r0
	bf	1f
	mov     DELAY_REG, TMP
	tst	TMP, TMP
2:
	bf/s   2b
	 dt	TMP
	bra	do_decode
	 nop

	/*  SCR0 = @SRC0 */
1:	cmp/eq	#OP_LOAD_SRC0, r0
	bf	1f
	JUMPER()
	mov.l	@SRC0, SRC0
	bra	do_decode
	 nop

1:	/* SRC1 = @SRC0 */
	cmp/eq	#OP_LOAD_SRC1, r0
	bf	1f
	mov.l  @SRC0, SRC1
	bra	do_decode
	 nop

1:	/* SRC2 = @SRC0 */
	cmp/eq	#OP_LOAD_SRC2, r0
	bf	1f
	mov.l  @SRC0, SRC2
	bra	do_decode
	 nop
1:

200:	bra sh4_really_suspend		! Now we jump on sh4_really_suspend
	 nop				! to really suspend (and resume... ;-)

.balign 32
