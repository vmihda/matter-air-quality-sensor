# Matter air quality sensor

A DIY air quality sensor for Apple Home. It measures CO2, PM2.5 and VOC, joins Home over Matter on 2.4 GHz Wi-Fi, and fits in an 80 × 50 × 30 mm 3D printed enclosure. It runs on USB power and has no screen.

Apple Home shows CO2, PM2.5, PM10 and TVOC in the air quality tile, plus temperature and humidity from the SCD41. The overall level is the worse of CO2 and PM2.5; VOC is shown but does not change the level.

| Level | CO2 | PM2.5 |
|---|---|---|
| Good | below 800 ppm | below 15 µg/m³ |
| Moderate | from 800 ppm | from 15 µg/m³ |
| Poor | from 1000 ppm for 2 minutes | from 35 µg/m³ for 5 minutes |

The level leaves Poor only once the value drops below the Moderate threshold, so it does not flicker around the limit. The thresholds and both hold times can be changed on the device web page.

## Status

The firmware builds for the XIAO ESP32-C6 and fits the 4 MB flash with two update slots. It has been tested on a XIAO ESP32-S3 without sensors: pairing with Apple Home, the demo values, the device page and a firmware update through the page all work. The real sensors, the C6 board itself and the printed enclosure have not been tested yet, because the parts are still on order. The enclosure sizes come from datasheets and shop listings and will be corrected once the parts are measured.

## Parts

| Part | Notes |
|---|---|
| [Seeed XIAO ESP32-C6](https://www.aliexpress.com/item/1005010295660092.html) | 4 MB flash, ceramic antenna, U.FL for an external one |
| [SCD41 module](https://www.aliexpress.com/item/1005008696769890.html) | CO2, temperature and humidity over I2C; pick the SCD41 variant |
| [ENS160+AHT21 module, blue](https://www.aliexpress.com/item/1005004761725413.html) | TVOC over I2C; the AHT21 on the same board is not used |
| [PMS5003 with G5 adapter and cable](https://www.aliexpress.com/item/1005008530758518.html) | PM1, PM2.5 and PM10 over UART, 5 V supply |
| [28 AWG silicone wire](https://www.aliexpress.com/item/1005008467990750.html) | for the wiring inside the enclosure |
| [JST-PH 2.0 4-pin pigtails](https://www.aliexpress.com/item/1005006227685119.html) | two pairs, so the sensors unplug without soldering |
| [1 mm double-sided foam tape](https://www.aliexpress.com/item/1005006341630347.html) | seals the PMS5003 ports and holds the sensor down |
| [Heat shrink kit](https://www.aliexpress.com/item/1005009874296174.html) | for the four wire splices under the board |
| [Female-to-female Dupont wires](https://www.aliexpress.com/item/1005004937144019.html) | for the first test on the bench |

The parts cost about 65 USD for one unit as of November 2026. A USB-C data cable and a 5 V charger rated at 1 A or more are also needed.

## Wiring

| From | To the XIAO | Notes |
|---|---|---|
| SCD41 VCC | 3V3 | not 5 V |
| SCD41 GND | GND | |
| SCD41 SDA | D4 (GPIO22) | |
| SCD41 SCL | D5 (GPIO23) | |
| ENS160+AHT21 VIN | 3V3 | never 5 V; check the module on its own first and leave its 3V3 pin unconnected |
| ENS160 GND | GND | |
| ENS160 SDA | D4 (GPIO22) | shares the bus with the SCD41 |
| ENS160 SCL | D5 (GPIO23) | shares the bus with the SCD41 |
| ENS160 ADD, CS | not connected until their voltage is measured | limit is VDDIO + 0.3 V |
| PMS5003 VCC | 5V | through the adapter |
| PMS5003 GND | GND | |
| PMS5003 TX | D7 (GPIO17, UART1 RX) | |
| PMS5003 RX | D6 (GPIO16, UART1 TX) | |

GPIO3 and GPIO14 drive the antenna switch and stay free of sensors. The pins of every board are in [firmware/main/board_config.h](firmware/main/board_config.h).

## Firmware

The firmware is written in C and C++ for ESP-IDF v6.0.2 and esp-matter v1.6.1. With both installed in `~/esp`:

```bash
source ~/esp/esp-idf/export.sh
source ~/esp/esp-matter/export.sh
cd firmware
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.local" build flash monitor
```

`sdkconfig.local` holds the settings of one unit and is not committed. The device page stays off until it sets a password:

```
CONFIG_AQ_WEB_PASSWORD="choose-a-password"
```

More configuration fragments can be added to the list:

| Fragment | What it does |
|---|---|
| `sdkconfig.debug` | full logs and error names |
| `sdkconfig.demo` | Matter with demo values that change every 40 seconds, for checking Apple Home without sensors |
| `sdkconfig.bringup` | reads the sensors and prints them, without Matter |
| `sdkconfig.board-s3` | builds for a XIAO ESP32-S3 test board |

The XIAO ESP32-S3 needs its 2.4 GHz antenna on the U.FL connector, otherwise it finds no Wi-Fi networks.

The device uses the Matter test vendor ID 0xFFF1, so Apple Home warns that the accessory is not certified; adding it anyway works. During pairing the iPhone has to be on the 2.4 GHz network.

### Device page and console

The device page is at the IP address of the device, with the login `admin` and the password from `sdkconfig.local`. It shows the readings, changes the thresholds, hold times, PM2.5 sampling interval and temperature offset, and installs a new firmware file. The page is in Ukrainian.

The USB console has the commands `status`, `settings` and `log`.

Holding BOOT for 5 seconds erases the pairing and restarts the device. A short press lights the LED for 3 seconds to show the current state.

| LED | State |
|---|---|
| steady | starting, waiting for the first readings |
| blinks once a second | waiting to be added to Home |
| two short flashes every 2 seconds | added to Home, no Wi-Fi |
| blinks twice a second | a sensor does not respond |
| blinks very fast, then restart | erasing after a long BOOT press |
| short flash every 30 seconds | everything works |

### Tests

The sensor protocols, the level logic, the settings and the LED states are tested on the computer, without ESP-IDF:

```bash
cmake -S firmware/test_host -B build-host
cmake --build build-host
ctest --test-dir build-host
```

## Enclosure

The enclosure is a parametric [CadQuery](https://cadquery.readthedocs.io/) model. Its tests check that the parts and wires fit without touching the plastic, that the air openings are large enough, and that the lid can close.

```bash
python3 -m venv .venv
.venv/bin/pip install -r enclosure/requirements.txt
.venv/bin/pytest enclosure
.venv/bin/python enclosure/enclosure.py
```

The last command writes `base.stl`, `lid.stl`, the STEP files and `assembly.step` to `enclosure/out/`. Print in PETG, base and lid each with the outside face down, about 40 g in total. The part sizes, their sources, the layout decisions and the assembly order are in [enclosure/dimensions.md](enclosure/dimensions.md).

[virtual/3d-layout.html](virtual/3d-layout.html) shows the model in the browser. Serve the `virtual` folder over HTTP, for example with `python3 -m http.server`, so the page can load `enclosure-mesh.js`.

## Simulation

[virtual/README.md](virtual/README.md) explains how to run the Wokwi simulation of the sensors and the level logic.
