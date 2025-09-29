#define _USE_MATH_DEFINES
#define _GNU_SOURCE
#include "vr_telemetry.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <signal.h>
#include <pthread.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Ensure math functions are available
#ifndef fmaxf
#define fmaxf(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifndef fminf
#define fminf(a, b) ((a) < (b) ? (a) : (b))
#endif

// Embedded System State
static vr_embedded_config_t g_embedded_config;
static vr_embedded_status_t g_embedded_status;
static vr_telemetry_packet_t g_telemetry_packet;
static bool g_system_running = true;
static pthread_mutex_t g_system_mutex = PTHREAD_MUTEX_INITIALIZER;

// System Timing
static uint32_t g_system_tick = 0;
static uint32_t g_last_sensor_update = 0;
static uint32_t g_last_telemetry_send = 0;
static uint32_t g_last_watchdog_feed = 0;

// Sensor Data Buffers
static float g_sensor_buffer[32];  // Circular buffer for sensor data
static uint8_t g_sensor_buffer_index = 0;

// Power Management
static float g_system_voltage = 3.3f;
static float g_system_current = 0.5f;
static bool g_power_save_active = false;

// Error Codes
#define VR_ERROR_SENSOR_INIT_FAILED    0x01
#define VR_ERROR_COMM_TIMEOUT          0x02
#define VR_ERROR_WATCHDOG_TIMEOUT     0x03
#define VR_ERROR_POWER_LOW             0x04
#define VR_ERROR_SENSOR_CALIBRATION    0x05
#define VR_ERROR_MEMORY_ALLOC          0x06

// Initialize embedded system
void vr_embedded_init(vr_embedded_config_t *config) {
    pthread_mutex_lock(&g_system_mutex);
    
    if (config) {
        memcpy(&g_embedded_config, config, sizeof(vr_embedded_config_t));
    } else {
        // Default embedded configuration
        g_embedded_config.system_clock_hz = 168000000;  // 168 MHz ARM Cortex-M4
        g_embedded_config.sensor_update_hz = 1000;      // 1 kHz sensor updates
        g_embedded_config.telemetry_rate_hz = 60;      // 60 Hz telemetry
        g_embedded_config.watchdog_enabled = true;
        g_embedded_config.watchdog_timeout_ms = 5000;   // 5 second timeout
        g_embedded_config.power_save_enabled = true;
        g_embedded_config.cpu_sleep_level = 1;
    }
    
    // Initialize system status
    memset(&g_embedded_status, 0, sizeof(vr_embedded_status_t));
    g_embedded_status.state = VR_SYSTEM_INIT;
    g_embedded_status.uptime_ms = 0;
    g_embedded_status.error_count = 0;
    g_embedded_status.reset_count = 0;
    g_embedded_status.sensors_initialized = false;
    g_embedded_status.communication_ready = false;
    
    // Initialize sensors
    vr_sensors_init();
    
    // Initialize power management
    vr_power_init();
    
    // Initialize watchdog
    if (g_embedded_config.watchdog_enabled) {
        vr_watchdog_init(g_embedded_config.watchdog_timeout_ms);
    }
    
    // Initialize telemetry system
    vr_telemetry_init();
    
    // Set system state to ready
    g_embedded_status.state = VR_SYSTEM_READY;
    g_embedded_status.sensors_initialized = true;
    g_embedded_status.communication_ready = true;
    
    pthread_mutex_unlock(&g_system_mutex);
    
    printf("[EMBEDDED] System initialized - Clock: %u Hz, Sensors: %u Hz, Telemetry: %u Hz\n",
           g_embedded_config.system_clock_hz,
           g_embedded_config.sensor_update_hz,
           g_embedded_config.telemetry_rate_hz);
}

// Main embedded system loop
void vr_embedded_main_loop(void) {
    printf("[EMBEDDED] Starting main loop...\n");
    
    while (g_system_running) {
        // System tick update
        vr_embedded_system_tick();
        
        // Update sensors at configured rate
        uint32_t sensor_interval = 1000 / g_embedded_config.sensor_update_hz;
        if (g_system_tick - g_last_sensor_update >= sensor_interval) {
            vr_sensors_update();
            g_last_sensor_update = g_system_tick;
        }
        
        // Send telemetry at configured rate
        uint32_t telemetry_interval = 1000 / g_embedded_config.telemetry_rate_hz;
        if (g_system_tick - g_last_telemetry_send >= telemetry_interval) {
            vr_telemetry_send_packet(&g_telemetry_packet);
            g_last_telemetry_send = g_system_tick;
        }
        
        // Feed watchdog
        if (g_embedded_config.watchdog_enabled) {
            uint32_t watchdog_interval = g_embedded_config.watchdog_timeout_ms / 2;
            if (g_system_tick - g_last_watchdog_feed >= watchdog_interval) {
                vr_watchdog_feed();
                g_last_watchdog_feed = g_system_tick;
            }
        }
        
        // Power management
        if (g_embedded_config.power_save_enabled && g_power_save_active) {
            vr_power_enter_sleep(g_embedded_config.cpu_sleep_level);
        }
        
        // Small delay to prevent 100% CPU usage
        vr_delay_us(100);
    }
    
    printf("[EMBEDDED] Main loop stopped\n");
}

// System tick handler (called by timer interrupt)
void vr_embedded_system_tick(void) {
    pthread_mutex_lock(&g_system_mutex);
    
    g_system_tick++;
    g_embedded_status.uptime_ms = g_system_tick;
    
    // Check for system errors
    if (g_embedded_status.error_count > 10) {
        vr_embedded_set_state(VR_SYSTEM_ERROR);
        vr_error_handler(VR_ERROR_SENSOR_INIT_FAILED);
    }
    
    // Check power levels
    if (g_system_voltage < 3.0f) {
        vr_error_handler(VR_ERROR_POWER_LOW);
    }
    
    pthread_mutex_unlock(&g_system_mutex);
}

// Get current system state
vr_system_state_t vr_embedded_get_state(void) {
    pthread_mutex_lock(&g_system_mutex);
    vr_system_state_t state = g_embedded_status.state;
    pthread_mutex_unlock(&g_system_mutex);
    return state;
}

// Set system state
void vr_embedded_set_state(vr_system_state_t state) {
    pthread_mutex_lock(&g_system_mutex);
    g_embedded_status.state = state;
    printf("[EMBEDDED] State changed to: %d\n", state);
    pthread_mutex_unlock(&g_system_mutex);
}

// Initialize sensors
void vr_sensors_init(void) {
    printf("[SENSORS] Initializing sensors...\n");
    
    // Initialize sensor buffer
    memset(g_sensor_buffer, 0, sizeof(g_sensor_buffer));
    g_sensor_buffer_index = 0;
    
    // Initialize telemetry packet
    memset(&g_telemetry_packet, 0, sizeof(vr_telemetry_packet_t));
    g_telemetry_packet.head_position.y = 1.7f;  // Average head height
    g_telemetry_packet.head_orientation.w = 1.0f;
    g_telemetry_packet.battery_level = 100;
    g_telemetry_packet.is_connected = true;
    
    // Self-test sensors
    if (!vr_sensors_self_test()) {
        vr_error_handler(VR_ERROR_SENSOR_INIT_FAILED);
        return;
    }
    
    // Calibrate sensors
    vr_sensors_calibrate();
    
    printf("[SENSORS] Sensors initialized successfully\n");
}

// Update sensor data
void vr_sensors_update(void) {
    static float simulation_time = 0.0f;
    static uint32_t frame_counter = 0;
    
    // Update simulation time
    simulation_time += 0.001f;  // 1ms update rate
    
    // Update timestamp and frame ID
    g_telemetry_packet.timestamp_us = vr_get_timestamp_us();
    g_telemetry_packet.frame_id = frame_counter++;
    
    // Simulate head movement
    g_telemetry_packet.head_position.x = sin(simulation_time * 0.5f) * 0.1f;
    g_telemetry_packet.head_position.y = 1.7f + sin(simulation_time * 0.3f) * 0.02f;
    g_telemetry_packet.head_position.z = cos(simulation_time * 0.4f) * 0.1f;
    
    // Simulate head orientation
    g_telemetry_packet.head_orientation.x = sin(simulation_time * 0.2f) * 0.1f;
    g_telemetry_packet.head_orientation.y = sin(simulation_time * 0.15f) * 0.2f;
    g_telemetry_packet.head_orientation.z = sin(simulation_time * 0.1f) * 0.05f;
    g_telemetry_packet.head_orientation.w = sqrt(1.0f - 
        (g_telemetry_packet.head_orientation.x * g_telemetry_packet.head_orientation.x +
         g_telemetry_packet.head_orientation.y * g_telemetry_packet.head_orientation.y +
         g_telemetry_packet.head_orientation.z * g_telemetry_packet.head_orientation.z));
    
    // Simulate eye tracking
    g_telemetry_packet.left_eye.x = 0.5f + sin(simulation_time * 2.0f) * 0.1f;
    g_telemetry_packet.left_eye.y = 0.5f + cos(simulation_time * 1.5f) * 0.1f;
    g_telemetry_packet.left_eye.pupil_diameter = 3.5f + sin(simulation_time * 0.5f) * 0.5f;
    g_telemetry_packet.left_eye.is_blinking = (fmod(simulation_time, 3.0f) > 2.9f);
    
    g_telemetry_packet.right_eye.x = 0.5f + sin(simulation_time * 2.1f) * 0.1f;
    g_telemetry_packet.right_eye.y = 0.5f + cos(simulation_time * 1.6f) * 0.1f;
    g_telemetry_packet.right_eye.pupil_diameter = 3.5f + sin(simulation_time * 0.51f) * 0.5f;
    g_telemetry_packet.right_eye.is_blinking = g_telemetry_packet.left_eye.is_blinking;
    
    // Simulate hand tracking
    g_telemetry_packet.left_hand.x = 0.3f + sin(simulation_time) * 0.2f;
    g_telemetry_packet.left_hand.y = 1.2f + cos(simulation_time * 0.7f) * 0.3f;
    g_telemetry_packet.left_hand.z = 0.1f + sin(simulation_time * 1.2f) * 0.15f;
    g_telemetry_packet.left_hand.grip_strength = 0.5f + sin(simulation_time * 0.4f) * 0.3f;
    g_telemetry_packet.left_hand.is_tracking = true;
    
    g_telemetry_packet.right_hand.x = -0.3f + sin(simulation_time * 1.1f) * 0.2f;
    g_telemetry_packet.right_hand.y = 1.2f + cos(simulation_time * 0.7f) * 0.3f;
    g_telemetry_packet.right_hand.z = 0.1f + sin(simulation_time * 1.2f) * 0.15f;
    g_telemetry_packet.right_hand.grip_strength = 0.5f + sin(simulation_time * 0.4f) * 0.3f;
    g_telemetry_packet.right_hand.is_tracking = true;
    
    // Simulate system metrics
    g_telemetry_packet.cpu_usage = 45.0f + sin(simulation_time * 0.8f) * 10.0f;
    g_telemetry_packet.gpu_usage = 60.0f + cos(simulation_time * 0.6f) * 15.0f;
    g_telemetry_packet.temperature = 35.0f + (g_telemetry_packet.cpu_usage + g_telemetry_packet.gpu_usage) * 0.1f;
    g_telemetry_packet.battery_level = (uint8_t)(100.0f - simulation_time * 0.1f);
    g_telemetry_packet.is_connected = (simulation_time < 300.0f || fmod(simulation_time, 60.0f) < 58.0f);
    
    // Store in sensor buffer
    g_sensor_buffer[g_sensor_buffer_index] = g_telemetry_packet.head_position.x;
    g_sensor_buffer_index = (g_sensor_buffer_index + 1) % 32;
}

// Sensor self-test
bool vr_sensors_self_test(void) {
    printf("[SENSORS] Running self-test...\n");
    
    // Simulate sensor self-test
    for (int i = 0; i < 10; i++) {
        vr_delay_ms(10);
        if (i == 5) {
            // Simulate occasional sensor failure
            if (rand() % 100 < 5) {  // 5% failure rate
                printf("[SENSORS] Self-test failed at iteration %d\n", i);
                return false;
            }
        }
    }
    
    printf("[SENSORS] Self-test passed\n");
    return true;
}

// Sensor calibration
void vr_sensors_calibrate(void) {
    printf("[SENSORS] Calibrating sensors...\n");
    
    // Simulate calibration process
    for (int i = 0; i < 100; i++) {
        vr_delay_ms(1);
        // Simulate calibration data collection
    }
    
    printf("[SENSORS] Calibration complete\n");
}

// Initialize telemetry system
void vr_telemetry_init(void) {
    printf("[TELEMETRY] Initializing telemetry system...\n");
    
    // Initialize RabbitMQ connection
    if (vr_rabbitmq_init(NULL, 5672, "guest", "guest", "/", "vr_telemetry", "telemetry.data") != 0) {
        printf("[TELEMETRY] Failed to initialize RabbitMQ connection\n");
        g_embedded_status.communication_ready = false;
    } else {
        printf("[TELEMETRY] Telemetry system ready\n");
    }
}

// Send telemetry packet
void vr_telemetry_send_packet(const vr_telemetry_packet_t *packet) {
    if (!vr_telemetry_is_ready()) {
        return;
    }
    
    if (vr_rabbitmq_send_telemetry(packet) != 0) {
        g_embedded_status.error_count++;
        printf("[TELEMETRY] Failed to send packet, error count: %u\n", g_embedded_status.error_count);
    }
}

// Check if telemetry is ready
bool vr_telemetry_is_ready(void) {
    return g_embedded_status.communication_ready && vr_rabbitmq_is_connected();
}

// Set telemetry rate
void vr_telemetry_set_rate(uint32_t rate_hz) {
    g_embedded_config.telemetry_rate_hz = rate_hz;
    printf("[TELEMETRY] Telemetry rate set to %u Hz\n", rate_hz);
}

// Initialize power management
void vr_power_init(void) {
    printf("[POWER] Initializing power management...\n");
    g_system_voltage = 3.3f;
    g_system_current = 0.5f;
    g_power_save_active = false;
    printf("[POWER] Power management ready\n");
}

// Enter sleep mode
void vr_power_enter_sleep(uint8_t sleep_level) {
    if (sleep_level > 0) {
        printf("[POWER] Entering sleep level %d\n", sleep_level);
        g_power_save_active = true;
        vr_delay_ms(10);  // Simulate sleep
        g_power_save_active = false;
    }
}

// Wake up from sleep
void vr_power_wake_up(void) {
    printf("[POWER] Waking up from sleep\n");
    g_power_save_active = false;
}

// Get system voltage
float vr_power_get_voltage(void) {
    return g_system_voltage;
}

// Get system current
float vr_power_get_current(void) {
    return g_system_current;
}

// Initialize watchdog
void vr_watchdog_init(uint32_t timeout_ms) {
    printf("[WATCHDOG] Initializing watchdog with %u ms timeout\n", timeout_ms);
    g_embedded_status.last_watchdog_reset = g_system_tick;
}

// Feed watchdog
void vr_watchdog_feed(void) {
    g_embedded_status.last_watchdog_reset = g_system_tick;
}

// Disable watchdog
void vr_watchdog_disable(void) {
    printf("[WATCHDOG] Watchdog disabled\n");
}

// Error handler
void vr_error_handler(uint32_t error_code) {
    g_embedded_status.error_count++;
    printf("[ERROR] Error code: 0x%02X, count: %u\n", error_code, g_embedded_status.error_count);
    
    if (g_embedded_status.error_count > 5) {
        vr_embedded_set_state(VR_SYSTEM_ERROR);
        printf("[ERROR] Too many errors, entering error state\n");
    }
}

// System reset
void vr_system_reset(void) {
    printf("[SYSTEM] Performing system reset...\n");
    g_embedded_status.reset_count++;
    g_embedded_status.error_count = 0;
    g_embedded_status.state = VR_SYSTEM_INIT;
    vr_embedded_init(&g_embedded_config);
}

// Get error count
uint32_t vr_get_error_count(void) {
    return g_embedded_status.error_count;
}

// Get system tick
uint32_t vr_get_system_tick(void) {
    return g_system_tick;
}

// Delay in milliseconds
void vr_delay_ms(uint32_t ms) {
    usleep(ms * 1000);
}

// Delay in microseconds
void vr_delay_us(uint32_t us) {
    usleep(us);
}

// Get timestamp in microseconds
uint64_t vr_get_timestamp_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

// Add sensor noise
void vr_add_sensor_noise(float *value, float noise_level) {
    if (value == NULL) return;
    float noise = ((float)rand() / RAND_MAX - 0.5f) * 2.0f * noise_level;
    *value += noise;
}

// Generate sine wave
float vr_generate_sine_wave(float time, float frequency, float amplitude) {
    return amplitude * sin(2.0f * M_PI * frequency * time);
}

// Generate random walk
float vr_generate_random_walk(float *last_value, float max_change) {
    if (last_value == NULL) return 0.0f;
    float change = ((float)rand() / RAND_MAX - 0.5f) * 2.0f * max_change;
    *last_value += change;
    return *last_value;
}
