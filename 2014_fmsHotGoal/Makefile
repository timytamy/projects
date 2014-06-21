CC=mcs
CFLAGS=-debug

SOURCES=hotGoalTx.cs
EXECUTABLES=hotGoalTx.exe

#this makes sure that typing 'make' will execute 'make all'
.default: all

hotGoalTx.exe: hotGoalTx.cs
	$(CC) $(CFLAGS) -out:$@ $+

all: $(OBJECTS) $(EXECUTABLES)

clean:
	rm $(EXECUTABLES)
	rm *.exe* *.pyc
