"""Inject Wi-Fi credentials from environment variables into the PlatformIO build.

Usage:
    set JINGGUA_WIFI_SSID=YourNetworkName
    set JINGGUA_WIFI_PASSWORD=YourPassword
    pio run

SSID and password are passed as C string literals.  When the environment
variables are unset or empty the build proceeds without credentials (the
firmware will show "not configured" when the user tries to connect).
"""

import os

Import("env")  # type: ignore  # PlatformIO built-in


def _inject_credentials() -> None:
    ssid = os.environ.get("JINGGUA_WIFI_SSID", "")
    password = os.environ.get("JINGGUA_WIFI_PASSWORD", "")
    if not ssid:
        return
    # Pass the values as C string literals so the compiler sees
    #   -DJINGGUA_WIFI_SSID="MyNetwork"
    # which defines the macro as the string literal "MyNetwork".
    env.Append(
        BUILD_FLAGS=[
            '-DJINGGUA_WIFI_SSID="{}"'.format(ssid),
            '-DJINGGUA_WIFI_PASSWORD="{}"'.format(password),
        ]
    )
    print("[WiFi] credentials injected from environment: SSID={}".format(ssid))


_inject_credentials()
