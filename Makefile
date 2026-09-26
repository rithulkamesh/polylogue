# Polylogue: shortcuts over the CMake presets. `make` lists the targets.
#
# Everything here is a thin wrapper; CMakePresets.json is the source of truth for how each
# configuration is built.

CLANG_FORMAT ?= clang-format
DIST_BUILD   ?= build/release
ARTEFACTS     = $(DIST_BUILD)/src/plugin/Polylogue_artefacts/Release
ZIP           = Polylogue-macOS.zip

AU_DIR   = $(HOME)/Library/Audio/Plug-Ins/Components
VST3_DIR = $(HOME)/Library/Audio/Plug-Ins/VST3
APP_DIR  = $(HOME)/Applications

.DEFAULT_GOAL := help
.PHONY: help build test debug run bench presets sanitize tsan ci format format-check screenshots \
        universal dist install uninstall clean distclean

help: ## Show this list
	@awk 'BEGIN {FS = ":.*## "} /^[a-zA-Z_-]+:.*## / {printf "  \033[1m%-14s\033[0m %s\n", $$1, $$2}' $(MAKEFILE_LIST)

# ---- Build and test -------------------------------------------------------------------------

build: ## Build VST3, AU, Standalone and the tools (Release)
	cmake --preset release
	cmake --build --preset release

test: build ## Build, then run every test (Release)
	ctest --test-dir build/release --output-on-failure

debug: ## Build everything in Debug and run the tests
	cmake --preset dev
	cmake --build --preset dev
	ctest --preset dev

sanitize: ## Tests under AddressSanitizer and UBSan
	cmake --preset sanitize
	cmake --build --preset sanitize
	ctest --preset sanitize

tsan: ## Host tests under ThreadSanitizer (proves the audio thread never races)
	cmake --preset tsan
	cmake --build --preset tsan --target polylogue_host_tests
	./build/tsan/tests/polylogue_host_tests

ci: format-check sanitize tsan test ## Everything the CI workflow runs

# ---- Using it -------------------------------------------------------------------------------

run: build ## Build and open the standalone app
	open "$(ARTEFACTS)/Standalone/Polylogue.app"

bench: build ## CPU use per scenario
	./build/release/tools/benchmark/polylogue-benchmark

presets: build ## List every factory preset with its peak, loudness and tail length
	./build/release/tools/render/polylogue-render --list --notes 48 --hold 2 --seconds 12

screenshots: build ## Regenerate the README screenshots in docs/
	./build/release/tools/screenshot/polylogue-screenshot --preset "Warm Pad" --touch CUTOFF --scale 1.5 --out docs/panel.png
	./build/release/tools/screenshot/polylogue-screenshot --preset "Glass Bell" --spectrum --touch INT --note 60 --scale 1.5 --out docs/spectrum.png
	./build/release/tools/screenshot/polylogue-screenshot --preset "Growl" --map --scale 1.5 --out docs/midi-learn.png

# ---- Code quality ---------------------------------------------------------------------------

format: ## Format all C++ sources in place (needs clang-format)
	CLANG_FORMAT=$(CLANG_FORMAT) scripts/format.sh

format-check: ## Fail if any source is not formatted
	CLANG_FORMAT=$(CLANG_FORMAT) scripts/format.sh --check

# ---- Packaging ------------------------------------------------------------------------------

universal: ## Build a universal (Apple Silicon + Intel) Release into build/universal
	cmake -S . -B build/universal -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64"
	cmake --build build/universal

dist: build ## Zip the AU, VST3 and Standalone (DIST_BUILD=build/universal for a universal zip)
	rm -f $(ZIP)
	cd "$(ARTEFACTS)" && zip -qry "$(CURDIR)/$(ZIP)" AU/Polylogue.component VST3/Polylogue.vst3 Standalone/Polylogue.app
	@echo "wrote $(ZIP)"

install: build ## Install the AU, VST3 and app for this user (ad-hoc signed, no sudo)
	mkdir -p "$(AU_DIR)" "$(VST3_DIR)" "$(APP_DIR)"
	rm -rf "$(AU_DIR)/Polylogue.component" "$(VST3_DIR)/Polylogue.vst3" "$(APP_DIR)/Polylogue.app"
	cp -R "$(ARTEFACTS)/AU/Polylogue.component" "$(AU_DIR)/"
	cp -R "$(ARTEFACTS)/VST3/Polylogue.vst3" "$(VST3_DIR)/"
	cp -R "$(ARTEFACTS)/Standalone/Polylogue.app" "$(APP_DIR)/"
	xattr -cr "$(AU_DIR)/Polylogue.component" "$(VST3_DIR)/Polylogue.vst3" "$(APP_DIR)/Polylogue.app"
	codesign --force --deep -s - "$(AU_DIR)/Polylogue.component" "$(VST3_DIR)/Polylogue.vst3" "$(APP_DIR)/Polylogue.app"
	@echo "installed; rescan plugins in your DAW"

uninstall: ## Remove the installed plugins and app (leaves your presets and MIDI map alone)
	rm -rf "$(AU_DIR)/Polylogue.component" "$(VST3_DIR)/Polylogue.vst3" "$(APP_DIR)/Polylogue.app"

# ---- Housekeeping ---------------------------------------------------------------------------

clean: ## Remove the Release build
	rm -rf build/release

distclean: ## Remove every build directory and the release zip
	rm -rf build $(ZIP)
