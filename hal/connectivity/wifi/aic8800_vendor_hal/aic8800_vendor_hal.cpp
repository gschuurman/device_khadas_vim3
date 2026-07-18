/*
 * Minimal legacy vendor HAL for the AIC8800D80 dongle (wlan1), loaded as a
 * second /vendor/etc/wifi/vendor_hals/ *.xml entry alongside the primary
 * BCM4359 HAL (libwifi-hal-bcm). aic8800 has no vendor-proprietary
 * extensions -- it's a plain cfg80211 driver -- so almost everything here
 * stays the default WIFI_ERROR_NOT_SUPPORTED stub that
 * initHalFuncTableWithStubs() already pre-populates before this runs.
 *
 * Only wifi_initialize/wifi_get_ifaces/wifi_get_iface_name are overridden:
 * WifiLegacyHal::start() (hardware/interfaces/wifi/aidl/default) requires
 * wifi_get_ifaces to report at least one interface or start() fails for
 * this chip -- by design, that failure is caught and logged per-chip in
 * WifiChip::handleChipConfiguration(), it does not affect the primary
 * chip's independent WifiLegacyHal instance. So when the dongle isn't
 * plugged in (wlan1 doesn't exist), this chip just fails to start
 * cleanly instead of crashing anything.
 */

#include <net/if.h>
#include <string.h>

#include <hardware_legacy/wifi_hal.h>

namespace {

constexpr char kIfaceName[] = "wlan1";

/* wifi_handle/wifi_interface_handle are opaque pointers to incomplete
 * struct types -- never dereferenced by our own code, only compared and
 * passed back to us, so a dummy sentinel address is safe. */
int g_dummy_handle;

wifi_error Aic8800Initialize(wifi_handle* handle) {
    *handle = reinterpret_cast<wifi_handle>(&g_dummy_handle);
    return WIFI_SUCCESS;
}

wifi_error Aic8800GetIfaces(wifi_handle /*handle*/, int* num_ifaces,
                             wifi_interface_handle** ifaces) {
    static wifi_interface_handle iface_handles[1];
    if (!if_nametoindex(kIfaceName)) {
        *num_ifaces = 0;
        *ifaces = nullptr;
        return WIFI_SUCCESS;
    }
    iface_handles[0] = reinterpret_cast<wifi_interface_handle>(&g_dummy_handle);
    *num_ifaces = 1;
    *ifaces = iface_handles;
    return WIFI_SUCCESS;
}

wifi_error Aic8800GetIfaceName(wifi_interface_handle /*iface*/, char* name, size_t size) {
    strlcpy(name, kIfaceName, size);
    return WIFI_SUCCESS;
}

}  // namespace

extern "C" wifi_error init_wifi_vendor_hal_func_table(wifi_hal_fn* fn) {
    fn->wifi_initialize = Aic8800Initialize;
    fn->wifi_get_ifaces = Aic8800GetIfaces;
    fn->wifi_get_iface_name = Aic8800GetIfaceName;
    return WIFI_SUCCESS;
}
