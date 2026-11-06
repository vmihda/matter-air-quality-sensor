"""Parametric printed enclosure for the air quality sensor.

Origin is the outer front-left-bottom corner. X runs to the right, Y to the back, Z up.
The sources of the part sizes and the layout decisions are in dimensions.md.
When a part is measured, change its size here; everything that depends on it follows.
"""
import json
from dataclasses import dataclass
from pathlib import Path

import cadquery as cq


@dataclass(frozen=True)
class Box:
    x0: float
    x1: float
    y0: float
    y1: float
    z0: float
    z1: float

    @property
    def volume(self) -> float:
        return (self.x1 - self.x0) * (self.y1 - self.y0) * (self.z1 - self.z0)

    def solid(self) -> cq.Workplane:
        size = (self.x1 - self.x0, self.y1 - self.y0, self.z1 - self.z0)
        return cq.Workplane("XY").box(*size, centered=False).translate((self.x0, self.y0, self.z0))


# Shell
OUTER_X, OUTER_Y, OUTER_Z = 80.0, 50.0, 30.0
WALL = 2.0
FLOOR = 2.0
LID_PLATE = 2.0
BASE_Z = OUTER_Z - LID_PLATE
CORNER_RADIUS = 3.0
INNER_X0, INNER_X1 = WALL, OUTER_X - WALL
INNER_Y0, INNER_Y1 = WALL, OUTER_Y - WALL

# Fit and print settings
CLEARANCE = 0.5
GASKET_GAP = 0.5
LID_FIT = 0.25
PARTITION_TOP_GAP = 0.3
PARTITION_THICKNESS = 1.2
RAIL = 1.0
SLOT_WIDTH = 1.6
SLOT_PITCH = 3.0
CUT_OVERSHOOT = 0.2

# PMS5003: label up, air face against the front wall
PMS_SIZE = (50.0, 38.0, 20.9)
PMS_PAD_HEIGHT = 1.0
PMS_FRAME_DEPTH = 1.0
PMS_FRAME_RIB = 1.0
PMS_FACE_Y = INNER_Y0 + PMS_FRAME_DEPTH + GASKET_GAP
PMS_X0 = INNER_X0 + CLEARANCE
PMS_Z0 = FLOOR + PMS_PAD_HEIGHT
PMS_BOX = Box(
    PMS_X0, PMS_X0 + PMS_SIZE[0],
    PMS_FACE_Y, PMS_FACE_Y + PMS_SIZE[1],
    PMS_Z0, PMS_Z0 + PMS_SIZE[2],
)
PMS_FRAME_Z0 = PMS_BOX.z0 + 0.6
PMS_FRAME_Z1 = PMS_BOX.z1
PMS_SLOT_Z0 = PMS_FRAME_Z0 + PMS_FRAME_RIB + 0.4
PMS_SLOT_Z1 = PMS_FRAME_Z1 - PMS_FRAME_RIB
PMS_SMALL_SLOT_X = PMS_X0 + 5.5
PMS_SMALL_SLOT_COUNT = 5
PMS_FAN_SLOT_X = PMS_X0 + 27.5
PMS_FAN_SLOT_COUNT = 6
PMS_PLUG_DEPTH = 6.0

# The ports are the parts of the air face that must stay open, read from the drawing.
PMS_SMALL_PORTS = Box(PMS_X0 + 4.9, PMS_X0 + 18.9, INNER_Y0, PMS_FACE_Y, PMS_BOX.z1 - 3.45, PMS_BOX.z1 - 1.45)
PMS_SMALL_PORTS_AREA = 4 * 3.14159 * 1.0**2
# The top of the fan ring is above the frame rib; that part draws through the 0.5 mm gasket gap.
PMS_FAN_PORT = Box(PMS_X0 + 26.8, PMS_X0 + 44.8, INNER_Y0, PMS_FACE_Y, PMS_BOX.z1 - 17.5, PMS_SLOT_Z1)
PMS_FAN_PORT_AREA = 3.14159 * (9.0**2 - 6.0**2)
PMS_DIVIDER_X = (PMS_SMALL_PORTS.x1 + PMS_FAN_PORT.x0) / 2

# Bay walls
PMS_WALL_X0 = PMS_BOX.x1 + CLEARANCE
PARTITION_X = PMS_WALL_X0 + PARTITION_THICKNESS
PARTITION_Y0 = 24.5
PARTITION_Y1 = PARTITION_Y0 + PARTITION_THICKNESS
BOARD_BAY_Y0 = PARTITION_Y1
# Low in the partition, where the air is coldest, so the board heat does not reach the sensors.
WIRE_PASSAGE = Box(PARTITION_X + 3.8, PARTITION_X + 11.8, PARTITION_Y0, PARTITION_Y1, FLOOR, FLOOR + 4.5)

# XIAO ESP32-C6: USB towards the back wall
XIAO_SIZE = (17.8, 21.0, 5.0)
XIAO_PCB = 1.6
XIAO_SHELF_Z = 19.0
XIAO_SHELF_DEPTH = 2.0
XIAO_WALL_GAP = 2.5
XIAO_X0 = INNER_X1 - XIAO_WALL_GAP - XIAO_SIZE[0]
XIAO_Y0 = BOARD_BAY_Y0 + CLEARANCE
XIAO_BOX = Box(
    XIAO_X0, XIAO_X0 + XIAO_SIZE[0],
    XIAO_Y0, XIAO_Y0 + XIAO_SIZE[1],
    XIAO_SHELF_Z, XIAO_SHELF_Z + XIAO_SIZE[2],
)
USB_SHELL = (9.0, 1.3, 3.2)
USB_CENTER_X = (XIAO_BOX.x0 + XIAO_BOX.x1) / 2
USB_CENTER_Z = XIAO_SHELF_Z + XIAO_PCB + USB_SHELL[2] / 2
USB_OPENING = (11.0, 6.0)
BOOT_ZONE = Box(
    XIAO_BOX.x0 + 0.8, XIAO_BOX.x0 + 3.8,
    XIAO_BOX.y1 - 3.7, XIAO_BOX.y1 - 0.7,
    XIAO_BOX.z0 + XIAO_PCB, XIAO_BOX.z0 + 3.0,
)
RESET_ZONE = Box(XIAO_BOX.x1 - 3.8, XIAO_BOX.x1 - 0.8, BOOT_ZONE.y0, BOOT_ZONE.y1, BOOT_ZONE.z0, BOOT_ZONE.z1)
BOARD_INLET_SLOT_COUNT = 6
BOARD_INLET_Z = (5.0, 15.0)

# PMS5003 adapter under the board, its plug towards the back wall
ADAPTER_SIZE = (20.0, 16.0, 6.0)
ADAPTER_PLUG = (10.0, 4.0, 5.0)
ADAPTER_X0 = USB_CENTER_X - ADAPTER_SIZE[0] / 2
ADAPTER_Y0 = BOARD_BAY_Y0 + 1.8
ADAPTER_BOX = Box(
    ADAPTER_X0, ADAPTER_X0 + ADAPTER_SIZE[0],
    ADAPTER_Y0, ADAPTER_Y0 + ADAPTER_SIZE[1],
    FLOOR, FLOOR + ADAPTER_SIZE[2],
)
ADAPTER_FENCE_Z = FLOOR + 1.5

# SCD41: stands on the floor, sensor towards the right wall
SCD41_SIZE = (13.4, 21.6)
SCD41_FRONT_DEPTH = 2.6
SCD41_REAR_DEPTH = 4.2
SCD41_SLOT_COUNT = 4
SCD41_SLOT_Z = (6.0, 22.5)
SCD41_PCB_X1 = INNER_X1 - CLEARANCE - SCD41_FRONT_DEPTH
SCD41_PCB_Y1 = PARTITION_Y0 - CLEARANCE
SCD41_PCB = Box(
    SCD41_PCB_X1 - XIAO_PCB, SCD41_PCB_X1,
    SCD41_PCB_Y1 - SCD41_SIZE[0], SCD41_PCB_Y1,
    FLOOR, FLOOR + SCD41_SIZE[1],
)
SCD41_CENTER_Y = (SCD41_PCB.y0 + SCD41_PCB.y1) / 2
SCD41_CENTER_Z = (SCD41_PCB.z0 + SCD41_PCB.z1) / 2
SCD41_SENSOR = Box(
    SCD41_PCB_X1, INNER_X1 - CLEARANCE,
    SCD41_CENTER_Y - 3.75, SCD41_CENTER_Y + 3.75,
    SCD41_CENTER_Z - 3.75, SCD41_CENTER_Z + 3.75,
)
SCD41_RAIL_Z1 = SCD41_PCB.z1 + 0.4

# ENS160: stands on the floor, sensor towards the front wall
ENS160_SIZE = (19.0, 24.0)
ENS160_FRONT_DEPTH = 2.0
ENS160_REAR_DEPTH = 2.9
ENS160_SLOT_COUNT = 5
ENS160_SLOT_Z = (6.0, 24.0)
ENS160_WALL = 1.2
ENS160_RIB_Z1 = FLOOR + 2.0
ENS160_PCB_Y0 = INNER_Y0 + CLEARANCE + ENS160_FRONT_DEPTH
ENS160_PCB_X0 = PARTITION_X + ENS160_WALL + CLEARANCE
ENS160_PCB = Box(
    ENS160_PCB_X0, ENS160_PCB_X0 + ENS160_SIZE[0],
    ENS160_PCB_Y0, ENS160_PCB_Y0 + XIAO_PCB,
    FLOOR, FLOOR + ENS160_SIZE[1],
)
ENS160_CENTER_X = (ENS160_PCB.x0 + ENS160_PCB.x1) / 2
ENS160_CENTER_Z = (ENS160_PCB.z0 + ENS160_PCB.z1) / 2
ENS160_SENSOR = Box(
    ENS160_CENTER_X - 4.5, ENS160_CENTER_X + 4.5,
    INNER_Y0 + CLEARANCE, ENS160_PCB_Y0,
    ENS160_CENTER_Z - 4.0, ENS160_CENTER_Z + 4.0,
)
ENS160_RETAINER_Z0 = ENS160_PCB.z1 - 1.5

# Lid and snaps
SNAP_WINDOW_Z = (19.8, 21.4)
SNAP_WINDOWS = (
    Box(8.0, 18.0, INNER_Y1, OUTER_Y, *SNAP_WINDOW_Z),
    Box(30.0, 40.0, INNER_Y1, OUTER_Y, *SNAP_WINDOW_Z),
)
HOOK_SPANS = ((10.0, 20.0), (34.0, 44.0))
LID_TONGUE_Z0 = 25.8
HOOK_Z = (26.0, 26.8)
LEFT_TONGUE_Y = (15.0, 35.0)
RIGHT_TONGUE_Y = (30.0, 45.0)
LID_BAR = 3.0
LID_PANE_DEPTH = 9.7
LID_EXIT_SLOTS_Y = (XIAO_BOX.y0 + 7.3, XIAO_BOX.y0 + 9.8)

PMS_PLUG = Box(
    PMS_X0 + 27.5, PMS_X0 + 44.5,
    PMS_BOX.y1, PMS_BOX.y1 + PMS_PLUG_DEPTH,
    PMS_BOX.z0 + 6.5, PMS_BOX.z0 + 13.5,
)
ADAPTER_PLUG_BOX = Box(
    USB_CENTER_X - ADAPTER_PLUG[0] / 2, USB_CENTER_X + ADAPTER_PLUG[0] / 2,
    ADAPTER_BOX.y1, ADAPTER_BOX.y1 + ADAPTER_PLUG[1],
    FLOOR + 1.0, FLOOR + 1.0 + ADAPTER_PLUG[2],
)
PMS_CABLE_ROUTE = Box(PMS_PLUG.x1, ADAPTER_PLUG_BOX.x0, PMS_BOX.y1 + 1.5, INNER_Y1 - 1.0, FLOOR + 1.0, PMS_PLUG.z1 - 0.5)

COMPONENT_BOXES: dict[str, Box] = {
    "pms5003": PMS_BOX,
    "pms5003_plug": PMS_PLUG,
    "xiao": XIAO_BOX,
    "xiao_usb": Box(
        USB_CENTER_X - USB_SHELL[0] / 2, USB_CENTER_X + USB_SHELL[0] / 2,
        XIAO_BOX.y1, XIAO_BOX.y1 + USB_SHELL[1],
        USB_CENTER_Z - USB_SHELL[2] / 2, USB_CENTER_Z + USB_SHELL[2] / 2,
    ),
    "adapter": ADAPTER_BOX,
    "adapter_plug": ADAPTER_PLUG_BOX,
    "scd41_pcb": SCD41_PCB,
    "scd41_front": Box(
        SCD41_PCB.x1, INNER_X1 - CLEARANCE,
        SCD41_PCB.y0 + 1.5, SCD41_PCB.y1 - 1.5,
        FLOOR + 0.5, SCD41_PCB.z1 - 0.5,
    ),
    "scd41_rear": Box(
        SCD41_PCB.x0 - SCD41_REAR_DEPTH, SCD41_PCB.x0,
        SCD41_PCB.y0 + 1.5, SCD41_PCB.y1 - 1.5,
        FLOOR, SCD41_PCB.z1,
    ),
    "ens160_pcb": ENS160_PCB,
    "ens160_front": Box(
        ENS160_PCB.x0 + 1.5, ENS160_PCB.x1 - 1.5,
        INNER_Y0 + CLEARANCE, ENS160_PCB.y0,
        ENS160_RIB_Z1, ENS160_RETAINER_Z0 - 0.1,
    ),
    "ens160_rear": Box(
        ENS160_PCB.x0 + 1.5, ENS160_PCB.x1 - 1.5,
        ENS160_PCB.y1, ENS160_PCB.y1 + ENS160_REAR_DEPTH,
        ENS160_RIB_Z1, ENS160_RETAINER_Z0 - 0.5,
    ),
}


def component_envelopes() -> dict[str, cq.Workplane]:
    return {name: box.solid() for name, box in COMPONENT_BOXES.items()}


def _spread(center: float, count: int) -> list[float]:
    """Positions of `count` slots centered on `center`."""
    return [center + SLOT_PITCH * (i - (count - 1) / 2) for i in range(count)]


def _slot(box: Box, axis: str) -> cq.Workplane:
    """Rounded opening; `axis` is the direction it passes through the wall."""
    selector = {"x": "|X", "y": "|Y", "z": "|Z"}[axis]
    return box.solid().edges(selector).fillet(SLOT_WIDTH / 2 - 0.01)


def _wedge(x0: float, x1: float, profile: list[tuple[float, float]]) -> cq.Workplane:
    """Prism along X from a (y, z) profile."""
    return cq.Workplane("YZ").workplane(offset=x0).polyline(profile).close().extrude(x1 - x0)


def _shell() -> cq.Workplane:
    outer = (
        cq.Workplane("XY")
        .box(OUTER_X, OUTER_Y, BASE_Z, centered=False)
        .edges("|Z")
        .fillet(CORNER_RADIUS)
        .faces("<Z")
        .edges()
        .chamfer(0.6)
    )
    return outer.cut(Box(INNER_X0, INNER_X1, INNER_Y0, INNER_Y1, FLOOR, BASE_Z + 1).solid())


def _pms_frame_and_seats() -> list[Box]:
    pms = PMS_BOX
    y0, y1 = INNER_Y0, INNER_Y0 + PMS_FRAME_DEPTH
    stop_y0 = pms.y1 + CLEARANCE
    stop_z1 = FLOOR + 12.0
    return [
        # Frame that seals the air face to the front wall, with a rib between the ports
        Box(INNER_X0, pms.x0 + PMS_FRAME_RIB, y0, y1, PMS_FRAME_Z0, PMS_FRAME_Z1),
        Box(pms.x1 - PMS_FRAME_RIB, PMS_WALL_X0, y0, y1, PMS_FRAME_Z0, PMS_FRAME_Z1),
        Box(PMS_DIVIDER_X - 0.6, PMS_DIVIDER_X + 0.6, y0, y1, PMS_FRAME_Z0, PMS_FRAME_Z1),
        Box(INNER_X0, PMS_WALL_X0, y0, y1, PMS_FRAME_Z0, PMS_FRAME_Z0 + PMS_FRAME_RIB),
        Box(INNER_X0, PMS_WALL_X0, y0, y1, PMS_FRAME_Z1 - PMS_FRAME_RIB, PMS_FRAME_Z1),
        # Floor pads
        Box(pms.x0 + 3.5, pms.x0 + 6.5, pms.y0 + 1.5, pms.y1 - 1.5, FLOOR, pms.z0),
        Box(pms.x1 - 6.5, pms.x1 - 3.5, pms.y0 + 1.5, pms.y1 - 1.5, FLOOR, pms.z0),
        # Rear stops, clear of the connector
        Box(pms.x0 + 2.5, pms.x0 + 12.5, stop_y0, stop_y0 + 1.2, FLOOR, stop_z1),
        Box(pms.x0 + 17.5, pms.x0 + 25.5, stop_y0, stop_y0 + 1.2, FLOOR, stop_z1),
    ]


def _chamber_walls() -> list[Box]:
    top = BASE_Z - PARTITION_TOP_GAP
    return [
        Box(PMS_WALL_X0, PARTITION_X, INNER_Y0, PARTITION_Y1, FLOOR, top),
        Box(PARTITION_X, INNER_X1, PARTITION_Y0, PARTITION_Y1, FLOOR, top),
    ]


def _scd41_rails() -> list[Box]:
    """Open at the top so the module slides in."""
    pcb = SCD41_PCB
    lip_x0, lip_x1 = pcb.x0 - CLEARANCE - RAIL, pcb.x0 - CLEARANCE
    rib_x0 = pcb.x1 + CLEARANCE
    edge_y0 = pcb.y0 - CLEARANCE
    top = SCD41_RAIL_Z1
    return [
        Box(lip_x0, INNER_X1, edge_y0 - RAIL, edge_y0, FLOOR, top),
        Box(rib_x0, INNER_X1, edge_y0, pcb.y0 + RAIL, FLOOR, top),
        Box(lip_x0, lip_x1, edge_y0, pcb.y0 + RAIL, FLOOR, top),
        Box(rib_x0, INNER_X1, pcb.y1 - RAIL, PARTITION_Y0, FLOOR, top),
        Box(lip_x0, lip_x1, pcb.y1 - RAIL, PARTITION_Y0, FLOOR, top),
    ]


def _ens160_seat() -> list[Box]:
    """Side walls and short ribs only: the long edges of the module stay free for its contact row."""
    pcb = ENS160_PCB
    rib_y1 = pcb.y0 - CLEARANCE
    lip_y0 = pcb.y1 + CLEARANCE
    lip_y1 = lip_y0 + RAIL
    return [
        Box(PARTITION_X, pcb.x0 - CLEARANCE, INNER_Y0, lip_y1, FLOOR, pcb.z1),
        Box(pcb.x1 + CLEARANCE, pcb.x1 + CLEARANCE + ENS160_WALL, INNER_Y0, lip_y1, FLOOR, pcb.z1),
        Box(pcb.x0 - CLEARANCE, pcb.x0 + RAIL, INNER_Y0, rib_y1, FLOOR, pcb.z1),
        Box(pcb.x1 - RAIL, pcb.x1 + CLEARANCE, INNER_Y0, rib_y1, FLOOR, pcb.z1),
        Box(pcb.x0 + 1.1, pcb.x1 - 1.1, INNER_Y0, rib_y1, FLOOR, ENS160_RIB_Z1),
        Box(pcb.x0 + 1.1, pcb.x1 - 1.1, lip_y0, lip_y1, FLOOR, ENS160_RIB_Z1),
    ]


def _adapter_fences() -> list[Box]:
    box = ADAPTER_BOX
    return [
        Box(box.x0 + 0.4, box.x1 - 0.6, box.y0 - CLEARANCE - RAIL, box.y0 - CLEARANCE, FLOOR, ADAPTER_FENCE_Z),
        Box(box.x0 - CLEARANCE - RAIL, box.x0 - CLEARANCE, box.y0 + CLEARANCE, box.y1 - 1.5, FLOOR, ADAPTER_FENCE_Z),
        Box(box.x1 + CLEARANCE, INNER_X1, box.y0 + CLEARANCE, box.y1 - CLEARANCE, FLOOR, ADAPTER_FENCE_Z),
    ]


def _board_stops() -> list[Box]:
    """Side stops for the board, carried by the partition and the back wall."""
    board = XIAO_BOX
    left = (board.x0 - CLEARANCE - RAIL, board.x0 - CLEARANCE)
    right = (board.x1 + CLEARANCE, board.x1 + CLEARANCE + RAIL)
    z0, z1 = XIAO_SHELF_Z - 1.0, XIAO_SHELF_Z + XIAO_PCB
    return [
        Box(*x_span, *y_span, z0, z1)
        for x_span in (left, right)
        for y_span in ((BOARD_BAY_Y0, board.y0 + 2.3), (board.y1 - 2.2, INNER_Y1))
    ]


def _board_shelves() -> list[cq.Workplane]:
    """45 degree undersides print without support."""
    x0, x1 = XIAO_BOX.x0 - CLEARANCE - RAIL, XIAO_BOX.x1 + CLEARANCE + RAIL
    top, depth = XIAO_SHELF_Z, XIAO_SHELF_DEPTH
    return [
        _wedge(x0, x1, [(wall_y, top - depth), (wall_y + sign * depth, top), (wall_y, top)])
        for wall_y, sign in ((BOARD_BAY_Y0, 1), (INNER_Y1, -1))
    ]


def _base_openings() -> list[cq.Workplane]:
    """Cutters overshoot the outer face and go slightly past the inner one to avoid coplanar faces."""
    half = SLOT_WIDTH / 2
    inner_front = INNER_Y0 + CUT_OVERSHOOT
    inner_right = INNER_X1 - CUT_OVERSHOOT
    outside_right = OUTER_X + 1

    def front_slots(positions: list[float], z_span: tuple[float, float]) -> list[cq.Workplane]:
        return [_slot(Box(x - half, x + half, -1, inner_front, *z_span), "y") for x in positions]

    def right_slots(positions: list[float], z_span: tuple[float, float]) -> list[cq.Workplane]:
        return [_slot(Box(inner_right, outside_right, y - half, y + half, *z_span), "x") for y in positions]

    pms_z = (PMS_SLOT_Z0, PMS_SLOT_Z1)
    usb_x, usb_z = USB_OPENING[0] / 2, USB_OPENING[1] / 2
    usb = Box(
        USB_CENTER_X - usb_x, USB_CENTER_X + usb_x,
        INNER_Y1 - CUT_OVERSHOOT, OUTER_Y + 1,
        USB_CENTER_Z - usb_z, USB_CENTER_Z + usb_z,
    )
    return [
        *front_slots([PMS_SMALL_SLOT_X + SLOT_PITCH * i for i in range(PMS_SMALL_SLOT_COUNT)], pms_z),
        *front_slots([PMS_FAN_SLOT_X + SLOT_PITCH * i for i in range(PMS_FAN_SLOT_COUNT)], pms_z),
        *front_slots(_spread(ENS160_CENTER_X, ENS160_SLOT_COUNT), ENS160_SLOT_Z),
        *right_slots(_spread(SCD41_CENTER_Y, SCD41_SLOT_COUNT), SCD41_SLOT_Z),
        *right_slots(_spread((BOARD_BAY_Y0 + INNER_Y1) / 2, BOARD_INLET_SLOT_COUNT), BOARD_INLET_Z),
        usb.solid().edges("|Y").fillet(1.0),
        WIRE_PASSAGE.solid(),
        *[Box(w.x0, w.x1, w.y0 - CUT_OVERSHOOT, w.y1 + 1, w.z0, w.z1).solid() for w in SNAP_WINDOWS],
        # Grooves for the rigid front hooks of the lid
        *[
            Box(x0 - 0.5, x1 + 0.5, INNER_Y0 - 0.6, INNER_Y0 + CUT_OVERSHOOT, LID_TONGUE_Z0 + 0.05, HOOK_Z[1] + 0.2).solid()
            for x0, x1 in HOOK_SPANS
        ],
    ]


def build_base() -> cq.Workplane:
    base = _shell()
    boxes = [
        *_pms_frame_and_seats(),
        *_chamber_walls(),
        *_scd41_rails(),
        *_ens160_seat(),
        *_adapter_fences(),
        *_board_stops(),
    ]
    for box in boxes:
        base = base.union(box.solid(), clean=False)
    for shelf in _board_shelves():
        base = base.union(shelf, clean=False)
    for opening in _base_openings():
        base = base.cut(opening, clean=False)
    return base.clean()


def _lid_structure() -> list[Box]:
    pms, board, scd, ens = PMS_BOX, XIAO_BOX, SCD41_PCB, ENS160_PCB
    top = BASE_Z
    pad_z0 = pms.z1 + CLEARANCE
    finger_z0 = board.z1 + CLEARANCE
    finger_x = (USB_CENTER_X - LID_BAR / 2, USB_CENTER_X + LID_BAR / 2)
    tongue = RAIL
    return [
        # Pads that hold the PMS5003 through foam tape
        Box(pms.x0 + 5.5, pms.x0 + 8.5, pms.y0 + 2.5, pms.y1 - 3.5, pad_z0, top),
        Box(pms.x1 - 8.5, pms.x1 - 5.5, pms.y0 + 2.5, pms.y1 - 3.5, pad_z0, top),
        # Fingers that keep the board on its shelves: on the USB shell and on the far end
        Box(*finger_x, board.y1 - 9.2, board.y1 - 0.7, finger_z0, top),
        Box(*finger_x, board.y0 + 1.8, board.y0 + 5.8, finger_z0, top),
        # Retainers above the sensor modules
        Box(scd.x0 - 0.3, scd.x1 + 0.1, scd.y0 + 1.4, scd.y1 - 2.0, scd.z1 + CLEARANCE, top),
        Box(ens.x0 + 1.1, ens.x1 - 1.1, INNER_Y0 + LID_FIT, ens.y0 - CLEARANCE, ENS160_RETAINER_Z0, top),
        Box(ens.x0 + 1.1, ens.x1 - 1.1, ens.y1 + CLEARANCE, ens.y1 + CLEARANCE + RAIL, ENS160_RETAINER_Z0, top),
        # Location tongues along the side walls
        Box(INNER_X0 + LID_FIT, INNER_X0 + LID_FIT + tongue, *LEFT_TONGUE_Y, LID_TONGUE_Z0, top),
        Box(INNER_X1 - LID_FIT - tongue, INNER_X1 - LID_FIT, *RIGHT_TONGUE_Y, LID_TONGUE_Z0, top),
    ]


def _lid_catches() -> list[cq.Workplane]:
    top = BASE_Z
    front_y = INNER_Y0 + LID_FIT
    back_y = INNER_Y1 - LID_FIT
    catches: list[cq.Workplane] = []
    for x0, x1 in HOOK_SPANS:
        catches.append(Box(x0, x1, front_y, front_y + RAIL, LID_TONGUE_Z0, top).solid())
        catches.append(Box(x0, x1, INNER_Y0 - 0.4, front_y, *HOOK_Z).solid())
    for window in SNAP_WINDOWS:
        x0, x1 = window.x0 + 0.5, window.x1 - 0.5
        z = window.z0 + 0.2
        catches.append(Box(x0, x1, back_y - 1.2, back_y, window.z0 - 0.8, top).solid())
        # The ramp faces down so the snap deflects as the lid closes.
        catches.append(_wedge(x0, x1, [(back_y, z), (back_y + 0.75, z + 0.8), (back_y + 0.75, z + 1.0), (back_y, z + 1.0)]))
    return catches


def _lid_openings() -> list[cq.Workplane]:
    z0, z1 = BASE_Z - CUT_OVERSHOOT, OUTER_Z + 1
    pane_y0 = XIAO_BOX.y1 - LID_PANE_DEPTH
    pane_y1 = BOOT_ZONE.y1 + 1.0
    panes = [
        Box(BOOT_ZONE.x0 - 1.0, USB_CENTER_X - LID_BAR / 2, pane_y0, pane_y1, z0, z1),
        Box(USB_CENTER_X + LID_BAR / 2, RESET_ZONE.x1 + 1.0, pane_y0, pane_y1, z0, z1),
    ]
    half = SLOT_WIDTH / 2
    slots = [Box(XIAO_BOX.x0 + 1.5, XIAO_BOX.x1 - 1.5, y - half, y + half, z0, z1) for y in LID_EXIT_SLOTS_Y]
    return [pane.solid().edges("|Z").fillet(1.0) for pane in panes] + [_slot(slot, "z") for slot in slots]


def build_lid() -> cq.Workplane:
    lid = (
        cq.Workplane("XY")
        .box(OUTER_X, OUTER_Y, LID_PLATE, centered=False)
        .edges("|Z")
        .fillet(CORNER_RADIUS)
        .faces(">Z")
        .edges()
        .fillet(0.8)
        .translate((0, 0, BASE_Z))
    )
    for box in _lid_structure():
        lid = lid.union(box.solid(), clean=False)
    for catch in _lid_catches():
        lid = lid.union(catch, clean=False)
    for opening in _lid_openings():
        lid = lid.cut(opening, clean=False)
    return lid.clean()


def build_assembly() -> cq.Assembly:
    assembly = cq.Assembly(name="air_quality_sensor")
    assembly.add(build_base(), name="base", color=cq.Color(0.92, 0.92, 0.9))
    assembly.add(build_lid(), name="lid", color=cq.Color(0.8, 0.82, 0.85))
    for name, envelope in component_envelopes().items():
        assembly.add(envelope, name=name, color=cq.Color(0.2, 0.45, 0.75))
    return assembly


def export_all(out_dir: Path) -> list[Path]:
    out_dir.mkdir(parents=True, exist_ok=True)
    written: list[Path] = []
    for name, part in (("base", build_base()), ("lid", build_lid())):
        for extension in ("step", "stl"):
            path = out_dir / f"{name}.{extension}"
            cq.exporters.export(part, str(path), tolerance=0.02, angularTolerance=0.1)
            written.append(path)
    assembly_path = out_dir / "assembly.step"
    build_assembly().export(str(assembly_path))
    written.append(assembly_path)
    return written


def _mesh(part: cq.Workplane) -> dict[str, list[float] | list[int]]:
    vertices, triangles = part.val().tessellate(0.03, 0.2)
    return {
        "vertices": [round(value, 2) for vertex in vertices for value in vertex.toTuple()],
        "triangles": [index for triangle in triangles for index in triangle],
    }


def mesh_data() -> dict[str, dict]:
    """Geometry for the browser preview in virtual/3d-layout.html."""
    return {
        "parts": {"base": _mesh(build_base()), "lid": _mesh(build_lid())},
        "components": {
            name: [box.x0, box.x1, box.y0, box.y1, box.z0, box.z1] for name, box in COMPONENT_BOXES.items()
        },
    }


def write_viewer_mesh(path: Path) -> None:
    path.write_text(f"window.ENCLOSURE_MESH={json.dumps(mesh_data(), separators=(',', ':'))};\n", encoding="utf-8")


if __name__ == "__main__":
    for exported in export_all(Path(__file__).parent / "out"):
        print(exported)
    viewer_mesh = Path(__file__).parent.parent / "virtual" / "enclosure-mesh.js"
    write_viewer_mesh(viewer_mesh)
    print(viewer_mesh)
