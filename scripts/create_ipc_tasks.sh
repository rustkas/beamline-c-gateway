#!/bin/bash
# Скрипт для создания задач 2-14 IPC Gateway
set -e

BASE_DIR="/home/rustkas/aigroup/apps/c-gateway/.ai"

echo "Creating tasks 2-14..."

# ========== Task 2: Config Contract ==========
echo "Creating task 2..."
mkdir -p "$BASE_DIR/task_cgw_ipc_config_contract"

cat > "$BASE_DIR/task_cgw_ipc_config_contract/goal.md" << 'EOF'
# Goal (RU)
Сделать единый конфиг (env contract) для IPC/bridge: явные переменные окружения, валидация на старте, санитизированный config snapshot.

# Goal (EN)
Create a unified config (env contract) for IPC/bridge: explicit environment variables, startup validation, sanitized config snapshot.

# Scope
- Определить перечень env vars.
- Валидировать значения, дефолты, обязательные поля.
- Логировать snapshot без секретов.

# Non-goals
- Изменение бизнес-семантики сообщений.
EOF

cat > "$BASE_DIR/task_cgw_ipc_config_contract/acceptance.md" << 'EOF'
# Acceptance Criteria
1) Есть документированный список env vars:
   - CGW_IPC_ENABLE (0/1)
   - CGW_IPC_SOCKET_PATH
   - CGW_IDE_ENABLE_NATS (0/1)
   - CGW_IDE_NATS_URL
   - CGW_IDE_ROUTER_SUBJECT
   - CGW_IDE_TIMEOUT_MS
2) При старте конфиг валидируется, некорректный => ошибка + exit code
3) Config snapshot печатается без секретов.

# Verification
- Валидный конфиг => OK
- Невалидный => FAIL с причиной
EOF

cat > "$BASE_DIR/task_cgw_ipc_config_contract/plan.md" << 'EOF'
# Plan
1) Зафиксировать env contract.
2) Добавить парсер/валидатор.
3) Маскирование секретов.
4) Unit tests.
5) Обновить docs.
EOF

cat > "$BASE_DIR/task_cgw_ipc_config_contract/progress.md" << 'EOF'
# Progress

## 2025-12-24
- CREATED task
EOF

cat > "$BASE_DIR/task_cgw_ipc_config_contract/state_capture_prompt.md" << 'EOF'
Зафиксируй task_cgw_ipc_config_contract:
- env vars (имена)
- дефолты/обязательные
- примеры ошибок
- тесты
EOF

echo "Task 2 done"
echo "All batch creation complete"
