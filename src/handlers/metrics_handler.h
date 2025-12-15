#ifndef METRICS_HANDLER_H
#define METRICS_HANDLER_H

/**
 * Handle GET /metrics request
 * Returns Prometheus text format metrics
 * @param client_fd Client socket file descriptor
 * @return 0 on success, -1 on error
 */
int handle_metrics_request(int client_fd);

#endif // METRICS_HANDLER_H

