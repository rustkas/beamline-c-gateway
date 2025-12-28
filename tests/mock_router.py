#!/usr/bin/env python3
"""
mock_router.py - Mock Router for E2E testing

Simulates Router behavior:
- Subscribes to NATS subjects
- Responds to requests
- Can simulate delays/errors

Usage:
    python3 mock_router.py [--delay SECONDS] [--error-rate PERCENT]
"""

import asyncio
import json
import sys
import argparse
from nats.aio.client import Client as NATS

class MockRouter:
    def __init__(self, nats_url="nats://localhost:4222", delay=0, error_rate=0):
        self.nats_url = nats_url
        self.delay = delay
        self.error_rate = error_rate
        self.nc = NATS()
        self.request_count = 0
        
    async def connect(self):
        try:
            await self.nc.connect(self.nats_url)
            print(f"✓ Connected to NATS at {self.nats_url}")
            return True
        except Exception as e:
            print(f"✗ Failed to connect to NATS: {e}")
            return False
    
    async def handle_request(self, msg):
        """Handle incoming request from gateway"""
        self.request_count += 1
        
        try:
            # Parse request
            request = json.loads(msg.data.decode())
            task_id = request.get("task_id", "unknown")
            
            print(f"[{self.request_count}] Request: task_id={task_id}")
            
            # Simulate delay if configured
            if self.delay > 0:
                await asyncio.sleep(self.delay)
            
            # Simulate error if configured
            if self.error_rate > 0 and (self.request_count % (100 // self.error_rate)) == 0:
                response = {
                    "error": "simulated_error",
                    "code": 500,
                    "message": "Mock Router error"
                }
                print(f"[{self.request_count}] → ERROR (simulated)")
            else:
                # Success response
                response = {
                    "task_id": task_id,
                    "status": "success",
                    "result": f"Processed by mock router"
                }
                print(f"[{self.request_count}] → SUCCESS")
            
            # Send response
            await self.nc.publish(msg.reply, json.dumps(response).encode())
            
        except Exception as e:
            print(f"[{self.request_count}] ✗ Error handling request: {e}")
            error_response = {
                "error": "internal_error",
                "message": str(e)
            }
            if msg.reply:
                await self.nc.publish(msg.reply, json.dumps(error_response).encode())
    
    async def start(self):
        """Start listening for requests"""
        print("Mock Router starting...")
        print(f"Configuration:")
        print(f"  Delay: {self.delay}s")
        print(f"  Error rate: {self.error_rate}%")
        print()
        
        # Subscribe to gateway requests
        await self.nc.subscribe("gateway.request.*", cb=self.handle_request)
        
        print("✓ Mock Router ready")
        print("  Listening on: gateway.request.*")
        print("  Press Ctrl+C to stop")
        print()
        
        # Keep running
        try:
            while True:
                await asyncio.sleep(1)
        except KeyboardInterrupt:
            print("\nShutting down...")
    
    async def cleanup(self):
        if self.nc.is_connected:
            await self.nc.close()
            print("✓ Disconnected from NATS")

async def main():
    parser = argparse.ArgumentParser(description="Mock Router for E2E testing")
    parser.add_argument("--delay", type=float, default=0, help="Response delay in seconds")
    parser.add_argument("--error-rate", type=int, default=0, help="Error rate percentage (0-100)")
    parser.add_argument("--nats", default="nats://localhost:4222", help="NATS server URL")
    
    args = parser.parse_args()
    
    router = MockRouter(nats_url=args.nats, delay=args.delay, error_rate=args.error_rate)
    
    if not await router.connect():
        print("Failed to start mock router")
        return 1
    
    try:
        await router.start()
    finally:
        await router.cleanup()
    
    return 0

if __name__ == "__main__":
    try:
        sys.exit(asyncio.run(main()))
    except KeyboardInterrupt:
        print("\nStopped")
        sys.exit(0)
