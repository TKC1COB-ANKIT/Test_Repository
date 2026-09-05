# Android Application Requirements

## 1. App purpose
The Android app provisions the unit, controls four relay channels, shows honest device status, manages networks and ownership, and optionally enables remote cloud control.

## 2. Required screens
- Add device
- Scan QR or enter setup credential
- Connect to setup network
- Select or manually enter Wi-Fi
- Connection progress and diagnostics
- Claim ownership
- Name channels 1 through 4
- Four-channel dashboard
- Quick controls configuration
- Networks, maximum 3
- Local/cloud service status
- Firmware update
- Members and access
- Redacted diagnostics/support
- Ownership transfer
- Factory reset

## 3. Dashboard behavior
Each channel shows:
- physical channel number 1 through 4
- user-defined name
- requested state
- acknowledgement state
- local/cloud transport
- last result or error

Command UI state flow:
```text
READY -> PENDING -> APPLIED
                 -> REJECTED
                 -> TIMED_OUT_UNKNOWN
```
Do not display ON/OFF as confirmed before device acknowledgement.

## 4. Command rules
- Use explicit `SET_ON` and `SET_OFF` for remote commands.
- Assign a unique command ID.
- Debounce repeated taps.
- Retrying the same command ID must not operate twice.
- Do not queue stale toggle commands for execution after reconnection.
- If load feedback hardware does not exist, label status as `Commanded state`, not `Fan running` or `Light powered`.

## 5. Local and remote behavior
- Prefer the authenticated local path when verified and reachable.
- Use cloud path when outside the LAN or local path is unavailable.
- Show the selected path.
- Local control continues without Internet after ownership has been established.
- Schedules that must survive phone/cloud outage should execute on the device, subject to confirmed device capacity.

## 6. Notification quick controls
- Feature is optional and user-enabled.
- **REQUIRED LIMIT:** maximum 2 favorite actions in the ongoing quick-control notification.
- User chooses the actions. Do not hard-code fan or light to a channel.
- An action sends an authenticated idempotent command.
- Notification changes to applied state only after acknowledgement.
- Lock-screen detail visibility is configurable and privacy-preserving by default.
- High-impact loads may require confirmation. The high-impact load list is **TBD**.

## 7. Volume buttons
Global background volume-button interception is not a required feature and must not be implemented by abusing accessibility services or media controls.

Use supported alternatives:
- notification actions
- home-screen widget
- Quick Settings tile
- app shortcut
- optional wearable control

## 8. Credentials and privacy
- Use platform-protected credential storage where available.
- Protect exported app components, deep links, receivers, and notification intents.
- Use TLS with normal certificate validation for cloud.
- Do not log Wi-Fi passwords, setup credentials, access tokens, or complete command credentials.
- Require recent authentication for ownership transfer, factory reset, and sensitive diagnostics.
- Support member revocation and ownership transfer.
- Telemetry fields, retention, and deletion policy are **TBD**.

## 9. App behavior during uncertainty
- `PENDING`: command sent, no final acknowledgement yet.
- `TIMED_OUT_UNKNOWN`: final device state is not confirmed.
- `LOCAL_ONLY`: device is reachable locally but cloud is unavailable.
- `OFFLINE`: neither approved path is reachable.
- Never guess the relay or appliance state after timeout.
