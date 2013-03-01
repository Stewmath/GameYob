@ 'EF' means it could be more efficient
.text
.align 2
#include "macros.s"

.align 2
.global drawPatternTableASM
.global runFrameASM
.global initASM

.global cheat
.global storevars
.global gbPC
.global gbSP
.global af
.global bc
.global de
.global hl
.global halt


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
	b endCycle
@ --------------------------------------
@ LD BC,nn
_01:
	readhword regpc
	mov regbc,r0
	add regpc,#2
	b endCycle
@ --------------------------------------
@ LD (BC),A
_02:
	_stA r0
	writeMemory regbc r0
	b endCycle
@ --------------------------------------
@ INC BC
_03:
	inc16 regbc
	b endCycle
@ --------------------------------------
@ INC B
_04:
	_stB var1
	inc var1
	_ldB var1
	b endCycle
@ --------------------------------------
@ DEC B
_05:
	_stB var1
	dec var1
	_ldB var1
	b endCycle
@ --------------------------------------
@ LD B,n
_06:
	readMemory regpc
	_ldB r0
	add regpc,#1
	b endCycle
@ --------------------------------------
@ RLCA
_07:
	_stA var1
	rlc var1
	_ldA var1
	b endCycle
@ --------------------------------------
@ LD (nn),SP
_08:
	readhword regpc
	_stSP var1
	writehword r0,var1
	add regpc,#2
	b endCycle
@ --------------------------------------
@ ADD HL,BC
_09:
	add16 reghl,regbc
	b endCycle
@ --------------------------------------
@ LD A,(BC)
_0A:
	readMemory regbc
	_ldA r0
	b endCycle
@ --------------------------------------
@ DEC BC
_0B:
	dec16 regbc
	b endCycle
@ --------------------------------------
@ INC C
_0C:
	_stC var1
	inc var1
	_ldC var1
	b endCycle
@ --------------------------------------
@ DEC C
_0D:
	_stC var1
	dec var1
	_ldC var1
	b endCycle
@ --------------------------------------
@ LD C,n
_0E:
	readMemory regpc
	_ldC r0
	add regpc,#1
	b endCycle
@ --------------------------------------
@ RRCA
_0F:
	_stA var1
	rrc var1
	_ldA var1
	b endCycle
@ --------------------------------------
@ STOP			TODO
_10:
	b endCycle
@ --------------------------------------
@ LD DE,nn
_11:
	readhword regpc
	mov regde,r0
	add regpc,#2
	b endCycle
@ --------------------------------------
@ LD (DE),A
_12:
	_stA var1
	writeMemory regde var1
	b endCycle
@ --------------------------------------
@ INC DE
_13:
	inc16 regde
	b endCycle
@ --------------------------------------
@ INC D
_14:
	_stD var1
	inc var1
	_ldD var1
	b endCycle
@ --------------------------------------
@ DEC D
_15:
	_stD var1
	dec var1
	_ldD var1
	b endCycle
@ --------------------------------------
@ LD D,n
_16:
	readMemory regpc
	_ldD r0
	add regpc,#1
	b endCycle
@ --------------------------------------
@ RLA
_17:
	_stA var1
	rl var1
	_ldA var1
	b endCycle
@ --------------------------------------
@ JR n
_18:
	readMemory regpc
	jr r0
	add regpc,#1
	b endCycle
@ --------------------------------------
@ ADD HL,DE
_19:
	add16 reghl,regde
	b endCycle
@ --------------------------------------
@ LD A,(DE)
_1A:
	readMemory regde
	_ldA r0
	b endCycle
@ --------------------------------------
@ DEC DE
_1B:
	dec16 regde
	b endCycle
@ --------------------------------------
@ INC E
_1C:
	_stE var1
	inc var1
	_ldE var1
	b endCycle
@ --------------------------------------
@ DEC E
_1D:
	_stE var1
	dec var1
	_ldE var1
	b endCycle
@ --------------------------------------
@ LD E,n
_1E:
	readMemory regpc
	_ldE r0
	add regpc,#1
	b endCycle
@ --------------------------------------
@ RRA
_1F:
	_stA var1
	rr var1
	_ldA var1
	b endCycle
@ --------------------------------------
@ JR NZ,n
_20:
	readMemory regpc
	add regpc,#1
	tst regaf,#Z
	bne endCycle
	jr r0
	b endCycle
@ --------------------------------------
@ LD HL,nn
_21:
	readhword regpc
	mov reghl,r0
	add regpc,#2
	b endCycle
@ --------------------------------------
@ LDI (HL),A
_22:
	_stA var1
	writeMemory reghl,var1
	add reghl,#1
	b endCycle
@ --------------------------------------
@ INC HL
_23:
	inc16 reghl
	b endCycle
@ --------------------------------------
@ INC H
_24:
	_stH var1
	inc var1
	_ldH var1
	b endCycle
@ --------------------------------------
@ DEC H
_25:
	_stH var1
	dec var1
	_ldH var1
	b endCycle
@ --------------------------------------
@ LD H,n
_26:
	readMemory regpc
	_ldH r0
	add regpc,#1
	b endCycle
@ --------------------------------------
@ DAA	TODO
_27:
	b _FF
@ --------------------------------------
@ JR Z,n
_28:
	readMemory regpc
	add regpc,#1
	tst regaf,#Z
	beq endCycle
	jr r0
	b endCycle
@ --------------------------------------
@ ADD HL,HL
_29:
	add16 reghl,reghl
	b endCycle
@ --------------------------------------
@ LDI A,(HL)
_2A:
	readMemory reghl
	_ldA r0
	add reghl,#1
	b endCycle
@ --------------------------------------
@ DEC HL
_2B:
	dec16 reghl
	b endCycle
@ --------------------------------------
@ INC L
_2C:
	_stL var1
	inc var1
	_ldL var1
	b endCycle
@ --------------------------------------
@ DEC L
_2D:
	_stL var1
	dec var1
	_ldL var1
	b endCycle
@ --------------------------------------
@ LD L,n
_2E:
	readMemory regpc
	_ldL r0
	add regpc,#1
	b endCycle
@ --------------------------------------
@ CPL
_2F:
	@_stA var1
	@eor var1,#0xFF
	@_ldA var1
	eor regaf,#0xFF00

	orr regaf,#H
	orr regaf,#N
	b endCycle
@ --------------------------------------
@ JR NC,n
_30:
	readMemory regpc
	add regpc,#1
	tst regaf,#C
	bne endCycle
	jr r0
	b endCycle
@ --------------------------------------
@ LD SP,nn
_31:
	readhword regpc
	_ldSP r0
	add regpc,#2
	b endCycle
@ --------------------------------------
@ LDD (HL),A
_32:
	_stA var1
	writeMemory reghl,var1
	sub reghl,#1
	b endCycle
@ --------------------------------------
@ INC SP
_33:
	inc16 regsp		@ If it goes over 0xFFFF, this could go screwy...
					@ But if that happens something is already wrong.
	b endCycle
@ --------------------------------------
@ INC (HL)
_34:
	readMemory reghl
	inc r0
	writeMemory reghl r0
	b endCycle
@ --------------------------------------
@ DEC (HL)
_35:
	readMemory reghl
	dec r0
	writeMemory reghl r0
	b endCycle
@ --------------------------------------
@ LD (HL),n
_36:
	readMemory regpc
	writeMemory reghl,r0
	add regpc,#1
	b endCycle
@ --------------------------------------
@ SCF
_37:
	orr regaf,#C
	bic regaf,#N
	bic regaf,#H
	b endCycle
@ --------------------------------------
@ JR C,n
_38:
	readMemory regpc
	add regpc,#1
	tst regaf,#C
	beq endCycle
	jr r0
	b endCycle
@ --------------------------------------
@ ADD HL,SP
_39:
	_stSP var1
	add16 reghl,var1
	b endCycle
@ --------------------------------------
@ LDD A,(HL)
_3A:
	readMemory reghl
	_ldA r0
	sub reghl,#1
	b endCycle
@ --------------------------------------
@ DEC SP
_3B:
	dec16 regsp		@ If it goes under 0, this could go screwy...
					@ But if that happens something is already wrong.
	b endCycle
@ --------------------------------------
@ INC A
_3C:
	_stA var1
	inc var1
	_ldA var1
	b endCycle
@ --------------------------------------
@ DEC A
_3D:
	_stA var1
	dec var1
	_ldA var1
	b endCycle
@ --------------------------------------
@ LD A,n
_3E:
	readMemory regpc
	_ldA r0
	add regpc,#1
	b endCycle
@ --------------------------------------
@ CCF
_3F:
	eor regaf,#C
	bic regaf,#N
	bic regaf,#H
	b endCycle
@ --------------------------------------
@ LD B,B
_40:
	b endCycle
@ --------------------------------------
@ LD B,C
_41:
	_stC var1
	_ldB var1
	b endCycle
@ --------------------------------------
@ LD B,D
_42:
	_stD var1
	_ldB var1
	b endCycle
@ --------------------------------------
@ LD B,E
_43:
	_stE var1
	_ldB var1
	b endCycle
@ --------------------------------------
@ LD B,H
_44:
	_stH var1
	_ldB var1
	b endCycle
@ --------------------------------------
@ LD B,L
_45:
	_stL var1
	_ldB var1
	b endCycle
@ --------------------------------------
@ LD B,(HL)
_46:
	readMemory reghl
	_ldB r0
	b endCycle
@ --------------------------------------
@ LD B,A
_47:
	_stA var1
	_ldB var1
	b endCycle
@ --------------------------------------
@ LD C,B
_48:
	_stB var1
	_ldC var1
	b endCycle
@ --------------------------------------
@ LD C,C
_49:
	b endCycle
@ --------------------------------------
@ LD C,D
_4A:
	_stD var1
	_ldC var1
	b endCycle
@ --------------------------------------
@ LD C,E
_4B:
	_stE var1
	_ldC var1
	b endCycle
@ --------------------------------------
@ LD C,H
_4C:
	_stH var1
	_ldC var1
	b endCycle
@ --------------------------------------
@ LD C,L
_4D:
	_stL var1
	_ldC var1
	b endCycle
@ --------------------------------------
@ LD C,(HL)
_4E:
	readMemory reghl
	_ldC r0
	b endCycle
@ --------------------------------------
@ LD C,A
_4F:
	_stA var1
	_ldC var1
	b endCycle
@ --------------------------------------
@ LD D,B
_50:
	_stB var1
	_ldD var1
	b endCycle
@ --------------------------------------
@ LD D,C
_51:
	_stC var1
	_ldD var1
	b endCycle
@ --------------------------------------
@ LD D,D
_52:
	b endCycle
@ --------------------------------------
@ LD D,E
_53:
	_stE var1
	_ldD var1
	b endCycle
@ --------------------------------------
@ LD D,H
_54:
	_stH var1
	_ldD var1
	b endCycle
@ --------------------------------------
@ LD D,L
_55:
	_stL var1
	_ldD var1
	b endCycle
@ --------------------------------------
@ LD D,(HL)
_56:
	readMemory reghl
	_ldD r0
	b endCycle
@ --------------------------------------
@ LD D,A
_57:
	_stA var1
	_ldD var1
	b endCycle
@ --------------------------------------
@ LD E,B
_58:
	_stB var1
	_ldE var1
	b endCycle
@ --------------------------------------
@ LD E,C
_59:
	_stC var1
	_ldE var1
	b endCycle
@ --------------------------------------
@ LD E,D
_5A:
	_stD var1
	_ldE var1
	b endCycle
@ --------------------------------------
@ LD E,E
_5B:
	b endCycle
@ --------------------------------------
@ LD E,H
_5C:
	_stH var1
	_ldE var1
	b endCycle
@ --------------------------------------
@ LD E,L
_5D:
	_stL var1
	_ldE var1
	b endCycle
@ --------------------------------------
@ LD E,(HL)
_5E:
	readMemory reghl
	_ldE r0
	b endCycle
@ --------------------------------------
@ LD E,A
_5F:
	_stA var1
	_ldE var1
	b endCycle
@ --------------------------------------
@ LD H,B
_60:
	_stB var1
	_ldH var1
	b endCycle
@ --------------------------------------
@ LD H,C
_61:
	_stC var1
	_ldH var1
	b endCycle
@ --------------------------------------
@ LD H,D
_62:
	_stD var1
	_ldH var1
	b endCycle
@ --------------------------------------
@ LD H,E
_63:
	_stE var1
	_ldH var1
	b endCycle
@ --------------------------------------
@ LD H,H
_64:
	b endCycle
@ --------------------------------------
@ LD H,L
_65:
	_stL var1
	_ldH var1
	b endCycle
@ --------------------------------------
@ LD H,(HL)
_66:
	readMemory reghl
	_ldH r0
	b endCycle
@ --------------------------------------
@ LD H,A
_67:
	_stA var1
	_ldH var1
	b endCycle
@ --------------------------------------
@ LD L,B
_68:
	_stB var1
	_ldL var1
	b endCycle
@ --------------------------------------
@ LD L,C
_69:
	_stC var1
	_ldL var1
	b endCycle
@ --------------------------------------
@ LD L,D
_6A:
	_stD var1
	_ldL var1
	b endCycle
@ --------------------------------------
@ LD L,E
_6B:
	_stE var1
	_ldL var1
	b endCycle
@ --------------------------------------
@ LD L,H
_6C:
	_stH var1
	_ldL var1
	b endCycle
@ --------------------------------------
@ LD L,L
_6D:
	b endCycle
@ --------------------------------------
@ LD L,(HL)
_6E:
	readMemory reghl
	_ldL r0
	b endCycle
@ --------------------------------------
@ LD L,A
_6F:
	_stA var1
	_ldL var1
	b endCycle
@ --------------------------------------
@ LD (HL),B
_70:
	_stB var1
	writeMemory reghl var1
	b endCycle
@ --------------------------------------
@ LD (HL),C
_71:
	_stC var1
	writeMemory reghl var1
	b endCycle
@ --------------------------------------
@ LD (HL),D
_72:
	_stD var1
	writeMemory reghl var1
	b endCycle
@ --------------------------------------
@ LD (HL),E
_73:
	_stE var1
	writeMemory reghl var1
	b endCycle
@ --------------------------------------
@ LD (HL),H
_74:
	_stH var1
	writeMemory reghl var1
	b endCycle
@ --------------------------------------
@ LD (HL),L
_75:
	_stL var1
	writeMemory reghl var1
	b endCycle
@ --------------------------------------
@ LD (HL),A
_77:
	_stA var1
	writeMemory reghl var1
	b endCycle
@ --------------------------------------
@ LD A,B
_78:
	_stB var1
	_ldA var1
	b endCycle
@ --------------------------------------
@ LD A,C
_79:
	_stC var1
	_ldA var1
	b endCycle
@ --------------------------------------
@ LD A,D
_7A:
	_stD var1
	_ldA var1
	b endCycle
@ --------------------------------------
@ LD A,E
_7B:
	_stE var1
	_ldA var1
	b endCycle
@ --------------------------------------
@ LD A,H
_7C:
	_stH var1
	_ldA var1
	b endCycle
@ --------------------------------------
@ LD A,L
_7D:
	_stL var1
	_ldA var1
	b endCycle
@ --------------------------------------
@ LD A,(HL)
_7E:
	readMemory reghl
	_ldA r0
	b endCycle
@ --------------------------------------
@ LD A,A
_7F:
	b endCycle
@ --------------------------------------
@ ADD A,B
_80:
	_stA var1
	_stB r0
	_add8 var1,r0
	_ldA var1
	b endCycle
@ --------------------------------------
@ ADD A,C
_81:
	_stA var1
	_stC r0
	_add8 var1,r0
	_ldA var1
	b endCycle
@ --------------------------------------
@ ADD A,D
_82:
	_stA var1
	_stD r0
	_add8 var1,r0
	_ldA var1
	b endCycle
@ --------------------------------------
@ ADD A,E
_83:
	_stA var1
	_stE r0
	_add8 var1,r0
	_ldA var1
	b endCycle
@ --------------------------------------
@ ADD A,H
_84:
	_stA var1
	_stH r0
	_add8 var1,r0
	_ldA var1
	b endCycle
@ --------------------------------------
@ ADD A,L
_85:
	_stA var1
	_stL r0
	_add8 var1,r0
	_ldA var1
	b endCycle
@ --------------------------------------
@ ADD A,(HL)
_86:
	_stA var1
	readMemory reghl
	_add8 var1,r0
	_ldA var1
	b endCycle
@ --------------------------------------
@ ADD A,A
_87:
	_stA var1
	_add8 var1,var1
	_ldA var1
	b endCycle
@ --------------------------------------
@ ADC A,B
_88:
	_stA var1
	_stB r0
	_adc8 var1,r0
	_ldA var1
	b endCycle
@ --------------------------------------
@ ADC A,C
_89:
	_stA var1
	_stC r0
	_adc8 var1,r0
	_ldA var1
	b endCycle
@ --------------------------------------
@ ADC A,D
_8A:
	_stA var1
	_stD r0
	_adc8 var1,r0
	_ldA var1
	b endCycle
@ --------------------------------------
@ ADC A,E
_8B:
	_stA var1
	_stE r0
	_adc8 var1,r0
	_ldA var1
	b endCycle
@ --------------------------------------
@ ADC A,H
_8C:
	_stA var1
	_stH r0
	_adc8 var1,r0
	_ldA var1
	b endCycle
@ --------------------------------------
@ ADC A,L
_8D:
	_stA var1
	_stL r0
	_adc8 var1,r0
	_ldA var1
	b endCycle
@ --------------------------------------
@ ADC A,(HL)
_8E:
	_stA var1
	readMemory reghl
	_adc8 var1,r0
	_ldA var1
	b endCycle
@ --------------------------------------
@ ADC A,A
_8F:
	_stA var1
	_adc8 var1,var1
	_ldA var1
	b endCycle
@ --------------------------------------
@ SUB A,B
_90:
	_stA var1
	_stB r0
	_sub8 var1 r0
	_ldA var1
	b endCycle
@ --------------------------------------
@ SUB A,C
_91:
	_stA var1
	_stC r0
	_sub8 var1 r0
	_ldA var1
	b endCycle
@ --------------------------------------
@ SUB A,D
_92:
	_stA var1
	_stD r0
	_sub8 var1 r0
	_ldA var1
	b endCycle
@ --------------------------------------
@ SUB A,E
_93:
	_stA var1
	_stE r0
	_sub8 var1 r0
	_ldA var1
	b endCycle
@ --------------------------------------
@ SUB A,H
_94:
	_stA var1
	_stH r0
	_sub8 var1 r0
	_ldA var1
	b endCycle
@ --------------------------------------
@ SUB A,L
_95:
	_stA var1
	_stL r0
	_sub8 var1 r0
	_ldA var1
	b endCycle
@ --------------------------------------
@ SUB A,(HL)
_96:
	_stA var1
	readMemory reghl
	_sub8 var1 r0
	_ldA var1
	b endCycle
@ --------------------------------------
@ SUB A,A
_97:
	_stA var1
	_sub8 var1 var1
	_ldA var1
	b endCycle
@ --------------------------------------
@ SBC A,B
_98:
	_stB var1
	_sbc8 var1
	b endCycle
@ --------------------------------------
@ SBC A,C
_99:
	_stC var1
	_sbc8 var1
	b endCycle
@ --------------------------------------
@ SBC A,D
_9A:
	_stD var1
	_sbc8 var1
	b endCycle
@ --------------------------------------
@ SBC A,E
_9B:
	_stE var1
	_sbc8 var1
	b endCycle
@ --------------------------------------
@ SBC A,H
_9C:
	_stH var1
	_sbc8 var1
	b endCycle
@ --------------------------------------
@ SBC A,L
_9D:
	_stL var1
	_sbc8 var1
	b endCycle
@ --------------------------------------
@ SBC A,(HL)
_9E:
	readMemory reghl
	_sbc8 r0
	b endCycle
@ --------------------------------------
@ SBC A,A
_9F:
	_stA var1
	_sbc8 var1
	b endCycle
@ --------------------------------------
@ AND A,B
_A0:
	_stB var1
	_and var1
	b endCycle
@ --------------------------------------
@ AND A,C
_A1:
	_stC var1
	_and var1
	b endCycle
@ --------------------------------------
@ AND A,D
_A2:
	_stD var1
	_and var1
	b endCycle
@ --------------------------------------
@ AND A,E
_A3:
	_stE var1
	_and var1
	b endCycle
@ --------------------------------------
@ AND A,H
_A4:
	_stH var1
	_and var1
	b endCycle
@ --------------------------------------
@ AND A,L
_A5:
	_stL var1
	_and var1
	b endCycle
@ --------------------------------------
@ AND A,(HL)
_A6:
	readMemory reghl
	_and r0
	b endCycle
@ --------------------------------------
@ AND A,A
_A7:
	_stA var1
	_and var1
	b endCycle
@ --------------------------------------
@ XOR A,B
_A8:
	_stB var1
	_xor var1
	b endCycle
@ --------------------------------------
@ XOR A,C
_A9:
	_stC var1
	_xor var1
	b endCycle
@ --------------------------------------
@ XOR A,D
_AA:
	_stD var1
	_xor var1
	b endCycle
@ --------------------------------------
@ XOR A,E
_AB:
	_stE var1
	_xor var1
	b endCycle
@ --------------------------------------
@ XOR A,H
_AC:
	_stH var1
	_xor var1
	b endCycle
@ --------------------------------------
@ XOR A,L
_AD:
	_stL var1
	_xor var1
	b endCycle
@ --------------------------------------
@ XOR A,(HL)
_AE:
	readMemory reghl
	_xor r0
	b endCycle
@ --------------------------------------
@ XOR A,A
_AF:
	_stA var1
	_xor var1
	b endCycle
@ --------------------------------------
@ OR A,B
_B0:
	_stB var1
	_or var1
	b endCycle
@ --------------------------------------
@ OR A,C
_B1:
	_stC var1
	_or var1
	b endCycle
@ --------------------------------------
@ OR A,D
_B2:
	_stD var1
	_or var1
	b endCycle
@ --------------------------------------
@ OR A,E
_B3:
	_stE var1
	_or var1
	b endCycle
@ --------------------------------------
@ OR A,H
_B4:
	_stH var1
	_or var1
	b endCycle
@ --------------------------------------
@ OR A,L
_B5:
	_stL var1
	_or var1
	b endCycle
@ --------------------------------------
@ OR A,(HL)
_B6:
	readMemory reghl
	_or r0
	b endCycle
@ --------------------------------------
@ OR A,A
_B7:
	_stA var1
	_or var1
	b endCycle
@ --------------------------------------
@ CP A,B
_B8:
	_stB var1
	_cp var1
	b endCycle
@ --------------------------------------
@ CP A,C
_B9:
	_stC var1
	_cp var1
	b endCycle
@ --------------------------------------
@ CP A,D
_BA:
	_stD var1
	_cp var1
	b endCycle
@ --------------------------------------
@ CP A,E
_BB:
	_stE var1
	_cp var1
	b endCycle
@ --------------------------------------
@ CP A,H
_BC:
	_stH var1
	_cp var1
	b endCycle
@ --------------------------------------
@ CP A,L
_BD:
	_stL var1
	_cp var1
	b endCycle
@ --------------------------------------
@ CP A,(HL)
_BE:
	readMemory reghl
	_cp r0
	b endCycle
@ --------------------------------------
@ CP A,A
_BF:
	_stA var1
	_cp var1
	b endCycle
@ --------------------------------------
@ RET NZ
_C0:
	tst regaf,#Z
	bne endCycle
	_ret
	b endCycle
@ --------------------------------------
@ POP BC
_C1:
	_pop regbc
	b endCycle
@ --------------------------------------
@ JP NZ
_C2:
	tst regaf,#Z
	addne regpc,#2
	bne endCycle
	readhword regpc
	_jp r0
	b endCycle
@ --------------------------------------
@ JP
_C3:
	readhword regpc
	_jp r0
	b endCycle
@ --------------------------------------
@ CALL NZ
_C4:
	tst regaf,#Z
	addne regpc,#2
	bne endCycle
	readhword regpc
	add regpc,#2
	mov var1,r0
	_call var1
	b endCycle
@ --------------------------------------
@ PUSH BC
_C5:
	_push regbc
	b endCycle
@ --------------------------------------
@ ADD A,n
_C6:
	readMemory regpc
	add regpc,#1
	_stA var1
	_add8 var1 r0
	_ldA var1
	b endCycle
@ --------------------------------------
@ RST 00h
_C7:
	_push regpc
	mov regpc,#0
	b endCycle
@ --------------------------------------
@ RET Z
_C8:
	tst regaf,#Z
	beq endCycle
	_ret
	b endCycle
@ --------------------------------------
@ RET
_C9:
	_ret
	b endCycle
@ --------------------------------------
@ JP Z
_CA:
	tst regaf,#Z
	addeq regpc,#2
	beq endCycle
	readhword regpc
	_jp r0
	b endCycle
@ --------------------------------------
@ extended opcode set
_CB:
	b _FF
@ --------------------------------------
@ CALL Z
_CC:
	tst regaf,#Z
	addeq regpc,#2
	beq endCycle
	readhword regpc
	add regpc,#2
	mov var1,r0
	_call var1
	b endCycle
@ --------------------------------------
@ CALL
_CD:
	readhword regpc
	add regpc,#2
	mov var1,r0
	_call var1
	b endCycle
@ --------------------------------------
@ ADC A,n
_CE:
	readMemory regpc
	add regpc,#1
	_stA var1
	_adc8 var1,r0
	_ldA var1
	b endCycle
@ --------------------------------------
@ RST 08h
_CF:
	_push regpc
	mov regpc,#0x8
	b endCycle
@ --------------------------------------
@ RET NC
_D0:
	tst regaf,#C
	bne endCycle
	_ret
	b endCycle
@ --------------------------------------
@ POP DE
_D1:
	_pop regde
	b endCycle
@ --------------------------------------
@ JP NC
_D2:
	tst regaf,#C
	addne regpc,#2
	bne endCycle
	readhword regpc
	_jp r0
	b endCycle
@ --------------------------------------
@ CALL NC
_D4:
	tst regaf,#C
	addne regpc,#2
	bne endCycle
	readhword regpc
	add regpc,#2
	mov var1,r0
	_call var1
	b endCycle
@ --------------------------------------
@ PUSH DE
_D5:
	_push regde
	b endCycle
@ --------------------------------------
@ SUB A,n
_D6:
	readMemory regpc
	add regpc,#1
	_stA var1
	_sub8 var1,r0
	_ldA var1
	b endCycle
@ --------------------------------------
@ RST 10h
_D7:
	_push regpc
	mov regpc,#0x10
	b endCycle
@ --------------------------------------
@ RET C
_D8:
	tst regaf,#C
	beq endCycle
	_ret
	b endCycle
@ --------------------------------------
@ RETI
_D9:
	_ret
	bl _Z16enableInterruptsv
	b endCycle
@ --------------------------------------
@ JP C
_DA:
	tst regaf,#C
	addeq regpc,#2
	beq endCycle
	readhword regpc
	_jp r0
	b endCycle
@ --------------------------------------
@ CALL C
_DC:
	tst regaf,#C
	addeq regpc,#2
	beq endCycle
	readhword regpc
	add regpc,#2
	mov var1,r0
	_call var1
	b endCycle
@ --------------------------------------
@ SBC A,n
_DE:
	readMemory regpc
	add regpc,#1
	_sbc8 r0
	b endCycle
@ --------------------------------------
@ RST 18h
_DF:
	_push regpc
	mov regpc,#0x18
	b endCycle
@ --------------------------------------
@ LDH (n),A
_E0:
@	add regpc,#1
@	mov r1,#0xff00
@	add r0,r0,r1
@	mov r1
_E1:
_E2:
_E3:
_E4:
_E5:
_E6:
_E7:
_E8:
_E9:
_EA:
_EB:
_EC:
_ED:
_EE:
_EF:
_F0:
_F1:
_F2:
_F3:
_F4:
_F5:
_F6:
_F7:
_F8:
_F9:
_FA:
_FB:
_FC:
_FD:
_FE:
_FF:
_xx:
	@mov r0,opcode		@ r0 is still "opcode" from readMemory, so it is implied
	cheat
	b endCycle
@ --------------------------------------
@ HALT
_76:
	mov r0,#1
	str r0,halt
	b endCycle

val:		.word 0
gbPC:		.word 0x0100
gbSP:		.word 0xFFFE
af:			.hword 0x01b0
bc:			.hword 0x0013
de:			.hword 0x00d8
hl:			.hword 0x014d
cycleDelay:	.word 0
null:		.word 0		@ Because I can't write in half-words!

cyclesptr:		.word 0
CBcyclesptr:	.word 0
cycles:			.word 0
halt:			.word 0

@ Function initASM(int* cycles, int* opcycles); initializes various things.
initASM:
	str r0,cyclesptr
	str r1,CBcyclesptr
	bx lr
	
@ Function runFrameASM(); the main emulation loop, returns after one frame.
runFrameASM:
	stmfd sp!,{r4-r11,lr}
	str r0,cycleDelay
	loadvars
	mov regcycles,#0

startCycle:
	@ Fetch opcode and increment pc
	readMemory regpc				@ opcode is now stored in var1
	mov var1,r0
	add regpc,#1

	@ Get cycle count for this opcode
	cmp var1,#0xCB
	ldrne r0,cyclesptr
	ldreq r0,CBcyclesptr
	ldrb r1,[r0,var1]
	add regcycles,#5

	@ Jump to the appropriate opcode
	ldr r0,=op_table
	ldr r0,[r0,var1,lsl#2]
	mov pc,r0						@ branch to op_table + (opcode*4)
	@cheat
	b endCycle

endCycle:

	ldr r0,cycleDelay
	cmp regcycles,r0
	bmi startCycle

	storevars
	mov r0,regcycles
	mov r1,#0
	ldmfd sp!,{r4-r11,lr}
	bx lr

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
.word _E0,_E1,_E2,_E3,_E4,_E5,_E6,_E7,_E8,_E9,_EA,_EB,_EC,_ED,_EE,_EF
.word _F0,_F1,_F2,_F3,_F4,_F5,_F6,_F7,_F8,_F9,_FA,_FB,_FC,_FD,_FE,_FF
