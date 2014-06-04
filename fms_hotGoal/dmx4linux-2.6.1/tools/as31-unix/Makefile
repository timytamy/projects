OBJECTS= run.o lexer.o parser.o symbol.o emitter.o as31.o

modules all : parser.o as31

as31 : $(OBJECTS)

clean:
	rm -f as31 *.o parser.c parser.h

distclean: clean

modules_install:

parser.c parser.h: parser.y
	bison -d -o parser.c parser.y
