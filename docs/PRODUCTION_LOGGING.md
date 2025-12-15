# Production Logging Guide

**Component**: Gateway (`apps/c-gateway/`)  
**Purpose**: Production-ready log rotation and management  
**Last Updated**: 2025-01-27

---

## Overview

Gateway logs to **stderr** in structured JSON format. In production environments, logs should be redirected to files and managed with log rotation to prevent disk space issues.

This guide provides:
- Log rotation strategies
- Configuration examples for common tools
- Best practices for production logging
- Log aggregation setup

---

## Log Output

### Default Behavior

Gateway writes all logs to **stderr** in JSONL format (one JSON object per line):

```json
{"timestamp":"2025-01-27T12:00:00.123456Z","level":"INFO","component":"gateway","message":"Request processed","tenant_id":"tenant_123","trace_id":"trace_abc"}
{"timestamp":"2025-01-27T12:00:00.124000Z","level":"ERROR","component":"gateway","message":"Invalid request","error_code":"E_INVALID_REQUEST"}
```

### Redirecting Logs to File

**Basic redirection**:
```bash
./c-gateway 2> /var/log/gateway/gateway.log
```

**With stdout redirection** (if needed):
```bash
./c-gateway > /dev/null 2> /var/log/gateway/gateway.log
```

---

## Log Rotation Strategies

### 1. systemd Service with Log Rotation

**Service file**: `/etc/systemd/system/gateway.service`

```ini
[Unit]
Description=Beamline Gateway Service
After=network.target

[Service]
Type=simple
User=gateway
Group=gateway
WorkingDirectory=/opt/gateway
ExecStart=/opt/gateway/c-gateway
StandardError=append:/var/log/gateway/gateway.log
StandardOutput=journal
Restart=on-failure
RestartSec=5

# Resource limits
LimitNOFILE=65536
MemoryMax=512M

[Install]
WantedBy=multi-user.target
```

**Enable and start**:
```bash
sudo systemctl daemon-reload
sudo systemctl enable gateway
sudo systemctl start gateway
```

**View logs**:
```bash
# Via systemd journal
sudo journalctl -u gateway -f

# Via log file
tail -f /var/log/gateway/gateway.log
```

### 2. logrotate Configuration

**Configuration file**: `/etc/logrotate.d/gateway`

```bash
/var/log/gateway/*.log {
    daily
    rotate 7
    compress
    delaycompress
    notifempty
    create 0640 gateway gateway
    sharedscripts
    postrotate
        systemctl reload gateway > /dev/null 2>&1 || true
    endscript
}
```

**Options explained**:
- `daily`: Rotate logs daily
- `rotate 7`: Keep 7 days of logs
- `compress`: Compress rotated logs (gzip)
- `delaycompress`: Don't compress immediately (compress on next rotation)
- `notifempty`: Don't rotate empty files
- `create 0640 gateway gateway`: Create new log file with permissions and ownership
- `sharedscripts`: Run postrotate script once for all matching files
- `postrotate`: Reload service after rotation (if needed)

**Test configuration**:
```bash
sudo logrotate -d /etc/logrotate.d/gateway  # Dry run
sudo logrotate -f /etc/logrotate.d/gateway  # Force rotation
```

### 3. Docker Log Rotation

**Docker Compose**:
```yaml
version: '3.8'

services:
  gateway:
    image: beamline/gateway:latest
    logging:
      driver: "json-file"
      options:
        max-size: "10m"
        max-file: "3"
        labels: "production"
    environment:
      - GATEWAY_PORT=8080
```

**Docker run**:
```bash
docker run -d \
  --name gateway \
  --log-driver json-file \
  --log-opt max-size=10m \
  --log-opt max-file=3 \
  beamline/gateway:latest
```

**Docker log driver options**:
- `max-size`: Maximum size of log file before rotation (e.g., "10m", "100k")
- `max-file`: Number of log files to keep
- `labels`: Add labels to logs

**View logs**:
```bash
docker logs -f gateway
docker logs --tail 100 gateway
```

### 4. Kubernetes Log Rotation

**Deployment with log rotation**:
```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: gateway
spec:
  replicas: 2
  template:
    spec:
      containers:
      - name: gateway
        image: beamline/gateway:latest
        resources:
          limits:
            memory: "512Mi"
            cpu: "500m"
        volumeMounts:
        - name: log-volume
          mountPath: /var/log/gateway
      volumes:
      - name: log-volume
        emptyDir: {}
```

**Use sidecar for log rotation**:
```yaml
containers:
- name: gateway
  image: beamline/gateway:latest
  volumeMounts:
  - name: log-volume
    mountPath: /var/log/gateway
- name: logrotate
  image: busybox:latest
  command: ["/bin/sh", "-c"]
  args:
    - |
      while true; do
        if [ -f /var/log/gateway/gateway.log ]; then
          if [ $(stat -c%s /var/log/gateway/gateway.log) -gt 10485760 ]; then
            mv /var/log/gateway/gateway.log /var/log/gateway/gateway.log.$(date +%Y%m%d%H%M%S)
            touch /var/log/gateway/gateway.log
          fi
        fi
        sleep 60
      done
  volumeMounts:
  - name: log-volume
    mountPath: /var/log/gateway
```

---

## Log Aggregation

### Loki (Grafana Loki)

**Promtail configuration**: `/etc/promtail/config.yml`

```yaml
server:
  http_listen_port: 9080
  grpc_listen_port: 0

positions:
  filename: /tmp/positions.yaml

clients:
  - url: http://loki:3100/loki/api/v1/push

scrape_configs:
  - job_name: gateway
    static_configs:
      - targets:
          - localhost
        labels:
          job: gateway
          __path__: /var/log/gateway/*.log
    pipeline_stages:
      - json:
          expressions:
            timestamp: timestamp
            level: level
            component: component
            message: message
            tenant_id: tenant_id
            trace_id: trace_id
      - labels:
          level:
          component:
          tenant_id:
          trace_id:
      - timestamp:
          source: timestamp
          format: RFC3339Nano
```

**Docker Compose with Loki**:
```yaml
version: '3.8'

services:
  gateway:
    image: beamline/gateway:latest
    logging:
      driver: "json-file"
      options:
        max-size: "10m"
        max-file: "3"

  promtail:
    image: grafana/promtail:latest
    volumes:
      - /var/log/gateway:/var/log/gateway:ro
      - ./promtail-config.yml:/etc/promtail/config.yml
    command: -config.file=/etc/promtail/config.yml

  loki:
    image: grafana/loki:latest
    ports:
      - "3100:3100"
    command: -config.file=/etc/loki/local-config.yaml

  grafana:
    image: grafana/grafana:latest
    ports:
      - "3000:3000"
    environment:
      - GF_SECURITY_ADMIN_PASSWORD=admin
```

### ELK Stack (Elasticsearch, Logstash, Kibana)

**Logstash configuration**: `/etc/logstash/conf.d/gateway.conf`

```ruby
input {
  file {
    path => "/var/log/gateway/*.log"
    codec => json
    type => "gateway"
    start_position => "beginning"
  }
}

filter {
  if [type] == "gateway" {
    # Parse timestamp
    date {
      match => [ "timestamp", "ISO8601" ]
    }
    
    # Add fields
    mutate {
      add_field => { "service" => "gateway" }
    }
  }
}

output {
  elasticsearch {
    hosts => ["elasticsearch:9200"]
    index => "gateway-%{+YYYY.MM.dd}"
  }
}
```

---

## Best Practices

### 1. Log Retention

**Recommended retention periods**:
- **Development**: 3-7 days
- **Staging**: 7-14 days
- **Production**: 14-30 days (or based on compliance requirements)

**Adjust logrotate**:
```bash
/var/log/gateway/*.log {
    daily
    rotate 30        # Keep 30 days
    compress
    delaycompress
    # ...
}
```

### 2. Disk Space Management

**Monitor disk usage**:
```bash
# Check log directory size
du -sh /var/log/gateway/

# Check individual log files
ls -lh /var/log/gateway/

# Find large log files
find /var/log/gateway -type f -size +100M
```

**Set up alerts**:
- Alert when log directory exceeds 80% of disk space
- Alert when log rotation fails
- Alert when log aggregation stops

### 3. Log Format Consistency

Gateway uses structured JSON logging. Ensure:
- Log aggregation tools parse JSON correctly
- Timestamp format is preserved (ISO 8601)
- CP1 fields (tenant_id, trace_id, run_id) are indexed for search

### 4. Performance Considerations

**High-volume logging**:
- Use async logging if available
- Buffer logs before writing to disk
- Compress rotated logs to save space
- Use log aggregation to offload processing

**Example with buffering**:
```bash
# Use stdbuf for line buffering
stdbuf -oL -eL ./c-gateway 2> /var/log/gateway/gateway.log
```

### 5. Security

**Log file permissions**:
```bash
# Restrict access to log files
chmod 640 /var/log/gateway/gateway.log
chown gateway:gateway /var/log/gateway/gateway.log
```

**PII filtering**:
- Gateway automatically filters PII in logs
- Verify PII filtering is working correctly
- Review logs periodically for sensitive data

---

## Troubleshooting

### Logs Not Rotating

**Check logrotate**:
```bash
# Test configuration
sudo logrotate -d /etc/logrotate.d/gateway

# Check logrotate status
cat /var/lib/logrotate/status | grep gateway

# Force rotation
sudo logrotate -f /etc/logrotate.d/gateway
```

### Disk Space Issues

**Emergency cleanup**:
```bash
# Remove old rotated logs
find /var/log/gateway -name "*.log.*" -mtime +30 -delete

# Compress large log files
gzip /var/log/gateway/gateway.log

# Truncate current log (if needed)
> /var/log/gateway/gateway.log
```

### Log Aggregation Not Working

**Check Promtail/Logstash**:
```bash
# Check Promtail logs
docker logs promtail

# Check Logstash logs
docker logs logstash

# Test log file access
cat /var/log/gateway/gateway.log | head -n 1
```

---

## References

- [Gateway Observability Documentation](./OBSERVABILITY.md) - Complete observability guide
- [logrotate Manual](https://linux.die.net/man/8/logrotate) - logrotate documentation
- [systemd Journal](https://www.freedesktop.org/software/systemd/man/systemd-journald.service.html) - systemd logging
- [Docker Logging](https://docs.docker.com/config/containers/logging/) - Docker log drivers
- [Grafana Loki](https://grafana.com/docs/loki/latest/) - Loki documentation
- [ELK Stack](https://www.elastic.co/guide/index.html) - Elasticsearch, Logstash, Kibana

