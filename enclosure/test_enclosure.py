"""Geometry checks for the printed enclosure. All volumes are measured on the real solids."""
import math
from functools import reduce
from itertools import combinations
from pathlib import Path

import cadquery as cq
import pytest

import enclosure as enc

EMPTY = 1e-3  # mm^3, below the noise of the boolean operations


@pytest.fixture(scope="module")
def base() -> cq.Workplane:
    return enc.build_base()


@pytest.fixture(scope="module")
def lid() -> cq.Workplane:
    return enc.build_lid()


@pytest.fixture(scope="module")
def envelopes() -> dict[str, cq.Workplane]:
    return enc.component_envelopes()


def overlap(a: cq.Workplane, b: cq.Workplane) -> float:
    return sum(solid.Volume() for solid in a.intersect(b).solids().vals())


def solid_in(part: cq.Workplane, box: enc.Box) -> float:
    return overlap(part, box.solid())


def open_area_through_wall(part: cq.Workplane, region: enc.Box, thickness: float) -> float:
    """Open area of a wall inside a region, given the region spans exactly the wall thickness."""
    return (region.volume - solid_in(part, region)) / thickness


def test_outer_size_fits_the_agreed_limit(base: cq.Workplane, lid: cq.Workplane) -> None:
    box = base.union(lid).val().BoundingBox()
    assert box.xlen <= 80 + 1e-6
    assert box.ylen <= 50 + 1e-6
    assert box.zlen <= 30 + 1e-6


@pytest.mark.parametrize("name", ["base", "lid"])
def test_printed_part_is_one_valid_solid(name: str, request: pytest.FixtureRequest) -> None:
    part: cq.Workplane = request.getfixturevalue(name)
    assert len(part.solids().vals()) == 1
    assert part.val().isValid()


def test_lid_does_not_collide_with_base(base: cq.Workplane, lid: cq.Workplane) -> None:
    assert overlap(base, lid) < EMPTY


@pytest.mark.parametrize("component", sorted(enc.component_envelopes()))
def test_component_fits_without_touching_plastic(
    component: str, base: cq.Workplane, lid: cq.Workplane, envelopes: dict[str, cq.Workplane]
) -> None:
    assert overlap(envelopes[component], base) < EMPTY
    assert overlap(envelopes[component], lid) < EMPTY


def test_components_do_not_collide_with_each_other(envelopes: dict[str, cq.Workplane]) -> None:
    collisions = [
        (first, second)
        for first, second in combinations(sorted(envelopes), 2)
        if overlap(envelopes[first], envelopes[second]) >= EMPTY
    ]
    assert collisions == []


@pytest.mark.parametrize(
    ("port", "port_area"),
    [(enc.PMS_SMALL_PORTS, enc.PMS_SMALL_PORTS_AREA), (enc.PMS_FAN_PORT, enc.PMS_FAN_PORT_AREA)],
    ids=["small_holes", "fan"],
)
def test_pms_port_opening_is_not_smaller_than_the_port(
    port: enc.Box, port_area: float, base: cq.Workplane
) -> None:
    # Datasheet: the blowhole of the host must not be smaller than the sensor inlet.
    wall = enc.Box(port.x0, port.x1, 0, enc.WALL, port.z0, port.z1)
    assert open_area_through_wall(base, wall, enc.WALL) >= port_area


def test_pms_ports_match_the_drawing() -> None:
    # Numbers come from the datasheet drawing, measured from the left and top of the air face.
    body = enc.PMS_BOX
    fan = enc.PMS_FAN_PORT
    assert (fan.x0 + fan.x1) / 2 - body.x0 == pytest.approx(35.8)
    assert fan.x1 - fan.x0 == pytest.approx(18.0)
    assert body.z1 - fan.z1 == pytest.approx(20.9 / 2 - 9.0)
    assert body.z1 - fan.z0 == pytest.approx(20.9 / 2 + 9.0)
    small = enc.PMS_SMALL_PORTS
    assert small.x0 - body.x0 == pytest.approx(6.0 - 1.0)
    assert small.x1 - body.x0 == pytest.approx(18.0 + 1.0)
    assert body.z1 - (small.z0 + small.z1) / 2 == pytest.approx(2.45)


def test_pms_cavity_in_front_of_the_ports_is_clear(base: cq.Workplane) -> None:
    for port in (enc.PMS_SMALL_PORTS, enc.PMS_FAN_PORT):
        assert solid_in(base, enc.Box(port.x0, port.x1, enc.WALL, enc.PMS_FACE_Y, port.z0, port.z1)) < EMPTY


def test_pms_ports_are_divided_by_a_rib(base: cq.Workplane) -> None:
    # A rib that spans the cavity height is what keeps the air from going in a circle.
    x = (enc.PMS_SMALL_PORTS.x1 + enc.PMS_FAN_PORT.x0) / 2
    rib = enc.Box(
        x - 0.2,
        x + 0.2,
        enc.WALL,
        enc.PMS_FACE_Y - enc.GASKET_GAP,
        enc.PMS_BOX.z0 + 1.6,
        enc.PMS_BOX.z1 - 1,
    )
    assert solid_in(base, rib) == pytest.approx(rib.volume, rel=1e-3)


def test_pms_gasket_tape_closes_the_gap_at_the_rib_and_has_a_seat() -> None:
    # The rib stops short of the sensor, so without compressed tape the two ports are connected.
    assert enc.GASKET_TAPE >= enc.GASKET_GAP + 0.3
    assert enc.PMS_DIVIDER_WIDTH >= 2.5


def test_gasket_strip_fills_the_gap_between_the_ports() -> None:
    x = (enc.PMS_SMALL_PORTS.x1 + enc.PMS_FAN_PORT.x0) / 2
    gap = enc.Box(x - 0.2, x + 0.2, enc.PMS_FACE_Y - enc.GASKET_GAP, enc.PMS_FACE_Y, enc.PMS_BOX.z0 + 1.6, enc.PMS_BOX.z1 - 1)
    strip = enc.ASSEMBLY_AIDS["tape_frame_divider"].solid()
    assert overlap(strip, gap.solid()) == pytest.approx(gap.volume, rel=1e-3)


@pytest.mark.parametrize("name", sorted(enc.ASSEMBLY_AIDS))
def test_tape_or_sleeve_fits_without_pressing_on_plastic_or_parts(
    name: str, base: cq.Workplane, lid: cq.Workplane, envelopes: dict[str, cq.Workplane]
) -> None:
    aid = enc.ASSEMBLY_AIDS[name]
    assert solid_in(base, aid) < EMPTY
    assert solid_in(lid, aid) < EMPTY
    assert all(overlap(part, aid.solid()) < EMPTY for part in envelopes.values())


@pytest.mark.parametrize(("net", "pin"), [("3V3", "3V3"), ("GND", "GND"), ("SDA", "D4"), ("SCL", "D5")])
def test_splice_sleeve_sits_on_the_shared_pad_and_clears_other_wires(net: str, pin: str) -> None:
    sleeve = enc.ASSEMBLY_AIDS[f"sleeve_{net.lower()}"]
    pad_x, pad_y = enc.XIAO_PADS[pin]
    assert (sleeve.x0 + sleeve.x1) / 2 == pytest.approx(pad_x)
    assert (sleeve.y0 + sleeve.y1) / 2 == pytest.approx(pad_y)
    hits = [
        wire.name
        for wire in enc.WIRES
        if wire.net != net
        and overlap(reduce(lambda joined, part: joined.union(part), (b.solid() for b in enc.wire_segments(wire))), sleeve.solid())
        >= EMPTY
    ]
    assert hits == []


def test_usb_opening_clears_the_plug(base: cq.Workplane) -> None:
    # Metal of a USB-C plug is 8.4 x 2.6 mm; positions are estimates, so 1 mm of slack on each side.
    x, z = enc.USB_CENTER_X, enc.USB_CENTER_Z
    plug = enc.Box(x - 5.2, x + 5.2, enc.OUTER_Y - enc.WALL - 0.5, enc.OUTER_Y, z - 2.3, z + 2.3)
    assert solid_in(base, plug) < EMPTY


@pytest.mark.parametrize(
    "zone",
    [enc.BOOT_ZONE, enc.RESET_ZONE],
    ids=["boot", "reset"],
)
def test_button_is_reachable_from_above(zone: enc.Box, lid: cq.Workplane) -> None:
    above = enc.Box(zone.x0, zone.x1, zone.y0, zone.y1, zone.z1, enc.OUTER_Z)
    assert solid_in(lid, above) < EMPTY


@pytest.mark.parametrize("side", enc.LED_POSSIBLE_ZONES, ids=["left_of_usb", "right_of_usb"])
def test_led_light_reaches_outside_wherever_it_sits_beside_the_usb(side: enc.Box, lid: cq.Workplane) -> None:
    # The LED position is not published, so the whole band beside the USB connector must see out.
    above = enc.Box(side.x0, side.x1, side.y0, side.y1, side.z1, enc.OUTER_Z)
    assert solid_in(lid, above) < EMPTY


@pytest.mark.parametrize("name", sorted(enc.XIAO_DETAILS))
def test_board_detail_stays_inside_the_board_envelope(name: str) -> None:
    detail = enc.XIAO_DETAILS[name]
    usb = enc.COMPONENT_BOXES["xiao_usb"]
    board = enc.XIAO_BOX
    assert board.x0 <= detail.x0 and detail.x1 <= board.x1
    assert board.y0 <= detail.y0 and detail.y1 <= usb.y1
    assert board.z0 <= detail.z0 and detail.z1 <= board.z1


def test_board_has_air_inlet_low_and_outlet_high(base: cq.Workplane, lid: cq.Workplane) -> None:
    inlet = enc.Box(enc.OUTER_X - enc.WALL, enc.OUTER_X, enc.BOARD_BAY_Y0, enc.INNER_Y1, 3, enc.XIAO_SHELF_Z)
    outlet = enc.Box(enc.XIAO_BOX.x0, enc.XIAO_BOX.x1, enc.XIAO_BOX.y0, enc.XIAO_BOX.y1, enc.BASE_Z, enc.OUTER_Z)
    assert open_area_through_wall(base, inlet, enc.WALL) >= 30
    assert open_area_through_wall(lid, outlet, enc.LID_PLATE) >= 30


@pytest.mark.parametrize(
    ("sensor", "wall_axis"),
    [(enc.SCD41_SENSOR, "x"), (enc.ENS160_SENSOR, "y")],
    ids=["scd41", "ens160"],
)
def test_sensor_faces_openings_in_the_outer_wall(sensor: enc.Box, wall_axis: str, base: cq.Workplane) -> None:
    wall = (
        enc.Box(enc.OUTER_X - enc.WALL, enc.OUTER_X, sensor.y0, sensor.y1, sensor.z0, sensor.z1)
        if wall_axis == "x"
        else enc.Box(sensor.x0, sensor.x1, 0, enc.WALL, sensor.z0, sensor.z1)
    )
    assert open_area_through_wall(base, wall, enc.WALL) >= 20


@pytest.mark.parametrize(
    "sensor",
    [enc.SCD41_SENSOR, enc.ENS160_SENSOR],
    ids=["scd41", "ens160"],
)
def test_sensor_is_walled_off_from_the_board(sensor: enc.Box, base: cq.Workplane) -> None:
    # Above and below the wire passage the partition is continuous across the whole bay.
    for z0, z1 in (
        (enc.FLOOR, enc.WIRE_PASSAGE.z0),
        (enc.WIRE_PASSAGE.z1, enc.BASE_Z - enc.PARTITION_TOP_GAP),
    ):
        partition = enc.Box(enc.PARTITION_X, enc.INNER_X1, enc.PARTITION_Y0, enc.PARTITION_Y1, z0, z1)
        assert solid_in(base, partition) == pytest.approx(partition.volume, rel=1e-3)
    assert sensor.y1 <= enc.PARTITION_Y0


def test_partition_is_thick_enough_to_print() -> None:
    # Two perimeters of a 0.4 mm nozzle plus a margin.
    assert enc.PARTITION_Y1 - enc.PARTITION_Y0 >= 1.0
    assert enc.PARTITION_X - enc.PMS_WALL_X0 >= 1.0


def test_scd41_wall_is_away_from_the_pms_ports() -> None:
    # The SCD41 reads temperature, so it must not breathe air warmed by the PMS5003.
    assert enc.SCD41_SENSOR.x0 - enc.PMS_FAN_PORT.x1 >= 25


def test_wire_passage_is_open_and_lets_a_connector_through(base: cq.Workplane) -> None:
    passage = enc.WIRE_PASSAGE
    assert solid_in(base, passage) < EMPTY
    assert passage.x1 - passage.x0 >= enc.WIRE_CONNECTOR_SIZE[0] + 1
    assert passage.z1 - passage.z0 >= enc.WIRE_CONNECTOR_SIZE[1] + 0.5


def test_wire_passage_runs_above_the_adapter_and_below_the_board() -> None:
    assert enc.WIRE_PASSAGE.z0 >= enc.ADAPTER_BOX.z1
    assert enc.WIRE_PASSAGE.z1 <= enc.XIAO_SHELF_Z - enc.XIAO_SHELF_DEPTH


def test_wire_room_is_clear(base: cq.Workplane, lid: cq.Workplane, envelopes: dict[str, cq.Workplane]) -> None:
    for room in (enc.SENSOR_WIRE_ROOM, enc.BOARD_WIRE_ROOM, enc.PMS_CABLE_ROUTE, enc.PMS_CABLE_SLACK):
        assert solid_in(base, room) < EMPTY
        assert solid_in(lid, room) < EMPTY
    for room in (enc.SENSOR_WIRE_ROOM, enc.BOARD_WIRE_ROOM):
        for name, envelope in envelopes.items():
            assert overlap(envelope, room.solid()) < EMPTY, name


def test_pms_cable_slack_volume_is_enough_for_a_long_factory_cable() -> None:
    # 200 mm of 8-way flat cable, about 10 x 1 mm, with half as much again for folding.
    assert enc.PMS_CABLE_SLACK.volume >= 1.5 * 200 * 10 * 1


@pytest.mark.parametrize("name", sorted(enc.WIRE_CONNECTORS))
def test_placed_connector_fits_in_the_sensor_room(
    name: str, base: cq.Workplane, lid: cq.Workplane, envelopes: dict[str, cq.Workplane]
) -> None:
    box = enc.WIRE_CONNECTORS[name]
    room = enc.SENSOR_WIRE_ROOM
    assert room.x0 <= box.x0 and box.x1 <= room.x1
    assert room.y0 <= box.y0 and box.y1 <= room.y1
    assert room.z0 <= box.z0 and box.z1 <= room.z1
    size = sorted((box.x1 - box.x0, box.y1 - box.y0, box.z1 - box.z0))
    assert size == pytest.approx(sorted((*enc.WIRE_CONNECTOR_SIZE, enc.WIRE_CONNECTOR_MATED)))
    assert solid_in(base, box) < EMPTY
    assert solid_in(lid, box) < EMPTY
    assert all(overlap(part, box.solid()) < EMPTY for part in envelopes.values())


@pytest.mark.parametrize("name", sorted(enc.WIRE_CONNECTORS))
def test_placed_connector_does_not_press_on_wires_of_the_other_sensor(name: str) -> None:
    box = enc.WIRE_CONNECTORS[name].solid()
    hits = [
        wire.name
        for wire in enc.WIRES
        if not wire.name.startswith(name)
        and overlap(reduce(lambda joined, part: joined.union(part), (b.solid() for b in enc.wire_segments(wire))), box)
        >= EMPTY
    ]
    assert hits == []


def test_placed_connectors_do_not_collide() -> None:
    first, second = enc.WIRE_CONNECTORS.values()
    assert overlap(first.solid(), second.solid()) < EMPTY


@pytest.mark.parametrize(
    "edge",
    [
        enc.Box(enc.ENS160_PCB.x0, enc.ENS160_PCB.x0 + 2.6, enc.WALL, enc.ENS160_PCB.y0, enc.ENS160_RIB_Z1, enc.ENS160_RETAINER_Z0),
        enc.Box(enc.ENS160_PCB.x1 - 2.6, enc.ENS160_PCB.x1, enc.WALL, enc.ENS160_PCB.y0, enc.ENS160_RIB_Z1, enc.ENS160_RETAINER_Z0),
    ],
    ids=["left", "right"],
)
def test_ens160_contact_row_edge_is_free_on_the_solder_side(edge: enc.Box, base: cq.Workplane) -> None:
    assert solid_in(base, edge) < EMPTY


@pytest.mark.parametrize("wire", enc.WIRES, ids=lambda wire: wire.name)
def test_wire_does_not_pass_through_plastic_or_parts(
    wire: enc.Wire, base: cq.Workplane, lid: cq.Workplane, envelopes: dict[str, cq.Workplane]
) -> None:
    path = reduce(lambda joined, box: joined.union(box), (box.solid() for box in enc.wire_segments(wire)))
    hits = {
        name: round(volume, 3)
        for name, part in {"base": base, "lid": lid, **envelopes}.items()
        if (volume := overlap(part, path)) >= EMPTY
    }
    assert hits == {}


def test_wires_of_different_nets_do_not_cross() -> None:
    paths = {
        wire.name: reduce(lambda joined, box: joined.union(box), (box.solid() for box in enc.wire_segments(wire)))
        for wire in enc.WIRES
    }
    nets = {wire.name: wire.net for wire in enc.WIRES}
    crossings = [
        (first, second)
        for first, second in combinations(paths, 2)
        if nets[first] != nets[second] and overlap(paths[first], paths[second]) >= EMPTY
    ]
    assert crossings == []


def test_every_connection_from_the_wiring_table_has_a_wire() -> None:
    names = {wire.name for wire in enc.WIRES}
    assert {
        "scd41_vcc", "scd41_gnd", "scd41_sda", "scd41_scl",
        "ens160_vin", "ens160_gnd", "ens160_sda", "ens160_scl",
        "pms_5v", "pms_gnd", "pms_tx", "pms_rx", "pms_cable",
    } <= names


def test_rear_snap_windows_are_open(base: cq.Workplane) -> None:
    for window in enc.SNAP_WINDOWS:
        assert solid_in(base, window) < EMPTY


def tilted_lid(lid: cq.Workplane, degrees: float) -> cq.Workplane:
    pivot_y, pivot_z = enc.HOOK_PIVOT
    return lid.rotate((0, pivot_y, pivot_z), (1, pivot_y, pivot_z), degrees)


def tilted_point(y: float, z: float, degrees: float) -> tuple[float, float]:
    pivot_y, pivot_z = enc.HOOK_PIVOT
    angle = math.radians(degrees)
    dy, dz = y - pivot_y, z - pivot_z
    return (
        pivot_y + dy * math.cos(angle) - dz * math.sin(angle),
        pivot_z + dy * math.sin(angle) + dz * math.cos(angle),
    )


def position_when_crossing_the_rim(y: float, z: float) -> float:
    """Y of a lid point at the moment it comes down through the top of the base while the lid closes."""
    for tenth in range(300, -1, -1):
        crossing_y, crossing_z = tilted_point(y, z, tenth / 10)
        if crossing_z <= enc.BASE_Z:
            return crossing_y
    raise AssertionError("the point never reaches the rim")


@pytest.mark.parametrize("degrees", [2, 5, 8, 10, 12, 14])
def test_lid_front_clears_the_wall_while_it_closes(degrees: float, base: cq.Workplane, lid: cq.Workplane) -> None:
    front_half = enc.Box(-5, enc.OUTER_X + 5, -5, enc.OUTER_Y / 2, 15, 40).solid()
    assert overlap(tilted_lid(lid, degrees).intersect(front_half), base) < EMPTY


def test_front_hook_stays_under_the_rim_when_closed(base: cq.Workplane) -> None:
    x0, x1 = enc.HOOK_SPANS[0]
    above_hook = enc.Box(x0, x1, enc.INNER_Y0 - 0.3, enc.INNER_Y0, enc.HOOK_Z[1] + 0.3, enc.BASE_Z)
    assert solid_in(base, above_hook) == pytest.approx(above_hook.volume, rel=1e-3)


def test_rear_snap_tip_enters_the_base_without_catching_on_the_rim() -> None:
    assert position_when_crossing_the_rim(*enc.SNAP_TIP) <= enc.INNER_Y1


def test_rear_snap_bending_strain_is_low_while_the_lid_closes() -> None:
    # Cantilever strain 1.5 * t * deflection / L^2, with L from the root to the point that takes the load.
    # The tab bends across print layers, so 2% is the limit.
    deflection = position_when_crossing_the_rim(*enc.SNAP_BUMP_TIP) - enc.INNER_Y1
    root_to_contact = enc.BASE_Z - enc.SNAP_BUMP_TIP[1]
    strain = 1.5 * enc.SNAP_THICKNESS * deflection / root_to_contact**2
    assert 0 < strain <= 0.02


def test_rear_snap_engagement_in_the_wall_is_kept() -> None:
    assert enc.SNAP_BUMP_TIP[0] - enc.INNER_Y1 >= 0.5


def test_mesh_data_covers_printed_parts_and_components() -> None:
    data = enc.mesh_data()

    assert data["parts"].keys() == {"base", "lid"}
    assert data["components"].keys() == enc.COMPONENT_BOXES.keys()
    assert data["board_details"].keys() == enc.XIAO_DETAILS.keys()
    assert [wire["name"] for wire in data["wires"]] == [wire.name for wire in enc.WIRES]
    assert data["connectors"].keys() == enc.WIRE_CONNECTORS.keys()
    assert data["assembly_aids"].keys() == enc.ASSEMBLY_AIDS.keys()
    for mesh in data["parts"].values():
        assert len(mesh["vertices"]) % 3 == 0
        assert len(mesh["triangles"]) % 3 == 0
        assert max(mesh["triangles"]) < len(mesh["vertices"]) / 3


def test_export_writes_files_that_read_back(tmp_path: Path) -> None:
    written = enc.export_all(tmp_path)

    assert sorted(path.name for path in written) == [
        "assembly.step",
        "base.step",
        "base.stl",
        "lid.step",
        "lid.stl",
    ]
    assert all(path.stat().st_size > 0 for path in written)
    assert cq.importers.importStep(str(tmp_path / "base.step")).val().Volume() == pytest.approx(
        enc.build_base().val().Volume(), rel=1e-3
    )
