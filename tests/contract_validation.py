#!/usr/bin/env python3
"""
contract_validation.py - Validate Gateway compliance with Router contracts

This validates that the IPC Gateway is compatible with Router CP2 contracts.
"""

import json
import sys

print("="*60)
print("ROUTER CONTRACT VALIDATION")
print("="*60)
print()

# Load Router contracts
router_contracts_path = "/home/rustkas/aigroup/apps/otp/router/contracts/cp2_contracts.json"

try:
    with open(router_contracts_path, 'r') as f:
        contracts = json.load(f)
    print(f"✓ Loaded Router contracts: {router_contracts_path}")
except Exception as e:
    print(f"✗ Failed to load contracts: {e}")
    sys.exit(1)

print()
print("="*60)
print("CONTRACT METADATA")
print("="*60)

meta = contracts.get("meta", {})
print(f"Name: {meta.get('name')}")
print(f"Version: {meta.get('version')}")
print(f"Status: {meta.get('status')}")
print(f"Updated: {meta.get('updated_at')}")
print(f"Owners: {', '.join(meta.get('owners', []))}")

print()
print("="*60)
print("GATEWAY-RELEVANT SUBJECTS")
print("="*60)

subjects = contracts.get("subjects", [])

gateway_subjects = [s for s in subjects if 'gateway' in s.get('name', '').lower() or 'router' in s.get('name', '').lower()]

if gateway_subjects:
    print(f"\nFound {len(gateway_subjects)} Gateway-related subjects:")
    for subj in gateway_subjects:
        print(f"  - {subj.get('name', 'unnamed')}")
else:
    print("\n⚠ No Gateway-specific subjects found")
    print("  Looking for router.decide and similar...")

# Look for router decision subject
router_subjects = [s for s in subjects if 'router' in s.get('name', '')]
print(f"\nRouter subjects found: {len(router_subjects)}")
for subj in router_subjects[:10]:
    print(f"  - {subj.get('name', 'unnamed')}")

print()
print("="*60)
print("CONVENTIONS")
print("="*60)

conventions = contracts.get("conventions", {})
print(f"\nSubject Versioning:")
print(f"  Rule: {conventions.get('subject_versioning', {}).get('rule')}")
print(f"  Allowed: {conventions.get('subject_versioning', {}).get('allowed_versions')}")

print(f"\nEncoding:")
enc = conventions.get("encoding", {})
print(f"  Headers: {enc.get('headers')}")
print(f"  Payload: {enc.get('payload')}")

print(f"\nID Formats:")
ids = conventions.get("ids", {})
for id_name, id_spec in ids.items():
    print(f"  {id_name}:")
    print(f"    Format: {id_spec.get('format')}")
    print(f"    Max Length: {id_spec.get('max_len')}")

print()
print("="*60)
print("GATEWAY COMPATIBILITY CHECK")
print("="*60)

# Gateway expectations
gateway_expectations = {
    "subject_pattern": "Should use router.decide or similar",
    "headers": "UTF-8 strings",
    "payload": "Binary or JSON",
    "trace_id": "Max 128 chars",
    "tenant_id": "Max 128 chars"
}

print("\nGateway Implementation:")
for key, val in gateway_expectations.items():
    print(f"  {key}: {val}")

print()
print("="*60)
print("VALIDATION RESULT")
print("="*60)

# Basic validation
checks = []

# Check 1: Encoding compatibility
if enc.get('headers') == 'utf8-string' and enc.get('payload') in ['binary-or-json']:
    checks.append(('✓', 'Encoding', 'Compatible (UTF-8 headers, binary/JSON payload)'))
else:
    checks.append(('✗', 'Encoding', 'May be incompatible'))

# Check 2: ID format compatibility  
trace_id = ids.get('trace_id', {})
if trace_id.get('max_len', 0) >= 32:  # Gateway uses 32-byte trace IDs
    checks.append(('✓', 'Trace ID', f"Compatible (max {trace_id.get('max_len')} >= 32)"))
else:
    checks.append(('⚠', 'Trace ID', 'May be too short'))

# Check 3: Subject versioning
if 'v1' in conventions.get('subject_versioning', {}).get('allowed_versions', []):
    checks.append(('✓', 'Versioning', 'v1 subjects supported'))
else:
    checks.append(('⚠', 'Versioning', 'Check version compatibility'))

# Print checks
print()
for icon, check, result in checks:
    print(f"{icon} {check:20} : {result}")

print()
print("="*60)
print("SUMMARY")
print("="*60)

passed = sum(1 for check in checks if check[0] == '✓')
total = len(checks)

print(f"\nChecks Passed: {passed}/{total}")

if passed == total:
    print("\n✓ Gateway appears COMPATIBLE with Router contracts")
    print("  (Full E2E testing still required)")
    sys.exit(0)
elif passed >= total * 0.75:
    print("\n⚠ Gateway mostly compatible (minor issues)")
    print("  Review warnings before production")
    sys.exit(0)
else:
    print("\n✗ Potential compatibility issues found")
    print("  Review and fix before proceeding")
    sys.exit(1)
