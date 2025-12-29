#!/bin/bash
# =============================================================================
# test-trading-flow.sh — Ручное тестирование Trading API
# =============================================================================
# Аналог TradingFlowIntegrationTest, но через curl
#
# Использование:
#   ./test-trading-flow.sh [base_url]
#
# Примеры:
#   ./test-trading-flow.sh                    # localhost:8080
#   ./test-trading-flow.sh http://localhost:8080
#   ./test-trading-flow.sh http://trading-api:8080
# =============================================================================

set -e

# Цвета для вывода
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Base URL
BASE_URL="${1:-http://localhost:8080}"

# Тестовые данные (совпадают с InMemory репозиториями)
TEST_USER="trader1"
TEST_PASS="password1"
TEST_ACCOUNT="acc-001-sandbox"

# Переменные для токенов
SESSION_TOKEN=""
ACCESS_TOKEN=""

# =============================================================================
# Утилиты
# =============================================================================

log_step() {
    echo -e "\n${BLUE}═══════════════════════════════════════════════════════════════${NC}"
    echo -e "${BLUE}  $1${NC}"
    echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
}

log_success() {
    echo -e "${GREEN}  ✓ $1${NC}"
}

log_error() {
    echo -e "${RED}  ✗ $1${NC}"
}

log_info() {
    echo -e "${YELLOW}  → $1${NC}"
}

check_response() {
    local response="$1"
    local expected_field="$2"
    
    if echo "$response" | jq -e ".$expected_field" > /dev/null 2>&1; then
        return 0
    else
        return 1
    fi
}

# =============================================================================
# STEP 0: Health Check
# =============================================================================

health_check() {
    log_step "STEP 0: Health Check"
    
    log_info "GET $BASE_URL/api/v1/health"
    
    RESPONSE=$(curl -s -w "\n%{http_code}" "$BASE_URL/api/v1/health")
    HTTP_CODE=$(echo "$RESPONSE" | tail -n1)
    BODY=$(echo "$RESPONSE" | sed '$d')
    
    if [ "$HTTP_CODE" = "200" ]; then
        log_success "Health check passed (HTTP $HTTP_CODE)"
        echo "$BODY" | jq . 2>/dev/null || echo "$BODY"
    else
        log_error "Health check failed (HTTP $HTTP_CODE)"
        echo "$BODY"
        exit 1
    fi
}

# =============================================================================
# STEP 1: Login
# =============================================================================

do_login() {
    log_step "STEP 1: Login"
    
    log_info "POST $BASE_URL/api/v1/auth/login"
    log_info "Username: $TEST_USER"
    
    RESPONSE=$(curl -s -w "\n%{http_code}" \
        -X POST "$BASE_URL/api/v1/auth/login" \
        -H "Content-Type: application/json" \
        -d "{\"username\": \"$TEST_USER\", \"password\": \"$TEST_PASS\"}")
    
    HTTP_CODE=$(echo "$RESPONSE" | tail -n1)
    BODY=$(echo "$RESPONSE" | sed '$d')
    
    if [ "$HTTP_CODE" = "200" ]; then
        log_success "Login successful (HTTP $HTTP_CODE)"
        
        SESSION_TOKEN=$(echo "$BODY" | jq -r '.session_token')
        ACCOUNTS=$(echo "$BODY" | jq -r '.accounts | length')
        
        log_info "Session token: ${SESSION_TOKEN:0:20}..."
        log_info "Available accounts: $ACCOUNTS"
        
        echo "$BODY" | jq '.accounts'
    else
        log_error "Login failed (HTTP $HTTP_CODE)"
        echo "$BODY" | jq . 2>/dev/null || echo "$BODY"
        exit 1
    fi
}

# =============================================================================
# STEP 2: Select Account
# =============================================================================

select_account() {
    log_step "STEP 2: Select Account"
    
    log_info "POST $BASE_URL/api/v1/auth/select-account"
    log_info "Account ID: $TEST_ACCOUNT"
    
    RESPONSE=$(curl -s -w "\n%{http_code}" \
        -X POST "$BASE_URL/api/v1/auth/select-account" \
        -H "Content-Type: application/json" \
        -H "Authorization: Bearer $SESSION_TOKEN" \
        -d "{\"account_id\": \"$TEST_ACCOUNT\"}")
    
    HTTP_CODE=$(echo "$RESPONSE" | tail -n1)
    BODY=$(echo "$RESPONSE" | sed '$d')
    
    if [ "$HTTP_CODE" = "200" ]; then
        log_success "Account selected (HTTP $HTTP_CODE)"
        
        ACCESS_TOKEN=$(echo "$BODY" | jq -r '.access_token')
        ACCOUNT_NAME=$(echo "$BODY" | jq -r '.account.name // "N/A"')
        
        log_info "Access token: ${ACCESS_TOKEN:0:20}..."
        log_info "Account name: $ACCOUNT_NAME"
    else
        log_error "Select account failed (HTTP $HTTP_CODE)"
        echo "$BODY" | jq . 2>/dev/null || echo "$BODY"
        exit 1
    fi
}

# =============================================================================
# STEP 3: Get Portfolio
# =============================================================================

get_portfolio() {
    log_step "STEP 3: Get Portfolio"
    
    log_info "GET $BASE_URL/api/v1/portfolio"
    
    RESPONSE=$(curl -s -w "\n%{http_code}" \
        -X GET "$BASE_URL/api/v1/portfolio" \
        -H "Authorization: Bearer $ACCESS_TOKEN")
    
    HTTP_CODE=$(echo "$RESPONSE" | tail -n1)
    BODY=$(echo "$RESPONSE" | sed '$d')
    
    if [ "$HTTP_CODE" = "200" ]; then
        log_success "Portfolio retrieved (HTTP $HTTP_CODE)"
        
        CASH=$(echo "$BODY" | jq -r '.cash.amount // 0')
        POSITIONS=$(echo "$BODY" | jq -r '.positions | length')
        
        log_info "Cash: $CASH RUB"
        log_info "Positions: $POSITIONS"
        
        echo "$BODY" | jq '{ account_id, cash, total_value, positions: (.positions | length) }'
    else
        log_error "Get portfolio failed (HTTP $HTTP_CODE)"
        echo "$BODY" | jq . 2>/dev/null || echo "$BODY"
        exit 1
    fi
}

# =============================================================================
# STEP 4: Place Order
# =============================================================================

place_order() {
    log_step "STEP 4: Place Market Order"
    
    log_info "POST $BASE_URL/api/v1/orders"
    log_info "FIGI: BBG004730N88 (SBER)"
    log_info "Direction: BUY"
    log_info "Quantity: 10"
    
    RESPONSE=$(curl -s -w "\n%{http_code}" \
        -X POST "$BASE_URL/api/v1/orders" \
        -H "Content-Type: application/json" \
        -H "Authorization: Bearer $ACCESS_TOKEN" \
        -d '{
            "figi": "BBG004730N88",
            "direction": "BUY",
            "type": "MARKET",
            "quantity": 10
        }')
    
    HTTP_CODE=$(echo "$RESPONSE" | tail -n1)
    BODY=$(echo "$RESPONSE" | sed '$d')
    
    if [ "$HTTP_CODE" = "201" ]; then
        log_success "Order placed (HTTP $HTTP_CODE)"
        
        ORDER_ID=$(echo "$BODY" | jq -r '.order_id')
        ORDER_STATUS=$(echo "$BODY" | jq -r '.status')
        
        log_info "Order ID: $ORDER_ID"
        log_info "Status: $ORDER_STATUS"
        
        echo "$BODY" | jq .
    else
        log_error "Place order failed (HTTP $HTTP_CODE)"
        echo "$BODY" | jq . 2>/dev/null || echo "$BODY"
        # Не выходим, продолжаем тест
    fi
}

# =============================================================================
# STEP 5: Get Orders
# =============================================================================

get_orders() {
    log_step "STEP 5: Get Orders"
    
    log_info "GET $BASE_URL/api/v1/orders"
    
    RESPONSE=$(curl -s -w "\n%{http_code}" \
        -X GET "$BASE_URL/api/v1/orders" \
        -H "Authorization: Bearer $ACCESS_TOKEN")
    
    HTTP_CODE=$(echo "$RESPONSE" | tail -n1)
    BODY=$(echo "$RESPONSE" | sed '$d')
    
    if [ "$HTTP_CODE" = "200" ]; then
        log_success "Orders retrieved (HTTP $HTTP_CODE)"
        
        ORDERS_COUNT=$(echo "$BODY" | jq -r '.orders | length')
        log_info "Total orders: $ORDERS_COUNT"
        
        echo "$BODY" | jq '.orders[:3]'  # Показываем первые 3
    else
        log_error "Get orders failed (HTTP $HTTP_CODE)"
        echo "$BODY" | jq . 2>/dev/null || echo "$BODY"
    fi
}

# =============================================================================
# STEP 6: Get Market Data
# =============================================================================

get_quotes() {
    log_step "STEP 6: Get Market Quotes"
    
    log_info "GET $BASE_URL/api/v1/quotes?figis=BBG004730N88"
    
    RESPONSE=$(curl -s -w "\n%{http_code}" \
        -X GET "$BASE_URL/api/v1/quotes?figis=BBG004730N88" \
        -H "Authorization: Bearer $ACCESS_TOKEN")
    
    HTTP_CODE=$(echo "$RESPONSE" | tail -n1)
    BODY=$(echo "$RESPONSE" | sed '$d')
    
    if [ "$HTTP_CODE" = "200" ]; then
        log_success "Quote retrieved (HTTP $HTTP_CODE)"
        echo "$BODY" | jq .
    else
        log_error "Get quote failed (HTTP $HTTP_CODE)"
        echo "$BODY" | jq . 2>/dev/null || echo "$BODY"
    fi
}

# =============================================================================
# STEP 7: Error Cases
# =============================================================================

test_errors() {
    log_step "STEP 7: Test Error Cases"
    
    # Test 1: Request without token
    log_info "Test 1: Order without token (expect 401)"
    RESPONSE=$(curl -s -w "\n%{http_code}" \
        -X POST "$BASE_URL/api/v1/orders" \
        -H "Content-Type: application/json" \
        -d '{"figi": "BBG004730N88", "direction": "BUY", "type": "MARKET", "quantity": 10}')
    HTTP_CODE=$(echo "$RESPONSE" | tail -n1)
    
    if [ "$HTTP_CODE" = "401" ]; then
        log_success "Correctly rejected (HTTP $HTTP_CODE)"
    else
        log_error "Expected 401, got $HTTP_CODE"
    fi
    
    # Test 2: Invalid token
    log_info "Test 2: Order with invalid token (expect 401)"
    RESPONSE=$(curl -s -w "\n%{http_code}" \
        -X POST "$BASE_URL/api/v1/orders" \
        -H "Content-Type: application/json" \
        -H "Authorization: Bearer invalid_token_xyz" \
        -d '{"figi": "BBG004730N88", "direction": "BUY", "type": "MARKET", "quantity": 10}')
    HTTP_CODE=$(echo "$RESPONSE" | tail -n1)
    
    if [ "$HTTP_CODE" = "401" ]; then
        log_success "Correctly rejected (HTTP $HTTP_CODE)"
    else
        log_error "Expected 401, got $HTTP_CODE"
    fi
    
    # Test 3: Order with session token (not access token)
    log_info "Test 3: Order with session token (expect 401)"
    RESPONSE=$(curl -s -w "\n%{http_code}" \
        -X POST "$BASE_URL/api/v1/orders" \
        -H "Content-Type: application/json" \
        -H "Authorization: Bearer $SESSION_TOKEN" \
        -d '{"figi": "BBG004730N88", "direction": "BUY", "type": "MARKET", "quantity": 10}')
    HTTP_CODE=$(echo "$RESPONSE" | tail -n1)
    
    if [ "$HTTP_CODE" = "401" ]; then
        log_success "Correctly rejected session token (HTTP $HTTP_CODE)"
    else
        log_error "Expected 401, got $HTTP_CODE"
    fi
    
    # Test 4: Missing required field
    log_info "Test 4: Order without FIGI (expect 400)"
    RESPONSE=$(curl -s -w "\n%{http_code}" \
        -X POST "$BASE_URL/api/v1/orders" \
        -H "Content-Type: application/json" \
        -H "Authorization: Bearer $ACCESS_TOKEN" \
        -d '{"direction": "BUY", "type": "MARKET", "quantity": 10}')
    HTTP_CODE=$(echo "$RESPONSE" | tail -n1)
    
    if [ "$HTTP_CODE" = "400" ]; then
        log_success "Correctly rejected (HTTP $HTTP_CODE)"
    else
        log_error "Expected 400, got $HTTP_CODE"
    fi
}

# =============================================================================
# SUMMARY
# =============================================================================

print_summary() {
    log_step "TEST SUMMARY"
    
    echo -e "${GREEN}"
    echo "  ┌─────────────────────────────────────────────────────────────┐"
    echo "  │                    ALL TESTS COMPLETED                      │"
    echo "  └─────────────────────────────────────────────────────────────┘"
    echo -e "${NC}"
    
    echo "  Tested endpoints:"
    echo "    • POST /api/v1/auth/login"
    echo "    • POST /api/v1/auth/select-account"
    echo "    • GET  /api/v1/portfolio"
    echo "    • POST /api/v1/orders"
    echo "    • GET  /api/v1/orders"
    echo "    • GET  /api/v1/quotes"
    echo ""
    echo "  Test credentials:"
    echo "    • Username: $TEST_USER"
    echo "    • Password: $TEST_PASS"
    echo "    • Account:  $TEST_ACCOUNT"
}

# =============================================================================
# MAIN
# =============================================================================

main() {
    echo -e "${GREEN}"
    echo "╔═══════════════════════════════════════════════════════════════╗"
    echo "║           TRADING PLATFORM — API TEST SCRIPT                  ║"
    echo "║                                                               ║"
    echo "║  Base URL: $BASE_URL"
    echo "╚═══════════════════════════════════════════════════════════════╝"
    echo -e "${NC}"
    
    # Проверка наличия jq
    if ! command -v jq &> /dev/null; then
        log_error "jq is required but not installed. Install it with: apt install jq"
        exit 1
    fi
    
    # Выполняем шаги
    health_check
    do_login
    select_account
    get_portfolio
    place_order
    get_orders
    get_quotes
    test_errors
    print_summary
}

# Запуск
main "$@"
