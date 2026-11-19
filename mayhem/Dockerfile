# syntax=docker/dockerfile:1.7
# Commit image for MEGAsync (savantenvs). Builds on the org base; see mayhem/build.sh.
FROM ghcr.io/savantenvs/base:20261023

ARG COMMIT
ARG REPO_URL
ARG SOURCE_DATE_EPOCH
ARG SANITIZER_FLAGS
ARG COVERAGE_FLAGS=
ARG MAYHEM_JOBS=

# Qt5Core (control/RegistryHiveReader.cpp), Crypto++ (src/MEGAUpdater/UpdateTask.cpp) and the desktop
# app's build deps (Qt5 GUI/QML, SDK libraries; the httpserver target links the whole app) from apt, pinned via SOURCE_DATE_EPOCH.
USER root
RUN [ -n "$SOURCE_DATE_EPOCH" ] && repro-sources-list.sh || true ; \
    apt-get update && apt-get install -yq --no-install-recommends qtbase5-dev pkg-config libcrypto++-dev \
      cmake ninja-build qttools5-dev qttools5-dev-tools libqt5x11extras5-dev libqt5svg5-dev qtdeclarative5-dev \
      qml-module-qtquick2 qml-module-qtquick-controls2 qml-module-qtquick-window2 \
      libsodium-dev libsqlite3-dev libcurl4-gnutls-dev libssl-dev libicu-dev libuv1-dev libmediainfo-dev \
      libzen-dev zlib1g-dev libc-ares-dev libfreeimage-dev libglib2.0-dev libsysprof-capture-4-dev \
 && rm -rf /var/lib/apt/lists/*

# Catch2 v2 single header (upstream's unit tests include <catch.hpp>; Debian ships only Catch2 v3),
# pinned by sha256, cached for the offline re-run (SPEC §6.2 item 19). Header-only: src == include.
ARG CATCH2_VER=2.13.10
ARG CATCH2_SHA256=3725c0f0a75f376a5005dde31ead0feb8f7da7507644c201b814443de8355170
RUN d=/opt/toolchains/deps/catch2-${CATCH2_VER} && mkdir -p "$d/src" "$d/include" \
 && curl -fsSL -o "$d/src/catch.hpp" \
      https://github.com/catchorg/Catch2/releases/download/v${CATCH2_VER}/catch.hpp \
 && echo "${CATCH2_SHA256}  $d/src/catch.hpp" | sha256sum -c - \
 && cp "$d/src/catch.hpp" "$d/include/catch.hpp" && chmod -R a+rX "$d"

# The MEGA SDK, upstream's git submodule src/MEGASync/mega, at the gitlink commit (the CI checkout
# does not fetch submodules). Built by build.sh as part of the app, linked into the httpserver target.
ARG MEGA_SDK_REV=81755609b860b0c839cd08e4d5c708d65a731d54
ARG MEGA_SDK_SHA256=97f3de89d9aa58f857b7c3b777dd8c6d6083eb53cb01c400bdbaefc02715e08a
RUN d=/opt/toolchains/deps/mega-sdk-${MEGA_SDK_REV} && mkdir -p "$d/src" \
 && curl -fsSL -o /tmp/sdk.tgz https://github.com/meganz/sdk/archive/${MEGA_SDK_REV}.tar.gz \
 && echo "${MEGA_SDK_SHA256}  /tmp/sdk.tgz" | sha256sum -c - \
 && tar -xzf /tmp/sdk.tgz --strip-components=1 -C "$d/src" && rm -f /tmp/sdk.tgz && chmod -R a+rX "$d"

USER mayhem
WORKDIR /mayhem
COPY --chown=2000:2000 . /mayhem

RUN if [ -x /opt/mayhem/ccache/prefill ]; then /opt/mayhem/ccache/prefill bash mayhem/build.sh; \
    else bash mayhem/build.sh; fi

RUN if [ -f mayhem/test.sh ]; then bash mayhem/test.sh; else echo "no mayhem/test.sh"; fi

LABEL org.opencontainers.image.source="${REPO_URL}" \
      org.opencontainers.image.revision="${COMMIT}" \
      rlenv.tier="commit" \
      rlenv.source_date_epoch="${SOURCE_DATE_EPOCH}" \
      rlenv.user="mayhem"

USER mayhem
WORKDIR /mayhem
