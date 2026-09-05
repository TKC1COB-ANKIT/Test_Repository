# Open Decisions and Assumption Register

## Instruction
An AI model must not silently decide any item in this file. When generating a prototype, it may use a clearly labeled temporary value, but the choice must remain centralized and easy to change.

## Hardware decisions
- [ ] Exact ESP8266 module.
- [ ] Flash capacity.
- [ ] SDK/core and version.
- [ ] Bootloader and flash map.
- [ ] GPIO-to-relay mapping.
- [ ] Driver polarity.
- [ ] Relay model and coil voltage.
- [ ] Supported load types and ratings.
- [ ] Power-supply design.
- [ ] Feedback sensing, if any.
- [ ] Target countries and applicable standards.

## Firmware decisions
- [ ] Final Wi-Fi attempt count and backoff bounds.
- [ ] Final recovery delay and recovery AP timeout.
- [ ] Power-cycle gesture timings after hardware validation.
- [ ] Healthy-boot checkpoint and crash-loop threshold.
- [ ] Exact local discovery method.
- [ ] Local API transport and authentication design.
- [ ] Schedule capacity and storage format.
- [ ] Trusted-time strategy.
- [ ] Diagnostic log capacity and retention.

## Cloud decisions
- [ ] MQTT over TLS or HTTPS/WebSocket over TLS.
- [ ] Device credential provisioning and rotation.
- [ ] Endpoint health-check timeout, failure count, and cooldown.
- [ ] Endpoint-manifest signing algorithm and key rotation.
- [ ] Cloud provider and deployment topology.
- [ ] Data region, telemetry, retention, and deletion rules.

## OTA decisions
- [ ] Exact ESP8266 OTA implementation supported by selected stack.
- [ ] Whether true automatic rollback is available.
- [ ] Image signing algorithm and key storage.
- [ ] Candidate confirmation timeout.
- [ ] Stable/beta rollout mechanisms.
- [ ] Local update-page inclusion.

## Android decisions
- [ ] Minimum Android version.
- [ ] UI technology.
- [ ] Local discovery implementation.
- [ ] Quick Settings tile/widget scope.
- [ ] Which load types require command confirmation.
- [ ] Accountless local-only ownership model.
- [ ] Push-notification provider, if used.

## Confirmed values already used by this pack
- Relay channels: 4.
- Saved Wi-Fi networks: maximum 3.
- Default relay boot state: all OFF.
- Notification favorite actions: maximum 2.
- Arbitrary phone-written server URL: forbidden.
- Network failure does not factory-reset the device.
- UART: factory programming and last-resort service only.

## Proposed defaults, not final commitments
```text
Recovery after no LAN: 10 minutes
Recovery AP inactivity timeout: 15 minutes
Power gesture: 3 interrupted boots
Candidate boot interval: 5 to 20 seconds
Stable boot threshold: 45 seconds
Consecutive unconfirmed boots before safe mode: 3
```
