# Four-Relay Home-Automation Controller

## 1. Purpose of this document pack
This pack tells a human developer or AI coding model what the product is intended to do, what is already decided, what is only a proposal, and what must not be guessed.

## 2. Instruction to the implementation model
Follow these rules before generating hardware, firmware, web, cloud, or Android code:

1. Treat every item marked **REQUIRED** as mandatory.
2. Treat every item marked **PROPOSED DEFAULT** as a starting value that may be changed later.
3. Treat every item marked **TBD** as undecided. Do not silently choose a value. Put the choice in a configuration constant and record the assumption.
4. Do not replace a safety or security requirement with a simpler implementation.
5. Do not remove local control when cloud control is added.
6. Do not assume that a relay command proves the connected appliance is operating. Unless feedback hardware exists, only the commanded relay state is known.
7. Do not assume all ESP8266 boards have the same flash size, pins, bootloader, or OTA layout. These items must be measured or supplied before production firmware is finalized.
8. Do not generate mains-PCB dimensions, fuse values, creepage values, trace widths, or certification claims without the electrical ratings and target-market requirements.
9. If two requirements conflict, apply this priority: human safety, electrical safety, device security, data integrity, local control, cloud availability, convenience.
10. Record every remaining assumption in `08_OPEN_DECISIONS.md`.

## 3. Product goal
Build a controller installed inside a household electrical switchboard. It controls four independent relay channels. The owner can control it locally through the home network and, if enabled, remotely through a cloud service.

## 4. Current known hardware
- **REQUIRED:** ESP8266 is used in the current revision.
- **REQUIRED:** Four independent relay outputs exist.
- **REQUIRED:** Initial factory programming uses UART.
- **REQUIRED:** The base revision has no user-accessible manual input.
- **REQUIRED:** The device may become physically inaccessible after installation.
- **TBD:** Exact ESP8266 module and flash capacity.
- **TBD:** Relay model, coil voltage, contact ratings, and supported load classes.
- **TBD:** Power-supply topology and electrical ratings.
- **TBD:** GPIO assignment and active-high or active-low relay logic.

## 5. Required operating modes
- `UNPROVISIONED`: no valid Wi-Fi configuration exists; setup access is available.
- `CONNECTING`: device is trying stored Wi-Fi networks.
- `LOCAL_ONLY`: home LAN works but Internet or cloud does not.
- `ONLINE`: local LAN and cloud path are available.
- `RECOVERY`: owner can repair network configuration without cloud or enclosure access.
- `OTA_UPDATE`: signed firmware update is in progress.
- `SAFE_MODE`: repeated boot/update failure has disabled nonessential behavior.

## 6. Non-negotiable outcomes
### SYS-001 Safe by default
- On first power-up, reset, bootloader activity, crash, watchdog reset, OTA reboot, corrupt configuration, and factory reset, no relay may turn ON unexpectedly.
- Default output state is all relays OFF.
- Network loss by itself must not toggle any relay.

### SYS-002 Local first
- If the ESP8266 and phone are on the same trusted LAN, relay control must continue when Internet or cloud is unavailable.
- Cloud connection attempts must not block relay control, watchdog service, or the local API.

### SYS-003 Recoverable without cloud
- The owner must be able to repair Wi-Fi configuration without Internet, cloud service, UART, or opening the enclosure.
- Recovery uses a temporary local SoftAP and local setup page.
- A deliberate power-cycle gesture shall provide a second way to request recovery.
- Wi-Fi failure must never automatically erase settings or factory-reset the product.

### SYS-004 Unique device identity
- Each unit must have a unique device ID and unique setup credential.
- A universal setup password shared by all units is forbidden.

### SYS-005 Update safety
- Production firmware updates must be authenticated before activation.
- Interrupted or failed updates must leave a bootable known-good firmware or a documented service recovery path.

### SYS-006 Bounded retries
- Wi-Fi, cloud, DNS, update, and reboot retries must have timeouts and backoff.
- Infinite blocking loops and rapid reboot loops are forbidden.

### SYS-007 Observable status
- The local interface must expose firmware version, hardware revision, reset cause, Wi-Fi stage, local/cloud status, OTA status, and redacted recent error reasons.

## 7. End-to-end user journey
1. Factory flashes and tests the unit through UART.
2. Owner powers a new unit. All relays stay OFF.
3. Because no valid network exists, the device starts a unique setup SoftAP.
4. Owner connects by Android app or browser and proves possession using the unit's unique setup credential.
5. Owner chooses home Wi-Fi and submits the password.
6. Device tests the new network as a candidate. It does not destroy a previously working configuration during the test.
7. If LAN connection succeeds, the device commits the network and enables authenticated local control.
8. Cloud ownership is optional for local-only use and required only for remote control.
9. If the router later changes or all saved networks fail, recovery access becomes available again.
10. Firmware can later be updated only through a mechanism already present in the shipped firmware.

## 8. Files in this pack
- `01_HARDWARE_REQUIREMENTS.md`
- `02_FIRMWARE_STATE_MACHINE.md`
- `03_PROVISIONING_AND_RECOVERY.md`
- `04_LOCAL_AND_CLOUD_PROTOCOL.md`
- `05_OTA_AND_SERVICE.md`
- `06_ANDROID_APPLICATION.md`
- `07_VERIFICATION_PLAN.md`
- `08_OPEN_DECISIONS.md`
- `09_REFERENCE_NOTES.md`
