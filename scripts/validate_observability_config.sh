#!/bin/bash
# Gateway Observability Configuration Validation Script
# Validates observability configuration settings (for future configurable options)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GATEWAY_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Exit codes
EXIT_SUCCESS=0
EXIT_VALIDATION_ERROR=1
EXIT_CONFIG_MISSING=2

cd "${GATEWAY_DIR}"

echo "=== Gateway Observability Configuration Validation ==="
echo ""

# Current state: Gateway observability is hardcoded (no configuration needed)
# This script is prepared for future configuration options

VALIDATION_ERRORS=0

# Validate log format (if configurable in future)
validate_log_format() {
    local log_format="${GATEWAY_LOG_FORMAT:-json}"
    
    case "${log_format}" in
        json|text|both)
            echo "✅ Log format: ${log_format}"
            ;;
        *)
            echo "❌ Invalid log format: ${log_format} (expected: json, text, or both)"
            VALIDATION_ERRORS=$((VALIDATION_ERRORS + 1))
            ;;
    esac
}

# Validate log level (if configurable in future)
validate_log_level() {
    local log_level="${GATEWAY_LOG_LEVEL:-INFO}"
    
    case "${log_level}" in
        ERROR|WARN|INFO|DEBUG)
            echo "✅ Log level: ${log_level}"
            ;;
        *)
            echo "❌ Invalid log level: ${log_level} (expected: ERROR, WARN, INFO, or DEBUG)"
            VALIDATION_ERRORS=$((VALIDATION_ERRORS + 1))
            ;;
    esac
}

# Validate health endpoint configuration
validate_health_endpoint() {
    local health_port="${GATEWAY_HEALTH_PORT:-8080}"
    
    if [[ "${health_port}" =~ ^[0-9]+$ ]] && [ "${health_port}" -ge 1 ] && [ "${health_port}" -le 65535 ]; then
        echo "✅ Health endpoint port: ${health_port}"
    else
        echo "❌ Invalid health endpoint port: ${health_port} (expected: 1-65535)"
        VALIDATION_ERRORS=$((VALIDATION_ERRORS + 1))
    fi
}

# Validate PII filtering configuration (if configurable in future)
validate_pii_filtering() {
    local pii_filtering="${GATEWAY_PII_FILTERING:-enabled}"
    
    case "${pii_filtering}" in
        enabled|disabled)
            echo "✅ PII filtering: ${pii_filtering}"
            ;;
        *)
            echo "❌ Invalid PII filtering setting: ${pii_filtering} (expected: enabled or disabled)"
            VALIDATION_ERRORS=$((VALIDATION_ERRORS + 1))
            ;;
    esac
}

# Validate timestamp format (if configurable in future)
validate_timestamp_format() {
    local timestamp_format="${GATEWAY_TIMESTAMP_FORMAT:-iso8601}"
    
    case "${timestamp_format}" in
        iso8601|unix|rfc3339)
            echo "✅ Timestamp format: ${timestamp_format}"
            ;;
        *)
            echo "❌ Invalid timestamp format: ${timestamp_format} (expected: iso8601, unix, or rfc3339)"
            VALIDATION_ERRORS=$((VALIDATION_ERRORS + 1))
            ;;
    esac
}

# Validate observability features
validate_observability_features() {
    echo ""
    echo "Validating observability configuration..."
    echo ""
    
    validate_log_format
    validate_log_level
    validate_health_endpoint
    validate_pii_filtering
    validate_timestamp_format
}

# Main validation
main() {
    echo "Current configuration:"
    echo "  GATEWAY_LOG_FORMAT: ${GATEWAY_LOG_FORMAT:-json (default)}"
    echo "  GATEWAY_LOG_LEVEL: ${GATEWAY_LOG_LEVEL:-INFO (default)}"
    echo "  GATEWAY_HEALTH_PORT: ${GATEWAY_HEALTH_PORT:-8080 (default)}"
    echo "  GATEWAY_PII_FILTERING: ${GATEWAY_PII_FILTERING:-enabled (default)}"
    echo "  GATEWAY_TIMESTAMP_FORMAT: ${GATEWAY_TIMESTAMP_FORMAT:-iso8601 (default)}"
    echo ""
    
    validate_observability_features
    
    echo ""
    if [ ${VALIDATION_ERRORS} -eq 0 ]; then
        echo "✅ All configuration validations passed"
        echo ""
        echo "Note: Gateway observability is currently hardcoded (no configuration needed)."
        echo "This script validates future configuration options when they are implemented."
        return ${EXIT_SUCCESS}
    else
        echo "❌ Configuration validation failed (${VALIDATION_ERRORS} error(s))"
        return ${EXIT_VALIDATION_ERROR}
    fi
}

# Run validation
main "$@"

