#!/bin/bash -e

export NAME=udp

# shellcheck source=examples/verify-common.sh
. "$(dirname "${BASH_SOURCE[0]}")/../verify-common.sh"

run_log "Send some UDP packets"
echo -n HELO | nc -4u -w1 127.0.0.1 10000
echo -n OLEH | nc -4u -w1 127.0.0.1 10000

run_log "Check backend log"
docker-compose logs service-udp | grep HELO
docker-compose logs service-udp | grep OLEH

run_log "Check admin stats"
curl -s http://127.0.0.1:10001/stats | grep udp | grep -v "\: 0"
