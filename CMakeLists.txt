cmake_minimum_required(VERSION 3.15)
project(c_gateway C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Enhanced compiler flags for better code quality
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wpedantic -Wstrict-prototypes")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wmissing-prototypes -Wmissing-declarations")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wshadow -Wpointer-arith -Wcast-align")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wwrite-strings -Wconversion")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wunreachable-code")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Waggregate-return -Wstrict-overflow=5")

# Coverage support
option(ENABLE_COVERAGE "Enable code coverage reporting" OFF)
if(ENABLE_COVERAGE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage -fprofile-arcs -ftest-coverage")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
endif()

# Admin gRPC unit test target
add_executable(c-gateway-admin-grpc-test
    tests/admin_grpc_test.c
    src/admin_grpc.c
)

# Debug builds get additional flags
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0 -DDEBUG")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address,undefined")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-omit-frame-pointer")
endif()

option(USE_NATS_LIB "Build with real NATS C client" OFF)
option(BUILD_IPC_GATEWAY "Build IPC Gateway components" ON)

if(USE_NATS_LIB)
    # Build with real NATS C client (system libnats must be available)
    add_executable(c-gateway
        src/main.c
        src/http_server.c
        src/nats_client_real.c
        src/metrics/prometheus.c
        src/metrics/metrics_registry.c
        src/handlers/metrics_handler.c
        src/tracing/otel.c
        src/tracing/otlp_exporter.c
    )

    find_path(NATS_INCLUDE_DIR nats/nats.h)
    if(NATS_INCLUDE_DIR)
        target_include_directories(c-gateway PRIVATE ${NATS_INCLUDE_DIR})
    endif()

    find_library(NATS_LIB nats)
    if(NATS_LIB)
        target_link_libraries(c-gateway PRIVATE ${NATS_LIB})
    endif()

    find_library(JANSSON_LIB jansson)
    if(NOT JANSSON_LIB)
        message(FATAL_ERROR "jansson library not found (required for JSON DTO parsing)")
    endif()
    
    find_library(CURL_LIB curl)
    if(NOT CURL_LIB)
        message(WARNING "libcurl not found - OpenTelemetry tracing will be disabled")
    endif()
    
    if(CURL_LIB)
        target_link_libraries(c-gateway PRIVATE ${JANSSON_LIB} ${CURL_LIB} pthread)
    else()
        target_link_libraries(c-gateway PRIVATE ${JANSSON_LIB} pthread)
    endif()

    # NATS ↔ Router smoke test (only when real NATS client is enabled)
    add_executable(nats-router-smoke-test
        tests/nats-router-smoke-test.c
    )

    if(NATS_INCLUDE_DIR)
        target_include_directories(nats-router-smoke-test PRIVATE ${NATS_INCLUDE_DIR})
    endif()
    if(NATS_LIB)
        target_link_libraries(nats-router-smoke-test PRIVATE ${NATS_LIB})
    endif()
else()
    # Default: use stub implementation that does not require external deps
    add_executable(c-gateway
        src/main.c
        src/http_server.c
        src/nats_client_stub.c
        src/metrics/prometheus.c
        src/metrics/metrics_registry.c
        src/handlers/metrics_handler.c
        src/tracing/otel.c
        src/tracing/otlp_exporter.c
        src/rate_limiter.c
        src/rate_limiter_memory.c
        src/rate_limiter_redis.c
        src/redis_rate_limiter.c
        src/abuse_detection.c
        src/backpressure_client.c
    )

    target_include_directories(c-gateway PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    find_library(JANSSON_LIB jansson)
    if(NOT JANSSON_LIB)
        message(FATAL_ERROR "jansson library not found (required for JSON DTO parsing)")
    endif()
    
    find_library(CURL_LIB curl)
    if(NOT CURL_LIB)
        message(WARNING "libcurl not found - OpenTelemetry tracing will be disabled")
    endif()
    
    # Optional: hiredis for distributed rate limiting (Redis Rate Limiter PoC)
    option(USE_REDIS "Build with Redis support for distributed rate limiting" OFF)
    if(USE_REDIS)
        find_library(HIREDIS_LIB hiredis)
        find_path(HIREDIS_INCLUDE_DIR hiredis/hiredis.h)
        if(HIREDIS_LIB)
            target_compile_definitions(c-gateway PRIVATE HAVE_HIREDIS)
            if(HIREDIS_INCLUDE_DIR)
                target_include_directories(c-gateway PRIVATE ${HIREDIS_INCLUDE_DIR})
            endif()
            message(STATUS "hiredis found - Redis rate limiting enabled")
        else()
            message(WARNING "hiredis not found - Redis rate limiting will use fallback mode")
        endif()
    endif()
    
    if(CURL_LIB)
        if(USE_REDIS AND HIREDIS_LIB)
            target_link_libraries(c-gateway PRIVATE ${JANSSON_LIB} ${CURL_LIB} ${HIREDIS_LIB} pthread)
        else()
            target_link_libraries(c-gateway PRIVATE ${JANSSON_LIB} ${CURL_LIB} pthread)
        endif()
    else()
        if(USE_REDIS AND HIREDIS_LIB)
            target_link_libraries(c-gateway PRIVATE ${JANSSON_LIB} ${HIREDIS_LIB} pthread)
        else()
            target_link_libraries(c-gateway PRIVATE ${JANSSON_LIB} pthread)
        endif()
    endif()
endif()

# JSON unit tests for C-Gateway (optional)
add_executable(c-gateway-json-test
    tests/c-gateway-json-test.c
    src/http_server.c
    src/nats_client_stub.c
)

target_include_directories(c-gateway-json-test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(JANSSON_LIB)
    target_link_libraries(c-gateway-json-test PRIVATE ${JANSSON_LIB})
endif()

# Router status mapping tests (optional)
add_executable(c-gateway-router-test
    tests/c-gateway-router-test.c
    src/http_server.c
)

target_include_directories(c-gateway-router-test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(JANSSON_LIB)
    target_link_libraries(c-gateway-router-test PRIVATE ${JANSSON_LIB})
endif()

# Extension errors contract test (CP2-LC)
add_executable(c-gateway-router-extension-errors-test
    tests/c-gateway-router-extension-errors-test.c
    src/http_server.c
)

target_include_directories(c-gateway-router-extension-errors-test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(JANSSON_LIB)
    target_link_libraries(c-gateway-router-extension-errors-test PRIVATE ${JANSSON_LIB})
endif()

# Admin endpoints contract test (CP2-LC)
add_executable(c-gateway-router-admin-contract-test
    tests/c-gateway-router-admin-contract-test.c
    src/http_server.c
)

target_include_directories(c-gateway-router-admin-contract-test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(JANSSON_LIB)
    target_link_libraries(c-gateway-router-admin-contract-test PRIVATE ${JANSSON_LIB})
endif()

# NATS stub wrapper tests (optional)
add_executable(c-gateway-nats-test
    tests/c-gateway-nats-test.c
    src/nats_client_stub.c
)

# Distributed rate limiting tests (Redis backend)
add_executable(c-gateway-rate-limiter-distributed-test
    tests/test_rate_limiter_distributed.c
    src/rate_limiter.c
    src/rate_limiter_memory.c
    src/rate_limiter_redis.c
)

if(JANSSON_LIB)
    target_link_libraries(c-gateway-rate-limiter-distributed-test PRIVATE ${JANSSON_LIB})
endif()

option(USE_REDIS "Build with Redis support for distributed rate limiting" OFF)
if(USE_REDIS)
    find_library(HIREDIS_LIB hiredis)
    if(HIREDIS_LIB)
        target_compile_definitions(c-gateway-rate-limiter-distributed-test PRIVATE HAVE_HIREDIS)
        target_link_libraries(c-gateway-rate-limiter-distributed-test PRIVATE ${HIREDIS_LIB})
        message(STATUS "hiredis found - Redis rate limiting enabled")
    else()
        message(WARNING "hiredis not found - Redis rate limiting will use fallback mode")
    endif()
endif()

# HTTP integration tests for C-Gateway (optional)
add_executable(c-gateway-http-test
    tests/c-gateway-http-test.c
)

# Unity test framework (minimal header-only implementation)
# Full Unity can be added as git submodule later: https://github.com/ThrowTheSwitch/Unity
set(UNITY_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tests/unity)

# Observability unit tests (using Unity)
# Note: Tests validate JSON format and structure, not static functions in http_server.c
add_executable(c-gateway-observability-test
    tests/test_observability.c
)

target_include_directories(c-gateway-observability-test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${UNITY_INCLUDE_DIR}
)

# Code coverage support (gcov)
option(ENABLE_COVERAGE "Enable code coverage" OFF)
if(ENABLE_COVERAGE)
    message(STATUS "Code coverage enabled (gcov)")
    target_compile_options(c-gateway-observability-test PRIVATE
        --coverage
        -fprofile-arcs
        -ftest-coverage
    )
    target_link_options(c-gateway-observability-test PRIVATE
        --coverage
        -fprofile-arcs
    )
endif()

if(JANSSON_LIB)
    target_link_libraries(c-gateway-observability-test PRIVATE ${JANSSON_LIB} pthread)
endif()

# Health endpoint integration tests (using Unity)
# Note: Tests validate HTTP response format and JSON structure
add_executable(c-gateway-health-test
    tests/test_health_endpoint.c
)

target_include_directories(c-gateway-health-test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${UNITY_INCLUDE_DIR}
)

# Code coverage support (gcov)
if(ENABLE_COVERAGE)
    target_compile_options(c-gateway-health-test PRIVATE
        --coverage
        -fprofile-arcs
        -ftest-coverage
    )
    target_link_options(c-gateway-health-test PRIVATE
        --coverage
        -fprofile-arcs
    )
endif()

if(JANSSON_LIB)
    target_link_libraries(c-gateway-health-test PRIVATE ${JANSSON_LIB} pthread)
endif()

# Performance tests (using Unity)
add_executable(c-gateway-performance-test
    tests/test_performance.c
)

target_include_directories(c-gateway-performance-test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${UNITY_INCLUDE_DIR}
)

if(JANSSON_LIB)
    target_link_libraries(c-gateway-performance-test PRIVATE ${JANSSON_LIB} pthread)
endif()

# Rate limiting integration tests
add_executable(c-gateway-rate-limiting-test
    tests/test_rate_limiting.c
)

target_include_directories(c-gateway-rate-limiting-test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(JANSSON_LIB)
    target_link_libraries(c-gateway-rate-limiting-test PRIVATE ${JANSSON_LIB} pthread)
endif()

# Enable CTest for test discovery
enable_testing()
add_test(NAME observability_test COMMAND c-gateway-observability-test)
add_test(NAME health_test COMMAND c-gateway-health-test)
add_test(NAME performance_test COMMAND c-gateway-performance-test)
add_test(NAME rate_limiting_test COMMAND c-gateway-rate-limiting-test)
add_test(NAME rate_limiter_distributed_test COMMAND c-gateway-rate-limiter-distributed-test)

# Redis Rate Limiter PoC unit tests
add_executable(c-gateway-redis-rate-limiter-test
    tests/redis_rate_limiter_test.c
    src/redis_rate_limiter.c
    src/metrics/prometheus.c
    src/metrics/metrics_registry.c
)

target_include_directories(c-gateway-redis-rate-limiter-test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(c-gateway-redis-rate-limiter-test PRIVATE pthread)

# Redis Rate Limiter PoC integration tests (requires Redis)
add_executable(c-gateway-redis-rate-limiter-integration-test
    tests/redis_rate_limiter_integration_test.c
    src/redis_rate_limiter.c
    src/metrics/prometheus.c
    src/metrics/metrics_registry.c
)

target_include_directories(c-gateway-redis-rate-limiter-integration-test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

option(USE_REDIS_FOR_TESTS "Build integration tests with Redis support" OFF)
if(USE_REDIS_FOR_TESTS)
    find_library(HIREDIS_LIB hiredis)
    find_path(HIREDIS_INCLUDE_DIR hiredis/hiredis.h)
    if(HIREDIS_LIB)
        target_compile_definitions(c-gateway-redis-rate-limiter-integration-test PRIVATE HAVE_HIREDIS)
        if(HIREDIS_INCLUDE_DIR)
            target_include_directories(c-gateway-redis-rate-limiter-integration-test PRIVATE ${HIREDIS_INCLUDE_DIR})
        endif()
        target_link_libraries(c-gateway-redis-rate-limiter-integration-test PRIVATE ${HIREDIS_LIB} pthread)
        message(STATUS "hiredis found - Redis rate limiter integration tests enabled")
    else()
        message(WARNING "hiredis not found - Redis rate limiter integration tests will be skipped")
        target_link_libraries(c-gateway-redis-rate-limiter-integration-test PRIVATE pthread)
    endif()
else()
    target_link_libraries(c-gateway-redis-rate-limiter-integration-test PRIVATE pthread)
endif()

add_test(NAME redis_rate_limiter_unit_test COMMAND c-gateway-redis-rate-limiter-test)
add_test(NAME redis_rate_limiter_integration_test COMMAND c-gateway-redis-rate-limiter-integration-test)

# Gateway Conflict Contract tests
add_executable(c-gateway-conflict-contract-test
    tests/gateway_conflict_contract_test.c
)

target_include_directories(c-gateway-conflict-contract-test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

find_library(CURL_LIB curl)
find_library(JANSSON_LIB jansson)
if(CURL_LIB AND JANSSON_LIB)
    target_link_libraries(c-gateway-conflict-contract-test PRIVATE ${CURL_LIB} ${JANSSON_LIB})
    message(STATUS "curl and jansson found - conflict contract tests enabled")
else()
    message(WARNING "curl or jansson not found - conflict contract tests will be skipped")
endif()

add_test(NAME conflict_contract_test COMMAND c-gateway-conflict-contract-test)

# ============================================================================
# IPC Gateway Components (Phase 1+2: Unix socket + NATS integration)
# ============================================================================

if(BUILD_IPC_GATEWAY)
    message(STATUS "IPC Gateway: ENABLED")
    
    # IPC Protocol library (shared by server, bridge, tests)
    add_library(ipc-protocol STATIC
        src/ipc_protocol.c
    )
    
    target_include_directories(ipc-protocol PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    
    # IPC Config library
    add_library(ipc-config STATIC
        src/ipc_config.c
    )
    
    target_include_directories(ipc-config PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    
    # IPC Server library
    add_library(ipc-server STATIC
        src/ipc_server.c
    )
    
    target_include_directories(ipc-server PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    
    target_link_libraries(ipc-server PUBLIC
        ipc-protocol
    )
    
    # IPC-NATS Bridge library
    add_library(ipc-nats-bridge STATIC
        src/ipc_nats_bridge.c
        src/nats_client_stub.c
    )
    
    target_include_directories(ipc-nats-bridge PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    
    target_link_libraries(ipc-nats-bridge PUBLIC
        ipc-protocol
    )
    
    # IPC Protocol Unit Tests
    add_executable(ipc-protocol-test
        tests/test_ipc_protocol.c
    )
    
    target_link_libraries(ipc-protocol-test PRIVATE
        ipc-protocol
    )
    
    add_test(NAME ipc_protocol_test COMMAND ipc-protocol-test)
    
    # IPC Config Unit Tests
    add_executable(ipc-config-test
        tests/test_ipc_config.c
    )
    
    target_link_libraries(ipc-config-test PRIVATE
        ipc-config
    )
    
    add_test(NAME ipc_config_test COMMAND ipc-config-test)
    
    # IPC Server Demo (basic, no NATS)
    add_executable(ipc-server-demo
        examples/ipc_server_demo.c
    )
    
    target_link_libraries(ipc-server-demo PRIVATE
        ipc-server
        ipc-protocol
    )
    
    # IPC NATS Demo (with bridge)
    add_executable(ipc-nats-demo
        examples/ipc_nats_demo.c
    )
    
    target_link_libraries(ipc-nats-demo PRIVATE
        ipc-server
        ipc-protocol
        ipc-nats-bridge
    )
    
    # Install targets
    install(TARGETS ipc-protocol ipc-config ipc-server ipc-nats-bridge
        ARCHIVE DESTINATION lib
    )
    
    install(TARGETS ipc-server-demo ipc-nats-demo
        RUNTIME DESTINATION bin
    )
    
    install(FILES
        include/ipc_protocol.h
        include/ipc_config.h
        include/ipc_server.h
        include/ipc_nats_bridge.h
        DESTINATION include/c-gateway
    )
    
    message(STATUS "IPC Gateway targets added:")
    message(STATUS "  Libraries: ipc-protocol, ipc-config, ipc-server, ipc-nats-bridge")
    message(STATUS "  Tests: ipc-protocol-test, ipc-config-test")
    message(STATUS "  Demos: ipc-server-demo, ipc-nats-demo")
else()
    message(STATUS "IPC Gateway: DISABLED (use -DBUILD_IPC_GATEWAY=ON to enable)")
endif()

# Router Contract library
add_library(router-contract STATIC
    src/router_contract.c
)
target_include_directories(router-contract PUBLIC include)

# Link router-contract to ipc-nats-bridge
target_link_libraries(ipc-nats-bridge PUBLIC router-contract)

# NATS Resilience library  
add_library(nats-resilience STATIC
    src/nats_resilience.c
)
target_include_directories(nats-resilience PUBLIC include)

# Link to bridge
target_link_libraries(ipc-nats-bridge PUBLIC nats-resilience)

# JSONL Logger library
add_library(jsonl-logger STATIC src/jsonl_logger.c)
target_include_directories(jsonl-logger PUBLIC include)

# JSONL Logger test
add_executable(jsonl-logger-test tests/test_jsonl_logger.c)
target_link_libraries(jsonl-logger-test PRIVATE jsonl-logger)
add_test(NAME jsonl_logger_test COMMAND jsonl-logger-test)

# Add log sanitizer to jsonl-logger
target_sources(jsonl-logger PRIVATE src/log_sanitizer.c)

# Log sanitizer test
add_executable(log-sanitizer-test tests/test_log_sanitizer.c)
target_link_libraries(log-sanitizer-test PRIVATE jsonl-logger)
add_test(NAME log_sanitizer_test COMMAND log-sanitizer-test)
