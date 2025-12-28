#!/usr/bin/env python3
"""
production_load_test.py - Production-grade load testing

Simulates real production traffic patterns:
- Burst traffic
- Mixed payload sizes
- Error scenarios
- Concurrent connections
- Long-running stability
"""

import socket
import json
import time
import os
import sys
import threading
import random
from datetime import datetime
from collections import defaultdict

SOCKET_PATH = os.getenv('IPC_SOCKET_PATH', '/tmp/beamline-gateway.sock')
TEST_DURATION = int(os.getenv('TEST_DURATION', '3600'))  # 1 hour default
CONNECTIONS = int(os.getenv('TEST_CONNECTIONS', '100'))

# Statistics
stats = {
    'total_requests': 0,
    'successful': 0,
    'failed': 0,
    'timeouts': 0,
    'latencies': [],
    'errors': defaultdict(int)
}
stats_lock = threading.Lock()

def send_request(payload_size=64):
    """Send a single request and measure latency"""
    try:
        start = time.time()
        
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        sock.settimeout(5.0)
        sock.connect(SOCKET_PATH)
        
        # Create payload
        payload = {
            'task_id': f'load_test_{time.time()}_{random.randint(0, 1000000)}',
            'action': 'test',
            'data': 'x' * payload_size
        }
        
        message = json.dumps(payload).encode('utf-8')
        sock.sendall(message)
        
        try:
            response = sock.recv(4096)
            if response:
                latency = (time.time() - start) * 1000  # ms
                
                with stats_lock:
                    stats['successful'] += 1
                    stats['latencies'].append(latency)
                    
                sock.close()
                return True, latency
        except socket.timeout:
            with stats_lock:
                stats['timeouts'] += 1
            sock.close()
            return False, None
            
    except Exception as e:
        with stats_lock:
            stats['failed'] += 1
            stats['errors'][str(type(e).__name__)] += 1
        return False, None
    finally:
        with stats_lock:
            stats['total_requests'] += 1

def worker_thread(worker_id, duration):
    """Worker thread - sends continuous requests"""
    end_time = time.time() + duration
    requests = 0
    
    while time.time() < end_time:
        # Vary payload size (realistic pattern)
        payload_size = random.choice([64, 128, 256, 512, 1024])
        
        success, latency = send_request(payload_size)
        requests += 1
        
        # Realistic inter-request delay
        time.sleep(random.uniform(0.01, 0.1))
    
    print(f"Worker {worker_id}: {requests} requests completed")

def burst_traffic_test(duration=60):
    """Simulate burst traffic"""
    print("=== Burst Traffic Test ===")
    end_time = time.time() + duration
    
    while time.time() < end_time:
        # Send burst of 50 requests
        threads = []
        for i in range(50):
            t = threading.Thread(target=send_request, args=(128,))
            t.start()
            threads.append(t)
        
        for t in threads:
            t.join()
        
        # Wait before next burst
        time.sleep(random.uniform(5, 15))

def print_statistics():
    """Print current statistics"""
    with stats_lock:
        total = stats['total_requests']
        success = stats['successful']
        failed = stats['failed']
        timeouts = stats['timeouts']
        latencies = stats['latencies']
        
        if total == 0:
            return
        
        success_rate = (success / total) * 100 if total > 0 else 0
        
        print(f"\n{'='*60}")
        print(f"PRODUCTION LOAD TEST STATISTICS")
        print(f"{'='*60}")
        print(f"Total Requests:    {total}")
        print(f"Successful:        {success} ({success_rate:.2f}%)")
        print(f"Failed:            {failed}")
        print(f"Timeouts:          {timeouts}")
        
        if latencies:
            latencies_sorted = sorted(latencies)
            p50 = latencies_sorted[len(latencies_sorted) // 2]
            p95 = latencies_sorted[int(len(latencies_sorted) * 0.95)]
            p99 = latencies_sorted[int(len(latencies_sorted) * 0.99)]
            avg = sum(latencies) / len(latencies)
            
            print(f"\nLatency Statistics (ms):")
            print(f"  Average:  {avg:.2f}")
            print(f"  p50:      {p50:.2f}")
            print(f"  p95:      {p95:.2f}")
            print(f"  p99:      {p99:.2f}")
            print(f"  Min:      {min(latencies):.2f}")
            print(f"  Max:      {max(latencies):.2f}")
        
        if stats['errors']:
            print(f"\nErrors by type:")
            for error_type, count in stats['errors'].items():
                print(f"  {error_type}: {count}")
        
        print(f"{'='*60}\n")

def main():
    """Main production load test"""
    print(f"{'='*60}")
    print(f"PRODUCTION LOAD TEST")
    print(f"{'='*60}")
    print(f"Socket Path:     {SOCKET_PATH}")
    print(f"Duration:        {TEST_DURATION}s ({TEST_DURATION/3600:.1f}h)")
    print(f"Connections:     {CONNECTIONS}")
    print(f"Started:         {datetime.now()}")
    print(f"{'='*60}\n")
    
    # Wait for gateway to be ready
    print("Waiting for gateway...")
    for i in range(30):
        if os.path.exists(SOCKET_PATH):
            print("✓ Gateway socket found")
            break
        time.sleep(1)
    else:
        print("✗ Gateway socket not found")
        sys.exit(1)
    
    # Phase 1: Sustained load (70% of time)
    sustained_duration = int(TEST_DURATION * 0.7)
    print(f"\n=== Phase 1: Sustained Load ({sustained_duration}s) ===")
    
    workers = []
    for i in range(CONNECTIONS):
        t = threading.Thread(target=worker_thread, args=(i, sustained_duration))
        t.daemon = True
        t.start()
        workers.append(t)
    
    # Monitor progress
    start_time = time.time()
    while time.time() - start_time < sustained_duration:
        time.sleep(30)
        print_statistics()
    
    # Wait for workers
    for w in workers:
        w.join()
    
    # Phase 2: Burst traffic (20% of time)
    burst_duration = int(TEST_DURATION * 0.2)
    print(f"\n=== Phase 2: Burst Traffic ({burst_duration}s) ===")
    burst_traffic_test(burst_duration)
    
    # Phase 3: Stress test (10% of time)
    stress_duration = int(TEST_DURATION * 0.1)
    print(f"\n=== Phase 3: Stress Test ({stress_duration}s) ===")
    stress_workers = []
    for i in range(CONNECTIONS * 2):  # Double connections
        t = threading.Thread(target=worker_thread, args=(f"stress_{i}", stress_duration))
        t.daemon = True
        t.start()
        stress_workers.append(t)
    
    for w in stress_workers:
        w.join()
    
    # Final statistics
    print_statistics()
    
    # Save to artifacts
    try:
        with open('/artifacts/production_load_test_results.json', 'w') as f:
            json.dump({
                'timestamp': datetime.now().isoformat(),
                'duration': TEST_DURATION,
                'connections': CONNECTIONS,
                'statistics': {
                    'total_requests': stats['total_requests'],
                    'successful': stats['successful'],
                    'failed': stats['failed'],
                    'timeouts': stats['timeouts'],
                    'success_rate': (stats['successful'] / stats['total_requests'] * 100) if stats['total_requests'] > 0 else 0
                },
                'latencies': {
                    'count': len(stats['latencies']),
                    'min': min(stats['latencies']) if stats['latencies'] else 0,
                    'max': max(stats['latencies']) if stats['latencies'] else 0,
                    'avg': sum(stats['latencies']) / len(stats['latencies']) if stats['latencies'] else 0
                }
            }, f, indent=2)
        print("✓ Results saved to /artifacts/production_load_test_results.json")
    except Exception as e:
        print(f"⚠ Could not save results: {e}")
    
    print(f"\nTest completed at: {datetime.now()}")
    
    # Exit code based on success rate
    success_rate = (stats['successful'] / stats['total_requests'] * 100) if stats['total_requests'] > 0 else 0
    
    if success_rate >= 99.9:
        print("✓ EXCELLENT: >99.9% success rate")
        sys.exit(0)
    elif success_rate >= 99:
        print("✓ GOOD: >99% success rate")
        sys.exit(0)
    elif success_rate >= 95:
        print("⚠ ACCEPTABLE: >95% success rate")
        sys.exit(0)
    else:
        print("✗ POOR: <95% success rate")
        sys.exit(1)

if __name__ == '__main__':
    main()
