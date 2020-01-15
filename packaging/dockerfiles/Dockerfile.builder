ARG BASE_IMAGE


# ===============
# Acquire sources
# ===============
FROM ${BASE_IMAGE} AS acquire-sources
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

ARG DISTRIBUTION
ARG PKGTYPE
ARG VERSION
ARG NAME
ARG SOURCES=/sources

WORKDIR $SOURCES
RUN ./packaging/builder/fpm-package "${NAME}" "${DISTRIBUTION}" "${PKGTYPE}" "${VERSION}"
