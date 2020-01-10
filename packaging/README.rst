##############################################
Build, package and publish owntracks-publisher
##############################################

.. highlight:: bash


************
Introduction
************
This outlines the Makefile targets of a convenient
packaging subsystem for ocli / owntracks-publisher.

Currently, this is focused on Debian packages but
might be extended in the future.

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
Prepare baseline images for both amd64 and armhf architectures::

    # Debian buster amd64
    make build-debian-baseline arch=amd64 dist=buster version=0.1.0

    # Debian stretch armhf
    make build-debian-baseline arch=armv7hf dist=stretch version=0.1.0


*****
Build
*****
Build "owntracks-publisher" package::

    # Debian buster amd64
    make debian-package arch=amd64 dist=buster version=0.7.0

    # Debian stretch armhf
    make debian-package arch=armhf dist=stretch version=0.7.0


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
