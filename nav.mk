# Google apps selection: navigation app (Google Maps / Organic Maps) and assistant (Google car Assistant or
# none). Toggles and the resulting GAS SKU: nav_sku.mk.
#
# Only one APP_MAPS handler ships: with both Google Maps and Organic Maps (MapCardActivity), the CarLauncher map
# card falls into the chooser, which can't launch inside the TaskView (red card, handoff D4).
include device/khadas/vim3/nav_sku.mk

ifeq ($(VIM3_NAV_APP),google)
# Google Maps (car build, priv-app, vendor/google/gapps_auto) as the navigation app.
PRODUCT_PACKAGES += \
    com_google_android_apps_maps \
    privapp_permissions_com_google_android_apps_maps
else
# Organic Maps (vendor/gschuurman/vehicle_interfaces/automotive/organicmaps) with its map card. Map rendering on
# panvk is broken (handoff A7): set VulkanForbidden=true in OM's settings.ini on the device until the OM fork
# bans Mali-G52.
PRODUCT_PACKAGES += \
    OrganicMaps
endif

ifeq ($(VIM3_GOOGLE_ASSISTANT),true)
# Google car Assistant (GoogleSearchCarPrebuilt) holds the assistant role (GmsCarAssistantOverlay).
PRODUCT_PACKAGES += \
    com_google_android_carassistant \
    com_google_android_carassistant_googlecarui_rro \
    privapp_permissions_com_google_android_carassistant \
    GmsCarAssistantOverlay
endif

# GAS overlays for the SKU. They blank the framework/launcher configs of the Google apps the bundle leaves out;
# gas_full blanks nothing. The CarLauncher variants blank the map-card intent, leaving the card to the
# remaining APP_MAPS handler (Organic Maps).
ifeq ($(VIM3_GAS_SKU),gas_maps_playstore)
PRODUCT_PACKAGES += gas_maps_playstore_overlay
else ifeq ($(VIM3_GAS_SKU),gas_assistant_playstore)
PRODUCT_PACKAGES += gas_assistant_playstore_overlay car_launcher_gas_assistant_playstore_overlay
else ifeq ($(VIM3_GAS_SKU),gas_playstore)
PRODUCT_PACKAGES += gas_playstore_overlay car_launcher_gas_playstore_overlay
endif
