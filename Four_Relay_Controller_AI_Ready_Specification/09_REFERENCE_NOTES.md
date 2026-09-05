# Reference Notes

These references support platform facts used in the planning pack. They do not replace validation on the exact hardware, SDK, build, PCB, enclosure, and target market.

## ESP8266 Wi-Fi and OTA
- ESP8266 RTOS SDK Wi-Fi documentation describes Station mode, SoftAP mode, and combined AP+Station mode: https://docs.espressif.com/projects/esp8266-rtos-sdk/en/release-v3.4/api-reference/wifi/esp_wifi.html
- ESP8266 RTOS SDK partition documentation describes single-app and two-OTA-app partition-table options: https://docs.espressif.com/projects/esp8266-rtos-sdk/en/v3.4/api-guides/partition-tables.html
- ESP8266 Arduino OTA documentation states that initial firmware is uploaded serially, later OTA requires OTA support in the installed program, and OTA security is the developer's responsibility: https://github.com/esp8266/Arduino/blob/master/doc/ota_updates/readme.rst
- Espressif ESP8266 FOTA documentation: https://www.espressif.com/sites/default/files/99c-esp8266_ota_upgrade_en_v1.6.pdf

## Android
- Notification actions: https://developer.android.com/reference/androidx/core/app/NotificationCompat.Action
- Notification creation and permission behavior: https://developer.android.com/develop/ui/views/notifications/build-notification
- Background execution limits: https://developer.android.com/about/versions/oreo/background

## Security testing
- OWASP IoT Security Testing Guide: https://owasp.org/owasp-istg/index.html

## Electrical standards starting points
- IEC 60669-1 covers general requirements for household and similar fixed-installation switches.
- IEC 60669-2-1 covers particular requirements for electronic control devices.

A compliance professional must determine actual applicability and required regional standards.
