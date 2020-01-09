ARG BASE_IMAGE


# ===========================
# Customize build environment
# ===========================
FROM ${BASE_IMAGE} AS build-environment

RUN apt-get install --yes --no-install-recommends libgps-dev libmosquitto-dev


# ===============
# Acquire sources
# ===============
FROM build-environment AS acquire-sources

ARG SOURCES=/sources

COPY . $SOURCES


# =============
# Build program
# =============

FROM acquire-sources AS build-program

ENV TMPDIR=/var/tmp

ARG VERSION
ARG SOURCES=/sources

WORKDIR $SOURCES
RUN make all


# ===========================
# Create distribution package
# ===========================
FROM build-program AS package-program

ENV TMPDIR=/var/tmp

ARG NAME
ARG VERSION
ARG SOURCES=/sources

WORKDIR $SOURCES
RUN ./packaging/builder/fpm-package "${NAME}" "${VERSION}"
