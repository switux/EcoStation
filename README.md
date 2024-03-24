# EcoStation

ESP32 based station for urban ecology.

## PRE-REQUISITES (WIP)

## BUILD INSTRUCTIONS

  - Arduino IDE ( add the lines in bold to ~/.arduino15/packages/esp**xxxx**/hardware/esp**xxxx**/**{version}**/platform.local.txt )

    - A sequential build number is embedded in the code every time a build is made (whether it is successful or not)

    **recipe.hooks.prebuild.0.pattern={build.source.path}/../build_seq.sh {build.source.path}**

    For the hook to work, you will also need to create a link (or copy the file) to the file "build.sh" from sketch directory to the path of the sketch book. Sorry for the mess ... if you have a better solution, you are most welcome!

## STATUS & DEVELOPMENT

This is version 1.0 of the project. **It is still work in progress.**

The things that might be improved in v1.1 are:

  - Software
    - TBD
  - Hardware
    - TBD

## FEATURES

  - Weather parameters:

    - Temperature
    - Pressure
    - Relative humidity
    - Cloud coverage
    - Solar irradiance
    - Sky Quality Meter for light pollution
 
  - Alarms for:
 
    - Sensor unavailability
    - Low battery
 
  - Operations:

    - External reboot button
    - Debug button (to be pushed when rebooting to activate debug mode)
    - External micro USB socket for debugging (serial console) and firmware updates
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

## Alarms format

The station sends alarm to an HTTPS endpoint as a JSON string:

{
  "subject": "reason for the alarm",
  "message": "what happened"
}

## Data format

The station sends the data to a web server as a JSON string:

{
  "battery_level":92,
  "timestamp":1675767261,
  "temp":16.84000015,
  "pres":941.1413574,
  "rh":27.296875,
  "lux":68181,
  "ambient":21.42998695,
  "sky":-3.19000864,
  "sensors":63
}

Where

- battery_level: in % of 4.2V
- timestamp: Unix epoch time
- temp: in °C
- pres: in hPa (QFE)
- rh: relative humidity in %
- lux: solar illuminance
- ambient: IR sensor ambient temperature
- sky: IR sensor sky temperature (substract ambient to get sky temperature, if below -20° --> sky is clear)
- sensors: available sensors (see source code)

## REFERENCES

I found inspiration in the following pages / posts:

  - No need to push the BOOT button on the ESP32 to upload new sketch
    - https://randomnerdtutorials.com/solved-failed-to-connect-to-esp32-timed-out-waiting-for-packet-header
  - Reading battery load level without draining it
    - https://jeelabs.org/2013/05/18/zero-power-measurement-part-2/index.html
  - To workaround the 3.3V limitation to trigger the P-FET of the above ( I used an IRF930 to drive the IRF9540 )
    - https://electronics.stackexchange.com/a/562942
  - Solar panel tilt
    - https://globalsolaratlas.info/
  - TSL2591 response to temperature by Marco Gulino
    - https://github.com/gshau/SQM_TSL2591/commit/9a9ae893cad6f4f078f75384403336409ca85380  
  - Conversion of lux to W/m<sup>2</sup>
    - P. Michael, D. Johnston, W. Moreno, 2020. https://pdfs.semanticscholar.org/5d6d/ad2e803382910c8a8b0f2dd0d71e4290051a.pdf, section 5. Conclusions. The bottomline is 120 lx = 1 W/m<sup>2</sup> is the engineering rule of thumb.
