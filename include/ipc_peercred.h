/**
 * ipc_peercred.h - Unix socket peer credentials (SO_PEERCRED)
 * 
 * Linux-specific authorization using peer credentials
 */

#ifndef IPC_PEERCRED_H
#define IPC_PEERCRED_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Peer credentials
 */
typedef struct {
    uid_t uid;   /* User ID */
    gid_t gid;   /* Group ID */
    pid_t pid;   /* Process ID */
} ipc_peercred_t;

/**
 * Get peer credentials from socket
 * 
 * @param sockfd  Socket file descriptor
 * @param cred    Output credentials
 * @return 0 on success, -1 on error
 */
int ipc_peercred_get(int sockfd, ipc_peercred_t *cred);

/**
 * Check if peer is authorized
 * 
 * Policy: peer UID must match server UID, or peer is root
 * 
 * @param sockfd      Socket file descriptor
 * @param server_uid  Server's UID
 * @return 1 if authorized, 0 otherwise
 */
int ipc_peercred_is_authorized(int sockfd, uid_t server_uid);

#ifdef __cplusplus
}
#endif

#endif /* IPC_PEERCRED_H */
