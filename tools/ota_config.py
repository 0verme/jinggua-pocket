"""Inject non-secret OTA trust material and an optional firmware version.

Usage (macOS/Linux):
    JINGGUA_OTA_ROOT_CA="-----BEGIN CERTIFICATE-----..." \
      JINGGUA_FIRMWARE_VERSION="1.0.0" pio run -e m5stack-sticks3

The OTA CA is public trust material, not a credential. Without it, OTA fails
closed while offline divination and normal firmware builds remain available.
"""

import os

Import("env")  # type: ignore  # PlatformIO built-in


def _c_string(value: str) -> str:
    return (
        value.replace("\\", "\\\\")
        .replace('"', '\\"')
        .replace("\r", "")
        .replace("\n", "\\n")
    )


flags = []
root_ca = os.environ.get("JINGGUA_OTA_ROOT_CA", "")
version = os.environ.get("JINGGUA_FIRMWARE_VERSION", "")
if root_ca:
    flags.append('-DJINGGUA_OTA_ROOT_CA="{}"'.format(_c_string(root_ca)))
if version:
    flags.append('-DJINGGUA_FIRMWARE_VERSION="{}"'.format(_c_string(version)))
if flags:
    env.Append(BUILD_FLAGS=flags)
