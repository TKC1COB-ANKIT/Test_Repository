# OTA Firmware Update and Service Recovery

## 1. Important limitation
OTA works only if the firmware already installed on the device contains a compatible update mechanism. A phone cannot reprogram a sealed unit by software alone when the running firmware and bootloader provide no update path.

## 2. Required pre-work
Before OTA code is finalized, record:
- exact ESP8266 module
- physical flash capacity
- selected SDK/core and version
- bootloader
- partition/flash map
- maximum application image size
- filesystem/configuration allocation
- rollback capability actually supported by that stack

These items are **TBD**. Do not assume ESP32 OTA behavior applies to ESP8266.

## 3. Production update requirements
- Update manifest and image must be authenticated before activation.
- Transport must use TLS, but TLS alone is not a replacement for image signature verification.
- Manifest must identify product, hardware revisions, compatible flash layout, version, minimum version, image size, image hash, release channel, and signature.
- Reject corrupt, unsigned, wrong-product, wrong-hardware, oversized, and prohibited downgrade images.
- Never overwrite the currently bootable image unless the selected platform's validated update design explicitly requires and protects that action.
- Interrupted download or metadata writing must preserve a bootable known-good image or documented service recovery.

## 4. OTA state flow
```text
IDLE
CHECK_MANIFEST
VALIDATE_MANIFEST
DOWNLOAD_TO_INACTIVE_REGION
VERIFY_SIZE_HASH_SIGNATURE
MARK_CANDIDATE_PENDING
PREPARE_SAFE_REBOOT
BOOT_CANDIDATE
SELF_TEST
CONFIRM_CANDIDATE or ROLLBACK
```

## 5. Relay policy during update
- Before activation/reboot, command all four relays OFF.
- During bootloader, first boot, self-test, rollback, and recovery, hardware keeps all relays OFF.
- Update download may occur while the device runs, but activation waits until the device can safely turn outputs OFF.
- Version 1 does not restore relay ON states automatically after update.

## 6. Candidate confirmation
A candidate is confirmed only after:
- configuration can be read
- relay GPIOs initialize safely
- watchdog remains healthy
- local control stack initializes
- firmware remains stable through the defined checkpoint

Cloud availability must not be the only confirmation test, because the Internet may be unavailable during a valid update.

**TBD:** supported rollback mechanism and confirmation timeout. These depend on the exact bootloader and flash map.

## 7. Update release policy
- Separate development, test, beta, and production signing material.
- Keep private release keys offline or in a controlled signing system.
- Device stores public verification material only.
- Support signed key rotation with an overlap period.
- Use staged rollout and allow rollout pause.
- Store signed artifacts, hashes, compatibility data, and release approval records.

## 8. Local update
An optional local maintenance page may upload firmware only when:
- explicit maintenance mode is active
- user is recently authenticated
- image is signed
- image is compatible
- size is within the inactive update region

A reusable OTA password or checksum alone is not sufficient production authentication.

## 9. UART service recovery
UART remains the last-resort repair path.
- Unit must be isolated from mains before service.
- Use documented 3.3 V-compatible tooling.
- Service package must identify the exact hardware revision and flash layout.
- UART access is not considered a normal customer recovery flow.

## 10. Mandatory OTA fault tests
Cut power during:
- manifest storage
- early, middle, and final image download
- image verification
- pending-image metadata change
- first candidate boot
- candidate confirmation
- rollback

For every test, relays remain OFF and the unit returns to known-good firmware or the documented service recovery state.
