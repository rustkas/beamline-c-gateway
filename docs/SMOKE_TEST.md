# Smoke Test: c-gateway ↔ Router (Template/Strict)

Этот smoke orchestration нужен, чтобы **одинаково** (локально и в CI) проверить:
- что NATS поднимается и отвечает по monitor HTTP,
- что Router стартует,
- что c-gateway стартует и отвечает HTTP,
- что эндпоинт `POST /api/v1/routes/decide` возвращает `200` и валидный JSON,
- и собрать артефакты для диагностики.

## Режимы

### Template mode (по умолчанию, ADR-005 safe)
- `ALLOW_DUMMY_RESPONSE=1`
- Скрипт **не требует реальной NATS-интеграции** в c-gateway.
- Допускает stub/dummy response (например `message_id=dummy`).

### Strict mode (для будущей реальной интеграции)
- `ALLOW_DUMMY_RESPONSE=0`
- Скрипт **падает**, если ответ выглядит как dummy/stub.
- Этот режим используется только когда включён реальный NATS клиент в c-gateway.

## Быстрый старт

```bash
cd /home/rustkas/aigroup/apps/c-gateway
./scripts/smoke_cgateway_router.sh
ls -lh _artifacts/smoke_cgw_router_*/
```

Ожидаемо в Template mode:
- exit code `0`
- артефакты в `_artifacts/smoke_cgw_router_<timestamp>/`

## Переменные окружения

### Mode

* `ALLOW_DUMMY_RESPONSE` (default: `1`)
  * `1` = template safe
  * `0` = strict (requires real integration)

### Paths / Commands

Скрипт пытается авто-обнаружить Router и команды старта, но в CI лучше задавать явно:

* `ROUTER_DIR` — путь к `apps/otp/router`
* `ROUTER_START_CMD` — **детерминированная** команда старта Router (рекомендуется для CI)
* `CGW_DIR` — путь к c-gateway (default: текущий repo root)
* `CGW_START_CMD` — **детерминированная** команда старта c-gateway (рекомендуется для CI)

### NATS

* `NATS_HOST` (default: `127.0.0.1`)
* `NATS_PORT` (default: `4222`)
* `NATS_MON_PORT` (default: `8222`)

### HTTP

* `CGW_HOST` (default: `127.0.0.1`)
* `CGW_PORT` (default: `8080`)
* `CGW_DECIDE_PATH` (default: `/api/v1/routes/decide`)
* `CGW_HEALTH_PATH` (default: `/_health`)

### Timeouts

* `STARTUP_WAIT_SECONDS` (default: `20`)
* `STOP_WAIT_SECONDS` (default: `6`)
* `CURL_TIMEOUT_SECONDS` (default: `5`)
* `CURL_RETRIES` (default: `10`)
* `CURL_RETRY_SLEEP_MS` (default: `200`)

## Артефакты

Каждый запуск создаёт папку:

`_artifacts/smoke_cgw_router_<UTC_TIMESTAMP>/`

Содержимое:

* `env.txt` — snapshot окружения, версии, git SHA (best-effort)
* `nats_start.log` или `nats.log` — лог NATS
* `router.log` — лог Router
* `c_gateway.log` — лог c-gateway
* `request.json` — запрос
* `response.headers`, `response.body`, `http_code.txt`
* `message_id.txt` — извлечённый `message_id` (если есть)
* `nats_varz.json`, `nats_connz.json` (best-effort)

## CI пример (GitLab)

```yaml
smoke:cgateway:template:
  stage: integration-test
  script:
    - ./scripts/smoke_cgateway_router.sh
  artifacts:
    when: always
    paths:
      - _artifacts/smoke_cgw_router_*/
    expire_in: 7 days
  allow_failure: false
```

Для strict mode (когда реально интегрировано):

```yaml
smoke:cgateway:strict:
  stage: integration-test
  script:
    - ALLOW_DUMMY_RESPONSE=0 ./scripts/smoke_cgateway_router.sh
  artifacts:
    when: always
    paths:
      - _artifacts/smoke_cgw_router_*/
    expire_in: 7 days
  allow_failure: false
```

## Troubleshooting (быстро)

* `NATS monitor not ready`:
  * проверь `NATS_MON_PORT` (8222), и что NATS запущен с monitoring.
* `ROUTER_START_CMD not set and auto-discovery failed`:
  * в CI обязательно задавай `ROUTER_START_CMD`.
* `CGW_START_CMD not set and auto-discovery failed`:
  * в CI обязательно задавай `CGW_START_CMD`.
* `HTTP != 200`:
  * смотри `response.headers`, `response.body`, `c_gateway.log`.
