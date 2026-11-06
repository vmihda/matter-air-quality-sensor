#include "ens160_proto.h"
#include "minitest.h"

static void part_id_is_little_endian_0x0160(void)
{
    const uint8_t expected[2] = {0x60, 0x01};
    const uint8_t swapped[2] = {0x01, 0x60};
    CHECK(ens160_is_part_id(expected));
    CHECK(!ens160_is_part_id(swapped));
}

static void status_reports_normal_operation_with_new_data(void)
{
    const ens160_status_t status = ens160_decode_status(0x82);
    CHECK(status.validity == ENS160_VALID_NORMAL);
    CHECK(status.new_data);
    CHECK(!status.error);
    CHECK(status.running);
}

static void status_reports_each_validity_flag(void)
{
    CHECK(ens160_decode_status(0x84).validity == ENS160_VALID_WARM_UP);
    CHECK(ens160_decode_status(0x88).validity == ENS160_VALID_INITIAL_START_UP);
    CHECK(ens160_decode_status(0x8C).validity == ENS160_VALID_INVALID_OUTPUT);
}

static void status_reports_an_error_and_no_new_data(void)
{
    const ens160_status_t status = ens160_decode_status(0x40);
    CHECK(status.error);
    CHECK(!status.new_data);
    CHECK(!status.running);
}

// Bit 0 announces general purpose registers, which this firmware does not read.
static void new_data_ignores_the_general_purpose_flag(void)
{
    CHECK(!ens160_decode_status(0x81).new_data);
}

static void warm_up_covers_both_starting_phases(void)
{
    CHECK(!ens160_is_warming_up(ENS160_VALID_NORMAL));
    CHECK(ens160_is_warming_up(ENS160_VALID_WARM_UP));
    CHECK(ens160_is_warming_up(ENS160_VALID_INITIAL_START_UP));
    CHECK(!ens160_is_warming_up(ENS160_VALID_INVALID_OUTPUT));
}

static void only_normal_operation_is_reportable(void)
{
    CHECK(ens160_is_reportable(ENS160_VALID_NORMAL));
    CHECK(!ens160_is_reportable(ENS160_VALID_WARM_UP));
    CHECK(!ens160_is_reportable(ENS160_VALID_INITIAL_START_UP));
    CHECK(!ens160_is_reportable(ENS160_VALID_INVALID_OUTPUT));
}

// Both examples come from the ENS160 datasheet, sections 16.2.5 and 16.2.6.
static void ambient_encoding_matches_datasheet_examples(void)
{
    CHECK(ens160_encode_temperature(25.0f) == 0x4A8A);
    CHECK(ens160_encode_humidity(50.0f) == 0x6400);
}

static void ambient_encoding_stays_inside_the_register_range(void)
{
    CHECK(ens160_encode_temperature(-300.0f) == 0);
    CHECK(ens160_encode_humidity(-1.0f) == 0);
    CHECK(ens160_encode_humidity(120.0f) == 100 * 512);
    CHECK(ens160_encode_temperature(1000.0f) == UINT16_MAX);
    CHECK(ens160_encode_temperature(NAN) == 0);
    CHECK(ens160_encode_humidity(NAN) == 0);
}

static void ambient_frame_is_little_endian_from_temp_in(void)
{
    uint8_t frame[5];
    ens160_build_ambient_write(frame, 25.0f, 50.0f);
    CHECK(frame[0] == 0x13);
    CHECK(frame[1] == 0x8A);
    CHECK(frame[2] == 0x4A);
    CHECK(frame[3] == 0x00);
    CHECK(frame[4] == 0x64);
}

static void tvoc_bytes_are_little_endian(void)
{
    const uint8_t raw[2] = {0x2C, 0x01};
    CHECK(ens160_decode_tvoc_ppb(raw) == 300);
}

// Ethanol at 25 degrees C and 1 atm: 46.0684 g/mol and 24.45 L/mol.
static void ppb_converts_to_ethanol_equivalent_mass(void)
{
    CHECK_NEAR(ens160_ppb_to_ugm3(100), 188.4, 0.1);
    CHECK_NEAR(ens160_ppb_to_ugm3(0), 0.0, 0.001);
}

int main(void)
{
    RUN_TEST(part_id_is_little_endian_0x0160);
    RUN_TEST(status_reports_normal_operation_with_new_data);
    RUN_TEST(status_reports_each_validity_flag);
    RUN_TEST(status_reports_an_error_and_no_new_data);
    RUN_TEST(new_data_ignores_the_general_purpose_flag);
    RUN_TEST(warm_up_covers_both_starting_phases);
    RUN_TEST(only_normal_operation_is_reportable);
    RUN_TEST(ambient_encoding_matches_datasheet_examples);
    RUN_TEST(ambient_encoding_stays_inside_the_register_range);
    RUN_TEST(ambient_frame_is_little_endian_from_temp_in);
    RUN_TEST(tvoc_bytes_are_little_endian);
    RUN_TEST(ppb_converts_to_ethanol_equivalent_mass);
    return MINITEST_RESULT();
}
