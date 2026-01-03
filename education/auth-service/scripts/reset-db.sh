#!/bin/bash
# auth-service/scripts/reset-db.sh
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SQL_DIR="$SCRIPT_DIR/../sql"

NAMESPACE="${NAMESPACE:-trading}"
DB_POD_LABEL="${DB_POD_LABEL:-app=auth-postgres}"
DB_NAME="${DB_NAME:-auth_db}"
DB_USER="${DB_USER:-auth_user}"

echo "=== Auth DB Reset ==="

DB_POD=$(kubectl get pods -n "$NAMESPACE" -l "$DB_POD_LABEL" -o jsonpath='{.items[0].metadata.name}' 2>/dev/null)
if [ -z "$DB_POD" ]; then
    echo "ERROR: DB pod not found (label: $DB_POD_LABEL)"
    exit 1
fi
echo "Pod: $DB_POD"

echo "Step 1: Clean..."
kubectl exec -n "$NAMESPACE" "$DB_POD" -- psql -U "$DB_USER" -d "$DB_NAME" -f /dev/stdin < "$SQL_DIR/03-clean.sql"

echo "Step 2: Load data..."
kubectl exec -n "$NAMESPACE" "$DB_POD" -- psql -U "$DB_USER" -d "$DB_NAME" -f /dev/stdin < "$SQL_DIR/02-data.sql"

echo "=== Done ==="
