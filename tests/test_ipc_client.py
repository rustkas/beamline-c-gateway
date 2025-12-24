#!/usr/bin/env python3
"""
IPC Client Test - Test client for C-Gateway IPC server

Usage:
    python3 test_ipc_client.py [socket_path]
"""

import socket
import struct
import json
import sys

# Message types (from ipc_protocol.h)
IPC_MSG_TASK_SUBMIT      = 0x01
IPC_MSG_TASK_QUERY       = 0x02
IPC_MSG_TASK_CANCEL      = 0x03
IPC_MSG_STREAM_SUBSCRIBE = 0x04
IPC_MSG_PING             = 0xF0
IPC_MSG_PONG             = 0xF1
IPC_MSG_RESPONSE_OK      = 0x10
IPC_MSG_RESPONSE_ERROR   = 0x11

IPC_PROTOCOL_VERSION = 0x01

def send_message(sock, msg_type, payload):
    """Send binary-framed IPC message"""
    if isinstance(payload, str):
        payload_bytes = payload.encode('utf-8')
    elif isinstance(payload, dict):
        payload_bytes = json.dumps(payload).encode('utf-8')
    else:
        payload_bytes = payload
    
    frame_length = 6 + len(payload_bytes)  # header(6) + payload
    
    # Pack frame: length(4 BE) + version(1) + type(1) + payload(N)
    frame = struct.pack('!I B B', frame_length, IPC_PROTOCOL_VERSION, msg_type)
    frame += payload_bytes
    
    sock.sendall(frame)
    print(f"[→] Sent {frame_length} bytes (type=0x{msg_type:02x})")

def recv_message(sock):
    """Receive binary-framed IPC message"""
    # Read header (6 bytes)
    header = sock.recv(6)
    if not header or len(header) < 6:
        print("[←] Connection closed")
        return None
    
    length, version, msg_type = struct.unpack('!I B B', header)
    payload_len = length - 6
    
    print(f"[←] Receiving {length} bytes (type=0x{msg_type:02x}, version=0x{version:02x})")
    
    if payload_len > 0:
        payload_bytes = b''
        remaining = payload_len
        while remaining > 0:
            chunk = sock.recv(min(remaining, 4096))
            if not chunk:
                print("[←] Connection closed mid-frame")
                return None
            payload_bytes += chunk
            remaining -= len(chunk)
        
        try:
            payload = payload_bytes.decode('utf-8')
        except UnicodeDecodeError:
            payload = payload_bytes.hex()
    else:
        payload = None
    
    return {
        'type': msg_type,
        'version': version,
        'payload': payload
    }

def test_ping(sock):
    """Test ping/pong"""
    print("\n=== Test: Ping ===")
    send_message(sock, IPC_MSG_PING, b'')
    response = recv_message(sock)
    
    if response and response['type'] == IPC_MSG_PONG:
        print("✓ Ping successful")
        return True
    else:
        print("✗ Unexpected response")
        return False

def test_task_submit(sock):
    """Test task submission"""
    print("\n=== Test: Task Submit ===")
    
    request = {
        "task": "code_completion",
        "file": "test.py",
        "line": 42,
        "context": "def hello_world():"
    }
    
    send_message(sock, IPC_MSG_TASK_SUBMIT, request)
    response = recv_message(sock)
    
    if response:
        print(f"Response type: 0x{response['type']:02x}")
        if response['payload']:
            try:
                payload_json = json.loads(response['payload'])
                print(f"Payload (JSON): {json.dumps(payload_json, indent=2)}")
            except json.JSONDecodeError:
                print(f"Payload (raw): {response['payload']}")
        
        if response['type'] == IPC_MSG_RESPONSE_OK:
            print("✓ Task submit successful")
            return True
        else:
            print("✗ Task submit failed")
            return False
    else:
        print("✗ No response")
        return False

def test_invalid_version(sock):
    """Test invalid protocol version handling"""
    print("\n=== Test: Invalid Version ===")
    
    # Manually craft frame with wrong version
    payload = b'{"test":"invalid_version"}'
    length = 6 + len(payload)
    invalid_version = 0xFF  # Invalid
    msg_type = IPC_MSG_PING
    
    frame = struct.pack('!I B B', length, invalid_version, msg_type) + payload
    sock.sendall(frame)
    print(f"[→] Sent frame with invalid version 0x{invalid_version:02x}")
    
    response = recv_message(sock)
    
    if response and response['type'] == IPC_MSG_RESPONSE_ERROR:
        print("✓ Server correctly rejected invalid version")
        if response['payload']:
            print(f"Error: {response['payload']}")
        return True
    else:
        print("✗ Server did not reject invalid version")
        return False

def main():
    socket_path = sys.argv[1] if len(sys.argv) > 1 else '/tmp/beamline-gateway.sock'
    
    print(f"IPC Client Test")
    print(f"===============")
    print(f"Socket: {socket_path}\n")
    
    try:
        # Connect
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        sock.connect(socket_path)
        print(f"✓ Connected to {socket_path}\n")
        
        # Run tests
        tests_passed = 0
        tests_total = 0
        
        # Test 1: Ping
        tests_total += 1
        if test_ping(sock):
            tests_passed += 1
        
        # Test 2: Task Submit
        tests_total += 1
        if test_task_submit(sock):
            tests_passed += 1
        
        # Test 3: Invalid Version (creates new connection)
        tests_total += 1
        try:
            sock2 = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            sock2.connect(socket_path)
            if test_invalid_version(sock2):
                tests_passed += 1
            sock2.close()
        except Exception as e:
            print(f"✗ Test failed: {e}")
        
        # Summary
        print(f"\n{'='*50}")
        print(f"Tests: {tests_passed}/{tests_total} passed")
        print(f"{'='*50}")
        
        sock.close()
        
        return 0 if tests_passed == tests_total else 1
        
    except FileNotFoundError:
        print(f"✗ Error: Socket not found: {socket_path}")
        print(f"  Is the IPC server running?")
        return 1
    except ConnectionRefusedError:
        print(f"✗ Error: Connection refused: {socket_path}")
        print(f"  Is the IPC server running?")
        return 1
    except Exception as e:
        print(f"✗ Error: {e}")
        import traceback
        traceback.print_exc()
        return 1

if __name__ == '__main__':
    sys.exit(main())
