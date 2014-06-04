
;	.equ	IRQ3	P1.3
;	.equ	IRQ4	P1.4
;	.equ	IRQ7	P1.7




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
	mov  P0,   #0
	mov  P1,   #0ffh
	mov  P3,   #0ffh

; check if P3.1 and P1.2 are connected to each other
; This would mean that it is an SLH1512B or SLH1512B/LC
; If not it must be a SLH1512A or a SLH1512C or SLH1512D(whatever that is)
	mov  A,#0
	setb P3.1
	setb P1.2
	jnb  P1.2, not_B_LC
	clr  P3.1
	jb   P1.2, not_B_LC
	setb P3.1
	setb P1.2
	jnb  P3.1, not_B_LC
	clr  P1.2
	jb   P3.1, not_B_LC
	mov  A, #1 ; P3.1 and P1.2 are connected tohether
not_B_LC:
	mov  DPTR,#03fch
	movx @DPTR,A


; check if there is a receiver on that board
        clr  P3.0   ; pull receiver low => an interrupt INT0 should be raised
                    ; after 64uS. That would mean that it is a 16MHz board.
                    ; It could be an SLH1512B, SLH1512C or SLH1512D.


; read out P1 and write it to RAM

	mov  DPTR,#03fch
	mov  A,P1
	movx @DPTR,A

; read out P3 and write it to RAM
	inc  DPTR
	mov  A,P3
	movx @DPTR,A

; write 0xEE as mark to ram at 0x3ff
	inc  DPTR
	mov  A,#0EEh
	movx @DPTR,A


; check the card for being an SLH1512C,B or A(B/LC)
B012a:  mov     DPTR, #03f9h
        mov     0a9h, #0aah
        mov     A, 0a9h
        cjne    A, #0aah, not1512c
        mov     A, #04h
        jnb     21h, know_card
        inc     A
        sjmp    know_card

not1512c:
        mov     A, #80h
        jb      P1.5, know_card
        mov     A, #01h

know_card:
        movx    @DPTR, A


forever:ljmp forever


