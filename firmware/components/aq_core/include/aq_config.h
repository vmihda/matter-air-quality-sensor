// The single place for air quality thresholds and timing.
#pragma once

// CO2, ppm.
#define AQ_CO2_MODERATE_PPM 800.0f
#define AQ_CO2_POOR_PPM 1000.0f
#define AQ_CO2_ALERT_OFF_PPM 800.0f

// PM2.5, ug/m3.
#define AQ_PM25_MODERATE_UGM3 15.0f
#define AQ_PM25_POOR_UGM3 35.0f
#define AQ_PM25_ALERT_OFF_UGM3 15.0f

// How long a value must stay at the Poor threshold before an alert opens.
#define AQ_ALERT_HOLD_MS 300000u

// A sensor with no valid reading for this long is reported as failed.
#define AQ_CO2_STALE_MS 60000u
#define AQ_PM25_STALE_MS 300000u
