CC=cc
RPC_SYSTEM=rpc.o

.PHONY: format all

all: $(RPC_SYSTEM)

$(RPC_SYSTEM): rpc.c rpc.h
	$(CC) -c -o $@ $<

# RPC_SYSTEM_A=rpc.a
# $(RPC_SYSTEM_A): rpc.o
# 	ar rcs $(RPC_SYSTEM_A) $(RPC_SYSTEM)

format:
	clang-format -style=file -i *.c *.h

CC=gcc
CFLAGS=-Wall -g 
EXE=allocate

all: $(EXE)

$(EXE): src/main.c src/$(PROC_MAN_O) src/$(T4_O) src/$(INPUT_O) src/$(OUTPUT_O) src/$(PUTILS_O) src/$(DLL_O) 
	$(CC) $(CFLAGS) -o $(EXE) $^ -lm

#gcc -o server server.a rpc.a

#gcc -o client client.a rpc.a

%.o: %.c %.h
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm -f *.o src/*.o debug/*.o $(EXE) $(EXE_DEBUG)

format:
	clang-format -i *.c *.h