#!/bin/bash


# ==========================================
#               configuration
# ==========================================
DIST_PATH := ./dist
DIST_PACKAGE := owntracks-publisher


# ==========================================
#               image building
# ==========================================

# Synopsis:
#
# 	make build-debian-baseline arch=amd64 dist=buster version=0.1.0
# 	make build-debian-baseline arch=armv7hf dist=stretch version=0.1.0
#
# 	make build-centos-baseline arch=amd64 dist=7 version=0.1.0
# 	make build-centos-baseline arch=arm32v7 dist=7 version=0.1.0
#

build-debian-baseline: check-image-options
	docker build --tag owntracks/baseline-$(dist)-$(arch):$(version) --build-arg BASE_IMAGE=balenalib/$(arch)-debian:$(dist)-build - < dockerfiles/Dockerfile.debian.baseline
	docker tag owntracks/baseline-$(dist)-$(arch):$(version) owntracks/baseline-$(dist)-$(arch):latest

build-centos-baseline: check-image-options
	docker build --tag owntracks/baseline-centos$(dist)-$(arch):$(version) --build-arg BASE_IMAGE=$(arch)/centos:$(dist) - < dockerfiles/Dockerfile.centos.baseline
	docker tag owntracks/baseline-centos$(dist)-$(arch):$(version) owntracks/baseline-centos$(dist)-$(arch):latest

check-image-options:
	@if test "$(arch)" = ""; then \
		echo "ERROR: 'arch' not set"; \
		exit 1; \
	fi
	@if test "$(dist)" = ""; then \
		echo "ERROR: 'dist' not set"; \
		exit 1; \
	fi
	@if test "$(version)" = ""; then \
		echo "ERROR: 'version' not set"; \
		exit 1; \
	fi


# ==========================================
#                packaging
# ==========================================
#
# Build Debian package.
#
# Synopsis::
#
#    # Debian Buster amd64
#    make debian-package arch=amd64 dist=buster pkgtype=deb version=0.7.0
#
#    # Debian Stretch armhf
#    make debian-package arch=armhf dist=stretch pkgtype=deb version=0.7.0
#
#	 # CentOS 7
#    make debian-package arch=amd64 dist=centos7 pkgtype=rpm version=0.7.0
#

# https://stackoverflow.com/questions/5947742/how-to-change-the-output-color-of-echo-in-linux
# https://en.wikipedia.org/wiki/ANSI_escape_code
YELLOW := \033[0;33m\033[1m
NC := \033[0m

debian-package:
	$(eval name := $(DIST_PACKAGE))
	$(eval package_name := $(name)_$(version)-1~$(dist)_$(arch).deb)
	$(MAKE) pkg-build name=$(name) package_name=$(package_name)

centos-package:
ifeq ($(arch),armhf)
	$(eval name_arch := arm32v7)
else
	$(eval name_arch := x86_64)
endif
	$(eval name := $(DIST_PACKAGE))
	$(eval package_name := $(name)-$(version)-1~$(dist).$(name_arch).rpm)
	$(MAKE) pkg-build name=$(name) package_name=$(package_name)

pkg-build: check-build-options

ifeq ($(arch),armhf)
	$(eval build_arch := armv7hf)
else
	$(eval build_arch := $(arch))
endif

	@# Build Linux distribution package
	@echo "Building $(YELLOW)$(pkgtype)$(NC) package $(YELLOW)$(name)$(NC) version $(YELLOW)$(version)$(NC) for architecture $(YELLOW)$(arch)$(NC) and distribution $(YELLOW)$(dist)$(NC)."

	$(eval build_image := owntracks/baseline-$(dist)-$(build_arch):latest)
	$(eval build_tag := owntracks/$(DIST_PACKAGE)-build-$(dist)-$(arch):$(version))

	@# Invoke the builder.
	docker build --tag $(build_tag) --build-arg BASE_IMAGE=${build_image} --build-arg DISTRIBUTION=$(dist) --build-arg PKGTYPE=$(pkgtype) --build-arg VERSION=$(version) --build-arg NAME=$(name) --file dockerfiles/Dockerfile.builder ..


	@echo "Harvesting package $(YELLOW)$(package_name)$(NC) from Docker container to directory $(YELLOW)$(DIST_PATH)$(NC)."

	@# Spin up container.
	@docker container rm -f finalize; true
	docker container create --name finalize $(build_tag)

	@# Copy .deb file from Docker container to working tree.
	mkdir -p $(DIST_PATH)
	docker container cp finalize:/sources/dist/$(package_name) $(DIST_PATH)

	@# Destroy container.
	docker container rm -f finalize

	@echo "Enjoy your package at $(YELLOW)$(DIST_PATH)/$(package_name)$(NC)."


check-build-options:
	@if test "$(arch)" = ""; then \
		echo "ERROR: 'arch' not set"; \
		exit 1; \
	fi
	@if test "$(dist)" = ""; then \
		echo "ERROR: 'dist' not set"; \
		exit 1; \
	fi
	@if test "$(pkgtype)" = ""; then \
		echo "ERROR: 'pkgtype' not set"; \
		exit 1; \
	fi
	@if test "$(name)" = ""; then \
		echo "ERROR: 'name' not set"; \
		exit 1; \
	fi
	@if test "$(version)" = ""; then \
		echo "ERROR: 'version' not set"; \
		exit 1; \
	fi


# ==========================================
#                publishing
# ==========================================
#
# Publish Debian package.
#
# Synopsis::
#
#    # Debian amd64
#    make publish-package arch=amd64 dist=buster version=0.7.0
#
#    # Debian armhf
#    make debian-package arch=armhf dist=stretch version=0.7.0
#

$(eval github-release := ./bin/github-release)

publish-package: check-github-release check-publish-options

	$(eval package_file := $(DIST_PATH)/$(DIST_PACKAGE)_$(version)-1~$(dist)_$(arch).deb)

	@echo "Uploading release artefact $(package_file) to GitHub"

	@# Show current releases.
	@#$(github-release) info --user owntracks --repo ocli

    # Create Release.
	$(github-release) release --user owntracks --repo ocli --tag $(version) || true
	@# --draft

    # Upload artifact.
	$(github-release) upload --user owntracks --repo ocli --tag $(version) --name $(notdir $(package_file)) --file $(package_file) --replace

check-publish-options:
	@if test "$(arch)" = ""; then \
		echo "ERROR: 'arch' not set"; \
		exit 1; \
	fi
	@if test "$(dist)" = ""; then \
		echo "ERROR: 'dist' not set"; \
		exit 1; \
	fi
	@if test "$(version)" = ""; then \
		echo "ERROR: 'version' not set"; \
		exit 1; \
	fi


# ----------------------
# github-release program
# ----------------------

check-github-release:
	@test -e $(github-release) || (echo 'ERROR: "github-release" not found.\nPlease install "github-release" to "./bin/github-release".\nSee https://github.com/aktau/github-release\n'; exit 1)

install-github-release:
	# https://github.com/aktau/github-release
	$(eval url := https://github.com/aktau/github-release/releases/download/v0.7.2/darwin-amd64-github-release.tar.bz2)

	# NotYetImplemented
	@exit 1

	@#@test -e $(github-release) || cd tmp; wget $(url)
	@#$(eval github-release := $(tools_dir)tmp/bin/darwin/amd64/github-release)
