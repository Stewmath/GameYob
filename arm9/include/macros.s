Z	= 0x80
N	= 0x40
H	= 0x20
C	= 0x10

.macro cheat
	storevars
	mov r0,var1
	bl cheat
	loadvars
.endm
.macro cheatcb
	storevars
	mov r0,var1
	bl cheatcb
	loadvars
.endm

.macro storevars
	str regsp,gbSP
	str regpc,gbPC
	str regaf,af
	str regbc,bc
	str regde,de
	str reghl,hl
.endm

.macro loadvars
	ldr regsp,gbSP
	ldr regpc,gbPC
	ldr regaf,af
	ldr regbc,bc
	ldr regde,de
	ldr reghl,hl
.endm

.macro writehword addr val
	mov r0,\addr
	mov r1,\val
	bl writehword
.endm

.macro _stA num
	mov \num,regaf,lsr#8
.endm
	
.macro _stF num
	and \num,regaf,#0xF0
.endm

.macro _ldA num
	and regaf,#0x00FF
	orr regaf,\num,lsl#8
.endm

.macro _ldF num
	and regaf,#0xFF00
	orr regaf,\num
.endm
	
.macro _stB num
	mov \num,regbc,lsr#8
.endm
	
.macro _stC num
	and \num,regbc,#0xFF
.endm

.macro _ldB num
	and regbc,#0x00FF
	orr regbc,\num,lsl#8
.endm

.macro _ldC num
	and regbc,#0xFF00
	orr regbc,\num
.endm

.macro _stD num
	mov \num,regde,lsr#8
.endm
	
.macro _stE num
	and \num,regde,#0xFF
.endm

.macro _ldD num
	and regde,#0xFF
	orr regde,\num,lsl#8
.endm

.macro _ldE num
	and regde,#0xFF00
	orr regde,\num
.endm

.macro _stH num
	mov \num,reghl,lsr#8
.endm
	
.macro _stL num
	and \num,reghl,#0xFF
.endm

.macro _ldH num
	and reghl,#0x00FF
	orr reghl,\num,lsl#8
.endm

.macro _ldL num
	and reghl,#0xFF00
	orr reghl,\num
.endm

@ -----------------------------------------------
@ Note: When using the following macros, know that r1-r3 may be changed. Never pass them! (r0 is usually fine)

.macro _inc reg
	add \reg,#1
	ands \reg,#0xFF

@ Calculate flags
	@cmp \reg,#0
	orreq regaf,#Z
	bicne regaf,#Z

	tst \reg,#0xF
	orreq regaf,#H
	bicne regaf,#H

	bic regaf,#N
.endm

.macro _dec reg
	subs \reg,#1
	movlts \reg,#0xFF

	@cmp \reg,#0
	orreq regaf,#Z
	bicne regaf,#Z

	mov r3,\reg
	and r3,#0xF
	cmp r3,#0xF
	orreq regaf,#H
	bicne regaf,#H

	orr regaf,#N
.endm

.macro _inc16 reg
	add \reg,#1
	cmp \reg,#0x10000
	moveq \reg,#0
.endm

.macro _dec16 reg
	sub \reg,#1
	mov r3,#0x00FF
	orr r3,#0xFF00
	and \reg,r3
.endm

.macro _rlc reg
	ror \reg,#31
	tst \reg,#0x100
	orrne regaf,#C
	orrne \reg,#1
	andne \reg,#0xFF
	biceq regaf,#C

	cmp \reg,#0
	orreq regaf,#Z
	bicne regaf,#Z

	bic regaf,#(N|H)
.endm
@ Flag behaviour is slightly different from normal "rlc"
.macro _rlca reg
	ror \reg,#31
	tst \reg,#0x100
	orrne regaf,#C
	orrne \reg,#1
	andne \reg,#0xFF
	biceq regaf,#C

	bic regaf,#(N|H|Z)
.endm

.macro _rrc reg
	tst \reg,#1
	ror \reg,#1
	orrne regaf,#C
	orrne \reg,#0x80
	andne \reg,#0xFF
	biceq regaf,#C

	cmp \reg,#0
	orreq regaf,#Z
	bicne regaf,#Z

	bic regaf,#(N|H)
.endm
.macro _rrca reg
	tst \reg,#1
	ror \reg,#1
	orrne regaf,#C
	orrne \reg,#0x80
	andne \reg,#0xFF
	biceq regaf,#C

	bic regaf,#(Z|N|H)
.endm

.macro _rl reg
	ror \reg,#31

	@ Import bit 0 from carry flag
	tst regaf,#C
	orrne \reg,#1
	
	tst \reg,#0x100
	orrne regaf,#C
	andne \reg,#0xFF
	biceq regaf,#C

	cmp \reg,#0
	orreq regaf,#Z
	bicne regaf,#Z

	bic regaf,#(N|H)
.endm
.macro _rla reg
	ror \reg,#31

	@ Import bit 0 from carry flag
	tst regaf,#C
	orrne \reg,#1
	
	tst \reg,#0x100
	orrne regaf,#C
	andne \reg,#0xFF
	biceq regaf,#C

	bic regaf,#(N|H|Z)
.endm

.macro _rr reg
	ror \reg,#1

	@ Import bit 7 from carry flag
	tst regaf,#C
	orrne \reg,#0x80
	
	tst \reg,#0x80000000
	andne \reg,#0xFF
	orrne regaf,#C
	biceq regaf,#C

	cmp \reg,#0
	orreq regaf,#Z
	bicne regaf,#Z

	bic regaf,#(N|H)
.endm
.macro _rra reg
	ror \reg,#1

	@ Import bit 7 from carry flag
	tst regaf,#C
	orrne \reg,#0x80
	
	tst \reg,#0x80000000
	andne \reg,#0xFF
	orrne regaf,#C
	biceq regaf,#C

	bic regaf,#(N|H|Z)
.endm

.macro _sla reg
	ror \reg,#31
	tst \reg,#0x100
	orrne regaf,#C
	biceq regaf,#C
	ands \reg,#0xff
	orreq regaf,#Z
	bicne regaf,#Z
	bic regaf,#(H|N)
.endm

.macro _sra reg
	tst \reg,#1
	orrne regaf,#C
	biceq regaf,#C

	ror \reg,#1
	tst \reg,#0x40
	orrne \reg,#0x80
	ands \reg,#0xff
	orreq regaf,#Z
	bicne regaf,#Z
	bic regaf,#(H|N)
.endm

.macro _swaph reg
	and r1,\reg,#0xf000
	and r2,\reg,#0x0f00
	and \reg,#0x00ff
	orr \reg,r1,lsr#4
	orr \reg,r2,lsl#4
	tst \reg,#0xff00
	orreq regaf,#Z
	bicne regaf,#Z
	bic regaf,#(H|N|C)
.endm
.macro _swapl reg
	and r1,\reg,#0x00f0
	and r2,\reg,#0x000f
	and \reg,#0xff00
	orr \reg,r1,lsr#4
	orr \reg,r2,lsl#4
	tst \reg,#0x00ff
	orreq regaf,#Z
	bicne regaf,#Z
	bic regaf,#(H|N|C)
.endm

.macro _srl reg
	tst \reg,#1
	orrne regaf,#C
	biceq regaf,#C
	ror \reg,#1
	ands \reg,#0xff
	orreq regaf,#Z
	bicne regaf,#Z
	bic regaf,#(H|N)
.endm

@ test bit on high register
.macro _bith bit reg
	tst \reg,#((1<<\bit)<<8)
	orreq regaf,#Z
	bicne regaf,#Z
	bic regaf,#N
	orr regaf,#H
.endm
@ test bit on low register
.macro _bitl bit reg
	tst \reg,#(1<<\bit)
	orreq regaf,#Z
	bicne regaf,#Z
	bic regaf,#N
	orr regaf,#H
.endm

.macro _resh bit reg
	bic \reg,#((1<<\bit)<<8)
.endm
.macro _resl bit reg
	bic \reg,#(1<<\bit)
.endm

.macro _seth bit reg
	orr \reg,#((1<<\bit)<<8)
.endm
.macro _setl bit reg
	orr \reg,#(1<<\bit)
.endm

.macro _add8 reg
	_stA r3
	mov r1,r3
	mov r2,\reg
	add r3,\reg

	and r1,#0xF
	and r2,#0xF
	add r1,r2
	cmp r1,#0x10
	orrge regaf,#H
	biclt regaf,#H

	cmp r3,#0x100
	orrge regaf,#C
	andge r3,#0xFF
	biclt regaf,#C

	cmp r3,#0
	orreq regaf,#Z
	bicne regaf,#Z

	bic regaf,#N
	_ldA r3
.endm

.macro _adc8 reg
	_stA r3
	mov r1,r3
	mov r2,\reg
	adds r3,\reg
	tst regaf,#C
	addnes r3,#1

	and r1,#0xF
	and r2,#0xF
	tst regaf,#C
	addne r1,#1
	add r1,r2
	cmp r1,#0x10
	orrge regaf,#H
	biclt regaf,#H

	cmp r3,#0x100
	orrge regaf,#C
	andge r3,#0xFF
	biclt regaf,#C

	cmp r3,#0
	orreq regaf,#Z
	bicne regaf,#Z

	bic regaf,#N
	_ldA r3
.endm

.macro _sub8 reg
	_stA r3
	mov r1,r3
	mov r2,\reg

	and r1,#0xF
	and r2,#0xF
	subs r1,r2
	orrmi regaf,#H
	bicpl regaf,#H

	subs r3,\reg
	@cmp r3,#0
	orrmi regaf,#C
	andmi r3,#0xFF
	bicpl regaf,#C

	@cmp r3,#0
	orreq regaf,#Z
	bicne regaf,#Z

	orr regaf,#N
	_ldA r3
.endm

.macro _sbc8 reg
	_stA r3

	tst regaf,#C
	movne r0,#1
	moveq r0,#0

	mov r1,r3
	and r1,r1,#0x0f
	mov r2,\reg
	and r2,r2,#0x0f
	add r2,r2,r0
	cmp r1,r2
	orrlt regaf,regaf,#H
	bicge regaf,regaf,#H

	mov r2,\reg
	add r2,r2,r0
	subs r3,r3,r2
	orrlt regaf,regaf,#C
	bicge regaf,regaf,#C

	ands r3,r3,#0xff
	orreq regaf,regaf,#Z
	bicne regaf,regaf,#Z

	orr regaf,regaf,#N

	_ldA r3
.endm

.macro _add16 reg reg2
	mov r1,\reg
	mov r2,\reg2
	add \reg,\reg2
	mov r3,#0xFF00
	orr r3,#0x00FF

	cmp \reg,r3
	bicle regaf,#C
	orrgt regaf,#C
	andgt \reg,r3

	sub r3,#0xF000	@ r3 = 0xFFF
	and r1,r3
	and r2,r3
	add r1,r2
	cmp r1,r3
	bicle regaf,#H
	orrgt regaf,#H

	bic regaf,#N
.endm

.macro _and reg
	ands regaf,\reg,lsl#8	@ This erases F, but I change all the flags anyway
	@tst regaf,#0xFF00
	orreq regaf,#Z
	@bicne regaf,#Z

	orr regaf,#H
	@bic regaf,#(N|C)
.endm

.macro _xor reg
	and regaf,#0xff00	@ Clear F
	eor regaf,\reg,lsl#8
	tst regaf,#0xFF00
	orreq regaf,#Z
	@bicne regaf,#Z

	@bic regaf,#(H|N|C)
.endm

.macro _or reg
	and regaf,#0xff00	@ Clear F
	orr regaf,\reg,lsl#8
	tst regaf,#0xFF00
	orreq regaf,#Z
	@bicne regaf,#Z

	@bic regaf,#(H|N|C)
.endm

.macro _cp reg
	mov r3,regaf,lsr#8

	mov r1,r3
	mov r2,\reg
	and r1,#0xF
	and r2,#0xF
	cmp r1,r2
	orrmi regaf,#H
	bicge regaf,#H

	cmp r3,\reg
	orreq regaf,#Z
	bicne regaf,#Z

	orrmi regaf,#C
	bicge regaf,#C

	orr regaf,#N
.endm

.macro _pop reg
	mov r0,regsp
	bl readhword
	mov \reg,r0
	add regsp,#2
.endm

.macro _push reg
	sub regsp,#2
	mov r1,\reg
	mov r0,regsp
	bl writehword
.endm

.macro _jp reg
	mov regpc,\reg
.endm

.macro _call reg
	_push regpc
	mov regpc,\reg
.endm

.macro _ret
	_pop regpc
.endm

.macro _jr reg
	tst \reg,#0x80
	movne r3,#0xFF
	mvnne r3,r3
	orrne \reg,r3				@ Convert 8-bit negative to 32-bit negative
	add regpc,\reg
	mov r3,#0x00FF
	orr r3,#0xFF00
	and regpc,r3
.endm

.macro enableInterrupts
	mov r0,#1
	ldr r1,=ime
	str r0,[r1]

	ldr r2,=ioRam
	ldrb r0,[r2,#0xf]
	ldrb r1,[r2,#0xff]
	ands r0,r1
	movne r0,#0
	ldrne r1,=cyclesToExecute
	strne r0,[r1]
.endm

.macro disableInterrupts
	mov r0,#0
	ldr r1,=ime
	str r0,[r1]
.endm

.macro quickRead src dest
	ldr r3,=memory
	mov r1,\src,lsr#12
	ldr r3,[r3,r1,lsl#2]
	mov r2,#0x00ff
	orr r2,#0x0f00
	mov r1,\src
	and r1,r2
	ldrb \dest,[r3,r1]
	add \src,#1
.endm

.macro quickRead16 src dest
	quickRead \src \dest

	ldr r3,=memory
	mov r1,\src,lsr#12
	ldr r3,[r3,r1,lsl#2]
	mov r1,\src
	and r1,r2
	ldrb r3,[r3,r1]
	orr \dest,r3,lsl#8
	add \src,#1
.endm

.macro quickWrite addr val
	ldr r3,=memory
	mov r1,\addr,lsr#12
	ldr r3,[r3,r1,lsl#2]
	mov r2,#0x00ff
	orr r2,#0x0f00
	mov r1,\addr
	and r1,r2
	strb \val,[r3,r1]
	add \addr,#1
.endm

.macro quickWrite16 addr val
	and r1,\val,#0xff
	quickWrite \addr r1
	mov r1,\val,lsr#8
	quickWrite \addr r1
.endm

.macro endOp cycles
	add regcycles,#\cycles
	b endOp
.endm
