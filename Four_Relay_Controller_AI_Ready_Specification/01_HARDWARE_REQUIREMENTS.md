# Hardware Requirements

## 1. Scope
This file defines required hardware behavior. It does not provide final mains design values because the electrical ratings, load types, target markets, enclosure, and PCB layout are not yet defined.

## 2. Relay output rules
### HW-REL-001 Default state
- **REQUIRED:** All four relays are OFF when the MCU is unpowered, held in reset, starting, crashing, or being programmed.
- **REQUIRED:** Each relay driver has a hardware bias resistor that forces the OFF state when its GPIO is high impedance.
- **REQUIRED:** Relay control pins must not glitch because of ESP8266 boot-strap behavior.
- **TBD:** GPIO mapping and driver polarity.

### HW-REL-002 Driver stage
- **REQUIRED:** Do not drive a relay coil directly from an ESP8266 GPIO.
- **REQUIRED:** Use a suitable transistor or MOSFET driver and coil suppression appropriate to the relay.
- **REQUIRED:** Provide local power-supply decoupling.
- **REQUIRED:** Consider a common hardware output-enable signal that defaults disabled.

### HW-REL-003 Load truth
- **REQUIRED:** Software reports `commanded ON/OFF` unless hardware feedback is added.
- **TBD:** Whether contact, current, or voltage feedback will exist.

## 3. Power and reset
### HW-PWR-001 Brownout
- Relays must not chatter during slow power rise, brownout, or repeated interruptions.
- The MCU must remain in reset or outputs disabled until logic power is valid.
- Reset supervisor and hold-up requirements are **TBD** after power-supply measurements.

### HW-PWR-002 Recovery gesture storage
- Do not write one flash flag every second.
- Preferred options, in order: external FRAM or suitable nonvolatile memory; validated RTC-retained state; rate-limited wear-leveled flash journal.
- Every persistent record must include a version, sequence number, payload length, and integrity check.

## 4. Recovery interfaces
The product needs at least two independent recovery methods:

1. **REQUIRED:** Automatic software recovery SoftAP.
2. **REQUIRED:** Deliberate power-cycle gesture that requests recovery.
3. **REQUIRED FOR SERVICE:** UART, boot, reset, ground, and 3.3 V service pads.
4. **OPTIONAL FUTURE REVISION:** concealed button, magnetic sensor, or other safe recovery input.

The power-cycle gesture starts provisioning only. It must not erase ownership or perform a factory reset.

## 5. UART service safety
- UART pads are for trained service use after mains isolation.
- They must not expose the user to mains potential.
- Production documentation must define voltage level, pinout, boot sequence, supported programming image, and safe service procedure.

## 6. Mains domain
The following are mandatory design-review topics, not values for an AI model to invent:

- supply isolation
- creepage and clearance
- protective earthing, if applicable
- fusing and overcurrent protection
- surge, EFT, and ESD protection
- relay contact arcing and snubber/MOV selection
- terminal touch safety and conductor retention
- load inrush and inductive switching
- fire enclosure and material selection
- PCB and relay thermal rise
- single-fault behavior
- EMC and radio performance in the final enclosure

**REQUIRED:** A qualified professional must review the schematic, PCB layout, enclosure, installation method, and target-market compliance. Relay current printed on a component is not enough to approve a fan, motor, LED driver, pump, or heater load.

## 7. Manufacturing data
Every unit shall have:
- unique device ID
- unique setup credential
- serial number
- hardware revision
- manufacturing lot
- QR code containing only the approved onboarding information
- production test result tied to serial number

## 8. Factory test access
Provide test access for:
- 3.3 V and ground
- UART TX/RX
- boot and reset
- each relay control output
- important power rails

Factory relay tests shall use an electrically safe test fixture and defined dummy loads.

## 9. Hardware acceptance results
Hardware is not accepted until tests prove:
- all relays remain OFF during every boot/reset/programming condition
- MCU removal or reset does not energize a relay
- brownouts do not cause relay chatter or persistent-data corruption
- final enclosure temperature is within approved limits at rated loads
- inrush and inductive loads are tested
- UART is safe when serviced according to instructions
- the recovery gesture is not triggered by common outages
- RF remains usable inside the installed enclosure
