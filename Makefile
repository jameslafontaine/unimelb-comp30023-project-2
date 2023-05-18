CC=cc
CFLAGS=-Wall -g 
RPC_SYSTEM=rpc.o
RPC_SYSTEM_A=rpc.a
SKEL_SERVER_A=skel_server.a
SKEL_CLIENT_A=skel_client.a
SKEL_SERVER_EXE=skel-server
SKEL_CLIENT_EXE=skel-client

NEW_SERVER_EXE=new-server

NEW_CLIENT_EXE=new-client
#LDFLAGS         any linker flags required

.PHONY: format all clean

skel-test: $(SKEL_SERVER_EXE) $(SKEL_CLIENT_EXE)

$(RPC_SYSTEM_A): rpc.o dict.o
	ar rcs $@ $^

$(SKEL_SERVER_A): skel_server.o
	ar rcs $@ $^

$(SKEL_CLIENT_A): skel_client.o
	ar rcs $@ $^	
	
$(SKEL_SERVER_EXE): skel_server.a rpc.a
	cc -o $@ $^ $(CFLAGS)

$(SKEL_CLIENT_EXE): skel_client.a rpc.a
	cc -o $@ $^ $(CFLAGS)

$(NEW_SERVER_EXE): src/new_server.a rpc.a
	cc -o $@ $^ $(CFLAGS)

$(NEW_CLIENT_EXE): src/new_client.a  rpc.a
	cc -o $@ $^ $(CFLAGS)

skel_client.o: src/skel_client.c
	$(CC) -c -o $@ $< $(CFLAGS)

skel_server.o: src/skel_server.c
	$(CC) -c -o $@ $< $(CFLAGS)


#$(RPC_SYSTEM): rpc.c rpc.h
#	$(CC) $(CFLAGS) -c -o $@ $<

%.o: src/%.c src/%.h
	$(CC) -c -o $@ $< $(CFLAGS)

format:
	clang-format -style=file -i *.c *.h

clean:
	rm -f *.a *.o src/*.o debug/*.o *.o src/*.o debug/*.o $(EXE) $(EXE_DEBUG)
