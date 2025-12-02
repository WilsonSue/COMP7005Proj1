CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200809L -g
LDFLAGS = -lm

# Targets
all: client server proxy

# Client
client: client.o protocol.o
	$(CC) $(CFLAGS) -o client client.o protocol.o $(LDFLAGS)

client.o: client.c client.h protocol.h
	$(CC) $(CFLAGS) -c client.c

# Server
server: server.o protocol.o
	$(CC) $(CFLAGS) -o server server.o protocol.o $(LDFLAGS)

server.o: server.c server.h protocol.h
	$(CC) $(CFLAGS) -c server.c

# Proxy
proxy: proxy.o
	$(CC) $(CFLAGS) -o proxy proxy.o $(LDFLAGS)

proxy.o: proxy.c proxy.h
	$(CC) $(CFLAGS) -c proxy.c

# Protocol
protocol.o: protocol.c protocol.h
	$(CC) $(CFLAGS) -c protocol.c

# Clean
clean:
	rm -f *.o client server proxy
	rm -f *.log

# Test without proxy (direct communication)
test-direct:
	@echo "Starting server..."
	./server --listen-ip 127.0.0.1 --listen-port 5000 --log-file server.log &
	@sleep 1
	@echo "Server started. Run client in another terminal:"
	@echo "./client --target-ip 127.0.0.1 --target-port 5000 --timeout 2 --max-retries 5 --log-file client.log"

# Test with proxy (0% drop, 0% delay)
test-proxy-0:
	@echo "Starting server..."
	./server --listen-ip 127.0.0.1 --listen-port 5000 --log-file server.log &
	@sleep 1
	@echo "Starting proxy..."
	./proxy --listen-ip 127.0.0.1 --listen-port 4000 \
	        --target-ip 127.0.0.1 --target-port 5000 \
	        --client-drop 0 --server-drop 0 \
	        --client-delay 0 --server-delay 0 \
	        --log-file proxy.log &
	@sleep 1
	@echo "Proxy started. Run client in another terminal:"
	@echo "./client --target-ip 127.0.0.1 --target-port 4000 --timeout 2 --max-retries 5 --log-file client.log"

# Kill all processes
kill-all:
	@pkill -f "./server" || true
	@pkill -f "./proxy" || true
	@echo "All processes killed"

.PHONY: all clean test-direct test-proxy-0 kill-all