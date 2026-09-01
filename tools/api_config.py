"""Inject non-secret JingGua API configuration into a PlatformIO build.

Usage (PowerShell):
    $env:JINGGUA_API_URL="https://your-jinggua-host.example/api/divinations"
    $env:JINGGUA_API_ROOT_CA="-----BEGIN CERTIFICATE-----..."
    python -m platformio run -e m5stack-sticks3

The CA certificate is public trust material, not a credential. The firmware
fails closed when either value is absent; it never falls back to insecure TLS.
Neither value is printed by this script.
"""

import os

Import("env")  # type: ignore  # PlatformIO built-in


def _c_string(value: str) -> str:
    """Escape an environment value for a C string build flag."""
    return (
        value.replace("\\", "\\\\")
        .replace('"', '\\"')
        .replace("\r", "")
        .replace("\n", "\\n")
    )


def _inject_api_config() -> None:
    flags = []
    url = os.environ.get("JINGGUA_API_URL", "")
    root_ca = os.environ.get("JINGGUA_API_ROOT_CA", "")
    if url:
        flags.append('-DJINGGUA_API_URL="{}"'.format(_c_string(url)))
    if root_ca:
        flags.append('-DJINGGUA_API_ROOT_CA="{}"'.format(_c_string(root_ca)))
    if flags:
        env.Append(BUILD_FLAGS=flags)


_inject_api_config()
