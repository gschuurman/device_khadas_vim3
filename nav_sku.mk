# Google apps selection, shared by nav.mk (packages) and BoardConfig.mk (GAS SKU on the kernel cmdline).
#   VIM3_NAV_APP          := google (default) | organic   navigation app: Google Maps or Organic Maps
#   VIM3_GOOGLE_ASSISTANT := true (default) | false       Google car Assistant; false = Dicio holds the assistant role
# Override per build: `VIM3_NAV_APP=organic VIM3_GOOGLE_ASSISTANT=false brunch vim3_nvme`.
#
# VIM3_GAS_SKU = androidboot.hardware.sku: SystemConfig loads sysconfig/sku_<sku>/ (google.software.gas.* features,
# per-SKU install exclusions) and the matching gas_*_overlay activates on it. google + Assistant = gas_full, the
# feature set of Google's own car image (no sku dir, no blanking overlay); the partial bundles add
# google.software.gas.components.
VIM3_NAV_APP ?= google
VIM3_GOOGLE_ASSISTANT ?= true

ifeq ($(filter google organic,$(VIM3_NAV_APP)),)
$(error VIM3_NAV_APP must be google or organic, got '$(VIM3_NAV_APP)')
endif
ifeq ($(filter true false,$(VIM3_GOOGLE_ASSISTANT)),)
$(error VIM3_GOOGLE_ASSISTANT must be true or false, got '$(VIM3_GOOGLE_ASSISTANT)')
endif

ifeq ($(VIM3_NAV_APP)-$(VIM3_GOOGLE_ASSISTANT),google-true)
VIM3_GAS_SKU := gas_full
else ifeq ($(VIM3_NAV_APP)-$(VIM3_GOOGLE_ASSISTANT),google-false)
VIM3_GAS_SKU := gas_maps_playstore
else ifeq ($(VIM3_NAV_APP)-$(VIM3_GOOGLE_ASSISTANT),organic-true)
VIM3_GAS_SKU := gas_assistant_playstore
else
VIM3_GAS_SKU := gas_playstore
endif
