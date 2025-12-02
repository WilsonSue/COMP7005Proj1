# UDP Reliable Messaging System

A reliable messaging system built on top of UDP with client-server architecture and an optional proxy for network simulation.

## Components

### 1. Client (`client.c`, `client.h`)
- Reads messages from stdin
- Implements reliability with sequence numbers and retransmission
- Waits for ACKs with configurable timeout
- Retries up to a maximum number of attempts

### 2. Server (`server.c`, `server.h`)
- Listens for UDP messages
- Sends ACKs for received messages
- Prints received messages to stdout
- Logs all activity

### 3. Proxy (`proxy.c`, `proxy.h`)
- Sits between client and server
- Simulates unreliable network conditions:
    - Packet dropping (configurable %)
    - Packet delays (configurable % and time range)
- Independent configuration for each direction

### 4. Protocol (`protocol.c`, `protocol.h`)
- Message format with magic number, type, sequence number, and payload
- Serialization/deserialization for network transmission

## Building

```bash
make
```

This creates three executables:
- `client`
- `server`
- `proxy`

## Usage

### Direct Communication (No Proxy)

**Terminal 1 - Start Server:**
```bash
./server --listen-ip 127.0.0.1 --listen-port 5000 --log-file server.log
```

**Terminal 2 - Run Client:**
```bash
./client --target-ip 127.0.0.1 --target-port 5000 --timeout 2 --max-retries 5 --log-file client.log
```

Then type messages and press Enter.

### With Proxy

**Terminal 1 - Start Server:**
```bash
./server --listen-ip 127.0.0.1 --listen-port 5000 --log-file server.log
```

**Terminal 2 - Start Proxy:**
```bash
./proxy --listen-ip 127.0.0.1 --listen-port 4000 \
        --target-ip 127.0.0.1 --target-port 5000 \
        --client-drop 10 --server-drop 5 \
        --client-delay 20 --server-delay 15 \
        --client-delay-time-min 100 --client-delay-time-max 200 \
        --server-delay-time-min 150 --server-delay-time-max 300 \
        --log-file proxy.log
```

**Terminal 3 - Run Client:**
```bash
./client --target-ip 127.0.0.1 --target-port 4000 --timeout 2 --max-retries 5 --log-file client.log
```

## Command-Line Arguments

### Client
- `--target-ip <ip>`: Server/proxy IP address
- `--target-port <port>`: Server/proxy port
- `--timeout <seconds>`: ACK timeout (default: 2.0)
- `--max-retries <n>`: Maximum retries per message (default: 5)
- `--log-file <file>`: Log file path (optional)

### Server
- `--listen-ip <ip>`: IP to bind to
- `--listen-port <port>`: Port to listen on
- `--log-file <file>`: Log file path (optional)

### Proxy
- `--listen-ip <ip>`: IP to bind for client packets
- `--listen-port <port>`: Port for client packets
- `--target-ip <ip>`: Server IP
- `--target-port <port>`: Server port
- `--client-drop <%>`: Drop rate client→server (0-100)
- `--server-drop <%>`: Drop rate server→client (0-100)
- `--client-delay <%>`: Delay rate client→server (0-100)
- `--server-delay <%>`: Delay rate server→client (0-100)
- `--client-delay-time-min <ms>`: Min delay for client packets
- `--client-delay-time-max <ms>`: Max delay for client packets
- `--server-delay-time-min <ms>`: Min delay for server packets
- `--server-delay-time-max <ms>`: Max delay for server packets
- `--log-file <file>`: Log file path (optional)

## Testing Scenarios

### Test 1: 0% drop, 0% delay
```bash
./proxy --listen-ip 127.0.0.1 --listen-port 4000 \
        --target-ip 127.0.0.1 --target-port 5000 \
        --client-drop 0 --server-drop 0 \
        --client-delay 0 --server-delay 0
```

### Test 2: 5% drop, 0% delay
```bash
./proxy --listen-ip 127.0.0.1 --listen-port 4000 \
        --target-ip 127.0.0.1 --target-port 5000 \
        --client-drop 5 --server-drop 5 \
        --client-delay 0 --server-delay 0
```

### Test 3: 10% drop, 0% delay
```bash
./proxy --listen-ip 127.0.0.1 --listen-port 4000 \
        --target-ip 127.0.0.1 --target-port 5000 \
        --client-drop 10 --server-drop 10 \
        --client-delay 0 --server-delay 0
```

### Test 4: 0% drop, 50% delay (100-500ms)
```bash
./proxy --listen-ip 127.0.0.1 --listen-port 4000 \
        --target-ip 127.0.0.1 --target-port 5000 \
        --client-drop 0 --server-drop 0 \
        --client-delay 50 --server-delay 50 \
        --client-delay-time-min 100 --client-delay-time-max 500 \
        --server-delay-time-min 100 --server-delay-time-max 500
```

### Test 5: 0% drop, 100% delay (>= client timeout)
```bash
# With client timeout of 2 seconds, use delay > 2000ms
./proxy --listen-ip 127.0.0.1 --listen-port 4000 \
        --target-ip 127.0.0.1 --target-port 5000 \
        --client-drop 0 --server-drop 0 \
        --client-delay 100 --server-delay 100 \
        --client-delay-time-min 2500 --client-delay-time-max 3000 \
        --server-delay-time-min 2500 --server-delay-time-max 3000
```

### Test 6: 50% drop, 50% delay
```bash
./proxy --listen-ip 127.0.0.1 --listen-port 4000 \
        --target-ip 127.0.0.1 --target-port 5000 \
        --client-drop 50 --server-drop 50 \
        --client-delay 50 --server-delay 50 \
        --client-delay-time-min 2500 --client-delay-time-max 3000 \
        --server-delay-time-min 2500 --server-delay-time-max 3000
```

## Log Visualization

After running tests, visualize the logs:

```bash
python3 visualize_logs.py
```

This generates statistics including:
- Messages sent/received
- Retransmission attempts
- Success rates
- Drop rates
- Delay statistics

## Protocol Format

```
+--------+------+------------+-------------+---------+
| Magic  | Type | Seq Number | Payload Len | Payload |
| 2 bytes| 1 byte| 4 bytes   | 2 bytes     | N bytes |
+--------+------+------------+-------------+---------+
```

- **Magic**: 0x55AA (validation)
- **Type**: 1 (DATA) or 2 (ACK)
- **Seq Number**: Unique sequence number
- **Payload Len**: Length of payload
- **Payload**: Actual message data

## Cleanup

```bash
make clean        # Remove binaries and logs
make kill-all     # Kill running server/proxy processes
```

## Notes

- All communication uses UDP (no TCP)
- No connection setup/teardown
- Supports one client at a time
- Logs to both stderr and file
- Network byte order used for multi-byte fields