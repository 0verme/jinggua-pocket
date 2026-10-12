import hashlib
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / "tools" / "create_ota_manifest.py"


class CreateOtaManifestTests(unittest.TestCase):
    def test_emits_exact_release_contract_and_digest(self):
        payload = b"native-fixture-firmware"
        with tempfile.TemporaryDirectory() as temporary:
            firmware = Path(temporary) / "firmware.bin"
            output = Path(temporary) / "firmware-manifest.txt"
            firmware.write_bytes(payload)
            result = subprocess.run(
                [
                    sys.executable,
                    str(TOOL),
                    "--firmware",
                    str(firmware),
                    "--version",
                    "1.2.3",
                    "--output",
                    str(output),
                ],
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(
                output.read_text(encoding="ascii"),
                "format=1\n"
                "version=1.2.3\n"
                f"size={len(payload)}\n"
                f"sha256={hashlib.sha256(payload).hexdigest()}\n"
                "url=https://github.com/0verme/jinggua-pocket/releases/"
                "download/v1.2.3/firmware.bin\n",
            )

    def test_rejects_invalid_version(self):
        with tempfile.TemporaryDirectory() as temporary:
            firmware = Path(temporary) / "firmware.bin"
            output = Path(temporary) / "firmware-manifest.txt"
            firmware.write_bytes(b"image")
            for version in ("01.2.3", "4294967296.2.3"):
                result = subprocess.run(
                    [
                        sys.executable,
                        str(TOOL),
                        "--firmware",
                        str(firmware),
                        "--version",
                        version,
                        "--output",
                        str(output),
                    ],
                    check=False,
                    capture_output=True,
                    text=True,
                )
                self.assertNotEqual(result.returncode, 0)
                self.assertFalse(output.exists())

    def test_rejects_empty_and_oversize_images(self):
        with tempfile.TemporaryDirectory() as temporary:
            firmware = Path(temporary) / "firmware.bin"
            output = Path(temporary) / "firmware-manifest.txt"
            for payload in (b"", b"x" * (0x330000 + 1)):
                firmware.write_bytes(payload)
                result = subprocess.run(
                    [
                        sys.executable,
                        str(TOOL),
                        "--firmware",
                        str(firmware),
                        "--version",
                        "1.2.3",
                        "--output",
                        str(output),
                    ],
                    check=False,
                    capture_output=True,
                    text=True,
                )
                self.assertNotEqual(result.returncode, 0)
                self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
