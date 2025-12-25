/**
 * test_ipc_peercred.c - Test peer credentials
 */

#include "ipc_peercred.h"
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <sys/socket.h>

static void test_invalid_socket(void) {
    printf("Test: invalid socket... ");
    
    ipc_peercred_t cred;
    int rc = ipc_peercred_get(-1, &cred);
    
    assert(rc == -1);  /* Should fail */
    
    printf("OK\n");
}

static void test_socketpair(void) {
    printf("Test: socketpair credentials... ");
    
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
        printf("SKIP (socketpair failed)\n");
        return;
    }
    
    ipc_peercred_t cred;
    int rc = ipc_peercred_get(sv[0], &cred);
    
    if (rc == 0) {
        /* On Linux: should get valid creds */
        printf("uid=%d, gid=%d, pid=%d ", cred.uid, cred.gid, cred.pid);
        
        /* Check authorization (should match ourselves) */
        uid_t my_uid = getuid();
        int authed = ipc_peercred_is_authorized(sv[0], my_uid);
        assert(authed == 1);
    }
    
    close(sv[0]);
    close(sv[1]);
    
    printf("OK\n");
}

int main(void) {
    printf("=== IPC PeerCred Tests ===\n");
    
    test_invalid_socket();
    test_socketpair();
    
    printf("\nAll tests passed!\n");
    return 0;
}
