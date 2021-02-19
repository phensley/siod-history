# Note: add the -f68881 flag if you are on a SUN III.
siod:	siod.o slib.o
	cc -o siod siod.o slib.o
siod.o: siod.c siod.h
	cc -O -c siod.c
slib.o:	slib.c siod.h
	cc -O -c slib.c

