
        .org 0

start:	mov  DPTR,#03fch
	movx A,@DPTR
	mov  DPTR,#0ffffh
	movx @DPTR,A

        ljmp start


