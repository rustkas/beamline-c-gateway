#!/bin/bash
# start_local_infrastructure.sh - Start complete local E2E environment
#
# Starts:
# 1. NATS server
# 2. Mock Router
# 3. IPC Gateway server

set -e

SOCKET_PATH=${IPC_SOCKET_PATH:-/tmp/beamline-gateway.sock}
NATS_PORT=4222

echo "=========================================="
echo "Local E2E Infrastructure Setup"
echo "=========================================="
echo ""

# Cleanup function
cleanup() {
    echo ""
    echo "Cleaning up..."
    
    # Kill all started processes
    if [ -n "$NATS_PID" ]; then
        kill $NATS_PID 2>/dev/null || true
        echo "✓ Stopped NATS server"
    fi
    
    if [ -n "$ROUTER_PID" ]; then
        kill $ROUTER_PID 2>/dev/null || true
        echo "✓ Stopped mock Router"
    fi
    
    if [ -n "$GATEWAY_PID" ]; then
        kill $GATEWAY_PID 2>/dev/null || true
        echo "✓ Stopped IPC Gateway"
    fi
    
    # Clean socket
    rm -f "$SOCKET_PATH"
    
    echo "✓ Cleanup complete"
    exit 0
}

trap cleanup EXIT INT TERM

# 1. Start NATS server
echo "=== Starting NATS Server ==="

if ! command -v nats-server &> /dev/null; then
    echo "✗ nats-server not found"
    exit 1
fi

killall -9 nats-server 2>/dev/null || true
sleep 1

nats-server -p $NATS_PORT -a 127.0.0.1 > /tmp/nats-e2e.log 2>&1 &
NATS_PID=$!

sleep 2

if nc -zv localhost $NATS_PORT 2>&1 | grep -q "succeeded"; then
    echo "✓ NATS server running (PID: $NATS_PID, port: $NATS_PORT)"
else
    echo "✗ NATS server failed to start"
    exit 1
fi

echo ""

# 2. Start Mock Router
echo "=== Starting Mock Router ==="

if [ ! -f "tests/mock_router.py" ]; then
    echo "✗ Mock Router not found"
    exit 1
fi

python3 tests/mock_router.py > /tmp/mock-router.log 2>&1 &
ROUTER_PID=$!

sleep 2

if kill -0 $ROUTER_PID 2>/dev/null; then
    echo "✓ Mock Router running (PID: $ROUTER_PID)"
else
    echo "✗ Mock Router failed to start"
    cat /tmp/mock-router.log
    exit 1
fi

echo ""

# 3. Start IPC Gateway Server  
echo "=== Starting IPC Gateway Server ==="

if [ ! -f "build/ipc-server-demo" ]; then
    echo "✗ IPC Gateway server not built"
    echo "Run: cd build && make ipc-server-demo"
    exit 1
fi

rm -f "$SOCKET_PATH"

build/ipc-server-demo "$SOCKET_PATH" > /tmp/ipc-gateway.log 2>&1 &
GATEWAY_PID=$!

sleep 2

if [ -S "$SOCKET_PATH" ]; then
    echo "✓ IPC Gateway running (PID: $GATEWAY_PID, socket: $SOCKET_PATH)"
else
    echo "✗ IPC Gateway failed to start"
    cat /tmp/ipc-gateway.log
    exit 1
fi

echo ""
echo "=========================================="
echo "✅ All Infrastructure Running"
echo "=========================================="
echo ""
echo "Components:"
echo "  NATS Server:   localhost:$NATS_PORT (PID: $NATS_PID)"
echo "  Mock Router:   PID $ROUTER_PID"
echo "  IPC Gateway:   $SOCKET_PATH (PID: $GATEWAY_PID)"
echo ""
echo "Logs:"
echo "  NATS:    /tmp/nats-e2e.log"
echo "  Router:  /tmp/mock-router.log"
echo "  Gateway: /tmp/ipc-gateway.log"
echo ""
echo "Test with:"
echo "  echo '{\"task_id\":\"test1\"}' | nc -U $SOCKET_PATH"
echo ""
echo "Press Ctrl+C to stop all services"
echo ""

# Keep running
while true; do
    # Check if all processes are still running
    if ! kill -0 $NATS_PID 2>/dev/null; then
        echo "✗ NATS server died"
        exit 1
    fi
    
    if ! kill -0 $ROUTER_PID 2>/dev/null; then
        echo "✗ Mock Router died"
        exit 1
    fi
    
    if ! kill -0 $GATEWAY_PID 2>/dev/null; then
        echo "✗ IPC Gateway died"
        exit 1
    fi
    
    sleep 5
done
