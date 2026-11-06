"""Geometry checks for the printed enclosure. All volumes are measured on the real solids."""
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
    # Above the wire passage the partition is continuous across the whole bay.
    partition = enc.Box(
        enc.PARTITION_X,
        enc.INNER_X1,
        enc.PARTITION_Y0,
        enc.PARTITION_Y1,
        enc.WIRE_PASSAGE.z1,
        enc.BASE_Z - enc.PARTITION_TOP_GAP,
    )
    assert solid_in(base, partition) == pytest.approx(partition.volume, rel=1e-3)
    assert sensor.y1 <= enc.PARTITION_Y0


def test_partition_is_thick_enough_to_print() -> None:
    # Two perimeters of a 0.4 mm nozzle plus a margin.
    assert enc.PARTITION_Y1 - enc.PARTITION_Y0 >= 1.0
    assert enc.PARTITION_X - enc.PMS_WALL_X0 >= 1.0


def test_scd41_wall_is_away_from_the_pms_ports() -> None:
    # The SCD41 reads temperature, so it must not breathe air warmed by the PMS5003.
    assert enc.SCD41_SENSOR.x0 - enc.PMS_FAN_PORT.x1 >= 25


def test_wires_can_leave_the_cold_chamber(base: cq.Workplane) -> None:
    assert solid_in(base, enc.WIRE_PASSAGE) < EMPTY
    assert enc.WIRE_PASSAGE.z1 <= enc.FLOOR + 6


def test_pms_cable_has_a_free_route_to_the_adapter(base: cq.Workplane, lid: cq.Workplane) -> None:
    assert solid_in(base, enc.PMS_CABLE_ROUTE) < EMPTY
    assert solid_in(lid, enc.PMS_CABLE_ROUTE) < EMPTY


def test_rear_snap_can_be_released_from_outside(base: cq.Workplane) -> None:
    for window in enc.SNAP_WINDOWS:
        assert solid_in(base, window) < EMPTY


def test_mesh_data_covers_printed_parts_and_components() -> None:
    data = enc.mesh_data()

    assert data["parts"].keys() == {"base", "lid"}
    assert data["components"].keys() == enc.COMPONENT_BOXES.keys()
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
