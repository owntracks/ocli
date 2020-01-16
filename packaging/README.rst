##############################################
Build, package and publish owntracks-cli-publisher
##############################################

.. highlight:: bash


************
Introduction
************
This outlines the Makefile targets of a convenient
packaging subsystem for owntracks-cli-publisher.

Currently, this is focused on Debian and CentOS packages
but might be extended in the future.

Three steps are required to successfully build packages.

1. Setup:
   Create appropriate baseline Docker images for the
   designated operating system.

2. Build:
   Build package using the respective baseline images.

3. Publish:
   Upload package file to GitHub release page.


*****
Usage
*****
First, go to the "packaging" directory::

    cd packaging


*****
Setup
*****
Prepare baseline images for ``amd64`` architecture::

    # Debian stretch amd64
    make build-debian-baseline arch=amd64 dist=stretch version=0.1.0

    # Debian buster amd64
    make build-debian-baseline arch=amd64 dist=buster version=0.1.0

    # CentOS 7 amd64
    make build-centos-baseline arch=amd64 dist=7 version=0.1.0

    # CentOS 8 amd64
    make build-centos-baseline arch=amd64 dist=8 version=0.1.0


Prepare baseline images for ``armhf`` architecture::

    # Debian stretch armhf
    make build-debian-baseline arch=armv7hf dist=stretch version=0.1.0

    # TODO: make build-centos-baseline arch=arm32v7 dist=7 version=0.1.0
    # TODO: make build-centos-baseline arch=arm64v8 dist=7 version=0.1.0


*****
Build
*****
Build ``owntracks-cli-publisher`` package for ``amd64`` architecture::

    # Debian stretch amd64
    make debian-package arch=amd64 dist=stretch pkgtype=deb version=0.7.0

    # Debian buster amd64
    make debian-package arch=amd64 dist=buster pkgtype=deb version=0.7.0

    # CentOS 7 amd64
    make centos-package arch=amd64 dist=centos7 pkgtype=rpm version=0.7.0

    # CentOS 8 amd64
    make centos-package arch=amd64 dist=centos8 pkgtype=rpm version=0.7.0


Build ``owntracks-cli-publisher`` package for ``armhf`` architecture::

    # Debian stretch armhf
    make debian-package arch=armhf dist=stretch pkgtype=deb version=0.7.0


*******
Publish
*******
Publish package to GitHub::

    # Prepare
    export GITHUB_TOKEN=642ff7c47b1697a79ab7f105e1d79f054d0bbeef

    # Debian buster amd64
    make publish-package arch=amd64 dist=buster version=0.7.0

    # Debian stretch armhf
    make publish-package arch=armhf dist=stretch version=0.7.0


*****
Notes
*****
These Docker images are used for creating the baseline images:

- https://hub.docker.com/r/amd64/debian (todo)
- https://hub.docker.com/r/arm32v7/debian (todo)
- https://hub.docker.com/r/arm64v8/debian (todo)

- https://hub.docker.com/r/amd64/centos
- https://hub.docker.com/r/arm32v7/centos
- https://hub.docker.com/r/arm64v8/centos
