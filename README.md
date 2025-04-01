# EcoStation

ESP32 based station for urban ecology using LoRaWAN to transmit data.

[![Quality gate](https://sonarcloud.io/api/project_badges/quality_gate?project=switux_EcoStation)](https://sonarcloud.io/summary/new_code?id=switux_EcoStation)

## Arduino Framework for ESP32

  - Currently using 2.0.17

## LIBRARIES

  - Adafruit_BME_280_Library (2.2.4)
  - Adafruit_BusIO (1.16.1) (dependency from other Adafruit libraries)
  - Adafruit_MLX90614_Library (2.1.5)
  - Adafruit_TSL2591_Library (1.4.5)
  - Adafruit_Unified_Sensor (1.1.14) (dependency from other Adafruit libraries)
  - ArduinoJson (7.2.1)
  - AsyncTCP (1.1.4)
  - Embedded_Template_Library_ETL (20.39.4)
  - ESP32Time (2.0.6)
  - ESPAsyncWebServer (3.1.0)
  - ESPping (1.0.4)
  - MCCI_LoRaWAN_LMIC_library (5.0.1)
  - SD (1.3.0)
  - SSLClient (1.6.11)

## BUILD INSTRUCTIONS

  - Arduino IDE ( add the lines in bold to ~/.arduino15/packages/esp32/hardware/esp32/2.0.17/platform.local.txt )

    - A sequential build number is embedded in the code every time a build is made (whether it is successful or not)

    **recipe.hooks.prebuild.0.pattern={build.source.path}/build_seq.sh {build.source.path} {build.path}**

    - The project uses ESPAsyncWebServer regex, add:

    **compiler.cpp.extra_flags=-DASYNCWEBSERVER_REGEX=1**

## STATUS & DEVELOPMENT

This is version 1.0 of the project. It has now reached production with the first 15 stations out in the field!

The things that will be improved in v1.1 are:

  - Software
    - Some reporting of data to ease operations / monitoring
  - Hardware
    - EEPROM to enable persistent, MCU independent, configuration items

## FEATURES

  - Environmental parameters:

    - Temperature
    - Atmospheric pressure
    - Relative humidity
    - Cloud coverage
    - Solar irradiance
    - Sky Quality Meter for light pollution
    - Sound pressure level
 
  - Operations:

    - External reboot button
    - Debug button (to be pushed when rebooting to activate debug mode)
    - External micro USB socket for debugging (serial console) and firmware updates
    - Feedback about station health (sensors,RTC,SDCard,...)
    - Remote configuration of:
      . SPL integration time
      . Deep sleep duration
      . RTC via LoRaWAN network time
    - Configuration mode and runtime configuration updates activable via button

## SPECIFICATIONS

This section is being reworked as there are different hardware setups.

  - Power consumption:
    - Solar panel version:
       - 12mA in sleep mode
       - ~40mA while active (for ~30s)
  - Autonomy: N/A (collecting data)
  - Measures (from sensor specs)
    - Illuminance range: 0-88k Lux ( up to ~730 W/m² )
    - Temperature range: -40°C to +85°C
    - Pressure: 300 to 1100 hPa

## Runtime configuration interface

## Data format

The station sends the data via a compact byte steam, it includes:

- data format version
- battery_level: in % of 4.2V
- timestamp: Unix epoch time, taken from external RTC
- temp: in °C
- pres: in hPa (QFE)
- rh: relative humidity in %
- lux: solar illuminance
- ambient: IR sensor ambient temperature
- sky: IR sensor sky temperature (substract ambient to get sky temperature, if below -20° --> sky is clear)
- sensors: available sensors (see source code)
- uptime (between soft/cold reboots)
- some debug data like free sdcard space, reset reason, build info, deepsleep duration

## REFERENCES

I found inspiration in the following pages / posts:

  - No need to push the BOOT button on the ESP32 to upload new sketch
    - https://randomnerdtutorials.com/solved-failed-to-connect-to-esp32-timed-out-waiting-for-packet-header
  - Reading battery load level without draining it
    - https://jeelabs.org/2013/05/18/zero-power-measurement-part-2/index.html
  - To workaround the 3.3V limitation to trigger the P-FET of the above
    - https://electronics.stackexchange.com/a/562942
  - Solar panel tilt
    - https://globalsolaratlas.info/
  - TSL2591 response to temperature by Marco Gulino
    - https://github.com/gshau/SQM_TSL2591/commit/9a9ae893cad6f4f078f75384403336409ca85380  
  - Conversion of lux to W/m<sup>2</sup>
    - P. Michael, D. Johnston, W. Moreno, 2020. https://pdfs.semanticscholar.org/5d6d/ad2e803382910c8a8b0f2dd0d71e4290051a.pdf, section 5. Conclusions. The bottomline is 120 lx = 1 W/m<sup>2</sup> is the engineering rule of thumb.
