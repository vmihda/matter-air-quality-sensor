# Part dimensions for the enclosure

The model was built before the parts were ordered, so the sizes come from datasheets and shop listings. When the parts arrive, check them with a ruler, update the constants at the top of `enclosure.py`, and only then print.

The source column says where each size comes from: a drawing in the manufacturer datasheet, a seller listing, or an estimate that still has to be measured.

| Part | Size, mm | Source |
|---|---|---|
| PMS5003, body | 50 × 38 × 20.9 | drawing, page 14 |
| PMS5003, 4 small holes | first at 6 from the left edge, last at 18, about ∅2, centre 2.45 below the top face | drawing, diameter read from the figure |
| PMS5003, fan | centre at 35.8 from the left edge and at mid height, R9 and R6 | drawing |
| PMS5003, connector | slot 17 × 4.9, 5.5 from the edge, on the rear face | drawing |
| PMS5003 cable plug | sticks out up to 6 from the rear face | estimate |
| G5 adapter | 20 × 16 × 6, plug up to 4 | estimate |
| XIAO ESP32-C6, board | 21 × 17.8 | Seeed wiki |
| XIAO ESP32-C6, height with components | 5 | estimate |
| XIAO ESP32-C6, USB overhang past the board | 1.3 | estimate |
| XIAO ESP32-C6, BOOT, RESET and LED positions | at the USB end, exact place unknown | estimate |
| SCD41, module | 13.4 × 21.6 × 8.2 | listing |
| ENS160+AHT21, blue module | 24 × 19 | listing |
| ENS160, thickness with wires | 6.5 | estimate |

## Layout decisions

### PMS5003 ports

Both ports are on one face of the sensor. The datasheet asks to keep that face close to the enclosure wall, or to put a shield between inlet and outlet so the air does not loop back. Here the face sits at the front wall, with a ribbed frame around and between the ports that stops 0.5 mm short of the sensor.

The frame must get 1 mm foam tape, including the 3 mm wide rib between the ports. Without it the two ports are joined by a through gap of about 10 mm², roughly the area of the four small holes.

The drawing does not say which port is the inlet. The enclosure works either way, because the ports are separated and each opening is at least as large as its port, as the datasheet requires. The slots run the full height of the ports, so a sensor that differs a little from the drawing still gets air.

The PMS5003 has no screws. The datasheet gives no depth for its M2 holes, and the metal case must not be opened. The sensor stands on floor ribs and is held down by lid pads through 1 mm foam tape, which also damps the fan vibration.

### Cold chamber

The SCD41 faces the side wall and the ENS160 the front wall. Partitions up to the lid separate them from the board and from the PMS5003. The only opening is the wire passage below the board, so the board heat rises away from it. Close the passage with a piece of foam tape once the wires are in. The SCD41 faces the wall farthest from the PMS5003 ports.

### Board

The board lies on shelves near the top of its bay, with the USB port in the rear wall. Windows above it let the LED light out, give access to the buttons and let warm air escape; the air comes in low on the side wall.

The windows are large because the button and LED positions are unknown. They open the whole strip of the board next to the USB port and can be made smaller once the board is measured. The light shows from above and at an angle, but not straight from the side, because the LED sits about 9 mm below the top of the lid. A device that will stand above eye level needs a light pipe to the side wall.

The board is moved towards the side wall, 5 mm away from the PMS5003 case, to keep the antenna clear. If Wi-Fi is weak inside the enclosure, connect an external antenna to the U.FL connector.

### Wires and connectors

Short wires are soldered to the boards and joined by 4-way wire-to-wire connectors, so a part can be replaced without soldering. A mated connector is about 10 × 4.5 mm and 12 mm long (estimate, JST-PH 2.0 style). The connectors sit in the sensor chamber, at places set in `WIRE_CONNECTORS` and checked for collisions. The board is replaced together with its wires, whose connector passes through the 12 × 5.5 mm passage. Dupont housings are about 14 mm tall and do not fit on the module pins, so the wires are soldered straight to the module contacts.

The wire paths are set in `WIRES` in `enclosure.py` and checked against plastic, parts and wires of other nets. The model uses a wire diameter of 1.2 mm, an estimate for 26 to 28 AWG silicone wire. The XIAO pin order follows the standard pinout of this board family; the order on the modules is a placeholder to check on the real parts.

Wires from the two sensors meet at 3V3, GND, D4 and D5, so two wires go to each of these pins and three to GND. A short splice with heat shrink under the board handles this and fits in the wire bay.

The ENS160 contact row runs along the long side of the module, so the seat holds it only by its short edges. Mount it with the contact row towards the passage, on the left when seen from the front.

The length of the PMS5003 cable is not confirmed. A cable of about 20 cm folds behind the sensor, where 3.2 cm³ of space leaves 50% to spare. Keep it below the rear snaps, or it will get in the way of the lid.

### Assembly order

1. Solder the wires and fit the connectors.
2. Put foam tape on the frame in front of the PMS5003, plug the cable into the sensor and lower it in.
3. Lay the adapter on the floor and fold the extra cable.
4. Feed the board wires through the passage, connector first, and close the passage with a piece of tape.
5. Tilt the USB port into its opening and lower the board onto the shelves.
6. Slide the SCD41 and ENS160 in from above and join the connectors.
7. Put tape on the lid pads, tilt the lid, hook the front catches into their grooves and press the rear down until it clicks.

### Lid

The lid has two rigid hooks at the front and two flexible snaps at the back. It closes by turning about the hooks: tilt it about 14°, hook it in and lower the rear. The underside of the front edge is tapered by 0.65 mm so the lid can turn, which shows as a thin shadow line. A thin tool through the windows in the rear wall releases the snaps. No screws are needed.

Printed lid down, the snap tabs bend across the print layers, so their strain is limited to 2%. The calculation gives about 2% while the lid closes, so the first print has to show whether they hold.

### Cooling, estimate

The enclosure surface is 0.0158 m². With a heat transfer coefficient of about 10 W/m²·K for free convection and radiation, a closed enclosure warms by 3 °C at 0.5 W and by 6 °C at 1 W, and the slots reduce this. That is harmless for the parts. It only shifts the temperature and humidity readings, which the temperature offset setting corrects on the assembled device. The estimate is rough, so the assembled device still needs a 2-hour comparison against an ordinary thermometer.

### Placement

Put the device at least 20 cm above the floor and out of the flow of an air purifier, as the PMS5003 datasheet requires.
