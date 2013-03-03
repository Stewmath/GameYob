.text
.align 2
#include "macros.s"

.align 2
.global runCpuASM

.global gbPC
.global gbSP
.global af
.global bc
.global de
.global hl
.global halt
.global ime
.global cyclesToExecute


regaf		.req r4
regbc		.req r5
regde		.req r6
reghl		.req r7
regsp		.req r8
regpc		.req r9
var1		.req r10
regcycles	.req r11
@ r1-r3 are used by some macros.

@ --------------------------------------
@ NOP
_00:
	endOp 4
@ --------------------------------------
@ LD BC,nn
_01:
	quickRead16 regpc regbc
	endOp 12
@ --------------------------------------
@ LD (BC),A
_02:
	mov r0,regbc
	_stA r1
	bl writeMemory
	endOp 8
@ --------------------------------------
@ INC BC
_03:
	_inc16 regbc
	endOp 8
@ --------------------------------------
@ INC B
_04:
	_stB var1
	_inc var1
	_ldB var1
	endOp 4
@ --------------------------------------
@ DEC B
_05:
	_stB var1
	_dec var1
	_ldB var1
	endOp 4
@ --------------------------------------
@ LD B,n
_06:
	quickRead regpc r0
	_ldB r0
	endOp 8
@ --------------------------------------
@ RLCA
_07:
	_stA var1
	_rlca var1
	_ldA var1
	endOp 4
@ --------------------------------------
@ LD (nn),SP
_08:
	quickRead16 regpc r0
	writehword r0,regsp
	endOp 20
@ --------------------------------------
@ ADD HL,BC
_09:
	_add16 reghl,regbc
	endOp 8
@ --------------------------------------
@ LD A,(BC)
_0A:
	mov r0,regbc
	bl readMemory
	_ldA r0
	endOp 8
@ --------------------------------------
@ DEC BC
_0B:
	_dec16 regbc
	endOp 8
@ --------------------------------------
@ INC C
_0C:
	_stC var1
	_inc var1
	_ldC var1
	endOp 4
@ --------------------------------------
@ DEC C
_0D:
	_stC var1
	_dec var1
	_ldC var1
	endOp 4
@ --------------------------------------
@ LD C,n
_0E:
	quickRead regpc r0
	_ldC r0
	endOp 8
@ --------------------------------------
@ RRCA
_0F:
	_stA var1
	_rrca var1
	_ldA var1
	endOp 4
@ --------------------------------------
@ LD DE,nn
_11:
	quickRead16 regpc r0
	mov regde,r0
	endOp 12
@ --------------------------------------
@ LD (DE),A
_12:
	mov r0,regde
	_stA r1
	bl writeMemory
	endOp 8
@ --------------------------------------
@ INC DE
_13:
	_inc16 regde
	endOp 8
@ --------------------------------------
@ INC D
_14:
	_stD var1
	_inc var1
	_ldD var1
	endOp 4
@ --------------------------------------
@ DEC D
_15:
	_stD var1
	_dec var1
	_ldD var1
	endOp 4
@ --------------------------------------
@ LD D,n
_16:
	quickRead regpc r0
	_ldD r0
	endOp 8
@ --------------------------------------
@ RLA
_17:
	_stA var1
	_rla var1
	_ldA var1
	endOp 4
@ --------------------------------------
@ JR n
_18:
	quickRead regpc r0
	_jr r0
	endOp 12
@ --------------------------------------
@ ADD HL,DE
_19:
	_add16 reghl,regde
	endOp 8
@ --------------------------------------
@ LD A,(DE)
_1A:
	mov r0,regde
	bl readMemory
	_ldA r0
	endOp 8
@ --------------------------------------
@ DEC DE
_1B:
	_dec16 regde
	endOp 8
@ --------------------------------------
@ INC E
_1C:
	_stE var1
	_inc var1
	_ldE var1
	endOp 4
@ --------------------------------------
@ DEC E
_1D:
	_stE var1
	_dec var1
	_ldE var1
	endOp 4
@ --------------------------------------
@ LD E,n
_1E:
	quickRead regpc r0
	_ldE r0
	endOp 8
@ --------------------------------------
@ RRA
_1F:
	_stA var1
	_rra var1
	_ldA var1
	endOp 4
@ --------------------------------------
@ JR NZ,n
_20:
	quickRead regpc r0
	tst regaf,#Z
	bne jrnz_end
	_jr r0
	endOp 12
jrnz_end:
	endOp 8
@ --------------------------------------
@ LD HL,nn
_21:
	quickRead16 regpc r0
	mov reghl,r0
	endOp 12
@ --------------------------------------
@ LDI (HL),A
_22:
	_stA r1
	mov r0,reghl
	bl writeMemory
	add reghl,#1
	endOp 8
@ --------------------------------------
@ INC HL
_23:
	_inc16 reghl
	endOp 8
@ --------------------------------------
@ INC H
_24:
	_stH var1
	_inc var1
	_ldH var1
	endOp 4
@ --------------------------------------
@ DEC H
_25:
	_stH var1
	_dec var1
	_ldH var1
	endOp 4
@ --------------------------------------
@ LD H,n
_26:
	quickRead regpc r0
	_ldH r0
	endOp 8
@ --------------------------------------
@ DAA
_27:
	_stA r3

	tst regaf,#N
	bne negSet
	tst regaf,#H
	bne d1
	mov r1,r3
	and r1,#0xf
	cmp r1,#9
	bgt d1
	b d2
d1:	add r3,#0x06
d2:	tst regaf,#C
	bne d3
	cmp r3,#0x9f
	bgt d3
	b d4
d3:	add r3,#0x60
d4:	b daaContinue

negSet:
	tst regaf,#H
	subne r3,#0x06
	andne r3,#0xff
	tst regaf,#C
	subne r3,#0x60
daaContinue:
	bic regaf,#(Z | H)
	tst r3,#0x100
	orrne regaf,#C
	ands r3,#0xff
	orreq regaf,#Z

	_ldA r3
	endOp 4
@ --------------------------------------
@ JR Z,n
_28:
	quickRead regpc r0
	tst regaf,#Z
	beq jrz_end
	_jr r0
	endOp 12
jrz_end:
	endOp 8
@ --------------------------------------
@ ADD HL,HL
_29:
	mov var1,reghl
	_add16 reghl var1
	endOp 8
@ --------------------------------------
@ LDI A,(HL)
_2A:
	mov r0,reghl
	bl readMemory
	_ldA r0
	add reghl,#1
	endOp 8
@ --------------------------------------
@ DEC HL
_2B:
	_dec16 reghl
	endOp 8
@ --------------------------------------
@ INC L
_2C:
	_stL var1
	_inc var1
	_ldL var1
	endOp 4
@ --------------------------------------
@ DEC L
_2D:
	_stL var1
	_dec var1
	_ldL var1
	endOp 4
@ --------------------------------------
@ LD L,n
_2E:
	quickRead regpc r0
	_ldL r0
	endOp 8
@ --------------------------------------
@ CPL
_2F:
	@_stA var1
	@eor var1,#0xFF
	@_ldA var1
	eor regaf,#0xFF00

	orr regaf,#H
	orr regaf,#N
	endOp 4
@ --------------------------------------
@ JR NC,n
_30:
	quickRead regpc r0
	tst regaf,#C
	bne jrnc_end
	_jr r0
	endOp 12
jrnc_end:
	endOp 8
@ --------------------------------------
@ LD SP,nn
_31:
	quickRead16 regpc r0
	mov regsp,r0
	endOp 12
@ --------------------------------------
@ LDD (HL),A
_32:
	mov r0,reghl
	_stA r1
	bl writeMemory
	sub reghl,#1
	endOp 8
@ --------------------------------------
@ INC SP
_33:
	_inc16 regsp		@ If it goes over 0xFFFF, this could go screwy...
					@ But if that happens something is already wrong.
	endOp 8
@ --------------------------------------
@ INC (HL)
_34:
	mov r0,reghl
	bl readMemory
	_inc r0
	mov r1,r0
	mov r0,reghl
	bl writeMemory
	endOp 12
@ --------------------------------------
@ DEC (HL)
_35:
	mov r0,reghl
	bl readMemory
	_dec r0
	mov r1,r0
	mov r0,reghl
	bl writeMemory
	endOp 12
@ --------------------------------------
@ LD (HL),n
_36:
	mov r0,reghl
	quickRead regpc r1
	bl writeMemory
	endOp 12
@ --------------------------------------
@ SCF
_37:
	orr regaf,#C
	bic regaf,#N
	bic regaf,#H
	endOp 4
@ --------------------------------------
@ JR C,n
_38:
	quickRead regpc r0
	tst regaf,#C
	beq jrc_end
	_jr r0
	endOp 12
jrc_end:
	endOp 8
@ --------------------------------------
@ ADD HL,SP
_39:
	_add16 reghl,regsp
	endOp 8
@ --------------------------------------
@ LDD A,(HL)
_3A:
	mov r0,reghl
	bl readMemory
	_ldA r0
	sub reghl,#1
	endOp 8
@ --------------------------------------
@ DEC SP
_3B:
	_dec16 regsp		@ If it goes under 0, this could go screwy...
					@ But if that happens something is already wrong.
	endOp 8
@ --------------------------------------
@ INC A
_3C:
	_stA var1
	_inc var1
	_ldA var1
	endOp 4
@ --------------------------------------
@ DEC A
_3D:
	_stA var1
	_dec var1
	_ldA var1
	endOp 4
@ --------------------------------------
@ LD A,n
_3E:
	quickRead regpc r0
	_ldA r0
	endOp 8
@ --------------------------------------
@ CCF
_3F:
	eor regaf,#C
	bic regaf,#(N|H)
	endOp 4
@ --------------------------------------
@ LD B,B
_40:
	endOp 4
@ --------------------------------------
@ LD B,C
_41:
	_stC var1
	_ldB var1
	endOp 4
@ --------------------------------------
@ LD B,D
_42:
	_stD var1
	_ldB var1
	endOp 4
@ --------------------------------------
@ LD B,E
_43:
	_stE var1
	_ldB var1
	endOp 4
@ --------------------------------------
@ LD B,H
_44:
	_stH var1
	_ldB var1
	endOp 4
@ --------------------------------------
@ LD B,L
_45:
	_stL var1
	_ldB var1
	endOp 4
@ --------------------------------------
@ LD B,(HL)
_46:
	mov r0,reghl
	bl readMemory
	_ldB r0
	endOp 8
@ --------------------------------------
@ LD B,A
_47:
	_stA var1
	_ldB var1
	endOp 4
@ --------------------------------------
@ LD C,B
_48:
	_stB var1
	_ldC var1
	endOp 4
@ --------------------------------------
@ LD C,C
_49:
	endOp 4
@ --------------------------------------
@ LD C,D
_4A:
	_stD var1
	_ldC var1
	endOp 4
@ --------------------------------------
@ LD C,E
_4B:
	_stE var1
	_ldC var1
	endOp 4
@ --------------------------------------
@ LD C,H
_4C:
	_stH var1
	_ldC var1
	endOp 4
@ --------------------------------------
@ LD C,L
_4D:
	_stL var1
	_ldC var1
	endOp 4
@ --------------------------------------
@ LD C,(HL)
_4E:
	mov r0,reghl
	bl readMemory
	_ldC r0
	endOp 8
@ --------------------------------------
@ LD C,A
_4F:
	_stA var1
	_ldC var1
	endOp 4
@ --------------------------------------
@ LD D,B
_50:
	_stB var1
	_ldD var1
	endOp 4
@ --------------------------------------
@ LD D,C
_51:
	_stC var1
	_ldD var1
	endOp 4
@ --------------------------------------
@ LD D,D
_52:
	endOp 4
@ --------------------------------------
@ LD D,E
_53:
	_stE var1
	_ldD var1
	endOp 4
@ --------------------------------------
@ LD D,H
_54:
	_stH var1
	_ldD var1
	endOp 4
@ --------------------------------------
@ LD D,L
_55:
	_stL var1
	_ldD var1
	endOp 4
@ --------------------------------------
@ LD D,(HL)
_56:
	mov r0,reghl
	bl readMemory
	_ldD r0
	endOp 8
@ --------------------------------------
@ LD D,A
_57:
	_stA var1
	_ldD var1
	endOp 4
@ --------------------------------------
@ LD E,B
_58:
	_stB var1
	_ldE var1
	endOp 4
@ --------------------------------------
@ LD E,C
_59:
	_stC var1
	_ldE var1
	endOp 4
@ --------------------------------------
@ LD E,D
_5A:
	_stD var1
	_ldE var1
	endOp 4
@ --------------------------------------
@ LD E,E
_5B:
	endOp 4
@ --------------------------------------
@ LD E,H
_5C:
	_stH var1
	_ldE var1
	endOp 4
@ --------------------------------------
@ LD E,L
_5D:
	_stL var1
	_ldE var1
	endOp 4
@ --------------------------------------
@ LD E,(HL)
_5E:
	mov r0,reghl
	bl readMemory
	_ldE r0
	endOp 8
@ --------------------------------------
@ LD E,A
_5F:
	_stA var1
	_ldE var1
	endOp 4

.ltorg

@ --------------------------------------
@ LD H,B
_60:
	_stB var1
	_ldH var1
	endOp 4
@ --------------------------------------
@ LD H,C
_61:
	_stC var1
	_ldH var1
	endOp 4
@ --------------------------------------
@ LD H,D
_62:
	_stD var1
	_ldH var1
	endOp 4
@ --------------------------------------
@ LD H,E
_63:
	_stE var1
	_ldH var1
	endOp 4
@ --------------------------------------
@ LD H,H
_64:
	endOp 4
@ --------------------------------------
@ LD H,L
_65:
	_stL var1
	_ldH var1
	endOp 4
@ --------------------------------------
@ LD H,(HL)
_66:
	mov r0,reghl
	bl readMemory
	_ldH r0
	endOp 8
@ --------------------------------------
@ LD H,A
_67:
	_stA var1
	_ldH var1
	endOp 4
@ --------------------------------------
@ LD L,B
_68:
	_stB var1
	_ldL var1
	endOp 4
@ --------------------------------------
@ LD L,C
_69:
	_stC var1
	_ldL var1
	endOp 4
@ --------------------------------------
@ LD L,D
_6A:
	_stD var1
	_ldL var1
	endOp 4
@ --------------------------------------
@ LD L,E
_6B:
	_stE var1
	_ldL var1
	endOp 4
@ --------------------------------------
@ LD L,H
_6C:
	_stH var1
	_ldL var1
	endOp 4
@ --------------------------------------
@ LD L,L
_6D:
	endOp 4
@ --------------------------------------
@ LD L,(HL)
_6E:
	mov r0,reghl
	bl readMemory
	_ldL r0
	endOp 8
@ --------------------------------------
@ LD L,A
_6F:
	_stA var1
	_ldL var1
	endOp 4
@ --------------------------------------
@ LD (HL),B
_70:
	mov r0,reghl
	_stB r1
	bl writeMemory
	endOp 8
@ --------------------------------------
@ LD (HL),C
_71:
	mov r0,reghl
	_stC r1
	bl writeMemory
	endOp 8
@ --------------------------------------
@ LD (HL),D
_72:
	mov r0,reghl
	_stD r1
	bl writeMemory
	endOp 8
@ --------------------------------------
@ LD (HL),E
_73:
	mov r0,reghl
	_stE r1
	bl writeMemory
	endOp 8
@ --------------------------------------
@ LD (HL),H
_74:
	mov r0,reghl
	_stH r1
	bl writeMemory
	endOp 8
@ --------------------------------------
@ LD (HL),L
_75:
	mov r0,reghl
	_stL r1
	bl writeMemory
	endOp 8
@ --------------------------------------
@ LD (HL),A
_77:
	mov r0,reghl
	_stA r1
	bl writeMemory
	endOp 8
@ --------------------------------------
@ LD A,B
_78:
	_stB var1
	_ldA var1
	endOp 4
@ --------------------------------------
@ LD A,C
_79:
	_stC var1
	_ldA var1
	endOp 4
@ --------------------------------------
@ LD A,D
_7A:
	_stD var1
	_ldA var1
	endOp 4
@ --------------------------------------
@ LD A,E
_7B:
	_stE var1
	_ldA var1
	endOp 4
@ --------------------------------------
@ LD A,H
_7C:
	_stH var1
	_ldA var1
	endOp 4
@ --------------------------------------
@ LD A,L
_7D:
	_stL var1
	_ldA var1
	endOp 4
@ --------------------------------------
@ LD A,(HL)
_7E:
	mov r0,reghl
	bl readMemory
	_ldA r0
	endOp 8
@ --------------------------------------
@ LD A,A
_7F:
	endOp 4

@ --------------------------------------
@ ADD A,B
_80:
	_stB r0
	_add8 r0
	endOp 4
@ --------------------------------------
@ ADD A,C
_81:
	_stC r0
	_add8 r0
	endOp 4
@ --------------------------------------
@ ADD A,D
_82:
	_stD r0
	_add8 r0
	endOp 4
@ --------------------------------------
@ ADD A,E
_83:
	_stE r0
	_add8 r0
	endOp 4
@ --------------------------------------
@ ADD A,H
_84:
	_stH r0
	_add8 r0
	endOp 4
@ --------------------------------------
@ ADD A,L
_85:
	_stL r0
	_add8 r0
	endOp 4
@ --------------------------------------
@ ADD A,(HL)
_86:
	mov r0,reghl
	bl readMemory
	_add8 r0
	endOp 8
@ --------------------------------------
@ ADD A,A
_87:
	_stA var1
	_add8 var1
	endOp 4
@ --------------------------------------
@ ADC A,B
_88:
	_stB r0
	_adc8 r0
	endOp 4
@ --------------------------------------
@ ADC A,C
_89:
	_stC r0
	_adc8 r0
	endOp 4
@ --------------------------------------
@ ADC A,D
_8A:
	_stD r0
	_adc8 r0
	endOp 4
@ --------------------------------------
@ ADC A,E
_8B:
	_stE r0
	_adc8 r0
	endOp 4
@ --------------------------------------
@ ADC A,H
_8C:
	_stH r0
	_adc8 r0
	endOp 4
@ --------------------------------------
@ ADC A,L
_8D:
	_stL r0
	_adc8 r0
	endOp 4
@ --------------------------------------
@ ADC A,(HL)
_8E:
	mov r0,reghl
	bl readMemory
	_adc8 r0
	endOp 8
@ --------------------------------------
@ ADC A,A
_8F:
	_stA var1
	_adc8 var1
	endOp 4
@ --------------------------------------
@ SUB A,B
_90:
	_stB r0
	_sub8 r0
	endOp 4
@ --------------------------------------
@ SUB A,C
_91:
	_stC r0
	_sub8 r0
	endOp 4
@ --------------------------------------
@ SUB A,D
_92:
	_stD r0
	_sub8 r0
	endOp 4
@ --------------------------------------
@ SUB A,E
_93:
	_stE r0
	_sub8 r0
	endOp 4
@ --------------------------------------
@ SUB A,H
_94:
	_stH r0
	_sub8 r0
	endOp 4
@ --------------------------------------
@ SUB A,L
_95:
	_stL r0
	_sub8 r0
	endOp 4
@ --------------------------------------
@ SUB A,(HL)
_96:
	mov r0,reghl
	bl readMemory
	_sub8 r0
	endOp 8
@ --------------------------------------
@ SUB A,A
_97:
	_stA var1
	_sub8 var1
	endOp 4
@ --------------------------------------
@ SBC A,B
_98:
	_stB var1
	_sbc8 var1
	endOp 4
@ --------------------------------------
@ SBC A,C
_99:
	_stC var1
	_sbc8 var1
	endOp 4
@ --------------------------------------
@ SBC A,D
_9A:
	_stD var1
	_sbc8 var1
	endOp 4
@ --------------------------------------
@ SBC A,E
_9B:
	_stE var1
	_sbc8 var1
	endOp 4
@ --------------------------------------
@ SBC A,H
_9C:
	_stH var1
	_sbc8 var1
	endOp 4
@ --------------------------------------
@ SBC A,L
_9D:
	_stL var1
	_sbc8 var1
	endOp 4
@ --------------------------------------
@ SBC A,(HL)
_9E:
	mov r0,reghl
	bl readMemory
	mov var1,r0
	_sbc8 var1
	endOp 8
@ --------------------------------------
@ SBC A,A
_9F:
	_stA var1
	_sbc8 var1
	endOp 4
@ --------------------------------------
@ AND A,B
_A0:
	_stB var1
	_and var1
	endOp 4
@ --------------------------------------
@ AND A,C
_A1:
	_stC var1
	_and var1
	endOp 4
@ --------------------------------------
@ AND A,D
_A2:
	_stD var1
	_and var1
	endOp 4
@ --------------------------------------
@ AND A,E
_A3:
	_stE var1
	_and var1
	endOp 4
@ --------------------------------------
@ AND A,H
_A4:
	_stH var1
	_and var1
	endOp 4
@ --------------------------------------
@ AND A,L
_A5:
	_stL var1
	_and var1
	endOp 4
@ --------------------------------------
@ AND A,(HL)
_A6:
	mov r0,reghl
	bl readMemory
	_and r0
	endOp 8
@ --------------------------------------
@ AND A,A
_A7:
	_stA var1
	_and var1
	endOp 4
@ --------------------------------------
@ XOR A,B
_A8:
	_stB var1
	_xor var1
	endOp 4
@ --------------------------------------
@ XOR A,C
_A9:
	_stC var1
	_xor var1
	endOp 4
@ --------------------------------------
@ XOR A,D
_AA:
	_stD var1
	_xor var1
	endOp 4
@ --------------------------------------
@ XOR A,E
_AB:
	_stE var1
	_xor var1
	endOp 4
@ --------------------------------------
@ XOR A,H
_AC:
	_stH var1
	_xor var1
	endOp 4
@ --------------------------------------
@ XOR A,L
_AD:
	_stL var1
	_xor var1
	endOp 4
@ --------------------------------------
@ XOR A,(HL)
_AE:
	mov r0,reghl
	bl readMemory
	_xor r0
	endOp 8
@ --------------------------------------
@ XOR A,A
_AF:
	_stA var1
	_xor var1
	endOp 4
@ --------------------------------------
@ OR A,B
_B0:
	_stB var1
	_or var1
	endOp 4
@ --------------------------------------
@ OR A,C
_B1:
	_stC var1
	_or var1
	endOp 4
@ --------------------------------------
@ OR A,D
_B2:
	_stD var1
	_or var1
	endOp 4
@ --------------------------------------
@ OR A,E
_B3:
	_stE var1
	_or var1
	endOp 4
@ --------------------------------------
@ OR A,H
_B4:
	_stH var1
	_or var1
	endOp 4
@ --------------------------------------
@ OR A,L
_B5:
	_stL var1
	_or var1
	endOp 4
@ --------------------------------------
@ OR A,(HL)
_B6:
	mov r0,reghl
	bl readMemory
	_or r0
	endOp 8
@ --------------------------------------
@ OR A,A
_B7:
	_stA var1
	_or var1
	endOp 4
@ --------------------------------------
@ CP A,B
_B8:
	_stB var1
	_cp var1
	endOp 4
@ --------------------------------------
@ CP A,C
_B9:
	_stC var1
	_cp var1
	endOp 4
@ --------------------------------------
@ CP A,D
_BA:
	_stD var1
	_cp var1
	endOp 4
@ --------------------------------------
@ CP A,E
_BB:
	_stE var1
	_cp var1
	endOp 4
@ --------------------------------------
@ CP A,H
_BC:
	_stH var1
	_cp var1
	endOp 4
@ --------------------------------------
@ CP A,L
_BD:
	_stL var1
	_cp var1
	endOp 4
@ --------------------------------------
@ CP A,(HL)
_BE:
	mov r0,reghl
	bl readMemory
	_cp r0
	endOp 8
@ --------------------------------------
@ CP A,A
_BF:
	_stA var1
	_cp var1
	endOp 4
@ --------------------------------------
@ RET NZ
_C0:
	tst regaf,#Z
	bne retnz_end
	_ret
	endOp 20
retnz_end:
	endOp 8
@ --------------------------------------
@ POP BC
_C1:
	_pop regbc
	endOp 12
@ --------------------------------------
@ JP NZ
_C2:
	tst regaf,#Z
	bne jpnz_end
	quickRead16 regpc r0
	_jp r0
	endOp 16
jpnz_end:
	add regpc,#2
	endOp 12
@ --------------------------------------
@ JP
_C3:
	quickRead16 regpc r0
	_jp r0
	endOp 16
@ --------------------------------------
@ CALL NZ
_C4:
	tst regaf,#Z
	bne callnz_end
	quickRead16 regpc var1
	_call var1
	endOp 24
callnz_end:
	add regpc,#2
	endOp 12
@ --------------------------------------
@ PUSH BC
_C5:
	_push regbc
	endOp 16
@ --------------------------------------
@ ADD A,n
_C6:
	quickRead regpc r0
	_add8 r0
	endOp 8
@ --------------------------------------
@ RST 00h
_C7:
	_push regpc
	mov regpc,#0
	endOp 16
@ --------------------------------------
@ RET Z
_C8:
	tst regaf,#Z
	beq retz_end
	_ret
	endOp 20
retz_end:
	endOp 8
@ --------------------------------------
@ RET
_C9:
	_ret
	endOp 16
@ --------------------------------------
@ JP Z
_CA:
	tst regaf,#Z
	beq jpz_end
	quickRead16 regpc var1
	_jp var1
	endOp 16
jpz_end:
	add regpc,#2
	endOp 12
@ --------------------------------------
@ CALL Z
_CC:
	tst regaf,#Z
	beq callz_end
	quickRead16 regpc var1
	_call var1
	endOp 24
callz_end:
	add regpc,#2
	endOp 12
@ --------------------------------------
@ CALL
_CD:
	quickRead16 regpc var1
	_call var1
	endOp 24
@ --------------------------------------
@ ADC A,n
_CE:
	quickRead regpc r0
	_adc8 r0
	endOp 8
@ --------------------------------------
@ RST 08h
_CF:
	_push regpc
	mov regpc,#0x8
	endOp 16
@ --------------------------------------
@ RET NC
_D0:
	tst regaf,#C
	bne retnc_end
	_ret
	endOp 20
retnc_end:
	endOp 8
@ --------------------------------------
@ POP DE
_D1:
	_pop regde
	endOp 12
@ --------------------------------------
@ JP NC
_D2:
	tst regaf,#C
	bne jpnc_end
	quickRead16 regpc var1
	_jp var1
	endOp 16
jpnc_end:
	add regpc,#2
	endOp 12
@ --------------------------------------
@ CALL NC
_D4:
	tst regaf,#C
	bne callnc_end
	quickRead16 regpc var1
	_call var1
	endOp 24
callnc_end:
	add regpc,#2
	endOp 12
@ --------------------------------------
@ PUSH DE
_D5:
	_push regde
	endOp 16
@ --------------------------------------
@ SUB A,n
_D6:
	quickRead regpc r0
	_sub8 r0
	endOp 8
@ --------------------------------------
@ RST 10h
_D7:
	_push regpc
	mov regpc,#0x10
	endOp 16
@ --------------------------------------
@ RET C
_D8:
	tst regaf,#C
	beq retc_end
	_ret
	endOp 20
retc_end:
	endOp 8
@ --------------------------------------
@ RETI
_D9:
	_ret
	enableInterrupts
	endOp 16
@ --------------------------------------
@ JP C
_DA:
	tst regaf,#C
	beq jpc_end
	quickRead16 regpc var1
	_jp var1
	endOp 16
jpc_end:
	add regpc,#2
	endOp 12
@ --------------------------------------
@ CALL C
_DC:
	tst regaf,#C
	beq callc_end
	quickRead16 regpc var1
	_call var1
	endOp 24
callc_end:
	add regpc,#2
	endOp 12
@ --------------------------------------
@ SBC A,n
_DE:
	quickRead regpc var1
	_sbc8 var1
	endOp 8
@ --------------------------------------
@ RST 18h
_DF:
	_push regpc
	mov regpc,#0x18
	endOp 16
@ --------------------------------------
@ EI
_FB:
	enableInterrupts
	endOp 4
@ --------------------------------------
@ LD (ff00+n), A
_E0:
	quickRead regpc r0
	add r0,#0xff00
	_stA r1
	bl writeMemory
	endOp 12
@ --------------------------------------
@ POP HL
_E1:
	_pop reghl
	endOp 12
@ --------------------------------------
@ LD (ff00+C), A
_E2:
	_stC r0
	add r0,#0xff00
	_stA r1
	bl writeMemory
	endOp 8
@ --------------------------------------
@ PUSH HL
_E5:
	_push reghl
	endOp 16
@ --------------------------------------
@ AND A,n
_E6:
	quickRead regpc var1
	_and var1
	endOp 8
@ --------------------------------------
@ RST 20H
_E7:
	_push regpc
	mov regpc,#0x20
	endOp 16
@ --------------------------------------
@ JP (HL)
_E9:
	mov regpc,reghl
	endOp 4
@ --------------------------------------
@ LD (nn),A
_EA:
	quickRead16 regpc r0
	_stA r1
	bl writeMemory
	endOp 16
@ --------------------------------------
@ XOR A,n
_EE:
	quickRead regpc var1
	_xor var1
	endOp 8
@ --------------------------------------
@ RST 28H
_EF:
	_push regpc
	mov regpc,#0x28
	endOp 16
@ --------------------------------------
@ LD A,(ff00+n)
_F0:
	quickRead regpc r0
	add r0,#0xff00
	bl readMemory
	_ldA r0
	endOp 12
@ --------------------------------------
@ POP AF
_F1:
	_pop regaf
	mvn r0,#0
	bic r0,#0xf
	and regaf,r0
	endOp 12
@ --------------------------------------
@ LD A,(ff00+C)
_F2:
	_stC r0
	add r0,#0xff00
	bl readMemory
	_ldA r0
	endOp 8
@ --------------------------------------
@ DI
_F3:
	disableInterrupts
	endOp 4
@ --------------------------------------
@ PUSH AF
_F5:
	_push regaf
	endOp 16
@ --------------------------------------
@ OR A,n
_F6:
	quickRead regpc var1
	_or var1
	endOp 8
@ --------------------------------------
@ RST 30H
_F7:
	_push regpc
	mov regpc,#0x30
	endOp 16
@ --------------------------------------
@ LD SP,HL
_F9:
	mov regsp,reghl
	endOp 8
@ --------------------------------------
@ LD A,(nn)
_FA:
	quickRead16 regpc r0
	bl readMemory
	_ldA r0
	endOp 16
@ --------------------------------------
@ CP n
_FE:
	quickRead regpc var1
	_cp var1
	endOp 8
@ --------------------------------------
@ RST 38H
_FF:
	_push regpc
	mov regpc,#0x38
	endOp 16

.ltorg

@ CB opcodes
@ --------------------------------------
@ RLC X
B_00:
	_stB var1
	_rlc var1
	_ldB var1
	endOp 8
B_01:
	_stC var1
	_rlc var1
	_ldC var1
	endOp 8
B_02:
	_stD var1
	_rlc var1
	_ldD var1
	endOp 8
B_03:
	_stE var1
	_rlc var1
	_ldE var1
	endOp 8
B_04:
	_stH var1
	_rlc var1
	_ldH var1
	endOp 8
B_05:
	_stL var1
	_rlc var1
	_ldL var1
	endOp 8
B_06:
	mov r0,reghl
	bl readMemory
	_rlc r0
	mov r1,r0
	mov r0,reghl
	bl writeMemory
	endOp 8
B_07:
	_stA var1
	_rlc var1
	_ldA var1
	endOp 8
@ --------------------------------------
@ RRC X
B_08:
	_stB var1
	_rrc var1
	_ldB var1
	endOp 8
B_09:
	_stC var1
	_rrc var1
	_ldC var1
	endOp 8
B_0A:
	_stD var1
	_rrc var1
	_ldD var1
	endOp 8
B_0B:
	_stE var1
	_rrc var1
	_ldE var1
	endOp 8
B_0C:
	_stH var1
	_rrc var1
	_ldH var1
	endOp 8
B_0D:
	_stL var1
	_rrc var1
	_ldL var1
	endOp 8
B_0E:
	mov r0,reghl
	bl readMemory
	_rrc r0
	mov r1,r0
	mov r0,reghl
	bl writeMemory
	endOp 8
B_0F:
	_stA var1
	_rrc var1
	_ldA var1
	endOp 8
@ --------------------------------------
@ RL X
B_10:
	_stB var1
	_rl var1
	_ldB var1
	endOp 8
B_11:
	_stC var1
	_rl var1
	_ldC var1
	endOp 8
B_12:
	_stD var1
	_rl var1
	_ldD var1
	endOp 8
B_13:
	_stE var1
	_rl var1
	_ldE var1
	endOp 8
B_14:
	_stH var1
	_rl var1
	_ldH var1
	endOp 8
B_15:
	_stL var1
	_rl var1
	_ldL var1
	endOp 8
B_16:
	mov r0,reghl
	bl readMemory
	_rl r0
	mov r1,r0
	mov r0,reghl
	bl writeMemory
	endOp 8
B_17:
	_stA var1
	_rl var1
	_ldA var1
	endOp 8
@ --------------------------------------
@ RR X
B_18:
	_stB var1
	_rr var1
	_ldB var1
	endOp 8
B_19:
	_stC var1
	_rr var1
	_ldC var1
	endOp 8
B_1A:
	_stD var1
	_rr var1
	_ldD var1
	endOp 8
B_1B:
	_stE var1
	_rr var1
	_ldE var1
	endOp 8
B_1C:
	_stH var1
	_rr var1
	_ldH var1
	endOp 8
B_1D:
	_stL var1
	_rr var1
	_ldL var1
	endOp 8
B_1E:
	mov r0,reghl
	bl readMemory
	_rr r0
	mov r1,r0
	mov r0,reghl
	bl writeMemory
	endOp 8
B_1F:
	_stA var1
	_rr var1
	_ldA var1
	endOp 8
@ --------------------------------------
@ SLA X
B_20:
	_stB var1
	_sla var1
	_ldB var1
	endOp 8
B_21:
	_stC var1
	_sla var1
	_ldC var1
	endOp 8
B_22:
	_stD var1
	_sla var1
	_ldD var1
	endOp 8
B_23:
	_stE var1
	_sla var1
	_ldE var1
	endOp 8
B_24:
	_stH var1
	_sla var1
	_ldH var1
	endOp 8
B_25:
	_stL var1
	_sla var1
	_ldL var1
	endOp 8
B_26:
	mov r0,reghl
	bl readMemory
	_sla r0
	mov r1,r0
	mov r0,reghl
	bl writeMemory
	endOp 8
B_27:
	_stA var1
	_sla var1
	_ldA var1
	endOp 8
@ --------------------------------------
@ SRA X
B_28:
	_stB var1
	_sra var1
	_ldB var1
	endOp 8
B_29:
	_stC var1
	_sra var1
	_ldC var1
	endOp 8
B_2A:
	_stD var1
	_sra var1
	_ldD var1
	endOp 8
B_2B:
	_stE var1
	_sra var1
	_ldE var1
	endOp 8
B_2C:
	_stH var1
	_sra var1
	_ldH var1
	endOp 8
B_2D:
	_stL var1
	_sra var1
	_ldL var1
	endOp 8
B_2E:
	mov r0,reghl
	bl readMemory
	_sra r0
	mov r1,r0
	mov r0,reghl
	bl writeMemory
	endOp 8
B_2F:
	_stA var1
	_sra var1
	_ldA var1
	endOp 8
@ --------------------------------------
@ SWAP X
B_30:
	_swaph regbc
	endOp 8
B_31:
	_swapl regbc
	endOp 8
B_32:
	_swaph regde
	endOp 8
B_33:
	_swapl regde
	endOp 8
B_34:
	_swaph reghl
	endOp 8
B_35:
	_swapl reghl
	endOp 8
B_36:
	mov r0,reghl
	bl readMemory
	_swapl r0
	mov r1,r0
	mov r0,reghl
	bl writeMemory
	endOp 8
B_37:
	_swaph regaf
	endOp 8
@ --------------------------------------
@ SRL X
B_38:
	_stB var1
	_srl var1
	_ldB var1
	endOp 8
B_39:
	_stC var1
	_srl var1
	_ldC var1
	endOp 8
B_3A:
	_stD var1
	_srl var1
	_ldD var1
	endOp 8
B_3B:
	_stE var1
	_srl var1
	_ldE var1
	endOp 8
B_3C:
	_stH var1
	_srl var1
	_ldH var1
	endOp 8
B_3D:
	_stL var1
	_srl var1
	_ldL var1
	endOp 8
B_3E:
	mov r0,reghl
	bl readMemory
	_srl r0
	mov r1,r0
	mov r0,reghl
	bl writeMemory
	endOp 8
B_3F:
	_stA var1
	_srl var1
	_ldA var1
	endOp 8
@ --------------------------------------
@ BIT
B_40:
	_bith 0 regbc
	endOp 8
B_41:
	_bitl 0 regbc
	endOp 8
B_42:
	_bith 0 regde
	endOp 8
B_43:
	_bitl 0 regde
	endOp 8
B_44:
	_bith 0 reghl
	endOp 8
B_45:
	_bitl 0 reghl
	endOp 8
B_46:
	mov r0,reghl
	bl readMemory
	_bitl 0 r0
	endOp 8
B_47:
	_bith 0 regaf
	endOp 8
B_48:
	_bith 1 regbc
	endOp 8
B_49:
	_bitl 1 regbc
	endOp 8
B_4A:
	_bith 1 regde
	endOp 8
B_4B:
	_bitl 1 regde
	endOp 8
B_4C:
	_bith 1 reghl
	endOp 8
B_4D:
	_bitl 1 reghl
	endOp 8
B_4E:
	mov r0,reghl
	bl readMemory
	_bitl 1 r0
	endOp 8
B_4F:
	_bith 1 regaf
	endOp 8
B_50:
	_bith 2 regbc
	endOp 8
B_51:
	_bitl 2 regbc
	endOp 8
B_52:
	_bith 2 regde
	endOp 8
B_53:
	_bitl 2 regde
	endOp 8
B_54:
	_bith 2 reghl
	endOp 8
B_55:
	_bitl 2 reghl
	endOp 8
B_56:
	mov r0,reghl
	bl readMemory
	_bitl 2 r0
	endOp 8
B_57:
	_bith 2 regaf
	endOp 8
B_58:
	_bith 3 regbc
	endOp 8
B_59:
	_bitl 3 regbc
	endOp 8
B_5A:
	_bith 3 regde
	endOp 8
B_5B:
	_bitl 3 regde
	endOp 8
B_5C:
	_bith 3 reghl
	endOp 8
B_5D:
	_bitl 3 reghl
	endOp 8
B_5E:
	mov r0,reghl
	bl readMemory
	_bitl 3 r0
	endOp 8
B_5F:
	_bith 3 regaf
	endOp 8
B_60:
	_bith 4 regbc
	endOp 8
B_61:
	_bitl 4 regbc
	endOp 8
B_62:
	_bith 4 regde
	endOp 8
B_63:
	_bitl 4 regde
	endOp 8
B_64:
	_bith 4 reghl
	endOp 8
B_65:
	_bitl 4 reghl
	endOp 8
B_66:
	mov r0,reghl
	bl readMemory
	_bitl 4 r0
	endOp 8
B_67:
	_bith 4 regaf
	endOp 8
B_68:
	_bith 5 regbc
	endOp 8
B_69:
	_bitl 5 regbc
	endOp 8
B_6A:
	_bith 5 regde
	endOp 8
B_6B:
	_bitl 5 regde
	endOp 8
B_6C:
	_bith 5 reghl
	endOp 8
B_6D:
	_bitl 5 reghl
	endOp 8
B_6E:
	mov r0,reghl
	bl readMemory
	_bitl 5 r0
	endOp 8
B_6F:
	_bith 5 regaf
	endOp 8
B_70:
	_bith 6 regbc
	endOp 8
B_71:
	_bitl 6 regbc
	endOp 8
B_72:
	_bith 6 regde
	endOp 8
B_73:
	_bitl 6 regde
	endOp 8
B_74:
	_bith 6 reghl
	endOp 8
B_75:
	_bitl 6 reghl
	endOp 8
B_76:
	mov r0,reghl
	bl readMemory
	_bitl 6 r0
	endOp 8
B_77:
	_bith 6 regaf
	endOp 8
B_78:
	_bith 7 regbc
	endOp 8
B_79:
	_bitl 7 regbc
	endOp 8
B_7A:
	_bith 7 regde
	endOp 8
B_7B:
	_bitl 7 regde
	endOp 8
B_7C:
	_bith 7 reghl
	endOp 8
B_7D:
	_bitl 7 reghl
	endOp 8
B_7E:
	mov r0,reghl
	bl readMemory
	_bitl 7 r0
	endOp 8
B_7F:
	_bith 7 regaf
	endOp 8
B_80:
	_resh 0 regbc
	endOp 8
B_81:
	_resl 0 regbc
	endOp 8
B_82:
	_resh 0 regde
	endOp 8
B_83:
	_resl 0 regde
	endOp 8
B_84:
	_resh 0 reghl
	endOp 8
B_85:
	_resl 0 reghl
	endOp 8
B_86:
	mov r0,reghl
	bl readMemory
	_resl 0 r0
	mov r1,r0
	mov r0,reghl
	bl writeMemory
	endOp 8
B_87:
	_resh 0 regaf
	endOp 8
B_88:
	_resh 1 regbc
	endOp 8
B_89:
	_resl 1 regbc
	endOp 8
B_8A:
	_resh 1 regde
	endOp 8
B_8B:
	_resl 1 regde
	endOp 8
B_8C:
	_resh 1 reghl
	endOp 8
B_8D:
	_resl 1 reghl
	endOp 8
B_8E:
	mov r0,reghl
	bl readMemory
	_resl 1 r0
	mov r1,r0
	mov r0,reghl
	bl writeMemory
	endOp 8
B_8F:
	_resh 1 regaf
	endOp 8
B_90:
	_resh 2 regbc
	endOp 8
B_91:
	_resl 2 regbc
	endOp 8
B_92:
	_resh 2 regde
	endOp 8
B_93:
	_resl 2 regde
	endOp 8
B_94:
	_resh 2 reghl
	endOp 8
B_95:
	_resl 2 reghl
	endOp 8
B_96:
	mov r0,reghl
	bl readMemory
	_resl 2 r0
	mov r1,r0
	mov r0,reghl
	bl writeMemory
	endOp 8
B_97:
	_resh 2 regaf
	endOp 8
B_98:
	_resh 3 regbc
	endOp 8
B_99:
	_resl 3 regbc
	endOp 8
B_9A:
	_resh 3 regde
	endOp 8
B_9B:
	_resl 3 regde
	endOp 8
B_9C:
	_resh 3 reghl
	endOp 8
B_9D:
	_resl 3 reghl
	endOp 8
B_9E:
	mov r0,reghl
	bl readMemory
	_resl 3 r0
	mov r1,r0
	mov r0,reghl
	bl writeMemory
	endOp 8
B_9F:
	_resh 3 regaf
	endOp 8
B_A0:
	_resh 4 regbc
	endOp 8
B_A1:
	_resl 4 regbc
	endOp 8
B_A2:
	_resh 4 regde
	endOp 8
B_A3:
	_resl 4 regde
	endOp 8
B_A4:
	_resh 4 reghl
	endOp 8
B_A5:
	_resl 4 reghl
	endOp 8
B_A6:
	mov r0,reghl
	bl readMemory
	_resl 4 r0
	mov r1,r0
	mov r0,reghl
	bl writeMemory
	endOp 8
B_A7:
	_resh 4 regaf
	endOp 8
B_A8:
	_resh 5 regbc
	endOp 8
B_A9:
	_resl 5 regbc
	endOp 8
B_AA:
	_resh 5 regde
	endOp 8
B_AB:
	_resl 5 regde
	endOp 8
B_AC:
	_resh 5 reghl
	endOp 8
B_AD:
	_resl 5 reghl
	endOp 8
B_AE:
	mov r0,reghl
	bl readMemory
	_resl 5 r0
	mov r1,r0
	mov r0,reghl
	bl writeMemory
	endOp 8
B_AF:
	_resh 5 regaf
	endOp 8
B_B0:
	_resh 6 regbc
	endOp 8
B_B1:
	_resl 6 regbc
	endOp 8
B_B2:
	_resh 6 regde
	endOp 8
B_B3:
	_resl 6 regde
	endOp 8
B_B4:
	_resh 6 reghl
	endOp 8
B_B5:
	_resl 6 reghl
	endOp 8
B_B6:
	mov r0,reghl
	bl readMemory
	_resl 6 r0
	mov r1,r0
	mov r0,reghl
	bl writeMemory
	endOp 8
B_B7:
	_resh 6 regaf
	endOp 8
B_B8:
	_resh 7 regbc
	endOp 8
B_B9:
	_resl 7 regbc
	endOp 8
B_BA:
	_resh 7 regde
	endOp 8
B_BB:
	_resl 7 regde
	endOp 8
B_BC:
	_resh 7 reghl
	endOp 8
B_BD:
	_resl 7 reghl
	endOp 8
B_BE:
	mov r0,reghl
	bl readMemory
	_resl 7 r0
	mov r1,r0
	mov r0,reghl
	bl writeMemory
	endOp 8
B_BF:
	_resh 7 regaf
	endOp 8
@ --------------------------------------
@ SET
B_C0:
	_seth 0 regbc
	endOp 8
B_C1:
	_setl 0 regbc
	endOp 8
B_C2:
	_seth 0 regde
	endOp 8
B_C3:
	_setl 0 regde
	endOp 8
B_C4:
	_seth 0 reghl
	endOp 8
B_C5:
	_setl 0 reghl
	endOp 8
B_C6:
	mov r0,reghl
	bl readMemory
	_setl 0 r0
	mov r1,r0
	mov r0,reghl
	bl writeMemory
	endOp 8
B_C7:
	_seth 0 regaf
	endOp 8
B_C8:
	_seth 1 regbc
	endOp 8
B_C9:
	_setl 1 regbc
	endOp 8
B_CA:
	_seth 1 regde
	endOp 8
B_CB:
	_setl 1 regde
	endOp 8
B_CC:
	_seth 1 reghl
	endOp 8
B_CD:
	_setl 1 reghl
	endOp 8
B_CE:
	mov r0,reghl
	bl readMemory
	_setl 1 r0
	mov r1,r0
	mov r0,reghl
	bl writeMemory
	endOp 8
B_CF:
	_seth 1 regaf
	endOp 8
B_D0:
	_seth 2 regbc
	endOp 8
B_D1:
	_setl 2 regbc
	endOp 8
B_D2:
	_seth 2 regde
	endOp 8
B_D3:
	_setl 2 regde
	endOp 8
B_D4:
	_seth 2 reghl
	endOp 8
B_D5:
	_setl 2 reghl
	endOp 8
B_D6:
	mov r0,reghl
	bl readMemory
	_setl 2 r0
	mov r1,r0
	mov r0,reghl
	bl writeMemory
	endOp 8
B_D7:
	_seth 2 regaf
	endOp 8
B_D8:
	_seth 3 regbc
	endOp 8
B_D9:
	_setl 3 regbc
	endOp 8
B_DA:
	_seth 3 regde
	endOp 8
B_DB:
	_setl 3 regde
	endOp 8
B_DC:
	_seth 3 reghl
	endOp 8
B_DD:
	_setl 3 reghl
	endOp 8
B_DE:
	mov r0,reghl
	bl readMemory
	_setl 3 r0
	mov r1,r0
	mov r0,reghl
	bl writeMemory
	endOp 8
B_DF:
	_seth 3 regaf
	endOp 8
B_E0:
	_seth 4 regbc
	endOp 8
B_E1:
	_setl 4 regbc
	endOp 8
B_E2:
	_seth 4 regde
	endOp 8
B_E3:
	_setl 4 regde
	endOp 8
B_E4:
	_seth 4 reghl
	endOp 8
B_E5:
	_setl 4 reghl
	endOp 8
B_E6:
	mov r0,reghl
	bl readMemory
	_setl 4 r0
	mov r1,r0
	mov r0,reghl
	bl writeMemory
	endOp 8
B_E7:
	_seth 4 regaf
	endOp 8
B_E8:
	_seth 5 regbc
	endOp 8
B_E9:
	_setl 5 regbc
	endOp 8
B_EA:
	_seth 5 regde
	endOp 8
B_EB:
	_setl 5 regde
	endOp 8
B_EC:
	_seth 5 reghl
	endOp 8
B_ED:
	_setl 5 reghl
	endOp 8
B_EE:
	mov r0,reghl
	bl readMemory
	_setl 5 r0
	mov r1,r0
	mov r0,reghl
	bl writeMemory
	endOp 8
B_EF:
	_seth 5 regaf
	endOp 8
B_F0:
	_seth 6 regbc
	endOp 8
B_F1:
	_setl 6 regbc
	endOp 8
B_F2:
	_seth 6 regde
	endOp 8
B_F3:
	_setl 6 regde
	endOp 8
B_F4:
	_seth 6 reghl
	endOp 8
B_F5:
	_setl 6 reghl
	endOp 8
B_F6:
	mov r0,reghl
	bl readMemory
	_setl 6 r0
	mov r1,r0
	mov r0,reghl
	bl writeMemory
	endOp 8
B_F7:
	_seth 6 regaf
	endOp 8
B_F8:
	_seth 7 regbc
	endOp 8
B_F9:
	_setl 7 regbc
	endOp 8
B_FA:
	_seth 7 regde
	endOp 8
B_FB:
	_setl 7 regde
	endOp 8
B_FC:
	_seth 7 reghl
	endOp 8
B_FD:
	_setl 7 reghl
	endOp 8
B_FE:
	mov r0,reghl
	bl readMemory
	_setl 7 r0
	mov r1,r0
	mov r0,reghl
	bl writeMemory
	endOp 8
B_FF:
	_seth 7 regaf
	endOp 8


gbPC:		.word 0x0100
gbSP:		.word 0xFFFE
af:			.word 0x01b0
bc:			.word 0x0013
de:			.word 0x00d8
hl:			.word 0x014d
ime:		.word 0
cyclesToExecute:	.word 0

halt:			.word 0


runCpuASM:
	stmfd sp!,{r4-r11,lr}
	str r0,cyclesToExecute
	loadvars
	mov regcycles,#0

startOp:
	@ Fetch opcode and increment pc
	quickRead regpc var1

	@ This part isn't necessary as cycles are specified within each opcode
	@ldr r0,=opCycles
	@ldrb r1,[r0,var1]
	@add regcycles,r1

	@ Jump to the appropriate opcode
	ldr r0,=op_table
	ldr pc,[r0,var1,lsl#2]
	@cheat
	@b endOp

endOp:

	ldr r0,cyclesToExecute
	cmp regcycles,r0
	blt startOp

	storevars
	mov r0,regcycles
	ldmfd sp!,{r4-r11,lr}
	bx lr

@ I put a few opcodes here because of arm's limitations on where data is put...

@ --------------------------------------
@ extended opcode set
_CB:
	quickRead regpc var1

	@ This part isn't necessary as cycles are specified within each opcode
	@ldr r0,=CBopCycles
	@ldrb r1,[r0,var1]
	@add regcycles,r1

	@ Jump to the appropriate opcode
	ldr r0,=cb_op_table
	ldr pc,[r0,var1,lsl#2]
	cheatcb
@ --------------------------------------
@ HALT
_76:
	mov r0,#1
	str r0,halt
	mov r0,#0
	str r0,cyclesToExecute
	endOp 0

@ These opcodes fall back to the other core for now.
_E8:
_F8:
_10:
_xx:
	cheat
	endOp 12

op_table:
.word _00,_01,_02,_03,_04,_05,_06,_07,_08,_09,_0A,_0B,_0C,_0D,_0E,_0F
.word _10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_1A,_1B,_1C,_1D,_1E,_1F
.word _20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_2A,_2B,_2C,_2D,_2E,_2F
.word _30,_31,_32,_33,_34,_35,_36,_37,_38,_39,_3A,_3B,_3C,_3D,_3E,_3F
.word _40,_41,_42,_43,_44,_45,_46,_47,_48,_49,_4A,_4B,_4C,_4D,_4E,_4F
.word _50,_51,_52,_53,_54,_55,_56,_57,_58,_59,_5A,_5B,_5C,_5D,_5E,_5F
.word _60,_61,_62,_63,_64,_65,_66,_67,_68,_69,_6A,_6B,_6C,_6D,_6E,_6F
.word _70,_71,_72,_73,_74,_75,_76,_77,_78,_79,_7A,_7B,_7C,_7D,_7E,_7F
.word _80,_81,_82,_83,_84,_85,_86,_87,_88,_89,_8A,_8B,_8C,_8D,_8E,_8F
.word _90,_91,_92,_93,_94,_95,_96,_97,_98,_99,_9A,_9B,_9C,_9D,_9E,_9F
.word _A0,_A1,_A2,_A3,_A4,_A5,_A6,_A7,_A8,_A9,_AA,_AB,_AC,_AD,_AE,_AF
.word _B0,_B1,_B2,_B3,_B4,_B5,_B6,_B7,_B8,_B9,_BA,_BB,_BC,_BD,_BE,_BF
.word _C0,_C1,_C2,_C3,_C4,_C5,_C6,_C7,_C8,_C9,_CA,_CB,_CC,_CD,_CE,_CF
.word _D0,_D1,_D2,_xx,_D4,_D5,_D6,_D7,_D8,_D9,_DA,_xx,_DC,_xx,_DE,_DF
.word _E0,_E1,_E2,_xx,_xx,_E5,_E6,_E7,_E8,_E9,_EA,_xx,_xx,_xx,_EE,_EF
.word _F0,_F1,_F2,_F3,_xx,_F5,_F6,_F7,_F8,_F9,_FA,_FB,_xx,_xx,_FE,_FF

cb_op_table:
.word B_00,B_01,B_02,B_03,B_04,B_05,B_06,B_07,B_08,B_09,B_0A,B_0B,B_0C,B_0D,B_0E,B_0F
.word B_10,B_11,B_12,B_13,B_14,B_15,B_16,B_17,B_18,B_19,B_1A,B_1B,B_1C,B_1D,B_1E,B_1F
.word B_20,B_21,B_22,B_23,B_24,B_25,B_26,B_27,B_28,B_29,B_2A,B_2B,B_2C,B_2D,B_2E,B_2F
.word B_30,B_31,B_32,B_33,B_34,B_35,B_36,B_37,B_38,B_39,B_3A,B_3B,B_3C,B_3D,B_3E,B_3F
.word B_40,B_41,B_42,B_43,B_44,B_45,B_46,B_47,B_48,B_49,B_4A,B_4B,B_4C,B_4D,B_4E,B_4F
.word B_50,B_51,B_52,B_53,B_54,B_55,B_56,B_57,B_58,B_59,B_5A,B_5B,B_5C,B_5D,B_5E,B_5F
.word B_60,B_61,B_62,B_63,B_64,B_65,B_66,B_67,B_68,B_69,B_6A,B_6B,B_6C,B_6D,B_6E,B_6F
.word B_70,B_71,B_72,B_73,B_74,B_75,B_76,B_77,B_78,B_79,B_7A,B_7B,B_7C,B_7D,B_7E,B_7F
.word B_80,B_81,B_82,B_83,B_84,B_85,B_86,B_87,B_88,B_89,B_8A,B_8B,B_8C,B_8D,B_8E,B_8F
.word B_90,B_91,B_92,B_93,B_94,B_95,B_96,B_97,B_98,B_99,B_9A,B_9B,B_9C,B_9D,B_9E,B_9F
.word B_A0,B_A1,B_A2,B_A3,B_A4,B_A5,B_A6,B_A7,B_A8,B_A9,B_AA,B_AB,B_AC,B_AD,B_AE,B_AF
.word B_B0,B_B1,B_B2,B_B3,B_B4,B_B5,B_B6,B_B7,B_B8,B_B9,B_BA,B_BB,B_BC,B_BD,B_BE,B_BF
.word B_C0,B_C1,B_C2,B_C3,B_C4,B_C5,B_C6,B_C7,B_C8,B_C9,B_CA,B_CB,B_CC,B_CD,B_CE,B_CF
.word B_D0,B_D1,B_D2,B_D3,B_D4,B_D5,B_D6,B_D7,B_D8,B_D9,B_DA,B_DB,B_DC,B_DD,B_DE,B_DF
.word B_E0,B_E1,B_E2,B_E3,B_E4,B_E5,B_E6,B_E7,B_E8,B_E9,B_EA,B_EB,B_EC,B_ED,B_EE,B_EF
.word B_F0,B_F1,B_F2,B_F3,B_F4,B_F5,B_F6,B_F7,B_F8,B_F9,B_FA,B_FB,B_FC,B_FD,B_FE,B_FF
