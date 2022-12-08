include config.mk

SRC = server.c http.c response.c routes.c
OBJ = ${SRC:.c=.o}

all: options server

options:
	@echo server build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	${CC} -c ${CFLAGS} $<

${OBJ}: config.mk

server: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	rm -f server ${OBJ}

clangd: clean
	bear -- make

.PHONY: all options clean clangd
