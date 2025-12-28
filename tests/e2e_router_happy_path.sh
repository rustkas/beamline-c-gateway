#!/bin/bash
# e2e_router_happy_path.sh - Scenario 1: Happy path validation
#
# Purpose: Validate basic request-reply flow through full stack
# Requirements: Router deployed, NATS accessible, IPC Gateway running

set -e

GATEWAY_SOCKET=${IPC_SOCKET_PATH:-/tmp/beamline-gateway.sock}
NUM_REQUESTS=${1:-1000}
ARTIFACT_DIR="artifacts/e2e"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

mkdir -p "$ARTIFACT_DIR"

echo "=========================================="
echo "E2E Router Test - Happy Path"
echo "=========================================="
echo "Gateway socket: $GATEWAY_SOCKET"
echo "Requests: $NUM_REQUESTS"
echo ""

# Check prerequisites
if [ ! -S "$GATEWAY_SOCKET" ]; then
    echo "ERROR: Gateway socket not found at $GATEWAY_SOCKET"
    echo "Is IPC Gateway running?"
    exit 1
fi

# Create test client
cat > /tmp/e2e_client.py << 'EOF'
#!/usr/bin/env python3
import socket
import json
import time
import sys

socket_path = sys.argv[1]
num_requests = int(sys.argv[2])

success = 0
failed = 0
latencies = []

for i in range(num_requests):
    try:
        # Connect
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        sock.settimeout(5.0)
        sock.connect(socket_path)
        
        # Send request
        request = {
            "task_id": f"test_{i}",
            "payload": "hello world"
        }
        
        start = time.time()
        sock.sendall(json.dumps(request).encode() + b'\n')
        
        # Receive response
        response = sock.recv(4096)
        elapsed = (time.time() - start) * 1000  # ms
        
        sock.close()
        
        if response:
            success += 1
            latencies.append(elapsed)
        else:
            failed += 1
            
        if (i + 1) % 100 == 0:
            print(f"Progress: {i+1}/{num_requests}", file=sys.stderr)
    except Exception as e:
        failed += 1
        print(f"Error: {e}", file=sys.stderr)

# Results
latencies.sort()
p50 = latencies[len(latencies)//2] if latencies else 0
p99 = latencies[int(len(latencies)*0.99)] if latencies else 0

print(f"Success: {success}")
print(f"Failed: {failed}")
print(f"Success rate: {success/num_requests*100:.2f}%")
print(f"Latency p50: {p50:.2f}ms")
print(f"Latency p99: {p99:.2f}ms")

sys.exit(0 if success == num_requests else 1)
EOF

chmod +x /tmp/e2e_client.py

# Run test
echo "Running $NUM_REQUESTS requests..."
python3 /tmp/e2e_client.py "$GATEWAY_SOCKET" "$NUM_REQUESTS" 2>&1 | tee "$ARTIFACT_DIR/happy_path_$TIMESTAMP.log"
EXIT_CODE=$?

echo ""
echo "=========================================="
if [ $EXIT_CODE -eq 0 ]; then
    echo "✅ PASS: Happy path test successful"
else
    echo "❌ FAIL: Happy path test failed"
fi
echo "=========================================="
echo ""
echo "Results: $ARTIFACT_DIR/happy_path_$TIMESTAMP.log"
echo ""

exit $EXIT_CODE
