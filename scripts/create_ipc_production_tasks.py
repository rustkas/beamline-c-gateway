#!/usr/bin/env python3
"""
Скрипт создания всех 15 IPC production readiness задач
"""
import os
from pathlib import Path

BASE_DIR = Path("/home/rustkas/aigroup/apps/c-gateway/.ai")

TASKS = {
    "task_cgw_ipc_build_integration": {
        "goal_ru": "Интегрировать IPC Gateway в основной build",
        "goal_en": "Integrate IPC Gateway into main build",
        "acceptance": "1) Единые таргеты\n2) CI тесты\n3) Демо из build output",
        "plan": "1) Определить build систему\n2) Подключить IPC\n3) Add targets\n4) Tests\n5) Docs",
    },
    "task_cgw_ipc_config_contract": {
        "goal_ru": "Единый env contract для IPC/bridge",
        "goal_en": "Unified env contract for IPC/bridge",
        "acceptance": "1) Документированные env vars\n2) Валидация\n3) Sanitized snapshot",
        "plan": "1) Env contract\n2) Parser/validator\n3) Sanitization\n4) Tests\n5) Docs",
    },
    "task_cgw_ipc_observability_jsonl": {
        "goal_ru": "CP1-style JSONL observability для IPC",
        "goal_en": "CP1-style JSONL observability for IPC",
        "acceptance": "1) JSONL logs\n2) request_id correlation\n3) Unit tests",
        "plan": "1) Log fields\n2) request_id/trace_id\n3) Lifecycle logs\n4) Tests",
    },
    "task_cgw_ipc_log_sanitization": {
        "goal_ru": "Фильтрация PII/секретов в логах",
        "goal_en": "PII/secret filtering in logs",
        "acceptance": "1) No raw secrets\n2) Recursive masking\n3) Tests",
        "plan": "1) Sensitive keys list\n2) Sanitizer\n3) Integration\n4) Tests",
    },
    "task_cgw_ipc_router_contract_alignment": {
        "goal_ru": "Согласование с Router контрактами",
        "goal_en": "Align with Router contracts",
        "acceptance": "1) Strict contract\n2) Unified errors\n3) Tests",
        "plan": "1) Canonical JSON\n2) Builder/validator\n3) Error mapping\n4) Tests",
    },
    "task_cgw_ipc_subjects_replyto_unification": {
        "goal_ru": "Унификация subjects/reply-to",
        "goal_en": "Unify subjects/reply-to",
        "acceptance": "1) Единая конфигурация\n2) Единая семантика\n3) Одинаковые ошибки",
        "plan": "1) Инвентаризация\n2) Общий слой\n3) Унификация\n4) Tests",
    },
    "task_cgw_ipc_nats_resilience": {
        "goal_ru": "NATS resilience: reconnect/backoff",
        "goal_en": "NATS resilience: reconnect/backoff",
        "acceptance": "1) No hang on NATS down\n2) Reconnect backoff\n3) Inflight limits\n4) E2E test",
        "plan": "1) Connection state\n2) Backoff\n3) Inflight limits\n4) E2E test",
    },
    "task_cgw_ipc_backpressure": {
        "goal_ru": "Backpressure: inflight лимиты",
        "goal_en": "Backpressure: inflight limits",
        "acceptance": "1) Per-conn/global limits\n2) BUSY rejection\n3) Burst test",
        "plan": "1) Inflight counters\n2) Rejection logic\n3) Burst test",
    },
    "task_cgw_ipc_peercred_authz": {
        "goal_ru": "SO_PEERCRED авторизация",
        "goal_en": "SO_PEERCRED authorization",
        "acceptance": "1) SO_PEERCRED check\n2) Allowlist\n3) Docs",
        "plan": "1) Extract SO_PEERCRED\n2) Allowlist\n3) Docs\n4) Tests",
    },
    "task_cgw_ipc_protocol_versioning_caps": {
        "goal_ru": "Версионирование протокола",
        "goal_en": "Protocol versioning",
        "acceptance": "1) Capabilities message\n2) Compatibility policy\n3) Tests",
        "plan": "1) Capabilities format\n2) Handler\n3) Docs\n4) Tests",
    },
    "task_cgw_ipc_json_schema_validation": {
        "goal_ru": "JSON Schema валидация",
        "goal_en": "JSON Schema validation",
        "acceptance": "1) Schema files\n2) Validation errors\n3) Tests",
        "plan": "1) Schemas\n2) Validator\n3) Integration\n4) Tests",
    },
    "task_cgw_ipc_task_cancel_e2e": {
        "goal_ru": "End-to-end cancellation",
        "goal_en": "End-to-end cancellation",
        "acceptance": "1) TaskCancel message\n2) Idempotency\n3) E2E test",
        "plan": "1) Identifier\n2) Message type\n3) Mock Router\n4) Tests",
    },
    "task_cgw_ipc_streaming_phase3": {
        "goal_ru": "Streaming ответы (Phase 3)",
        "goal_en": "Streaming responses (Phase 3)",
        "acceptance": "1) Stream lifecycle\n2) N chunks\n3) Error handling\n4) E2E test",
        "plan": "1) Message types\n2) Server delivery\n3) Mock Router\n4) Tests",
    },
    "task_cgw_ipc_fuzz_sanitizers_ci": {
        "goal_ru": "Fuzz + ASan/UBSan",
        "goal_en": "Fuzz + ASan/UBSan",
        "acceptance": "1) Fuzz test\n2) Sanitizer build\n3) Docs",
        "plan": "1) Fuzz harness\n2) Sanitizer flags\n3) CI integration",
    },
    "task_cgw_ipc_ops_release_compliance": {
        "goal_ru": "Ops/release compliance",
        "goal_en": "Ops/release compliance",
        "acceptance": "1) Runbook\n2) Health/degraded\n3) Compliance artifacts",
        "plan": "1) Runbook\n2) Health status\n3) Dependencies\n4) CI",
    },
}

def create_task(task_dir, task_data):
    """Create single task with 5 files"""
    task_path = BASE_DIR / task_dir
    task_path.mkdir(parents=True, exist_ok=True)
    
    # goal.md
    (task_path / "goal.md").write_text(f"""# Goal (RU)
{task_data['goal_ru']}

# Goal (EN)
{task_data['goal_en']}
""")
    
    # acceptance.md
    (task_path / "acceptance.md").write_text(f"""# Acceptance Criteria
{task_data['acceptance']}

# Verification
См. plan.md для деталей
""")
    
    # plan.md
    (task_path / "plan.md").write_text(f"""# Plan
{task_data['plan']}
""")
    
    # progress.md
    (task_path / "progress.md").write_text(f"""# Progress (facts only)

## 2025-12-24
- CREATED {task_dir}
- STATUS: Not Started
""")
    
    # state_capture_prompt.md
    (task_path / "state_capture_prompt.md").write_text(f"""Собери факты выполнения {task_dir}:
- измененные файлы
- команды проверки+результаты
- артефакты
- ограничения

Только факты, без планов.
""")
    
    print(f"✓ Created {task_dir}")

def main():
    print(f"Creating {len(TASKS)} IPC production readiness tasks...")
    for task_dir, task_data in TASKS.items():
        create_task(task_dir, task_data)
    print(f"\n✓ All {len(TASKS)} tasks created successfully!")
    print(f"Location: {BASE_DIR}")

if __name__ == "__main__":
    main()
