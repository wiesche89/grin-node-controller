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
- Mainnet and testnet can use different runtime versions.
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

The tags below are examples. Use the source URLs, commits and versions from
`releases.json`. Build the controller from its listed commit.

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
  --build-arg GRIN_REPO=%GRIN_REPO% ^
  --build-arg GRIN_REF=%GRIN_REF% ^
  . ^
  --push
```

### 3. Grin++ builder

```bat
docker buildx build --platform linux/amd64,linux/arm64 ^
  -t wiesche89/grinpp-builder:v1.0.0 ^
  -f Dockerfile.grinpp ^
  --build-arg GRINPP_REPO=%GRINPP_REPO% ^
  --build-arg GRINPP_REF=%GRINPP_REF% ^
  --build-arg GRINPP_VERSION=%GRINPP_VERSION% ^
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

This repo builds the runtime images for both Umbrel apps:

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
- separate runtime versions for release and staging
- network settings in the Umbrel app configurations

## Releases

`releases.json` defines runtime versions and source repositories, versions and commits.
Push to `main` to build new releases. Published versions are skipped.
Builder images are reused by source, architecture and Dockerfile content.
Identical components are built once for both networks. Change the component's
Dockerfile to refresh its build dependencies. Existing run-specific tags are
not reused; the first release with this workflow builds the components once.
Manual runs also use `main`.

For controller changes, commit the code and update its hash in `releases.json`.
Push both commits together. CI checks amd64 and arm64 before publishing.

## Notes

- The legacy monolithic `Dockerfile` is no longer the preferred build path.
- `Dockerfile.grinpp` includes the current `libsodium` hash workaround required by the upstream Grin++ vcpkg overlay port.

## Authentication

Set `CONTROLLER_PASSWORD` and start with `--require-auth`. Management requests
use Basic auth with username `grin`. On Umbrel, nginx supplies these credentials
after checking the app-proxy IP; no additional user login is needed.

POST `/wallet/v2/foreign` uses the caller's node credentials instead.
Other routes remain protected; OPTIONS preflight is unauthenticated.

```sh
python3 tests/auth.py /path/to/grin-node-controller
python3 tests/auth.py --image <image>
python3 tests/wallet_proxy.py /path/to/grin-node-controller
python3 tests/wallet_proxy.py /path/to/grin-node-controller --auth
```

For local review, build `Dockerfile.controller` as
`grin-node-controller-builder:auth-review`. Copy its `/out/grin-node-controller`
into the pinned runtime bases as `/opt/app/grin-node-controller`; tag them
`grin-node-runtime:auth-review-mainnet` and `grin-node-runtime:auth-review-testnet`.
Build the UI as `grin-node-ui:auth-review`.

Before publishing, update the source commit and versions in `releases.json`,
then pin the new image digests in both Umbrel stores.

Source: Apache-2.0. Dependencies retain their own licenses.
