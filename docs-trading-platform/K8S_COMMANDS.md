# Kubernetes: Справочник команд

## Содержание
- [Деплой](#деплой)
- [Просмотр состояния](#просмотр-состояния)
- [Логи](#логи)
- [Перезапуск](#перезапуск)
- [Остановка и удаление](#остановка-и-удаление)
- [Отладка](#отладка)
- [Работа с БД](#работа-с-бд)
- [Полезные алиасы](#полезные-алиасы)

---

## Деплой

```bash
# Деплой всего проекта (с Kustomize)
kubectl kustomize k8s/ --load-restrictor=LoadRestrictionsNone | kubectl apply -f -

# Деплой без Kustomize (если не используется)
kubectl apply -f k8s/

# Деплой конкретного файла
kubectl apply -f k8s/broker-service.yaml

# Посмотреть что сгенерирует Kustomize (без применения)
kubectl kustomize k8s/
```

---

## Просмотр состояния

### Поды

```bash
# Все поды в namespace
kubectl get pods -n trading

# С дополнительной информацией (IP, нода)
kubectl get pods -n trading -o wide

# Следить за изменениями в реальном времени
kubectl get pods -n trading -w

# Поды конкретного сервиса
kubectl get pods -n trading -l app=broker-service

# Детальная информация о поде
kubectl describe pod <pod-name> -n trading
```

### Сервисы и endpoints

```bash
# Все сервисы
kubectl get svc -n trading

# Endpoints (куда идёт трафик)
kubectl get endpoints -n trading

# Ingress
kubectl get ingress -n trading
kubectl describe ingress -n trading
```

### Deployments

```bash
# Список deployments
kubectl get deployments -n trading

# Статус rollout
kubectl rollout status deployment/broker-service -n trading

# История деплоев
kubectl rollout history deployment/broker-service -n trading
```

### Всё сразу

```bash
# Все ресурсы в namespace
kubectl get all -n trading

# Включая configmaps и secrets
kubectl get all,cm,secret -n trading
```

---

## Логи

```bash
# Логи пода
kubectl logs <pod-name> -n trading

# Логи с фолловингом (как tail -f)
kubectl logs -f <pod-name> -n trading

# Последние 100 строк
kubectl logs --tail=100 <pod-name> -n trading

# Логи за последний час
kubectl logs --since=1h <pod-name> -n trading

# Логи предыдущего контейнера (после рестарта)
kubectl logs <pod-name> -n trading --previous

# Логи по label (все поды сервиса)
kubectl logs -l app=broker-service -n trading

# Логи конкретного контейнера (если несколько в поде)
kubectl logs <pod-name> -c <container-name> -n trading
```

### Логи всех сервисов одновременно

```bash
# Установить stern (удобный инструмент)
# brew install stern  # macOS
# sudo apt install stern  # Ubuntu

# Логи всех подов по паттерну
stern broker -n trading
stern ".*-service" -n trading
```

---

## Перезапуск

### Перезапуск deployment (рекомендуется)

```bash
# Graceful restart - создаёт новые поды, потом удаляет старые
kubectl rollout restart deployment/broker-service -n trading

# Перезапуск всех deployments
kubectl rollout restart deployment -n trading

# Перезапуск конкретных сервисов
kubectl rollout restart deployment/auth-service deployment/broker-service -n trading
```

### Удаление пода (будет пересоздан)

```bash
# Удалить под - deployment создаст новый
kubectl delete pod <pod-name> -n trading

# Удалить все поды сервиса
kubectl delete pods -l app=broker-service -n trading
```

### Масштабирование

```bash
# Остановить (0 реплик)
kubectl scale deployment/broker-service --replicas=0 -n trading

# Запустить (1 реплика)
kubectl scale deployment/broker-service --replicas=1 -n trading

# Масштабировать
kubectl scale deployment/broker-service --replicas=3 -n trading
```

### Откат к предыдущей версии

```bash
# Откат на предыдущую версию
kubectl rollout undo deployment/broker-service -n trading

# Откат на конкретную ревизию
kubectl rollout undo deployment/broker-service --to-revision=2 -n trading
```

---

## Остановка и удаление

### Остановка (сохраняя конфигурацию)

```bash
# Остановить один сервис (0 реплик)
kubectl scale deployment/broker-service --replicas=0 -n trading

# Остановить все сервисы
kubectl scale deployment --all --replicas=0 -n trading
```

### Удаление

```bash
# Удалить конкретный ресурс
kubectl delete deployment/broker-service -n trading
kubectl delete service/broker-service -n trading

# Удалить по файлу
kubectl delete -f k8s/broker-service.yaml

# Удалить всё из Kustomize
#kubectl delete -k k8s/
# ограничение Kustomize - нельзя ссылаться на файлы вне текущей директории.
# напрямую по namespace:

kubectl delete namespace trading
#Или по отдельным ресурсам:

kubectl delete all --all -n trading
kubectl delete configmap --all -n trading
kubectl delete secret --all -n trading
kubectl delete pvc --all -n trading


# Удалить весь namespace (ОСТОРОЖНО - удалит ВСЁ)
kubectl delete namespace trading
```

---

## Отладка

### Зайти в контейнер

```bash
# Интерактивный shell
kubectl exec -it <pod-name> -n trading -- /bin/bash

# Если нет bash
kubectl exec -it <pod-name> -n trading -- /bin/sh

# Выполнить команду
kubectl exec <pod-name> -n trading -- ls -la /app
kubectl exec <pod-name> -n trading -- cat /etc/hosts
```

### Проверка сети

```bash
# Из пода проверить доступность сервиса
kubectl exec -it <pod-name> -n trading -- curl http://broker-service:8083/health
kubectl exec -it <pod-name> -n trading -- nc -zv broker-postgres 5432

# DNS lookup
kubectl exec -it <pod-name> -n trading -- nslookup broker-service
```

### Проброс портов (для локальной отладки)

```bash
# Проброс порта сервиса
kubectl port-forward svc/broker-service 8083:8083 -n trading

# Проброс порта пода
kubectl port-forward <pod-name> 8083:8083 -n trading

# Проброс PostgreSQL для локального подключения
kubectl port-forward svc/broker-postgres 5432:5432 -n trading
# Затем: psql -h localhost -U broker_user -d broker_db
```

### События

```bash
# События в namespace (ошибки, рестарты)
kubectl get events -n trading --sort-by='.lastTimestamp'

# События конкретного пода
kubectl describe pod <pod-name> -n trading | grep -A 20 Events
```

### Проверка ресурсов

```bash
# Использование CPU/Memory подами
kubectl top pods -n trading

# Использование нодами
kubectl top nodes
```

---

## Работа с БД

### Подключение к PostgreSQL

```bash
# Найти под с БД
kubectl get pods -n trading -l app=broker-postgres

# Зайти в psql
kubectl exec -it <broker-postgres-pod> -n trading -- psql -U broker_user -d broker_db

# Выполнить SQL команду
kubectl exec -it <broker-postgres-pod> -n trading -- psql -U broker_user -d broker_db -c "SELECT * FROM instruments;"
```

### Сброс данных

```bash
# Сброс одной БД
./broker-service/scripts/reset-db.sh

# Сброс всех БД
./scripts/reset-all-dbs.sh
```

---

## ConfigMaps и Secrets

```bash
# Просмотр ConfigMap
kubectl get configmap broker-postgres-init -n trading -o yaml

# Просмотр Secret (base64)
kubectl get secret trading-secrets -n trading -o yaml

# Декодировать значение из Secret
kubectl get secret trading-secrets -n trading -o jsonpath='{.data.BROKER_DB_PASSWORD}' | base64 -d
```

---

## Полезные алиасы

Добавить в `~/.bashrc` или `~/.zshrc`:

```bash
# Короткие команды
alias k='kubectl'
alias kn='kubectl -n trading'

# Частые операции
alias kpods='kubectl get pods -n trading'
alias klogs='kubectl logs -f -n trading'
alias kexec='kubectl exec -it -n trading'

# Перезапуск сервисов
alias krestart-auth='kubectl rollout restart deployment/auth-service -n trading'
alias krestart-broker='kubectl rollout restart deployment/broker-service -n trading'
alias krestart-trading='kubectl rollout restart deployment/trading-service -n trading'
alias krestart-all='kubectl rollout restart deployment -n trading'

# Логи сервисов
alias klogs-auth='kubectl logs -f -l app=auth-service -n trading'
alias klogs-broker='kubectl logs -f -l app=broker-service -n trading'
alias klogs-trading='kubectl logs -f -l app=trading-service -n trading'

# Быстрый статус
alias kstatus='kubectl get pods,svc,ingress -n trading'
```

После добавления:
```bash
source ~/.bashrc  # или source ~/.zshrc
```

---

## Типичные сценарии

### Обновить образ и задеплоить

```bash
# 1. Собрать и запушить образ
docker build -t tobantal/broker-service:latest .
docker push tobantal/broker-service:latest

# 2. Перезапустить deployment (подтянет новый образ)
kubectl rollout restart deployment/broker-service -n trading

# 3. Следить за статусом
kubectl rollout status deployment/broker-service -n trading
```

### Проблема: под в CrashLoopBackOff

```bash
# 1. Посмотреть события
kubectl describe pod <pod-name> -n trading

# 2. Посмотреть логи (включая предыдущий запуск)
kubectl logs <pod-name> -n trading --previous

# 3. Проверить readiness/liveness probes
kubectl get pod <pod-name> -n trading -o yaml | grep -A 10 readinessProbe
```

### Проблема: сервис недоступен

```bash
# 1. Проверить что поды Running
kubectl get pods -l app=broker-service -n trading

# 2. Проверить endpoints
kubectl get endpoints broker-service -n trading

# 3. Проверить сервис
kubectl describe svc broker-service -n trading

# 4. Проверить ingress
kubectl describe ingress -n trading
```

---

## Ссылки

- [kubectl Cheat Sheet](https://kubernetes.io/docs/reference/kubectl/cheatsheet/)
- [Kubernetes Documentation](https://kubernetes.io/docs/home/)
