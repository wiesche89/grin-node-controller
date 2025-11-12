# -------- Stage 1: Builder --------
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
    qt6-base-dev qt6-base-dev-tools qt6-tools-dev qt6-tools-dev-tools qt6-websockets-dev \
    libqt6sql6 libqt6sql6-sqlite \
    libssl-dev libcurl4-openssl-dev libjsoncpp-dev \
    build-essential autoconf libtool git unzip ca-certificates wget xz-utils patchelf \
    && apt-get clean

# Dein Projekt bauen
WORKDIR /src
RUN git clone --branch main --single-branch https://github.com/wiesche89/grin-node-controller.git .
RUN qmake6 grin-node-controller.pro && make -j"$(nproc)"

# Laufzeit-Bundle zusammenstellen (Binary + benötigte Qt-Plugins + Shared Libs)
# Zielstruktur: /bundle/{bin,lib,plugins}
RUN set -eux; \
    mkdir -p /bundle/bin /bundle/lib /bundle/plugins; \
    # Binary kopieren (Passe Name an, falls anders gebaut)
    cp ./grin-node-controller /bundle/bin/; \
    # Standard-Qt6-Plugin-Pfade (Ubuntu 24.04)
    QT_PLUG=/usr/lib/x86_64-linux-gnu/qt6/plugins; \
    for d in platforms styles imageformats iconengines sqldrivers networkinformation tls; do \
        if [ -d "$QT_PLUG/$d" ]; then mkdir -p "/bundle/plugins/$d" && cp -a "$QT_PLUG/$d"/* "/bundle/plugins/$d/"; fi; \
    done; \
    # benötigte Shared Libs automatisch einsammeln
    ldd /bundle/bin/grin-node-controller | awk '/=> \// {print $(NF-1)}' | sort -u | xargs -I{} cp -v --parents {} /bundle/lib/ || true; \
    # häufige Zusatzlibs
    cp -v /usr/lib/x86_64-linux-gnu/libjsoncpp.so.* /bundle/lib/ || true; \
    cp -v /usr/lib/x86_64-linux-gnu/libcurl.so.*    /bundle/lib/ || true; \
    cp -v /usr/lib/x86_64-linux-gnu/libssl.so.*     /bundle/lib/ || true; \
    cp -v /usr/lib/x86_64-linux-gnu/libcrypto.so.*  /bundle/lib/ || true; \
    # qt.conf, damit Qt seine Plugins relativ findet
    mkdir -p /bundle/bin; \
    printf "[Paths]\nPlugins=../plugins\n" > /bundle/bin/qt.conf; \
    # RPATH so setzen, dass Binary die Libs im ./lib findet
    patchelf --set-rpath '$ORIGIN/../lib' /bundle/bin/grin-node-controller; \
    # Startscript
    printf '#!/usr/bin/env bash\nset -euo pipefail\nDIR=\"$(cd \"$(dirname \"$0\")\" && pwd)\"\nexport LD_LIBRARY_PATH=\"$DIR/../lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}\"\nexec \"$DIR/grin-node-controller\" \"$@\"\n' > /bundle/bin/start.sh; \
    chmod +x /bundle/bin/start.sh

# -------- Stage 2: Runtime --------
FROM ubuntu:24.04 AS runtime
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y ca-certificates tor && apt-get clean
WORKDIR /opt/app
COPY --from=builder /bundle ./

# Nicht-root User
RUN useradd -m appuser && chown -R appuser:appuser /opt/app
USER appuser

ENV LD_LIBRARY_PATH=/opt/app/lib
ENV QT_QPA_PLATFORM=offscreen
CMD ["/opt/app/bin/start.sh"]
