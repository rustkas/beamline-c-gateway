/**
 * test_ipc_capabilities.c - Test capabilities
 */

#include "ipc_capabilities.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static void test_get_capabilities(void) {
    printf("Test: get capabilities JSON... ");
    
    char json[1024];
    int rc = ipc_get_capabilities_json(json, sizeof(json));
    
    assert(rc == 0);
    assert(strlen(json) > 0);
    
    /* Check for key fields */
    assert(strstr(json, "protocol_version") != NULL);
    assert(strstr(json, "supported_message_types") != NULL);
    assert(strstr(json, "max_payload_size") != NULL);
    
    printf("OK\n");
    printf("  Capabilities: %s\n", json);
}

static void test_message_type_support(void) {
    printf("Test: message type support... ");
    
    assert(ipc_is_message_type_supported(IPC_MSG_TASK_SUBMIT) == 1);
    assert(ipc_is_message_type_supported(IPC_MSG_PING) == 1);
    assert(ipc_is_message_type_supported(IPC_MSG_CAPABILITIES) == 1);
    assert(ipc_is_message_type_supported(0xFF) == 0);  /* Invalid */
    
    printf("OK\n");
}

static void test_version_support(void) {
    printf("Test: version support... ");
    
    assert(ipc_is_version_supported(0x01) == 1);  /* Current version */
    assert(ipc_is_version_supported(0x00) == 0);  /* Old version */
    assert(ipc_is_version_supported(0x02) == 0);  /* Future version */
    
    printf("OK\n");
}

int main(void) {
    printf("=== IPC Capabilities Tests ===\n");
    
    test_get_capabilities();
    test_message_type_support();
    test_version_support();
    
    printf("\nAll tests passed!\n");
    return 0;
}
