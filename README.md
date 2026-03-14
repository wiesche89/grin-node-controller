# Grin Node Controller

This repository builds and packages four Docker images:

- `wiesche89/grin-node-controller-builder`
- `wiesche89/grin-builder`
- `wiesche89/grinpp-builder`
- `wiesche89/grin-node-runtime`

The builder images contain the compiled binaries under `/out/...`.
The runtime image combines a tested set of these binaries into the final container used by Umbrel.

## Image layout

### Builder images

- `Dockerfile.controller`
  Builds the Qt controller binary and exports `/out/grin-node-controller`.
- `Dockerfile.grin`
  Builds the Rust Grin node and exports `/out/grin`.
- `Dockerfile.grinpp`
  Builds the Grin++ node and exports `/out/grinpp`.

### Runtime image

- `Dockerfile.runtime`
  Copies the artifacts from fixed builder image tags into the final runtime image.

## Versioning

Builder images and runtime images are versioned independently.

Example:

- `wiesche89/grin-node-controller-builder:v1.0.0`
- `wiesche89/grin-builder:v1.0.0`
- `wiesche89/grinpp-builder:v1.0.0`
- `wiesche89/grin-node-runtime:v1.0.0`

Important:

- A runtime tag represents a deliberately tested combination.
- Mainnet and testnet use the same runtime image.
- Network-specific behavior is configured in Umbrel via environment variables and volumes, not via separate Dockerfiles.

## Build prerequisites

- Docker with `buildx`
- Docker Hub login via `docker login`
- A buildx builder that supports `linux/amd64` and `linux/arm64`

Example:

```bat
docker buildx create --name multiarch --use
docker buildx inspect --bootstrap
```

## Build builder images

Start with version `v1.0.0`.

### 1. Controller builder

```bat
docker buildx build --platform linux/amd64,linux/arm64 ^
  -t wiesche89/grin-node-controller-builder:v1.0.0 ^
  -f Dockerfile.controller ^
  . ^
  --push
```

### 2. Rust Grin builder

```bat
docker buildx build --platform linux/amd64,linux/arm64 ^
  -t wiesche89/grin-builder:v1.0.0 ^
  -f Dockerfile.grin ^
  . ^
  --push
```

### 3. Grin++ builder

```bat
docker buildx build --platform linux/amd64,linux/arm64 ^
  -t wiesche89/grinpp-builder:v1.0.0 ^
  -f Dockerfile.grinpp ^
  . ^
  --push
```

## Build runtime image

Build the runtime image only after the referenced builder images have been pushed.

```bat
docker buildx build --platform linux/amd64,linux/arm64 ^
  -t wiesche89/grin-node-runtime:v1.0.0 ^
  -f Dockerfile.runtime ^
  --build-arg CONTROLLER_IMAGE=wiesche89/grin-node-controller-builder:v1.0.0 ^
  --build-arg GRIN_IMAGE=wiesche89/grin-builder:v1.0.0 ^
  --build-arg GRINPP_IMAGE=wiesche89/grinpp-builder:v1.0.0 ^
  . ^
  --push
```

## Release flow

Recommended release order:

1. Build and push `grin-node-controller-builder`
2. Build and push `grin-builder`
3. Build and push `grinpp-builder`
4. Build and push `grin-node-runtime`
5. Test the runtime image
6. Update the Umbrel app store repo to the new runtime tag

## Local runtime test

`docker-compose.yaml` is now oriented around the runtime image:

```bat
docker compose up -d
```

If you want to build the runtime image locally instead of pulling it, uncomment the `build:` section in `docker-compose.yaml` and set the builder image tags there.

## Umbrel usage

This repo builds one shared runtime image for both Umbrel apps:

- `grin-node`
- `grin-node-testnet`

The difference between mainnet and testnet belongs in the Umbrel app configuration:

- ports
- volumes
- `GRIN_RUST_ARGS`
- `GRIN_RUST_DATADIR`
- `GRINPP_ARGS`
- `GRINPP_DATADIR`

That means:

- no separate `Dockerfile-testnet`
- no separate testnet runtime image
- one tested runtime image, two different Umbrel app configurations

## Notes

- The legacy monolithic `Dockerfile` is no longer the preferred build path.
- `Dockerfile.grinpp` includes the current `libsodium` hash workaround required by the upstream Grin++ vcpkg overlay port.
