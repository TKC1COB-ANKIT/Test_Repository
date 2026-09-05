# Local Control, Cloud Control, and Endpoint Changes

## 1. Required architecture
```text
Android app -- authenticated local path --> device --> relays
Android app -- TLS --> cloud service <-- outbound TLS -- device
```
The device initiates cloud connections. Do not expose an unauthenticated or directly Internet-accessible relay port.

## 2. Local control
- Works after ownership setup when phone and device can reach each other on the trusted LAN.
- Does not require Internet or a working cloud service.
- Requires authentication. Presence on the same Wi-Fi is not authorization.
- App shows whether a command used `LOCAL` or `CLOUD` transport.
- Discovery method is **TBD**. Candidate methods are mDNS plus cached device identity/address fallback.

## 3. Cloud control
Cloud control is optional for local-only users and required for control from outside the local network.

The selected protocol is **TBD**. Approved candidates:
- MQTT over TLS
- HTTPS/WebSocket over TLS

Do not choose until RAM, flash, TLS certificate validation, reconnect behavior, and message semantics are measured on the exact ESP8266 build.

Cloud protocol must provide:
- unique device identity
- revocable device credential
- TLS certificate validation
- user/device authorization
- unique command ID
- command expiry where trustworthy time exists
- replay protection
- acknowledgement
- bounded payload and queues
- reconnect backoff with jitter
- server-side rate limiting and audit events

## 4. Desired versus reported state
Keep these separate:

```json
{
  "desired": {"channel_1": "OFF", "generation": 51},
  "reported": {"channel_1": "OFF", "generation": 51, "quality": "COMMANDED_ONLY"}
}
```

If there is no load/contact feedback, `reported` means the relay output was commanded. It does not prove that the appliance is powered or running.

## 5. Server address flexibility
The Android app must not write an arbitrary server URL into the device.

### Required endpoint-discovery design
1. Firmware contains a small factory bootstrap domain list and public verification key material.
2. Device downloads an endpoint manifest over TLS.
3. Manifest is digitally signed.
4. Device verifies signature, product, hardware compatibility, schema, size, version rules, and time/expiry policy before use.
5. Device stores factory, last-known-good, and candidate manifests separately.
6. Candidate becomes current only after verification and a successful health check.

Required manifest data:
```json
{
  "schema": 1,
  "product": "relay4",
  "issued_at": "TBD",
  "expires_at": "TBD",
  "api_endpoints": [
    {"url": "https://primary.example", "priority": 100},
    {"url": "https://backup.example", "priority": 90}
  ],
  "minimum_firmware": "TBD",
  "key_id": "TBD",
  "signature": "TBD"
}
```

## 6. Endpoint failover
- Do not fail over after one lost packet.
- Mark an endpoint unhealthy only after a defined health-check policy.
- Use a cooldown before retrying the failed endpoint.
- Do not bounce continuously between endpoints.
- Never use an endpoint with an untrusted certificate or invalid manifest signature.
- DNS names should be stable so addresses can change without firmware edits.

**TBD:** exact request timeout, consecutive failure count, and cooldown. The implementation must expose these as bounded configuration constants. Do not invent production values silently.

## 7. Time handling
TLS and expiry checks need a defined time policy.
- Store the last trusted time only if the threat model accepts its limitations.
- Obtain updated trusted time through the approved mechanism.
- Do not create a permanent `ignore certificate time` mode.
- Behavior with an unknown clock is **TBD** and must be resolved before production.

## 8. Ownership
- Factory identity is unique and immutable.
- First owner claim replaces or rotates bootstrap ownership credentials.
- Additional household members receive distinct grants, not one shared password.
- Removing a member revokes that member's grants.
- Ownership transfer revokes old user grants and tokens.
- Factory reset clears user ownership but preserves immutable device identity and update trust material.
