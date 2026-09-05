# Firmware State Machine and Data Rules

## 1. Implementation instruction
Implement connectivity as a non-blocking state machine. Do not place Wi-Fi or cloud connection code in an infinite loop. Every external operation needs a timeout, retry limit, and error result.

## 2. Required states
```text
BOOT
LOAD_CONFIG
SAFE_OUTPUT_INIT
UNPROVISIONED_AP
WIFI_CONNECTING
LAN_READY
LOCAL_ONLY
CLOUD_CONNECTING
ONLINE
RECOVERY_AP_STA
OTA_PREPARE
OTA_DOWNLOAD
OTA_VERIFY
OTA_PENDING_REBOOT
SAFE_MODE
```

## 3. Boot order
1. Configure relay GPIO hardware to OFF before filesystem, Wi-Fi, web, or cloud initialization.
2. Read reset cause.
3. Load the newest valid configuration record.
4. If no valid configuration exists, enter `UNPROVISIONED_AP`.
5. Detect interrupted OTA or repeated unhealthy boots.
6. Evaluate the power-cycle recovery gesture.
7. Attempt stored Wi-Fi networks.
8. Start authenticated local control after LAN readiness.
9. Attempt cloud connection independently.

## 4. Relay boot policy
- **REQUIRED DEFAULT:** all relays OFF after every reboot.
- `restore last state` and `always ON` are not part of version 1 unless explicitly approved in `08_OPEN_DECISIONS.md`.
- Losing Wi-Fi or cloud does not alter current relay state.
- Entering recovery AP does not alter current relay state.
- Entering OTA shall follow the OTA relay policy in `05_OTA_AND_SERVICE.md`.

## 5. Saved Wi-Fi networks
- **REQUIRED LIMIT:** maximum 3 saved networks.
- Each entry: SSID, secret reference, priority, last successful connection, enabled flag.
- Try the last-known-good enabled network first.
- Then scan and try other saved visible networks in priority order.
- A hidden SSID may be attempted by explicit configuration.
- Never log or return a saved Wi-Fi password.
- Never delete a network merely because it is temporarily unavailable.
- A new network remains a candidate until association and DHCP succeed.

## 6. Proposed retry defaults
These are **PROPOSED DEFAULTS**, not confirmed product decisions:

```text
PER_NETWORK_ATTEMPTS = 3
INITIAL_RETRY_DELAY_SECONDS = 2
MAX_RETRY_DELAY_SECONDS = 300
RECOVERY_AP_START_AFTER_NO_LAN_SECONDS = 600
RECOVERY_AP_IDLE_TIMEOUT_SECONDS = 900
```

Use exponential backoff with jitter. Make values compile-time configuration with safe minimum/maximum bounds. Do not allow a server to set values that create a tight retry loop.

## 7. Connectivity status must be separated
Track these independently:
- Wi-Fi radio and association
- DHCP/IP address
- gateway or LAN availability
- DNS resolution
- trusted time/TLS readiness
- primary cloud endpoint
- backup cloud endpoint

Never report `ONLINE` merely because Wi-Fi association succeeded.

## 8. Configuration storage
Use two configuration slots:

```text
magic
schema_version
sequence_number
payload_length
payload
CRC_or_integrity_value
```

Write process:
1. Write new data to inactive slot.
2. Read it back.
3. Validate length and integrity.
4. Mark it as the newest valid generation.
5. Keep the previous valid slot until the new slot is proven.

Do not persist transient retry counters, every relay action, or high-frequency logs.

## 9. Command processing
Every relay command must contain:

```json
{
  "command_id": "unique-id",
  "device_id": "target-device",
  "channel": 1,
  "requested_state": "OFF",
  "source": "LOCAL_OR_CLOUD",
  "issued_at": "timestamp-if-available",
  "expires_at": "timestamp-if-available",
  "expected_generation": 42
}
```

Rules:
- Channel is 1 through 4 only.
- Prefer explicit `ON` or `OFF`. Avoid remote `TOGGLE` because a retry can reverse the intended result.
- Repeating the same `command_id` must not operate the relay twice.
- Reject expired, unauthorized, malformed, wrong-device, and replayed commands.
- Safety rules override all commands.
- **REQUIRED VERSION-1 CONFLICT RULE:** process valid commands in the order accepted by the device; the latest accepted explicit state becomes desired state. Record source and result.
- Acknowledgement reports `accepted`, `applied`, `rejected`, or `unknown` with a reason.

## 10. Watchdog and safe mode
- Service hardware/software watchdogs during all normal states.
- Time-bound DNS, TLS, HTTP, MQTT, filesystem, and OTA operations.
- Mark a boot healthy only after core initialization and a stable-runtime checkpoint.
- **PROPOSED DEFAULT:** after 3 consecutive unconfirmed boots, enter `SAFE_MODE`.
- In `SAFE_MODE`, relays remain OFF, cloud automation is disabled, and local recovery diagnostics remain available.

## 11. Forbidden implementations
- infinite Wi-Fi connection loop
- open recovery AP without expiry or authentication
- universal setup password
- plaintext Internet relay commands or firmware
- automatic factory reset after Wi-Fi failure
- flash write on every retry or loop iteration
- automatic relay ON after reboot
- blocking cloud reconnect that prevents local control
