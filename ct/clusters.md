# Kubernetes Clusters for Component Testing

This document describes how to set up local Kubernetes clusters for running h2agent component tests.

## kind (Kubernetes IN Docker)

Lightweight, fast, ideal for CI/CD pipelines.

### Installation

```bash
# Linux
curl -Lo ./kind https://kind.sigs.k8s.io/dl/latest/kind-linux-amd64
chmod +x ./kind && sudo mv ./kind /usr/local/bin/

# macOS
brew install kind
```

### Create cluster

```bash
kind create cluster --name h2agent-ct
```

### Load local images

kind doesn't share Docker daemon, so images must be loaded explicitly:

```bash
kind load docker-image ghcr.io/testillano/h2agent:latest --name h2agent-ct
kind load docker-image ghcr.io/testillano/ct-h2agent:latest --name h2agent-ct
kind load docker-image busybox:latest --name h2agent-ct
```

### Run tests

```bash
ct/test.sh
```

### Delete cluster

```bash
kind delete cluster --name h2agent-ct
```

## minikube

Full-featured local Kubernetes, closer to production environments.

### Installation

```bash
# Linux
curl -Lo minikube https://storage.googleapis.com/minikube/releases/latest/minikube-linux-amd64
chmod +x minikube && sudo mv minikube /usr/local/bin/

# macOS
brew install minikube
```

### Create cluster

```bash
minikube start --driver=docker
```

### Use minikube's Docker daemon

Images built locally are available to pods without explicit loading:

```bash
eval $(minikube docker-env)
docker build -t ghcr.io/testillano/h2agent:latest .
```

### Run tests

```bash
ct/test.sh
```

### Stop/Delete cluster

```bash
minikube stop
minikube delete
```

## CI/CD Integration

### GitHub Actions with kind

```yaml
jobs:
  component-test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Setup kind
        uses: helm/kind-action@v1
        with:
          cluster_name: h2agent-ct

      - name: Build and load images
        run: |
          docker build -t ghcr.io/testillano/h2agent:latest .
          docker build -t ghcr.io/testillano/ct-h2agent:latest -f ct/Dockerfile ct/
          kind load docker-image ghcr.io/testillano/h2agent:latest --name h2agent-ct
          kind load docker-image ghcr.io/testillano/ct-h2agent:latest --name h2agent-ct

      - name: Run component tests
        run: ct/test.sh
```
