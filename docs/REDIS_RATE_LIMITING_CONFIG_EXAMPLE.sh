#!/bin/bash
# Example configuration for Redis Rate Limiting PoC

# Enable Redis rate limiting
export C_GATEWAY_REDIS_RATE_LIMIT_ENABLED=true

# Redis connection (choose one):
# Option 1: Redis URI (recommended for production)
export C_GATEWAY_REDIS_RATE_LIMIT_REDIS_URI=redis://localhost:6379
# Option 2: Separate host/port
# export C_GATEWAY_REDIS_RATE_LIMIT_REDIS_HOST=localhost
# export C_GATEWAY_REDIS_RATE_LIMIT_REDIS_PORT=6379

# Rate limiting configuration
export C_GATEWAY_REDIS_RATE_LIMIT_WINDOW_SEC=1
export C_GATEWAY_REDIS_RATE_LIMIT_GLOBAL_LIMIT=1000
export C_GATEWAY_REDIS_RATE_LIMIT_ROUTE_LIMIT_MESSAGES=200
export C_GATEWAY_REDIS_RATE_LIMIT_ROUTE_LIMIT_CHAT=100

# Connection pool configuration
export C_GATEWAY_REDIS_RATE_LIMIT_POOL_SIZE=32
export C_GATEWAY_REDIS_RATE_LIMIT_POOL_ACQUIRE_TIMEOUT_MS=10
export C_GATEWAY_REDIS_RATE_LIMIT_REDIS_TIMEOUT_MS=30

# Retry configuration
export C_GATEWAY_REDIS_RATE_LIMIT_RETRIES=2
export C_GATEWAY_REDIS_RATE_LIMIT_RETRY_BACKOFF_MS=5

# Circuit breaker configuration (fail-open for safety)
export C_GATEWAY_REDIS_RATE_LIMIT_CB_MODE=fail_open
export C_GATEWAY_REDIS_RATE_LIMIT_CB_ERROR_THRESHOLD=5
export C_GATEWAY_REDIS_RATE_LIMIT_CB_SLIDING_WINDOW_SEC=30
export C_GATEWAY_REDIS_RATE_LIMIT_CB_COOLDOWN_SEC=15
export C_GATEWAY_REDIS_RATE_LIMIT_CB_HALF_OPEN_ATTEMPTS=2

# Run C-Gateway with these settings
# ./c-gateway

