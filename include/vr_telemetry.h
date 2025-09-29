#ifndef VR_TELEMETRY_H
#define VR_TELEMETRY_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

// VR Sensor Data Types
typedef struct {
    float x, y, z;  // 3D position in meters
} vr_position_t;

typedef struct {
    float x, y, z, w;  // Quaternion (x, y, z, w)
} vr_orientation_t;

typedef struct {
    float x, y, z;  // Linear acceleration in m/s²
} vr_acceleration_t;

typedef struct {
    float x, y, z;  // Angular velocity in rad/s
} vr_angular_velocity_t;

typedef struct {
    float x, y;  // Eye gaze position (normalized coordinates)
    float pupil_diameter;  // Pupil diameter in mm
    bool is_blinking;
} vr_eye_tracking_t;

typedef struct {
    float x, y, z;  // Hand position in meters
    vr_orientation_t orientation;  // Hand orientation
    float grip_strength;  // 0.0 to 1.0
    bool is_tracking;
} vr_hand_tracking_t;

// Main VR Telemetry Packet
typedef struct {
    uint64_t timestamp_us;  // Microsecond timestamp
    uint32_t frame_id;       // Frame sequence number
    
    // Head tracking
    vr_position_t head_position;
    vr_orientation_t head_orientation;
    vr_acceleration_t head_acceleration;
    vr_angular_velocity_t head_angular_velocity;
    
    // Eye tracking
    vr_eye_tracking_t left_eye;
    vr_eye_tracking_t right_eye;
    
    // Hand tracking
    vr_hand_tracking_t left_hand;
    vr_hand_tracking_t right_hand;
    
    // System status
    float cpu_usage;          // CPU usage percentage
    float gpu_usage;          // GPU usage percentage
    float temperature;        // Headset temperature in °C
    uint8_t battery_level;    // Battery percentage (0-100)
    bool is_connected;        // Connection status
    
} vr_telemetry_packet_t;


// Embedded System Status
typedef enum {
    VR_SYSTEM_INIT,
    VR_SYSTEM_READY,
    VR_SYSTEM_TRACKING,
    VR_SYSTEM_ERROR,
    VR_SYSTEM_SLEEP,
    VR_SYSTEM_SHUTDOWN
} vr_system_state_t;

// Embedded System Configuration
typedef struct {
    uint32_t system_clock_hz;      // System clock frequency
    uint32_t sensor_update_hz;     // Sensor update frequency
    uint32_t telemetry_rate_hz;    // Telemetry transmission rate
    bool watchdog_enabled;         // Watchdog timer enabled
    uint32_t watchdog_timeout_ms;  // Watchdog timeout
    bool power_save_enabled;      // Power saving mode
    uint8_t cpu_sleep_level;       // CPU sleep level (0-3)
} vr_embedded_config_t;

// Embedded System Status
typedef struct {
    vr_system_state_t state;
    uint32_t uptime_ms;           // System uptime in milliseconds
    uint32_t last_watchdog_reset; // Last watchdog reset timestamp
    uint32_t error_count;         // Error counter
    uint32_t reset_count;         // System reset counter
    bool sensors_initialized;     // Sensor initialization status
    bool communication_ready;      // Communication system status
} vr_embedded_status_t;

// Function prototypes - Embedded System Core
void vr_embedded_init(vr_embedded_config_t *config, bool use_rabbitmq);
void vr_embedded_main_loop(void);
void vr_embedded_system_tick(void);
vr_system_state_t vr_embedded_get_state(void);
void vr_embedded_set_state(vr_system_state_t state);

// Sensor Management
void vr_sensors_init(void);
void vr_sensors_update(void);
bool vr_sensors_self_test(void);
void vr_sensors_calibrate(void);

// Telemetry System
void vr_telemetry_init(void);
void vr_telemetry_send_packet(const vr_telemetry_packet_t *packet);
bool vr_telemetry_is_ready(void);
void vr_telemetry_set_rate(uint32_t rate_hz);

// Power Management
void vr_power_init(void);
void vr_power_enter_sleep(uint8_t sleep_level);
void vr_power_wake_up(void);
float vr_power_get_voltage(void);
float vr_power_get_current(void);

// Watchdog
void vr_watchdog_init(uint32_t timeout_ms);
void vr_watchdog_feed(void);
void vr_watchdog_disable(void);

// Error Handling
void vr_error_handler(uint32_t error_code);
void vr_system_reset(void);
uint32_t vr_get_error_count(void);

// Real-time Functions
uint32_t vr_get_system_tick(void);
void vr_delay_ms(uint32_t ms);
void vr_delay_us(uint32_t us);

// Utility functions
uint64_t vr_get_timestamp_us(void);
void vr_add_sensor_noise(float *value, float noise_level);
float vr_generate_sine_wave(float time, float frequency, float amplitude);
float vr_generate_random_walk(float *last_value, float max_change);

#endif // VR_TELEMETRY_H
