CC=cc
CFLAGS=-Wall -g 
RPC_SYSTEM=rpc.o
RPC_SYSTEM_A=rpc.a
RPC_SYSTEM_A_DEBUG=debug/rpc.a

RPC_SERVER=rpc-server
RPC_CLIENT=rpc-client

TEST_SERVER_EXE=test-server
TEST_CLIENT_EXE=test-client
TEST_SERVER_DEBUG=test-server-debug
TEST_CLIENT_DEBUG=test-client-debug

SKEL_SERVER_A=skel_server.a
SKEL_CLIENT_A=skel_client.a
SKEL_SERVER_EXE=skel-server
SKEL_CLIENT_EXE=skel-client

# Any linker flags required
LDFLAGS=-lpthread         

.PHONY: format all clean

exe: $(RPC_SERVER) $(RPC_CLIENT)

$(RPC_SYSTEM_A): src/rpc.o src/dict.o src/transfer_utils.o
	ar rcs $@ $^

$(RPC_SYSTEM_A_DEBUG): debug/rpc.o debug/dict.o
	ar rcs $@ $^

$(RPC_SERVER): src/test_server.a $(RPC_SYSTEM_A) 
	cc -o $@ $^ $(CFLAGS) $(LDFLAGS)

$(RPC_CLIENT): src/test_client.a $(RPC_SYSTEM_A) 
	cc -o $@ $^ $(CFLAGS) $(LDFLAGS)

# --------------------------------------------------
# TEST SERVER AND CLIENT
# --------------------------------------------------

test-systems: $(TEST_SERVER_EXE) $(TEST_CLIENT_EXE)

test-debug: $(TEST_SERVER_DEBUG) $(TEST_CLIENT_DEBUG)

$(TEST_SERVER_EXE): src/test_server.a rpc.a
	cc -o $@ $^ $(CFLAGS) $(LDFLAGS)

$(TEST_CLIENT_EXE): src/test_client.a  rpc.a
	cc -o $@ $^ $(CFLAGS) $(LDFLAGS)

$(TEST_SERVER_DEBUG): debug/test_server.a debug/rpc.a
	cc -o $@ $^ $(CFLAGS)

$(TEST_CLIENT_DEBUG): debug/test_client.a debug/rpc.a
	cc -o $@ $^ $(CFLAGS)

# --------------------------------------------------
# SKELETON SERVER AND CLIENT
# --------------------------------------------------

skel-test: $(SKEL_SERVER_EXE) $(SKEL_CLIENT_EXE)

$(SKEL_SERVER_A): src/skel_server.o
	ar rcs $@ $^

$(SKEL_CLIENT_A): src/skel_client.o
	ar rcs $@ $^	
	
$(SKEL_SERVER_EXE): skel_server.a rpc.a
	cc -o $@ $^ $(CFLAGS)

$(SKEL_CLIENT_EXE): skel_client.a rpc.a
	cc -o $@ $^ $(CFLAGS)

skel_client.o: src/skel_client.c
	$(CC) -c -o $@ $< $(CFLAGS)

skel_server.o: src/skel_server.c
	$(CC) -c -o $@ $< $(CFLAGS)

# --------------------------------------------------
# MISCELLANEOUS
# --------------------------------------------------


debug/%.o: debug/%.c debug/%.h
	$(CC) -c -o $@ $< $(CFLAGS)

src/%.o: src/%.c src/%.h
	$(CC) -c -o $@ $< $(CFLAGS)

format:
	clang-format -style=file -i *.c *.h

clean:
	rm -f *.a *.o src/*.o debug/*.o *.o src/*.o debug/*.o $(SKEL_SERVER_EXE) \
	      $(SKEL_CLIENT_EXE) $(TEST_CLIENT_EXE) $(TEST_SERVER_EXE) $(TEST_CLIENT_DEBUG) \
		  $(TEST_SERVER_DEBUG) $(RPC_SERVER) $(RPC_CLIENT)

