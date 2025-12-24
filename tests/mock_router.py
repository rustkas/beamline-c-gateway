#!/usr/bin/env python3
"""
Mock Router - NATS subscriber for testing IPC-NATS bridge

This simulates the Router by subscribing to NATS subjects and responding
to requests with mock data.

Requirements:
    pip install nats-py

Usage:
    python3 mock_router.py [nats_url] [subject]

Example:
    python3 mock_router.py nats://localhost:4222 "beamline.router.v1.decide"
"""

import asyncio
import json
import sys
from nats.aio.client import Client as NATS

async def message_handler(msg):
    """Handle incoming NATS messages"""
    subject = msg.subject
    reply = msg.reply
    data = msg.data.decode()
    
    print(f"[mock_router] Received on '{subject}'")
    print(f"  Reply-to: {reply}")
    print(f"  Data: {data[:200]}...")  # Truncate for display
    
    try:
        request = json.loads(data)
        message_id = request.get('message_id', 'unknown')
        input_data = request.get('input', {})
        
        # Generate mock response
        response = {
            "message_id": message_id,
            "status": "ok",
            "result": {
                "mock": True,
                "echo_input": input_data,
                "completion": "def hello_world():\n    print('Hello from mock Router!')",
                "confidence": 0.95,
                "latency_ms": 42
            }
        }
        
        response_json = json.dumps(response)
        print(f"[mock_router] Sending response: {response_json[:200]}...")
        
        # Send response
        await msg.respond(response_json.encode())
        print(f"[mock_router] Response sent\n")
        
    except json.JSONDecodeError as e:
        print(f"[mock_router] JSON decode error: {e}")
        error_response = {
            "message_id": "error",
            "status": "error",
            "error": "Invalid JSON"
        }
        await msg.respond(json.dumps(error_response).encode())
    except Exception as e:
        print(f"[mock_router] Error: {e}")

async def run(nats_url, subject):
    """Run mock Router"""
    nc = NATS()
    
    print(f"Mock Router")
    print(f"===========")
    print(f"NATS URL: {nats_url}")
    print(f"Subject:  {subject}")
    print(f"\nConnecting...")
    
    try:
        await nc.connect(nats_url)
        print(f"✓ Connected to NATS")
        print(f"✓ Subscribed to '{subject}'")
        print(f"\nWaiting for requests (Press Ctrl+C to stop)...\n")
        
        # Subscribe to subject
        sub = await nc.subscribe(subject, cb=message_handler)
        
        # Keep running
        while True:
            await asyncio.sleep(1)
            
    except KeyboardInterrupt:
        print("\n\nShutting down...")
    except Exception as e:
        print(f"✗ Error: {e}")
        return 1
    finally:
        if nc.is_connected:
            await nc.close()
        print("Mock Router stopped.")
    
    return 0

def main():
    nats_url = sys.argv[1] if len(sys.argv) > 1 else "nats://localhost:4222"
    subject = sys.argv[2] if len(sys.argv) > 2 else "beamline.router.v1.decide"
    
    try:
        return asyncio.run(run(nats_url, subject))
    except Exception as e:
        print(f"Fatal error: {e}")
        return 1

if __name__ == '__main__':
    sys.exit(main())
