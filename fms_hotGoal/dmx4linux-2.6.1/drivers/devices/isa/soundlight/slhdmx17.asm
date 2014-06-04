	.org 0
boot:	ljmp	start

; INT0 - Break on Input universe0
	.org 3
	jnb	1bh, D001b
	clr	RI
	ljmp	B023a


	.org 00bh
	ljmp	B02e8


; INT1 - Break on Input universe1
	.org 0013h
B0013:	jnb	1ch, D001b
	clr	0c0h
	ljmp	B0245

D001b:	reti

	.org 0023h
B0023:	jb	RI, B002c
	clr	TI
	setb	20h
	reti

B002b:	reti

B002c:	clr	RI
	ljmp	B0250

	.org 0033h
B0033:	reti

	.org 003bh
B003b:	jb	0c0h, B007c
	clr	0c1h
	setb	21h
	reti

B0043:	reti

	.org 004bh
B004b:	reti

	.org 0053h
B0053:	reti

	.org 005bh
B005b:	reti

	.org 0063h
B0063:	ljmp	start
B0066:	reti


	.org 006bh
B006b:	reti

	.org 0073h
B0073:	reti

	.org 007ch
B007c:	clr	0c0h
	ljmp	B029c


; This is the main entry point
start:	mov	SP, #50h
	mov	PCON, #00h
	mov	PSW, #00h
	mov	TMOD, #31h
	mov	TL0, #01h
	mov	TH0, #7fh
	mov	P1, #0ffh
	mov	P3, #0ffh
	mov	TCON, #15h
	mov	SCON, #98h
	mov	0c0h, #98h
	mov	IP, #50h
	mov	0f8h, #10h
	mov	0e8h, #80h
	mov	8eh, #0c2h
	lcall	B022d ; set something

	mov	A, 29h
	cjne	A, #0aah, B00b8
	sjmp	B00be

B00b8:	lcall	B01ca
	mov	29h, #0aah

B00be:	clr	REN
	mov	20h, #00h
	setb	02h
	clr	07h
	setb	06h
	setb	REN
	setb	0c4h
	setb	1bh
	setb	1ch
	clr	04h
	mov	P3, #0ffh
	clr	RI
	clr	TI
	clr	20h
	clr	0c0h
	clr	0c1h
	mov	IE, #0d5h
	mov	DPTR, #0400h
	mov	A, #00h
	movx	@DPTR, A
C00e9:	lcall	C0101
	lcall	B010b
	jb	0fh, C00fb
	jnb	0eh, C00fb
	lcall	C0101
	jnb	0dh, C00e9
C00fb:	lcall	B017a
	ljmp	C00e9
C0101:	mov	0c7h, #0aah
	mov	0c7h, #55h
	orl	0d8h, #01h
	ret

B010b:	mov	R6, #00h
	mov	R7, #01h
	mov	0c4h, #60h
	mov	DPTR, #03f0h
	movx	A, @DPTR
	jz	B012a
	inc	DPTR
	movx	A, @DPTR
	mov	3dh, A
	inc	DPTR
	inc	DPTR
	movx	A, @DPTR
	mov	R6, A
	inc	DPTR
	movx	A, @DPTR
	mov	R7, A
	inc	DPTR
	movx	A, @DPTR
	mov	21h, A
	inc	DPTR
	inc	DPTR
	inc	DPTR

B012a:	mov	DPTR, #03f9h
	mov	0a9h, #0aah
	mov	A, 0a9h
	cjne	A, #0aah, B013d
	mov	A, #04h
	jnb	21h, D0144
	inc	A
	sjmp	D0144

B013d:	mov	A, #80h
	jb	P1.5, D0144
	mov	A, #01h

D0144:	movx	@DPTR, A
	inc	DPTR
	djnz	3ah, B0152
	inc	3ah
	mov	A, #00h
	movx	@DPTR, A
	inc	DPTR
	movx	@DPTR, A
	sjmp	B015b

B0152:	mov	A, 30h
	movx	@DPTR, A
	inc	DPTR
	mov	A, 31h
	anl	A, #01h
	movx	@DPTR, A

B015b:	inc	DPTR
	djnz	3bh, B0168
	inc	3bh
	mov	A, #00h
	movx	@DPTR, A
	inc	DPTR
	movx	@DPTR, A
	sjmp	B0171

B0168:	mov	A, 32h
	movx	@DPTR, A
	inc	DPTR
	mov	A, 33h
	anl	A, #01h
	movx	@DPTR, A

B0171:	inc	DPTR
	mov	A, 3ch
	movx	@DPTR, A
	mov	0c4h, #61h
	inc	R7
	ret

B017a:	clr	20h
	clr	P3.1 ; TXD
	clr	P1.3
	mov	SBUF, #55h
B0183:	jnb	20h, B0183
	clr	20h
	mov	SBUF, #55h
	mov	R5, #14h
B018d:	jnb	20h, B018d
	clr	20h
	setb	P3.1 ; TXD
	setb	P1.3
B0196:	djnz	R5, B0196
	mov	SBUF, #00h
	mov	0c1h, #00h
	mov	DPL, #00h
	mov	DPH, #04h
	movx	A, @DPTR
	mov	B, A
	orl	DPH, #02h
	movx	A, @DPTR
B01ab:	jnb	20h, B01ab
C01ae:	mov	SBUF, B
	mov	0c1h, A
	clr	20h
	inc	DPTR
	anl	DPH, #0fdh
	movx	A, @DPTR
	mov	B, A
	orl	DPH, #02h
	movx	A, @DPTR
B01c0:	jnb	20h, B01c0
	djnz	R6, C01ae
	djnz	R7, C01ae
	clr	20h
	ret

; function
B01ca:	mov	IE, #00h
	push	00h
	mov	R0, #80h
B01d1:	mov	@R0, #00h
	inc	R0
	cjne	R0, #00h, B01d1
	pop	00h
	lcall	B01e9
	lcall	B01f9
	lcall	B0209
	lcall	B021b
	mov	IE, #0d5h
	ret

; function
B01e9:	mov	DPH, #08h
	mov	DPL, #00h
B01ef:	mov	A, #00h
	movx	@DPTR, A
	inc	DPTR
	mov	A, DPH
	cjne	A, #0ch, B01ef
	ret

; function
B01f9:	mov	DPH, #0ch
	mov	DPL, #00h
B01ff:	mov	A, #00h
	movx	@DPTR, A
	inc	DPTR
	mov	A, DPH
	cjne	A, #10h, B01ff
	ret

;function
B0209:	clr	01h
	mov	DPH, #04h
	mov	DPL, #00h
B0211:	mov	A, #00h
	movx	@DPTR, A
	inc	DPTR
	mov	A, DPH
	cjne	A, #06h, B0211
	ret

; function
B021b:	clr	01h
	mov	DPH, #06h
	mov	DPL, #00h
B0223:	mov	A, #00h
	movx	@DPTR, A
	inc	DPTR
	mov	A, DPH
	cjne	A, #08h, B0223
	ret


B022d:	mov	28h, #02h
	mov	3dh, #00h
	mov	24h, #00h
	mov	21h, #80h
	ret


; called if a dmx-break interrupt arrived.
B023a:	clr	1dh
	setb	19h
	mov	3ah, #46h
	anl	3ch, #0f0h
	reti

B0245:	clr	1eh
	setb	1ah
	mov	3bh, #46h
	anl	3ch, #0fh
	reti

B0250:	jb	1dh, B027e
	jnb	RB8, B0279
	push	PSW
	jnb	19h, B027f
	clr	19h
	mov	R4, SBUF
	cjne	R4, #00h, B0271
	mov	30h, 36h
	mov	31h, 37h
	mov	36h, #00h
	mov	37h, #08h
	pop	PSW
	reti

B0271:	setb	1dh
	orl	3ch, #08h
	pop	PSW
	reti

B0279:	setb	1dh
	orl	3ch, #04h
B027e:	reti

	
B027f:	push	ACC
	mov	A, SBUF
	mov	86h, #01h
	mov	84h, 36h
	mov	85h, 37h
	movx	@DPTR, A
	inc	86h
	inc	36h
	mov	A, 36h
	jnz	B0297
	inc	37h
B0297:	pop	ACC
	pop	PSW
	reti

B029c:	jb	1eh, B02ca
	jnb	0c2h, B02c5
	push	PSW
	jnb	1ah, B02cb
	clr	1ah
	mov	R4, 0c1h
	cjne	R4, #00h, B02bd
	mov	32h, 38h
	mov	33h, 39h
	mov	38h, #00h
	mov	39h, #0ch
	pop	PSW
	reti

B02bd:	setb	1eh
	orl	3ch, #80h
	pop	PSW
	reti

B02c5:	setb	1eh
	orl	3ch, #40h
B02ca:	reti


B02cb:	push	ACC
	mov	A, 0c1h
	mov	86h, #01h
	mov	84h, 38h
	mov	85h, 39h
	movx	@DPTR, A
	inc	38h
	inc	86h
	mov	A, 38h
	jnz	B02e3
	inc	39h
B02e3:	pop	ACC
	pop	PSW
	reti

B02e8:	reti
B02e9:	mov	TL0, #01h
	mov	TH0, #7fh
	reti

