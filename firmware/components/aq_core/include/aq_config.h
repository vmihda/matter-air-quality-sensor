// The single place for the default air quality thresholds and timing.
// Owners change most of these from the device page; see aq_settings.h.
#pragma once

// CO2, ppm.
#define AQ_CO2_MODERATE_PPM 800.0f
#define AQ_CO2_POOR_PPM 1000.0f

// PM2.5, ug/m3.
#define AQ_PM25_MODERATE_UGM3 15.0f
#define AQ_PM25_POOR_UGM3 35.0f

// How long a value must stay at the Poor threshold before the level becomes Poor.
// The level leaves Poor only below the Moderate threshold, so it does not flicker.
#define AQ_CO2_HOLD_S 120u
#define AQ_PM25_HOLD_S 300u

// A PM2.5 reading can arrive a moment before a full interval has passed. Without this,
// a hold equal to the interval would need a third reading instead of a second.
#define AQ_PM25_HOLD_SLACK_MS 5000u

// A sensor with no valid reading for this long is reported as failed.
#define AQ_CO2_STALE_MS 60000u
#define AQ_PM25_STALE_CYCLES 3u

// PMS5003 duty cycle. The sensor needs 30 s after waking before its data is stable.
#define AQ_PMS_CYCLE_MS 300000u
#define AQ_PMS_SETTLE_MS 30000u
#define AQ_PMS_USE_SLEEP 1

// How often the slowly changing values are sent to Matter. The air quality level is sent at once.
#define AQ_PUBLISH_PERIOD_MS 30000u

// Heat from the board raises the temperature the SCD41 reads. Tune in the real enclosure.
#define AQ_TEMP_OFFSET_C 4.0f
