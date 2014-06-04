	.org 0
	ljmp start

; all the interrup vectors
	.org 0003h
	reti

	.org 000bh
	reti

	.org 0013h
	reti

	.org 001bh
	reti

	.org 0023h
	reti

	.org 002bh
	reti

	.org 0033h
	reti

	.org 003bh
	reti

	.org 0043h
	reti

	.org 004bh
	reti

	.org 0053h
	reti

	.org 005bh
	reti

	.org 0063h
	reti

	.org 006bh
	reti

	.org 0073h
	reti



	.org 007dh
start:
   mov  PCON, #00h
   mov  PSW,  #00h
   mov  SP,   #60h
   mov  IP,   #10h
   mov  IE,   #80h
   mov  SCON, #88h
   mov  P1,   #0ffh
   mov  P3,   #0ffh

;  clear ISR-Register
   mov  DPTR,#0ffffh
   mov  A,#0
   movx @DPTR,A

B0095:
   mov  DPTR, #03f0h
   movx A, @DPTR      ; [31] = {0x3f0}
   mov  31h, A

   inc  DPTR
   movx A, @DPTR      ; [32] = {0x3f1}
   mov  32h, A

   inc  DPTR
   movx A, @DPTR      ; [33] = {0x3f2}+1
   mov  33h, A
   inc  33h

   inc  DPTR
   movx A, @DPTR      ; [34] = {0x3f3}
   mov  34h, A

   inc  DPTR
   movx A, @DPTR      ; [35] = {0x3f4}
   mov  35h, A

   inc  DPTR
   movx A, @DPTR      ; [36] = {0x3f5}
   mov  36h, A

   inc  DPTR
   movx A, @DPTR      ; [39] = ({0x3f6}>>1)+1
   clr  C
   rrc  A
   mov  39h, A
   inc  39h

   inc  DPTR
   movx A, @DPTR      ; [3c] = {0x3f7}
   mov  3ch, A

   inc  DPTR
   movx A, @DPTR      ; [37] = {0x3f8}
   mov  37h, A

   inc  DPTR
   mov  38h, #01h     ; [38] = (P1.5)?0x80:1;
   jnb  P1.5, B00cb
   mov  38h, #80h

B00cb:
   mov  A, 38h
   movx @DPTR, A      ; {0x3f9} = 0x38
   mov  DPTR, #0400h
   mov  R2, 35h
   mov  R1, 34h
   mov  A, 3ch
   anl  A, #80h
   jnz  B00eb
   clr  P1.2

B00dd:
   djnz 31h, B00dd
   setb P1.2
   mov  DPTR, #0400h
   inc  R2
   nop
   nop
   ljmp B00f6

B00eb:
   clr  P1.2

B00ed:
   djnz 31h, B00ed
   setb P1.2
   mov  DPTR, #0400h
   inc  R2

B00f6:
   clr  P1.2
   mov  A, 32h
   rrc  A
   mov  P1.2, C
   nop
   rrc  A
   mov  P1.2, C
   nop
   rrc  A
   mov  P1.2, C
   nop
   rrc  A
   mov  P1.2, C
   nop
   rrc  A
   mov  P1.2, C
   nop
   rrc  A
   mov  P1.2, C
   nop
   rrc  A
   mov  P1.2, C
   nop
   rrc  A
   mov  P1.2, C
   nop
   nop
   nop
   setb P1.2
   nop
   mov  R3, 39h

B0121:
   djnz R3, B0121

C0123:
   movx A, @DPTR
   clr  P1.2
   nop
   rrc  A
   mov  P1.2, C
   nop
   rrc  A
   mov  P1.2, C
   nop
   rrc  A
   mov  P1.2, C
   nop
   rrc  A
   mov  P1.2, C
   nop
   rrc  A
   mov  P1.2, C
   nop
   rrc  A
   mov  P1.2, C
   nop
   rrc  A
   mov  P1.2, C
   nop
   rrc  A
   mov  P1.2, C
   inc  DPTR
   mov  A, 39h
   setb P1.2
   mov  R3, A

B014c:
   djnz R3, B014c

   djnz R1, C0123

   djnz R2, C0123

B0152:
   djnz 33h, B0152
   mov  A, 36h

B0157:
	jz   B0157  ; (single shot mode => wait forever)

	cjne A,#3,no_irq3
	mov  DPTR,#0ffffh
	mov  A,#023h
	movx @DPTR,A
	clr  P1.3  ; raise interrupt
	nop
	nop
	nop
	setb P1.3
	ljmp  loop_forever

no_irq3:

	cjne A,#4,no_irq4
	mov  DPTR,#0ffffh
	mov  A,#023h
	movx @DPTR,A
	clr  P1.4  ; raise interrupt
	nop
	nop
	nop
	setb P1.4
	ljmp  loop_forever

no_irq4:
	ljmp B0095

loop_forever:
	ljmp  loop_forever

;	.org 015ch
;copyright:
;	.byte "(C) D. Hoffmann & SLH 1994/95 with interrupt extension by M.Stickel"


