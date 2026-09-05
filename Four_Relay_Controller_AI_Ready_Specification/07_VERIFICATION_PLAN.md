# Verification and Release Plan

## 1. Test-result rule
Each test case must record:
- test ID
- hardware revision
- firmware version
- app version
- preconditions
- exact steps
- expected result
- actual result
- logs or measurements
- pass/fail
- defect link if failed

A checklist item is not complete merely because code exists.

## 2. Safety and output tests
- Power on with each possible prior relay state. Expected: all relays OFF.
- Reset, watchdog reset, crash, bootloader, UART programming, OTA, rollback, and factory reset. Expected: no unexpected relay ON or chatter.
- Disconnect Wi-Fi and cloud while relays are ON and OFF. Expected: network loss causes no relay change.
- Corrupt configuration. Expected: prior valid record or safe mode; never automatic relay ON.

## 3. Provisioning and recovery tests
- New device starts setup AP.
- Correct Wi-Fi commits successfully.
- Wrong password does not replace last-known-good network.
- Router removed long enough to trigger recovery AP.
- Recovery works without Internet and cloud.
- Recovery AP expires after configured authenticated inactivity.
- Power-cycle gesture enters recovery but does not factory-reset.
- Normal outages and brownouts do not falsely trigger gesture.
- Maximum 3 networks enforced.
- Candidate network failure leaves committed configuration valid.

## 4. Connectivity tests
- SSID renamed
- router replaced with same credentials
- DHCP unavailable
- gateway unavailable
- DNS unavailable
- Internet unavailable while LAN works
- cloud primary fails and backup is healthy
- all cloud endpoints fail while local control continues
- captive portal network
- low RSSI and channel changes
- AP+STA behavior on the exact selected stack

## 5. Command tests
- duplicate command ID
- expired command
- malformed/wrong-device/wrong-channel command
- repeated app tap
- device reset between accept and acknowledgement
- conflicting local and cloud commands
- authorization revoked during operation
- timeout shown as unknown, not as success
- explicit ON/OFF retry does not double-toggle

## 6. Storage tests
- empty configuration
- corrupt newest slot
- power cut during each configuration-write stage
- unknown schema version
- maximum field lengths
- malformed input and invalid UTF-8
- long-run flash-write measurement
- factory reset preserves immutable identity and update trust material

## 7. OTA tests
- valid signed image
- corrupt image
- unsigned image
- wrong signing key
- wrong hardware/product
- oversized image
- prohibited downgrade
- no Internet during candidate self-test
- power cut at every update phase listed in `05_OTA_AND_SERVICE.md`
- old configuration migration and interrupted migration
- rollback to known-good image

## 8. Security tests
- setup credential brute-force/rate limiting
- setup-session expiry
- unauthorized local API
- member and owner permission boundaries
- cross-site request and origin/host handling
- replayed relay command
- invalid TLS certificate and hostname
- endpoint-manifest tamper, replay, expiry, and wrong product
- token revocation and ownership transfer
- UART/physical interface review
- external security assessment covering device, firmware, wireless, app, cloud, API, and update path

## 9. Electrical and environmental validation
Values and pass limits must come from the approved hardware specification and compliance plan. Tests include:
- rated resistive load
- supported fan/motor load
- supported LED-driver inrush
- contact endurance
- overload and single fault
- thermal rise in final enclosure
- surge, EFT, ESD, and EMC
- terminal retention and touch safety
- enclosure/material assessment
- RF performance in the final installation

## 10. Long-run tests
- repeated network loss and recovery
- repeated cloud endpoint failover
- long uptime and clock rollover where applicable
- bounded logs and memory use
- repeated relay operation within approved limits
- update signing-key rotation rehearsal
- endpoint migration rehearsal

## 11. Release evidence
Every release archives:
- source revision and build information
- signed images and hashes
- supported hardware/flash matrix
- completed test report
- known limitations
- migration and rollback results
- safety/compliance evidence
- security/privacy review
- support and recovery procedure
- release approval

## 12. Market-ready gate
Do not label the product market ready until electrical safety, EMC/radio, cybersecurity, privacy, manufacturing consistency, installation instructions, update operations, support, warranty, and target-region legal requirements have been assessed and approved.
