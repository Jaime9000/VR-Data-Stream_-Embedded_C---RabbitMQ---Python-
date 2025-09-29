#include "vr_telemetry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>

// Global variables for signal handling
static volatile bool g_running = true;

// Signal handler for graceful shutdown
void signal_handler(int sig) {
    printf("\n[EMBEDDED] Received signal %d, shutting down gracefully...\n", sig);
    g_running = false;
}

// Print usage information
void print_usage(const char *program_name) {
    printf("VR Embedded Telemetry System\n");
    printf("Usage: %s [OPTIONS]\n", program_name);
    printf("\nOptions:\n");
    printf("  -h, --host HOST        RabbitMQ host (default: localhost)\n");
    printf("  -p, --port PORT        RabbitMQ port (default: 5672)\n");
    printf("  -u, --username USER    RabbitMQ username (default: guest)\n");
    printf("  -w, --password PASS    RabbitMQ password (default: guest)\n");
    printf("  -v, --vhost VHOST      RabbitMQ vhost (default: /)\n");
    printf("  -e, --exchange EXCH    RabbitMQ exchange (default: vr_telemetry)\n");
    printf("  -r, --routing-key KEY  RabbitMQ routing key (default: telemetry.data)\n");
    printf("  -f, --frequency FREQ   Sensor update frequency in Hz (default: 1000)\n");
    printf("  -t, --telemetry-rate RATE  Telemetry transmission rate in Hz (default: 60)\n");
    printf("  -d, --duration SEC     Duration in seconds (default: 0 = infinite)\n");
    printf("  -n, --no-rabbitmq      Run without RabbitMQ (console output only)\n");
    printf("  -w, --watchdog-timeout MS  Watchdog timeout in milliseconds (default: 5000)\n");
    printf("  --power-save           Enable power saving mode\n");
    printf("  --cpu-sleep-level LEVEL CPU sleep level 0-3 (default: 1)\n");
    printf("  --help                 Show this help message\n");
    printf("\nExamples:\n");
    printf("  %s                                    # Run with defaults\n", program_name);
    printf("  %s -f 500 -t 30 -d 60                 # 500Hz sensors, 30Hz telemetry for 60s\n", program_name);
    printf("  %s -h rabbitmq.example.com -p 5673    # Custom RabbitMQ server\n", program_name);
    printf("  %s -n --power-save                     # Console output with power saving\n", program_name);
}


int main(int argc, char *argv[]) {
    // Default embedded configuration
    vr_embedded_config_t embedded_config = {
        .system_clock_hz = 168000000,      // 168 MHz ARM Cortex-M4
        .sensor_update_hz = 1000,          // 1 kHz sensor updates
        .telemetry_rate_hz = 60,           // 60 Hz telemetry
        .watchdog_enabled = true,
        .watchdog_timeout_ms = 5000,       // 5 second timeout
        .power_save_enabled = false,
        .cpu_sleep_level = 1
    };
    
    // RabbitMQ configuration
    char *host = "localhost";
    int port = 5672;
    char *username = "guest";
    char *password = "guest";
    char *vhost = "/";
    char *exchange = "vr_telemetry";
    char *routing_key = "telemetry.data";
    int duration = 0; // 0 = infinite
    bool use_rabbitmq = true;
    
    // Parse command line arguments
    static struct option long_options[] = {
        {"host", required_argument, 0, 'h'},
        {"port", required_argument, 0, 'p'},
        {"username", required_argument, 0, 'u'},
        {"password", required_argument, 0, 'w'},
        {"vhost", required_argument, 0, 'v'},
        {"exchange", required_argument, 0, 'e'},
        {"routing-key", required_argument, 0, 'r'},
        {"frequency", required_argument, 0, 'f'},
        {"telemetry-rate", required_argument, 0, 't'},
        {"duration", required_argument, 0, 'd'},
        {"no-rabbitmq", no_argument, 0, 'n'},
        {"watchdog-timeout", required_argument, 0, 'w'},
        {"power-save", no_argument, 0, 0},
        {"cpu-sleep-level", required_argument, 0, 0},
        {"help", no_argument, 0, 0},
        {0, 0, 0, 0}
    };
    
    int option_index = 0;
    int c;
    
    while ((c = getopt_long(argc, argv, "h:p:u:w:v:e:r:f:t:d:n", long_options, &option_index)) != -1) {
        switch (c) {
            case 'h': host = optarg; break;
            case 'p': port = atoi(optarg); break;
            case 'u': username = optarg; break;
            case 'w': password = optarg; break;
            case 'v': vhost = optarg; break;
            case 'e': exchange = optarg; break;
            case 'r': routing_key = optarg; break;
            case 'f': embedded_config.sensor_update_hz = atoi(optarg); break;
            case 't': embedded_config.telemetry_rate_hz = atoi(optarg); break;
            case 'd': duration = atoi(optarg); break;
            case 'n': use_rabbitmq = false; break;
            case 0: // Long options
                if (strcmp(long_options[option_index].name, "watchdog-timeout") == 0) {
                    embedded_config.watchdog_timeout_ms = atoi(optarg);
                } else if (strcmp(long_options[option_index].name, "power-save") == 0) {
                    embedded_config.power_save_enabled = true;
                } else if (strcmp(long_options[option_index].name, "cpu-sleep-level") == 0) {
                    embedded_config.cpu_sleep_level = atoi(optarg);
                } else if (strcmp(long_options[option_index].name, "help") == 0) {
                    print_usage(argv[0]);
                    return 0;
                }
                break;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    printf("[EMBEDDED] VR Embedded Telemetry System Starting...\n");
    printf("[EMBEDDED] Configuration:\n");
    printf("  System Clock: %u Hz\n", embedded_config.system_clock_hz);
    printf("  Sensor Update: %u Hz\n", embedded_config.sensor_update_hz);
    printf("  Telemetry Rate: %u Hz\n", embedded_config.telemetry_rate_hz);
    printf("  Watchdog: %s (%u ms)\n", embedded_config.watchdog_enabled ? "enabled" : "disabled", 
           embedded_config.watchdog_timeout_ms);
    printf("  Power Save: %s\n", embedded_config.power_save_enabled ? "enabled" : "disabled");
    printf("  CPU Sleep Level: %u\n", embedded_config.cpu_sleep_level);
    printf("  Duration: %s\n", duration > 0 ? "limited" : "infinite");
    printf("  RabbitMQ: %s\n", use_rabbitmq ? "enabled" : "disabled");
    if (use_rabbitmq) {
        printf("  Host: %s:%d\n", host, port);
        printf("  Exchange: %s\n", exchange);
        printf("  Routing Key: %s\n", routing_key);
    }
    printf("\n");
    
    // Initialize embedded system
    vr_embedded_init(&embedded_config);
    
    // Initialize RabbitMQ if enabled
    if (use_rabbitmq) {
        if (vr_rabbitmq_init(host, port, username, password, vhost, exchange, routing_key) != 0) {
            fprintf(stderr, "[EMBEDDED] Failed to connect to RabbitMQ. Use -n flag to run without RabbitMQ.\n");
            return 1;
        }
    }
    
    // Start embedded system main loop
    printf("[EMBEDDED] Starting embedded system main loop... (Press Ctrl+C to stop)\n");
    
    time_t start_time = time(NULL);
    uint32_t loop_count = 0;
    
    while (g_running) {
        // Check duration limit
        if (duration > 0 && (time(NULL) - start_time) >= duration) {
            printf("[EMBEDDED] Duration limit reached, stopping system.\n");
            break;
        }
        
        // Run embedded system main loop
        vr_embedded_main_loop();
        
        // Print status every 1000 iterations
        if (loop_count % 1000 == 0) {
            vr_system_state_t state = vr_embedded_get_state();
            uint32_t error_count = vr_get_error_count();
            printf("[EMBEDDED] Loop %u: State=%d, Errors=%u, Uptime=%u ms\n", 
                   loop_count, state, error_count, vr_get_system_tick());
        }
        
        loop_count++;
        
        // Small delay to prevent 100% CPU usage
        vr_delay_ms(1);
    }
    
    // Cleanup
    if (use_rabbitmq) {
        vr_rabbitmq_close();
    }
    
    printf("[EMBEDDED] System shutdown completed. Total loops: %u\n", loop_count);
    return 0;
}
