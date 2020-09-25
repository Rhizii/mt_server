CC=gcc
CFLAGS=-Wall -Werror -Werror=vla -Werror=return-type -g -std=gnu11 -lm -lpthread -lrt 

CFLAG_SAN=$(CFLAGS) -fsanitize=address -g
DEPS=server.h requests.h compression.h thread_pool.h 
OBJ=server.o requests.o compression.o thread_pool.o 

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

server: $(OBJ)
	$(CC) -o $@ $^ $(CFLAG_SAN)



clean:
	rm *.o