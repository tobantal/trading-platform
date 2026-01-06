#!/bin/bash
# debug-api.sh - Скрипт для отладки Trading Platform API
# Использование: 
#   ./debug-api.sh                          # первый аккаунт
#   ./debug-api.sh "acc-sandbox-xxx"        # конкретный аккаунт
#   ./debug-api.sh -u myuser -p mypass      # другой пользователь
# ./scripts/debug-api.sh "acc-sandbox-1767675247671597415-4"

set -e

# =============================================================================
# КОНФИГУРАЦИЯ (захардкожена, не берётся из окружения!)
# =============================================================================

BASE_URL="http://arch.homework"
USERNAME="testuser"
PASSWORD="test123"
CURL_OPTS="--max-time 10 -s"

# Парсинг аргументов
ACCOUNT_ID=""
while [[ $# -gt 0 ]]; do
  case $1 in
    -u|--user) USERNAME="$2"; shift 2 ;;
    -p|--pass) PASSWORD="$2"; shift 2 ;;
    -h|--host) BASE_URL="$2"; shift 2 ;;
    *) ACCOUNT_ID="$1"; shift ;;
  esac
done

# Цвета для вывода
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== Trading Platform API Debug ===${NC}"
echo "Base URL: $BASE_URL"
echo "Username: $USERNAME"
echo ""

# =============================================================================
# 1. LOGIN - получаем session_token
# =============================================================================

echo -e "${YELLOW}[1/3] Login...${NC}"

LOGIN_RESPONSE=$(curl $CURL_OPTS -X POST "${BASE_URL}/auth/api/v1/auth/login" \
  -H "Content-Type: application/json" \
  -d "{\"username\":\"${USERNAME}\",\"password\":\"${PASSWORD}\"}")

if [ -z "$LOGIN_RESPONSE" ]; then
  echo -e "${RED}ERROR: No response from server (timeout?)${NC}"
  exit 1
fi

SESSION_TOKEN=$(echo "$LOGIN_RESPONSE" | jq -r '.session_token // empty')

if [ -z "$SESSION_TOKEN" ]; then
  echo -e "${RED}ERROR: Login failed${NC}"
  echo "$LOGIN_RESPONSE" | jq .
  exit 1
fi

echo -e "${GREEN}✓ Session token: ${SESSION_TOKEN:0:20}...${NC}"

# =============================================================================
# 2. GET ACCOUNTS - список аккаунтов
# =============================================================================

echo -e "${YELLOW}[2/3] Getting accounts...${NC}"

ACCOUNTS_RESPONSE=$(curl $CURL_OPTS "${BASE_URL}/auth/api/v1/accounts" \
  -H "Authorization: Bearer $SESSION_TOKEN")

echo "Accounts:"
echo "$ACCOUNTS_RESPONSE" | jq -r '.accounts[]? | "  - \(.account_id) (\(.name // "no name"))"' 2>/dev/null || echo "$ACCOUNTS_RESPONSE" | jq .

# Если account_id не указан - берём первый
if [ -z "$ACCOUNT_ID" ]; then
  ACCOUNT_ID=$(echo "$ACCOUNTS_RESPONSE" | jq -r '.accounts[0].account_id // empty')
fi

if [ -z "$ACCOUNT_ID" ]; then
  echo -e "${RED}ERROR: No accounts found. Create one first.${NC}"
  exit 1
fi

echo -e "${GREEN}✓ Using account: $ACCOUNT_ID${NC}"

# =============================================================================
# 3. SELECT ACCOUNT - получаем access_token
# =============================================================================

echo -e "${YELLOW}[3/3] Getting access token...${NC}"

TOKEN_RESPONSE=$(curl $CURL_OPTS -X POST "${BASE_URL}/auth/api/v1/auth/access-token" \
  -H "Authorization: Bearer $SESSION_TOKEN" \
  -H "Content-Type: application/json" \
  -d "{\"account_id\":\"${ACCOUNT_ID}\"}")

if [ -z "$TOKEN_RESPONSE" ]; then
  echo -e "${RED}ERROR: No response from server (timeout?)${NC}"
  exit 1
fi

ACCESS_TOKEN=$(echo "$TOKEN_RESPONSE" | jq -r '.access_token // empty')

if [ -z "$ACCESS_TOKEN" ]; then
  echo -e "${RED}ERROR: Failed to get access token${NC}"
  echo "$TOKEN_RESPONSE" | jq .
  exit 1
fi

echo -e "${GREEN}✓ Access token: ${ACCESS_TOKEN:0:30}...${NC}"
echo ""

# =============================================================================
# API ЗАПРОСЫ
# =============================================================================

echo -e "${BLUE}=== API Responses ===${NC}"
echo ""

# --- Portfolio ---
echo -e "${YELLOW}>>> GET /trading/api/v1/portfolio${NC}"
curl $CURL_OPTS "${BASE_URL}/trading/api/v1/portfolio" \
  -H "Authorization: Bearer $ACCESS_TOKEN" | jq .
echo ""

# --- Orders ---
echo -e "${YELLOW}>>> GET /trading/api/v1/orders${NC}"
curl $CURL_OPTS "${BASE_URL}/trading/api/v1/orders" \
  -H "Authorization: Bearer $ACCESS_TOKEN" | jq .
echo ""

# --- Quotes ---
echo -e "${YELLOW}>>> GET /trading/api/v1/quotes (SBER, GAZP)${NC}"
curl $CURL_OPTS "${BASE_URL}/trading/api/v1/quotes?figis=BBG004730N88,BBG004730RP0" \
  -H "Authorization: Bearer $ACCESS_TOKEN" | jq .
echo ""

# --- Instruments ---
echo -e "${YELLOW}>>> GET /trading/api/v1/instruments${NC}"
curl $CURL_OPTS "${BASE_URL}/trading/api/v1/instruments" \
  -H "Authorization: Bearer $ACCESS_TOKEN" | jq .
echo ""

# =============================================================================
# EXPORT для дальнейшего использования
# =============================================================================

echo -e "${BLUE}=== Export для ручных запросов ===${NC}"
echo ""
echo "export ACCESS_TOKEN='$ACCESS_TOKEN'"
echo ""
echo "# Примеры:"
echo "curl -s '${BASE_URL}/trading/api/v1/portfolio' -H \"Authorization: Bearer \$ACCESS_TOKEN\" | jq ."
echo "curl -s '${BASE_URL}/trading/api/v1/orders' -H \"Authorization: Bearer \$ACCESS_TOKEN\" | jq ."
echo ""
