JingGua Pocket v0.3.0 - StickS3 USB Flash Package

Target: M5Stack StickS3 / ESP32-S3 with 8 MB flash and this project's default_8MB.csv layout.

Contents:
  bootloader.bin  ESP32-S3 bootloader from the release build
  partitions.bin  default_8MB.csv partition table from the release build
  boot_app0.bin   Arduino ESP32 boot_app0 image
  firmware.bin    v0.3.0 application image
  flash_windows.ps1  guarded Windows PowerShell/esptool script

Windows:
  1. Verify the ZIP SHA-256 against SHA256SUMS.txt from the GitHub Release.
  2. Install Python 3 and a compatible esptool 4.x:
       py -3 -m pip install "esptool>=4.8,<5"
  3. Open PowerShell in the extracted directory and run:
       Set-ExecutionPolicy -Scope Process Bypass
       .\flash_windows.ps1 -Port COM5
     Replace COM5 with the port shown by Windows Device Manager. Confirm only after
     checking that the connected device is a StickS3/ESP32-S3.

The script writes the four images at their PlatformIO offsets and does not run
erase_flash (whole-chip erase); esptool may erase/rewrite sectors within those
image ranges. It rewrites the bootloader, partition table and OTA selection
metadata; NVS occupies a separate partition and is not in the write ranges.
Do not use this package on other boards or incompatible flash layouts. Do not
interrupt power during flashing. Hardware validation is still pending; successful
USB transfer does not certify device behavior.
