# k8s/deploy.sh
#!/bin/bash
kubectl kustomize . --load-restrictor=LoadRestrictionsNone | kubectl apply -f -
