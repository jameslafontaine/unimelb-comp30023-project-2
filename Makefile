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
EXE_DEBUG=alloc-debug
DLL_O=d_linked_list.o
PUTILS_O=process_utils.o
PROC_MAN_O=process_manager.o 
INPUT_O=input_parser.o
OUTPUT_O=output.o
T4_O=task4.o

all: $(EXE)

$(EXE): src/main.c src/$(PROC_MAN_O) src/$(T4_O) src/$(INPUT_O) src/$(OUTPUT_O) src/$(PUTILS_O) src/$(DLL_O) 
	$(CC) $(CFLAGS) -o $(EXE) $^ -lm

$(EXE_DEBUG): debug/main.c debug/$(PROC_MAN_O) debug/$(T4_O) debug/$(INPUT_O) debug/$(OUTPUT_O) debug/$(PUTILS_O) debug/$(DLL_O) 
	$(CC) $(CFLAGS) -o $(EXE_DEBUG) $^ -lm

%.o: %.c %.h
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm -f *.o src/*.o debug/*.o $(EXE) $(EXE_DEBUG)

format:
	clang-format -i *.c *.h