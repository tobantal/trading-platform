# 1. Включить ingress addon
minikube addons enable ingress


# 2. Проверить что ingress controller запустился
kubectl get pods -n ingress-nginx


# 3. Добавить запись в /etc/hosts
echo "$(minikube ip) arch.homework" | sudo tee -a /etc/hosts


# 4. Проверить ingress
kubectl get ingress -n trading


=====


После этого сервисы будут доступны:
```bash
curl http://arch.homework/auth/health
```

