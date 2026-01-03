#!/bin/bash
# scripts/reset-all-dbs.sh
# Сброс всех БД микросервисов
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$SCRIPT_DIR/.."

echo "=== Reset All Databases ==="

echo ""
echo "--- Auth Service ---"
"$ROOT_DIR/auth-service/scripts/reset-db.sh"

echo ""
echo "--- Broker Service ---"
"$ROOT_DIR/broker-service/scripts/reset-db.sh"

echo ""
echo "--- Trading Service ---"
"$ROOT_DIR/trading-service/scripts/reset-db.sh"

echo ""
echo "=== All Done ==="
