#ifndef MACHINE_SETTINGS_H
#define MACHINE_SETTINGS_H

// --- MOTOR SETTINGS ---
#define MAX_SPEED    30000.0f
#define DEFAULT_ACCEL 1000.0f
#define DEFAULT_FEED  3000.0f

#define HOME_FAST_SPEED   1000.0f
#define HOME_SLOW_SPEED   120.0f
#define HOME_BACKOFF_STEPS 200

// --- AXIS CONFIGURATION ---

// X-AXIS
#define X_STEPS_PER_MM  266.7f
#define X_MAX_TRAVEL    275.0f
#define X_INVERT_DIR    false
#define X_HOME_NEG      true

// Y-AXIS
#define Y_STEPS_PER_MM  266.7f
#define Y_MAX_TRAVEL    65.0f
#define Y_INVERT_DIR    true
#define Y_HOME_NEG      true

// Z-AXIS
#define Z_STEPS_PER_MM  400.0f
#define Z_MAX_TRAVEL    15.0f
#define Z_INVERT_DIR    false
#define Z_HOME_NEG      true

// A-AXIS (Infinite rotation)
#define A_STEPS_PER_DEG 3.5556f
#define A_MAX_TRAVEL    -1.0f   // -1 means infinite
#define A_INVERT_DIR    true
#define A_HOME_NEG      false

// --- SAFETY ---
#define OVERLOAD_THRESHOLD_KG 30.0f

// =============================================================================
// --- GPIO PIN CONFIGURATIE (TOEKOMSTBESTENDIG) ---
// =============================================================================

// --- X-AS (Motor & Limietschakelaar) ---
#define CONFIG_X_STEP_GPIO   12
#define CONFIG_X_DIR_GPIO    13
#define CONFIG_X_LIMIT_GPIO  16  // Nu met werkende interne pull-up!

// --- Y-AS (Motor & Limietschakelaar) ---
#define CONFIG_Y_STEP_GPIO   14
#define CONFIG_Y_DIR_GPIO    27
#define CONFIG_Y_LIMIT_GPIO  17  // Nu met werkende interne pull-up!

// --- Z-AS (Motor & Limietschakelaar) ---
#define CONFIG_Z_STEP_GPIO   25
#define CONFIG_Z_DIR_GPIO    26
#define CONFIG_Z_LIMIT_GPIO  21  // Nu met werkende interne pull-up!

// --- A-AS (Motor & Limietschakelaar) ---
#define CONFIG_A_STEP_GPIO   32
#define CONFIG_A_DIR_GPIO    33
#define CONFIG_A_LIMIT_GPIO  22  // Nu met werkende interne pull-up!

// --- HX711 WEEGCEEL (Krachtsensor) ---
#define CONFIG_HX711_DOUT_GPIO  4   
#define CONFIG_HX711_SCK_GPIO   5

#endif // MACHINE_SETTINGS_H