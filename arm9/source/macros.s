Z	= 0x80
N	= 0x40
H	= 0x20
C	= 0x10

.macro cheat
	storevars
	mov r0,var1
	bl _Z5cheati
	loadvars
.endm

.macro storevars
	mov r0,regaf
	mov r1,regbc
	mov r2,regde
	mov r3,reghl
	bl	_Z9storevarsiiii
	str regpc,gbPC
	str regsp,gbSP
.endm

.macro loadvars
	bl _Z5getafv
	mov regaf,r0
	bl _Z5getbcv
	mov regbc,r0
	bl _Z5getdev
	mov regde,r0
	bl _Z5gethlv
	mov reghl,r0
	ldr regpc,gbPC
	ldr regsp,gbSP
.endm

.macro setC
	orr regaf,#0x10
.endm

.macro setZ
	orr regaf,#0x80
.endm

.macro setN
	orr regaf,#0x40
.endm

.macro setH
	orr regaf,#0x20
.endm

.macro tstC
	tst regaf,#0x10
.endm

.macro tstZ
	tst regaf,#0x80
.endm

.macro tstN
	tst regaf,#0x40
.endm

.macro tstH
	tst regaf,#0x20
.endm

.macro clearC
	mov r3,#0xFF00
	orr r3,#0xEF
	and regaf,r3
.endm

.macro clearZ
	mov r3,#0xFF00
	orr r3,#0x7F
	and regaf,r3
.endm

.macro clearN
	mov r3,#0xFF00
	orr r3,#0xBF
	and regaf,r3
.endm

.macro clearH
	mov r3,#0xFF00
	orr r3,#0xDF
	and regaf,r3
.endm

.macro readMemory addr
	mov r0,\addr
	bl readMemory
.endm

.macro readhword addr
	mov r0,\addr
	bl readhword
.endm

.macro writehword addr val
@	mov r1,\val
@	and r1,r1,#0xff
@	mov r0,\addr
@	bl writeMemory
@	mov r1,\val,lsr#8
@	bl _Z11writeMemoryth
	mov r0,\addr
	mov r1,\val
	bl writehword
.endm

.macro writeMemory addr val
	mov r1,\val
	mov r0,\addr
	bl writeMemory
.endm

.macro _stA num
	mov \num,regaf,lsr#8
.endm
	
.macro _stF num
	mov \num,regaf
	and \num,#0xFF
.endm

.macro _ldA num
	and regaf,#0x00FF
	and \num,#0xFF	@ Ideally not necessary, I may remove this
	orr regaf,\num,lsl#8
.endm

.macro _ldF num
	and regaf,#0xFF00
	and \num,#0xFF	@ Ideally not necessary, I may remove this
	orr regaf,\num
.endm
	
.macro _stB num
	mov \num,regbc,lsr#8
.endm
	
.macro _stC num
	mov \num,regbc
	and \num,#0xFF
.endm

.macro _ldB num
	and regbc,#0x00FF
	and \num,#0xFF	@ Ideally not necessary, I may remove this
	orr regbc,\num,lsl#8
.endm

.macro _ldC num
	and regbc,#0xFF00
	and \num,#0xFF	@ Ideally not necessary, I may remove this
	orr regbc,\num
.endm

.macro _stD num
	mov \num,regde,lsr#8
.endm
	
.macro _stE num
	mov \num,regde
	and \num,#0xFF
.endm

.macro _ldD num
	and regde,#0xFF
	and \num,#0xFF	@ Ideally not necessary, I may remove this
	orr regde,\num,lsl#8
.endm

.macro _ldE num
	and regde,#0xFF00
	and \num,#0xFF	@ Ideally not necessary, I may remove this
	orr regde,\num
.endm

.macro _stH num
	mov \num,reghl,lsr#8	@ move to var"1" or var"2"
.endm
	
.macro _stL num
	mov \num,reghl
	and \num,#0xFF
.endm

.macro _ldH num
	and reghl,#0x00FF
	and \num,#0xFF	@ Ideally not necessary, I may remove this
	orr reghl,\num,lsl#8
.endm

.macro _ldL num
	and reghl,#0xFF00
	and \num,#0xFF	@ Ideally not necessary, I may remove this
	orr reghl,\num
.endm

.macro _stAF num
	mov \num,regaf
.endm

.macro _ldAF num
	@and \num,#0xFFFF	@ Ideally not necessary, I may remove this
	mov regaf,\num
.endm

.macro _stBC num
	mov \num,regbc
.endm

.macro _ldBC num
	mov regbc,\num
.endm

.macro _stDE num
	mov \num,regde
.endm

.macro _ldDE num
	mov regde,\num
.endm

.macro _stHL num
	mov \num,reghl
.endm

.macro _ldHL num
	@and \num,#0xFFFF	@ Ideally not necessary, I may remove this
	mov reghl,\num
.endm

.macro _ldSP num
	@and \num,#0xFFFF	@ Ideally not necessary, I may remove this
	mov regsp,\num
.endm

.macro _stSP num
	mov \num,regsp
.endm

@ -----------------------------------------------
@ Note: When using the following macros, know that r1-r3 may be changed. Never pass them! (r0 is fine)

.macro incSP
	add regsp,#1
.endm

.macro decSP
	sub regsp,#1
.endm

.macro addSP num
	add regsp,\num
.endm

.macro inc reg
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

.macro dec reg
	subs \reg,#1
	movlts \reg,#0xFF

@ Calculate flags
	@cmp \reg,#0
	orreq regaf,#Z
	bicne regaf,#Z

	mov r3,\reg
	and r3,#0xF
	cmp r3,#0xF
	orreq regaf,#H
	biceq regaf,#H

	orr regaf,#N
.endm

.macro inc16 reg
	add \reg,#1
	cmp \reg,#0x10000
	moveq \reg,#0
.endm

.macro dec16 reg
	sub \reg,#1
	mov r3,#0x00FF
	orr r3,#0xFF00
	and \reg,r3
.endm

.macro rlc reg
	ror \reg,#31
	tst \reg,#0x100
	orrne regaf,#C
	orrne \reg,#1
	andne \reg,#0xFF
	biceq regaf,#C

	cmp \reg,#0
	orreq regaf,#Z
	bicne regaf,#Z

	bic regaf,#N
	bic regaf,#H
.endm

.macro rrc reg
	ror \reg,#1
	tst \reg,#0x80000000
	orrne regaf,#C
	orrne \reg,#0x80
	andne \reg,#0xFF
	biceq regaf,#C

	cmp \reg,#0
	orreq regaf,#Z
	bicne regaf,#Z

	bic regaf,#N
	bic regaf,#H
.endm

.macro rl reg
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

	bic regaf,#N
	bic regaf,#H
.endm

.macro rr reg
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

	bic regaf,#N
	bic regaf,#H
.endm

.macro _add8 reg reg2
	mov r1,\reg
	mov r2,\reg2
	add \reg,\reg2

	and r1,#0xF
	and r2,#0xF
	add r1,r2
	cmp r1,#0x10
	orrge regaf,#H
	biclt regaf,#H

	cmp \reg,#0x100
	orrge regaf,#C
	andge \reg,#0xFF
	biclt regaf,#C

	cmp \reg,#0
	orreq regaf,#Z
	bicne regaf,#Z

	bic regaf,#N
.endm

.macro _adc8 reg reg2
	mov r1,\reg
	mov r2,\reg2
	add \reg,\reg2
	tst regaf,#C
	addne \reg,#1

	and r1,#0xF
	and r2,#0xF
	tst regaf,#C
	addne r1,#1
	add r1,r2
	cmp r1,#0x10
	orrge regaf,#H
	biclt regaf,#H

	cmp \reg,#0x100
	orrge regaf,#C
	andge \reg,#0xFF
	biclt regaf,#C

	cmp \reg,#0
	orreq regaf,#Z
	bicne regaf,#Z

	bic regaf,#N
.endm

.macro _sub8 reg reg2
	mov r1,\reg
	mov r2,\reg2
	sub \reg,\reg2

	and r1,#0xF
	and r2,#0xF
	subs r1,r2
	orrmi regaf,#H
	bicpl regaf,#H

	cmp \reg,#0
	orrmi regaf,#C
	andmi \reg,#0xFF
	bicpl regaf,#C

	@cmp \reg,#0
	orreq regaf,#Z
	bicne regaf,#Z

	orr regaf,#N
.endm

.macro _sbc8 reg
	_stA r3
	mov r1,r3
	mov r2,\reg
	sub r3,\reg
	tst regaf,#C
	subne r3,#1

	and r1,#0xF
	and r2,#0xF
	tst regaf,#C
	subne r1,#1
	subs r1,r2
	orrmi regaf,#H
	bicpl regaf,#H

	cmp r3,#0
	orrmi regaf,#C
	andmi r3,#0xFF
	bicpl regaf,#C

	@cmp \reg,#0
	orreq regaf,#Z
	bicne regaf,#Z

	orr regaf,#N

	_ldA r3
.endm

.macro add16 reg reg2
	mov r1,\reg
	mov r2,\reg2
	add \reg,\reg2
	mov r3,#0xFF00
	orr r3,#0x00FF

	cmp \reg,r3
	bicls regaf,#C
	orrhi regaf,#C

	ands \reg,r3
	orreq regaf,#Z
	bicne regaf,#Z

	sub r3,#0xF000	@ r0 = 0xFFF
	and r1,r3
	and r2,r3
	add r1,r2
	cmp r1,r3
	bicls regaf,#H
	orrhi regaf,#H

	bic regaf,#N
.endm

.macro _and reg
	ands regaf,\reg,lsl#8	@ This erases F, but I change all the flags anyway
	@tst regaf,#0xFF00
	orreq regaf,#Z
	bicne regaf,#Z

	orr regaf,#H
	bic regaf,#N
	bic regaf,#C
.endm

.macro _xor reg
	eor regaf,\reg,lsl#8
	tst regaf,#0xFF00
	orreq regaf,#Z
	bicne regaf,#Z

	bic regaf,#H
	bic regaf,#N
	bic regaf,#C
.endm

.macro _or reg
	orr regaf,\reg,lsl#8
	tst regaf,#0xFF00
	orreq regaf,#Z
	bicne regaf,#Z

	bic regaf,#H
	bic regaf,#N
	bic regaf,#C
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
	_stSP r3
	readhword r3
	mov \reg,r0
	add regsp,#2
.endm

.macro _push reg
	sub regsp,#2
	_stSP r3
	writehword r3 \reg
.endm

.macro _jp reg
	mov regpc,\reg
.endm

@ Call, unlike _jp, assumes regpc has been incremented by 2
.macro _call reg
	_push regpc
	mov regpc,\reg
.endm

.macro _ret
	_pop regpc
.endm

.macro jr reg
	tst \reg,#0x80
	movne r3,#0xFF
	mvnne r3,r3
	orrne \reg,r3				@ Convert 8-bit negative to 32-bit negative
	add regpc,\reg
	@add regpc,#1				@ Let the caller do this (simplifies conditional jumps)
.endm
