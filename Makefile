INSTALL=install -c -m 755 -o root -g wheel
CC=gcc
CCOPTS=-ansi -pedantic -pedantic-errors -Wall
PROGRAM_DIR=/usr/local/bin

all : jtool

jtool : jtool.c
	${CC} ${CCOPTS} $? -o $@
	strip $@
	${INSTALL} $@ ${PROGRAM_DIR}/$@
	rm -f $@
