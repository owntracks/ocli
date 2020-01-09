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

build-debian-baseline: check-image-options
	docker build --tag owntracks/baseline-$(dist)-$(arch):$(version) --build-arg BASE_IMAGE=balenalib/$(arch)-debian:$(dist)-build - < dockerfiles/Dockerfile.debian.baseline
	docker tag owntracks/baseline-$(dist)-$(arch):$(version) owntracks/baseline-$(dist)-$(arch):latest

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
#    make debian-package arch=amd64 dist=buster version=0.7.0
#
#    # Debian Stretch armhf
#    make debian-package arch=armhf dist=stretch version=0.7.0
#

debian-package:
	$(MAKE) deb-build name=$(DIST_PACKAGE)

deb-build: check-build-options

ifeq ($(arch),armhf)
	$(eval build_arch := armv7hf)
else
	$(eval build_arch := $(arch))
endif

	@# https://stackoverflow.com/questions/5947742/how-to-change-the-output-color-of-echo-in-linux
	@# https://en.wikipedia.org/wiki/ANSI_escape_code
	$(eval RED := "\033[0;31m")
	$(eval YELLOW := \033[0;33m\033[1m)
	$(eval NC := \033[0m)

	@# Build Linux distribution package
	@echo "Building package $(YELLOW)$(name)$(NC) version $(YELLOW)$(version)$(NC) for architecture $(YELLOW)$(arch)$(NC)."

	$(eval build_image := owntracks/baseline-$(dist)-$(build_arch):latest)
	$(eval build_tag := owntracks/$(DIST_PACKAGE)-build-$(dist)-$(arch):$(version))
	$(eval package_name := $(name)_$(version)-1_$(arch).deb)

	@# Invoke the builder.
	docker build --tag $(build_tag) --build-arg BASE_IMAGE=${build_image} --build-arg VERSION=$(version) --build-arg NAME=$(name) --file dockerfiles/Dockerfile.builder ..


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
#    make publish-package arch=amd64 version=0.7.0
#
#    # Debian armhf
#    make debian-package arch=armhf version=0.7.0
#

$(eval github-release := ./bin/github-release)

publish-package: check-github-release

	$(eval package_file := $(DIST_PATH)/$(DIST_PACKAGE)_$(version)-1_$(arch).deb)

	@echo "Uploading release artefact $(package_file) to GitHub"

	@# Show current releases.
	@#$(github-release) info --user hiveeyes --repo hiveeyes-micropython-firmware

    # Create Release.
	@#$(github-release) release --user hiveeyes --repo hiveeyes-micropython-firmware --tag $(version) --draft

	$(github-release) release --user owntracks --repo ocli --tag $(version) || true

    # Upload artifact.
	$(github-release) upload --user owntracks --repo ocli --tag $(version) --name $(notdir $(package_file)) --file $(package_file) --replace


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