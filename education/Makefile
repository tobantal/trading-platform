# Makefile
# Trading Platform - команды для разработки и тестирования

NAMESPACE := trading
DOCKER_USER := tobantal

.PHONY: help \
	test-all test-e2e test-e2e-debug test-unit test-trading test-broker test-auth \
	reset-dbs reset-auth-db reset-broker-db reset-trading-db \
	build build-auth build-broker build-trading \
	push push-auth push-broker push-trading \
	deploy redeploy restart logs status \
	ship ship-test ship-auth ship-broker ship-trading

help:
	@echo "Trading Platform Commands:"
	@echo ""
	@echo "=== TESTING ==="
	@echo "  make test-all         - Run all tests (unit + e2e)"
	@echo "  make test-e2e         - Reset DBs + run E2E tests"
	@echo "  make test-e2e-debug   - E2E tests + show logs on failure"
	@echo "  make test-unit        - Run all unit/integration tests"
	@echo "  make test-trading     - Trading service tests"
	@echo "  make test-broker      - Broker service tests"
	@echo "  make test-auth        - Auth service tests"
	@echo ""
	@echo "=== DOCKER ==="
	@echo "  make build            - Build all Docker images"
	@echo "  make build-auth       - Build auth-service image"
	@echo "  make build-broker     - Build broker-service image"
	@echo "  make build-trading    - Build trading-service image"
	@echo "  make push             - Push all images to Docker Hub"
	@echo ""
	@echo "=== KUBERNETES ==="
	@echo "  make deploy           - Apply k8s manifests"
	@echo "  make redeploy         - Full redeploy (delete all + apply)"
	@echo "  make restart          - Restart all deployments"
	@echo "  make status           - Show pods status"
	@echo "  make logs             - Show recent logs from all services"
	@echo ""
	@echo "=== DATABASE ==="
	@echo "  make reset-dbs        - Reset all databases to initial state"
	@echo ""
	@echo "=== WORKFLOWS ==="
	@echo "  make ship             - Build + push + redeploy (all services)"
	@echo "  make ship-test        - Ship + test-e2e-debug"
	@echo "  make ship-auth        - Build + push + restart auth-service"
	@echo "  make ship-broker      - Build + push + restart broker-service"
	@echo "  make ship-trading     - Build + push + restart trading-service"
	@echo ""

# =============================================================================
# DOCKER BUILD
# =============================================================================

build: build-auth build-broker build-trading
	@echo ""
	@echo "=== All images built ==="

build-auth:
	@echo "--- Building auth-service ---"
	docker build -t $(DOCKER_USER)/auth-service:latest -f auth-service/Dockerfile .

build-broker:
	@echo "--- Building broker-service ---"
	docker build -t $(DOCKER_USER)/broker-service:latest -f broker-service/Dockerfile .

build-trading:
	@echo "--- Building trading-service ---"
	docker build -t $(DOCKER_USER)/trading-service:latest -f trading-service/Dockerfile .

# =============================================================================
# DOCKER PUSH
# =============================================================================

push: push-auth push-broker push-trading
	@echo ""
	@echo "=== All images pushed ==="

push-auth:
	@echo "--- Pushing auth-service ---"
	docker push $(DOCKER_USER)/auth-service:latest

push-broker:
	@echo "--- Pushing broker-service ---"
	docker push $(DOCKER_USER)/broker-service:latest

push-trading:
	@echo "--- Pushing trading-service ---"
	docker push $(DOCKER_USER)/trading-service:latest

# =============================================================================
# KUBERNETES DEPLOY
# =============================================================================

deploy:
	@echo "--- Applying k8s manifests ---"
	kubectl apply -f k8s/
	@echo ""
	@echo "Waiting for pods..."
	@sleep 3
	@$(MAKE) status

redeploy:
	@echo "=== Full Redeploy ==="
	@echo ""
	@echo "--- Deleting deployments ---"
	kubectl delete deployment -n $(NAMESPACE) --all 2>/dev/null || true
	@echo ""
	@echo "--- Waiting for pods to terminate ---"
	@sleep 5
	@echo ""
	@echo "--- Applying k8s manifests ---"
	kubectl apply -f k8s/
	@echo ""
	@echo "--- Waiting for pods to start ---"
	@kubectl wait --for=condition=Ready pods --all -n $(NAMESPACE) --timeout=120s 2>/dev/null || true
	@echo ""
	@$(MAKE) status

restart:
	@echo "--- Restarting deployments ---"
	kubectl rollout restart deployment auth-service -n $(NAMESPACE)
	kubectl rollout restart deployment broker-service -n $(NAMESPACE)
	kubectl rollout restart deployment trading-service -n $(NAMESPACE)
	@echo ""
	@echo "--- Waiting for rollout ---"
	kubectl rollout status deployment auth-service -n $(NAMESPACE) --timeout=60s
	kubectl rollout status deployment broker-service -n $(NAMESPACE) --timeout=60s
	kubectl rollout status deployment trading-service -n $(NAMESPACE) --timeout=60s
	@echo ""
	@$(MAKE) status

status:
	@echo "--- Pods Status ---"
	@kubectl get pods -n $(NAMESPACE) -o wide

logs:
	@echo "=== Recent Logs ==="
	@echo ""
	@echo "--- Auth Service ---"
	@kubectl logs -n $(NAMESPACE) deployment/auth-service --tail=20 2>/dev/null || echo "No logs"
	@echo ""
	@echo "--- Broker Service ---"
	@kubectl logs -n $(NAMESPACE) deployment/broker-service --tail=20 2>/dev/null || echo "No logs"
	@echo ""
	@echo "--- Trading Service ---"
	@kubectl logs -n $(NAMESPACE) deployment/trading-service --tail=20 2>/dev/null || echo "No logs"

# =============================================================================
# WORKFLOWS (комбинированные команды)
# =============================================================================

ship: build push redeploy
	@echo ""
	@echo "=== Ship complete ==="

ship-test: ship reset-dbs
	@echo ""
	@sleep 5
	@$(MAKE) test-e2e-debug

# Ship отдельных сервисов
ship-auth: build-auth push-auth
	@echo "--- Restarting auth-service ---"
	kubectl rollout restart deployment auth-service -n $(NAMESPACE)
	kubectl rollout status deployment auth-service -n $(NAMESPACE) --timeout=120s

ship-broker: build-broker push-broker
	@echo "--- Restarting broker-service ---"
	kubectl rollout restart deployment broker-service -n $(NAMESPACE)
	kubectl rollout status deployment broker-service -n $(NAMESPACE) --timeout=120s

ship-trading: build-trading push-trading
	@echo "--- Restarting trading-service ---"
	kubectl rollout restart deployment trading-service -n $(NAMESPACE)
	kubectl rollout status deployment trading-service -n $(NAMESPACE) --timeout=120s

# =============================================================================
# ALL TESTS
# =============================================================================

test-all: test-unit test-e2e

# =============================================================================
# UNIT/INTEGRATION TESTS (per service, isolated)
# =============================================================================

test-unit: test-auth test-broker test-trading

test-trading: reset-dbs
	@echo ""
	@echo "=== Running Trading Service Tests ==="
	newman run postman/trading-service.postman_collection.json

test-broker: reset-broker-db
	@echo ""
	@echo "=== Running Broker Service Tests ==="
	newman run postman/broker-service.postman_collection.json

test-auth: reset-auth-db
	@echo ""
	@echo "=== Running Auth Service Tests ==="
	newman run postman/auth-service.postman_collection.json

# =============================================================================
# E2E TESTS (full system flow)
# =============================================================================

test-e2e: reset-dbs
	@echo ""
	@echo "=== Running E2E Tests ==="
	newman run postman/trading-service-e2e.postman_collection.json

# Debug версия - выводит логи сервисов при падении
test-e2e-debug: reset-dbs
	@echo ""
	@echo "=== Running E2E Tests (debug mode) ==="
	@newman run postman/trading-service-e2e.postman_collection.json || ( \
		echo ""; \
		echo "========================================"; \
		echo "=== TEST FAILED - COLLECTING LOGS ==="; \
		echo "========================================"; \
		echo ""; \
		echo "--- Auth Service Logs ---"; \
		kubectl logs -n $(NAMESPACE) deployment/auth-service --tail=30 2>/dev/null || echo "No logs"; \
		echo ""; \
		echo "--- Broker Service Logs ---"; \
		kubectl logs -n $(NAMESPACE) deployment/broker-service --tail=50 2>/dev/null || echo "No logs"; \
		echo ""; \
		echo "--- Trading Service Logs ---"; \
		kubectl logs -n $(NAMESPACE) deployment/trading-service --tail=50 2>/dev/null || echo "No logs"; \
		echo ""; \
		echo "========================================"; \
		exit 1 \
	)

# =============================================================================
# DATABASE RESET (k8s) - используем существующие SQL скрипты
# =============================================================================

reset-dbs: reset-auth-db reset-broker-db reset-trading-db
	@echo ""
	@echo "=== All databases reset ==="

reset-auth-db:
	@echo "--- Resetting Auth DB ---"
	@cat auth-service/sql/03-clean.sql | kubectl exec -i -n $(NAMESPACE) deployment/auth-postgres -- psql -U auth_user -d auth_db
	@cat auth-service/sql/02-data.sql | kubectl exec -i -n $(NAMESPACE) deployment/auth-postgres -- psql -U auth_user -d auth_db
	@echo "Auth DB reset complete"

reset-broker-db:
	@echo "--- Resetting Broker DB ---"
	@cat broker-service/sql/03-clean.sql | kubectl exec -i -n $(NAMESPACE) deployment/broker-postgres -- psql -U broker_user -d broker_db
	@cat broker-service/sql/02-data.sql | kubectl exec -i -n $(NAMESPACE) deployment/broker-postgres -- psql -U broker_user -d broker_db
	@echo "Broker DB reset complete"

reset-trading-db:
	@echo "--- Resetting Trading DB ---"
	@cat trading-service/sql/03-clean.sql | kubectl exec -i -n $(NAMESPACE) deployment/trading-postgres -- psql -U trading_user -d trading_db
	@cat trading-service/sql/02-data.sql | kubectl exec -i -n $(NAMESPACE) deployment/trading-postgres -- psql -U trading_user -d trading_db 2>/dev/null || echo "Trading DB: no seed data"
	@echo "Trading DB reset complete"
