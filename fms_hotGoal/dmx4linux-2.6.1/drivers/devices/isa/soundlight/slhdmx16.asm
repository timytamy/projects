
; Reset vector 
	.org 0
reset:
	ljmp B007d


; INT0
	.org 3
B0003:
	reti			; INT0 => break received

B0006:
	reti

; INT1
	.org 000bh
B000b:
	reti


	.org 0013h
B0013:
	reti

	.org 001bh
B001b:
	reti

; UART0 Interrupt
	.org 0023h
B0023:
;	jb RI, B002c
;	jb TI, B0034
	reti

	.org 002bh
B002b:
	reti
B002c:
	clr RI
;	ljmp B01df

	.org 0033h
B0033:
	reti
B0034:
;	ljmp B01de

	.org 003bh
B003b:
	reti

	.org 0043h
B0043:
	reti

	.org 004bh
B004b:
	reti

	.org 0053h
B0053:
	reti

	.org 005bh
B005b:
	reti

	.org 0063h
B0063:
	reti

	.org 006bh
B006b:
	reti

	.org 0073h
B0073:
	reti

	.org 007dh
B007d:
		mov PCON, #00h
		mov PSW,  #00h
		mov SP,   #60h
		mov IP,   #10h
		setb 08h
		mov SCON, #88h
		clr REN
		mov IE,   #80h
		mov TMOD, #11h
		mov TCON, #01h
		mov P1,   #65h
		mov P3,   #0ffh
		mov 3ah,  #01h
		jnb P1.5, D00a8
		mov 3ah,  #80h
D00a8:
		.equ	conf_breaksize,030h
		.equ	conf_startbyte,031h
		.equ	conf_mbf_time,032h
		.equ	conf_channels_lo,033h
		.equ	conf_channels_hi,034h
		.equ	conf_repeatflag,035h
		.equ	conf_idt_time,036h
		.equ	conf_txrx_flag,038h
		.equ	conf_return,039h

	;; read in the configuration
;		mov	DPTR,#03F0h
;		mov	R0,#conf_breaksize
;		mov	R1,#10
;initloop:	movx	A,@DPTR
;		inc	DPTR
;		mov	@R0,A
;		inc	R0
;		djnz	R1,initloop
	
		mov	conf_channels_hi,#1
		mov	conf_channels_lo,#0
	
	;; signal an ok
		mov	DPTR,#03FAh
		mov	A,#'O'
		movx	@DPTR,A

	;; ---------------------------------------------------------------------

loop:
		clr	IE

	;; send break
		mov     TH0,#255-0
		mov     TL0,#255-78h
		clr     TF0
		clr	P3.1
		setb    TR0

brkloop:	jnb     TF0,brkloop
		setb	P3.1
		clr     TR0

	;; mark after break
		mov	TH0, #255-0
		mov	TL0, #255-6
	setb    TR0
mabloop:	jnb     TF0,mabloop
	        clr     TR0

	;; send startbyte
		clr     TI
		mov	A,#0
		mov	SBUF,A

	;; send up to 512 channels
		mov	DPTR,#0400h
		mov	A,conf_channels_hi
		anl	A,#1
		inc	A
		mov	R0,A
		mov	R1,#0
		cjne	A,#1,serloop
outerloop:	mov	R1,conf_channels_lo
serloop:	jnb     TI,serloop
	        clr     TI	
		movx	A,@DPTR
		inc	DPTR
		mov	SBUF,A
		djnz	R1,serloop
		djnz	R0,outerloop

txready:jnb     TI,txready  ; warten bis das letzte Byte "ubertragen wurde
		clr     TI

		mov	DPTR,#03FBh
		mov	A,#'K'
		movx	@DPTR,A

wait:		ljmp	wait

