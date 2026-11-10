# Virtual checks before ordering

## Wokwi simulation

The simulation runs the real ESP32-C6 chip on a XIAO ESP32-C6 board, with real GPIO pins, I2C and UART traffic and the threshold logic. The SCD41 and PMS5003 are custom chips with sliders. The hold time is cut from 5 minutes to 10 seconds, and the fault timeouts are shorter too. The Ventilate, Dust and Fault indicators exist only in the simulation; the device itself reports through Apple Home and its web page.

It does not check the 3.3 V and 5 V supplies, real sensors, measurement quality, Matter, BLE or Apple Home.

### How to open it

1. Create a project: https://wokwi.com/projects/new/esp32-c6 (`diagram.json` from `wokwi/` switches the board to the XIAO).
2. Replace `sketch.ino` and `diagram.json` with the files from `wokwi/`.
3. Add `scd41-sim.chip.json`, `scd41-sim.chip.c`, `pms5003-sim.chip.json` and `pms5003-sim.chip.c` through `New file...` in the tab menu.
4. Start the simulation. Click a sensor to open its sliders.

### Run on 2026-11-06

The CO2, PM2.5, failure and recovery scenarios were run on the XIAO ESP32-C6 with the pins from `firmware/main/board_config.h`. The check with swapped TX and RX was run on the previous version of the circuit.

| Scenario | Result |
|---|---|
| Both sensors read over the connections in `diagram.json` | CO2=600, PM2.5=8, Good, no fault |
| CO2 1200 ppm | Poor at once, `ventilate=1` after the hold time |
| CO2 900 ppm during an alert | Moderate, `ventilate=1` stays |
| CO2 700 ppm | Good, `ventilate=0` |
| CO2 1200 ppm for 5 seconds, then 700 ppm | `ventilate=0`, no notification |
| PM2.5 40 µg/m³ | Poor, `dust=1` after the hold time, `ventilate=0` |
| PM2.5 20 µg/m³ during an alert | Moderate, `dust=1` stays |
| PM2.5 10 µg/m³ | Good, `dust=0` |
| SCD41 or PMS5003 failure | Unknown, `fault=1` |
| Sensor recovery | Good, `fault=0` |
| PMS5003 TX and RX swapped on purpose | PM2.5 not read, Unknown, `fault=1` |

## 3D layout

`3d-layout.html` shows the enclosure model with its parts and wires. It reads the geometry from `enclosure-mesh.js`, which `enclosure/enclosure.py` generates. The fit checks are in `enclosure/test_enclosure.py`, and the part sizes with their sources in `enclosure/dimensions.md`.
