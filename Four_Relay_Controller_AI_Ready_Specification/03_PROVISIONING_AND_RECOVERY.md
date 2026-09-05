# Provisioning and Cloud-Independent Recovery

## 1. Meaning of provisioning
Provisioning is the process of giving an unconfigured device its first valid home Wi-Fi configuration and, optionally, connecting it to an owner/cloud account.

## 2. First-time setup
### PROV-001 Entry condition
If no valid committed Wi-Fi configuration exists, start `UNPROVISIONED_AP` after safe relay initialization.

### PROV-002 Setup SoftAP
- **REQUIRED SSID FORMAT:** `RelayController-<6-character-device-suffix>`.
- **REQUIRED:** unique per-device setup credential. Do not use one password for every product.
- **PROPOSED DEFAULT IP:** `192.168.4.1`.
- **REQUIRED:** browser UI and app use the same device setup API.
- **REQUIRED:** no Internet or cloud login is needed to enter Wi-Fi credentials.

### PROV-003 First-time flow
1. User connects to the unit's setup network or the app guides the connection.
2. User proves physical possession using the QR/setup credential.
3. Device displays model, device suffix, hardware revision, firmware version, and ownership status. It displays no secrets.
4. Device scans Wi-Fi networks.
5. User selects or manually enters an SSID and password.
6. Device validates field lengths and creates a candidate configuration.
7. Device attempts association and DHCP while the setup session remains recoverable.
8. On success, commit candidate configuration.
9. Verify authenticated local control.
10. Cloud ownership is offered separately. Local-only operation remains possible.

## 3. Recovery without cloud
### REC-001 Trigger conditions
Enter `RECOVERY_AP_STA` when either condition is true:
- a valid power-cycle recovery gesture is detected; or
- all enabled Wi-Fi networks have failed to provide LAN connectivity for the configured recovery delay.

### REC-002 Required behavior
- Start a temporary SoftAP using the same unique SSID suffix and unique setup credential.
- Continue low-rate Station-mode attempts to saved networks where the chosen ESP8266 stack supports AP+STA.
- Preserve current relay state.
- Do not erase ownership, cloud credentials, or all Wi-Fi networks.
- Permit adding, testing, reprioritizing, and deleting saved Wi-Fi networks.
- Exit recovery after a new network is successfully committed and the authenticated session finishes.
- If recovery closes due to inactivity while no LAN is available, reopen it according to a bounded retry schedule.

### REC-003 Proposed default timing
```text
NO_LAN_BEFORE_RECOVERY = 10 minutes
RECOVERY_IDLE_TIMEOUT = 15 minutes after last authenticated activity
```
These values are proposed defaults. Keep them configurable within safe bounds.

## 4. Power-cycle gesture
### REC-POWER-001 Proposed gesture
- Three interrupted candidate boots.
- Each boot remains powered for 5 to 20 seconds.
- A boot is treated as stable after 45 seconds of healthy operation.
- After recognizing the third candidate boot, clear the gesture record and start recovery provisioning.

### REC-POWER-002 Restrictions
- Do not count sub-second power noise as a deliberate gesture where hardware detection permits.
- Do not factory-reset from this gesture.
- Do not make this the only recovery method.
- These timing values must be validated against real outages and brownouts before release.

## 5. Setup pages
Required pages or screens:
- device identity and setup status
- Wi-Fi scan/manual entry
- candidate connection progress
- network list with maximum 3 entries
- local/cloud connectivity diagnostics
- channel names, while preserving channel numbers 1 to 4
- firmware and hardware information
- authenticated reboot
- authenticated ownership transfer
- authenticated factory reset
- redacted diagnostic export

## 6. Connection result meanings
- `SSID_NOT_FOUND`: candidate retained; rescan permitted.
- `AUTHENTICATION_FAILED`: old committed network remains unchanged.
- `DHCP_FAILED`: Wi-Fi association succeeded, but no LAN address was obtained.
- `LAN_ONLY`: local control available; Internet/cloud unavailable.
- `DNS_FAILED`: no insecure fallback.
- `TLS_FAILED`: no plaintext fallback.
- `CLOUD_FAILED`: local control continues; endpoint failover may run.

## 7. Setup security
- Setup session requires proof of possession.
- Session expires and is rate-limited.
- State-changing requests require authentication and anti-replay protection.
- Validate origin/host and defend against cross-site requests as appropriate to the selected web stack.
- Never reveal a stored Wi-Fi password.
- Disable setup-only routes outside provisioning/recovery mode.
- Factory reset and ownership transfer require recent authenticated authorization.

## 8. Factory reset definition
Factory reset shall:
- turn all relays OFF
- remove user ownership grants
- remove saved Wi-Fi networks
- remove user configuration and cloud tokens
- return to first-time provisioning

Factory reset shall not:
- erase immutable device ID
- erase firmware signature trust material
- install a different firmware
- occur automatically because the router is unavailable
