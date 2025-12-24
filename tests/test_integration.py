#!/usr/bin/env python3
"""
Integration Test - Full IPC → NATS → Mock Router flow

This test verifies end-to-end integration:
1. Start NATS server
2. Start mock Router (NATS subscriber)
3. Start IPC server with NATS bridge
4. Send IPC request
5. Verify Router response

Usage:
    python3 test_integration.py
"""

import socket
import struct
import json
import sys
import time
import subprocess
import signal
import os

IPC_MSG_TASK_SUBMIT = 0x01
IPC_MSG_RESPONSE_OK = 0x10
IPC_PROTOCOL_VERSION = 0x01

def send_ipc_message(sock, msg_type, payload):
    """Send IPC binary-framed message"""
    if isinstance(payload, dict):
        payload_bytes = json.dumps(payload).encode('utf-8')
    elif isinstance(payload, str):
        payload_bytes = payload.encode('utf-8')
    else:
        payload_bytes = payload
    
    frame_length = 6 + len(payload_bytes)
    frame = struct.pack('!I B B', frame_length, IPC_PROTOCOL_VERSION, msg_type)
    frame += payload_bytes
    sock.sendall(frame)

def recv_ipc_message(sock):
    """Receive IPC binary-framed message"""
    header = sock.recv(6)
    if not header or len(header) < 6:
        return None
    
    length, version, msg_type = struct.unpack('!I B B', header)
    payload_len = length - 6
    
    if payload_len > 0:
        payload_bytes = b''
        remaining = payload_len
        while remaining > 0:
            chunk = sock.recv(min(remaining, 4096))
            if not chunk:
                return None
            payload_bytes += chunk
            remaining -= len(chunk)
        
        payload = payload_bytes.decode('utf-8')
    else:
        payload = None
    
    return {
        'type': msg_type,
        'version': version,
        'payload': payload
    }

def test_integration():
    """Test full integration flow"""
    print("="*60)
    print("INTEGRATION TEST: IPC → NATS → Mock Router")
    print("="*60)
    
    socket_path = '/tmp/beamline-gateway.sock'
    
    # Check if socket exists
    if not os.path.exists(socket_path):
        print(f"\n✗ Socket not found: {socket_path}")
        print(f"  Please start IPC server:")
        print(f"    ./build/ipc_nats_demo {socket_path} 1")
        return False
    
    print(f"\n1. Connecting to IPC server...")
    try:
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        sock.connect(socket_path)
        print(f"   ✓ Connected to {socket_path}")
    except Exception as e:
        print(f"   ✗ Connection failed: {e}")
        return False
    
    print(f"\n2. Sending TaskSubmit request...")
    request = {
        "command": "task_submit",
        "task_type": "code_completion",
        "file": "test.py",
        "line": 42,
        "context": "def hello_world():"
    }
    
    try:
        send_ipc_message(sock, IPC_MSG_TASK_SUBMIT, request)
        print(f"   ✓ Request sent")
        print(f"   Request: {json.dumps(request, indent=2)}")
    except Exception as e:
        print(f"   ✗ Send failed: {e}")
        sock.close()
        return False
    
    print(f"\n3. Waiting for response...")
    try:
        response = recv_ipc_message(sock)
        
        if not response:
            print(f"   ✗ No response received")
            sock.close()
            return False
        
        print(f"   ✓ Response received")
        print(f"   Type: 0x{response['type']:02x}")
        
        if response['payload']:
            try:
                payload_json = json.loads(response['payload'])
                print(f"   Payload: {json.dumps(payload_json, indent=2)}")
                
                # Verify response structure
                if 'status' in payload_json and payload_json['status'] == 'ok':
                    print(f"\n   ✓ Status: OK")
                    
                    if 'result' in payload_json:
                        result = payload_json['result']
                        print(f"   ✓ Result present")
                        
                        if result.get('mock'):
                            print(f"   ℹ Mock Router response (expected)")
                        
                        if 'completion' in result:
                            print(f"   ✓ Completion: {result['completion'][:50]}...")
                        
                        print(f"\n{'='*60}")
                        print(f"INTEGRATION TEST: PASSED ✓")
                        print(f"{'='*60}")
                        sock.close()
                        return True
                    else:
                        print(f"   ⚠ No 'result' field in response")
                else:
                    print(f"   ⚠ Status not 'ok': {payload_json.get('status')}")
                    
            except json.JSONDecodeError:
                print(f"   ⚠ Payload not valid JSON: {response['payload']}")
        else:
            print(f"   ⚠ Empty payload")
        
    except Exception as e:
        print(f"   ✗ Receive failed: {e}")
    finally:
        sock.close()
    
    print(f"\n{'='*60}")
    print(f"INTEGRATION TEST: FAILED ✗")
    print(f"{'='*60}")
    return False

def main():
    print("\nIntegration Test Setup")
    print("-" * 60)
    print("This test requires:")
    print("  1. NATS server running (nats-server -js)")
    print("  2. Mock Router running (python3 tests/mock_router.py)")
    print("  3. IPC server with NATS bridge (./build/ipc_nats_demo ... 1)")
    print("-" * 60)
    
    input("\nPress Enter when all components are ready...")
    
    success = test_integration()
    return 0 if success else 1

if __name__ == '__main__':
    sys.exit(main())
