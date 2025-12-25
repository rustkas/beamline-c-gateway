/**
 * ipc_capabilities.h - IPC protocol capabilities
 */

#ifndef IPC_CAPABILITIES_H
#define IPC_CAPABILITIES_H

#include "ipc_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Get capabilities response (JSON)
 * 
 * Returns JSON with:
 * {
 *   "protocol_version": "1.0",
 *   "supported_versions": ["1.0"],
 *   "supported_message_types": [1, 2, 3, ...],
 *   "max_payload_size": 4194298,
 *   "features": ["streaming", "cancellation"]
 * }
 * 
 * @param out_json   Output buffer
 * @param buf_size   Buffer size
 * @return 0 on success, -1 on error
 */
int ipc_get_capabilities_json(char *out_json, size_t buf_size);

/**
 * Check if message type is supported
 * 
 * @param msg_type  Message type
 * @return 1 if supported, 0 otherwise
 */
int ipc_is_message_type_supported(ipc_message_type_t msg_type);

/**
 * Check if version is supported
 * 
 * @param version  Protocol version
 * @return 1 if supported, 0 otherwise
 */
int ipc_is_version_supported(uint8_t version);

#ifdef __cplusplus
}
#endif

#endif /* IPC_CAPABILITIES_H */
