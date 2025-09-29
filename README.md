# VR Embedded Telemetry System

A comprehensive embedded VR telemetry system that simulates a real-time embedded VR headset with sensor data generation, power management, watchdog timers, and RabbitMQ streaming for real-time monitoring and analysis.

## Features

- **Embedded System Architecture**: Simulates real-time embedded VR headset with ARM Cortex-M4 characteristics
- **Real-time Sensor Processing**: 1kHz sensor updates with configurable rates
- **Power Management**: CPU sleep modes, voltage monitoring, and power saving
- **Watchdog Timer**: System reliability with configurable timeout
- **Error Handling**: Comprehensive error detection and recovery
- **RabbitMQ Integration**: High-performance message queuing for telemetry data
- **Real-time Visualization**: Python-based consumer with live plotting and monitoring
- **Configurable System**: Adjustable clock speeds, sensor rates, and power modes
- **Performance Monitoring**: System resource tracking and analysis
- **Data Export**: CSV export capabilities for offline analysis

## System Architecture

```
┌─────────────────────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│     Embedded VR System         │───▶│    RabbitMQ     │───▶│ Python Consumer │
│  ┌─────────────────────────┐   │    │   Message Queue │    │  (Visualization) │
│  │  ARM Cortex-M4 Core     │   │    └─────────────────┘    └─────────────────┘
│  │  - 168 MHz Clock         │   │
│  │  - Watchdog Timer        │   │
│  │  - Power Management      │   │
│  │  - Real-time Sensors    │   │
│  │  - Error Handling       │   │
│  └─────────────────────────┘   │
└─────────────────────────────────┘
```

## Components

### Embedded C System
- **`src/main.c`**: Embedded system main loop with command-line interface
- **`src/vr_embedded.c`**: Core embedded system with real-time processing, power management, and watchdog
- **`src/vr_simulation.c`**: VR sensor data generation algorithms
- **`src/vr_rabbitmq.c`**: RabbitMQ integration and message publishing
- **`include/vr_telemetry.h`**: Data structures, embedded system definitions, and function prototypes

### Python Consumer
- **`python/vr_consumer.py`**: RabbitMQ consumer with real-time visualization
- **`test_system.py`**: Comprehensive system testing
- **`requirements.txt`**: Python dependencies

## Telemetry Data Structure

The system generates comprehensive VR telemetry data including:

- **Head Tracking**: Position, orientation, acceleration, angular velocity
- **Eye Tracking**: Gaze position, pupil diameter, blink detection
- **Hand Tracking**: Position, orientation, grip strength, tracking status
- **System Metrics**: CPU/GPU usage, temperature, battery level, connection status

## Quick Start

### Prerequisites

1. **System Dependencies**:
   ```bash
   # Ubuntu/Debian
   sudo apt-get update
   sudo apt-get install -y gcc make librabbitmq-dev librabbitmq4
   
   # Install RabbitMQ
   sudo apt-get install -y rabbitmq-server
   sudo systemctl start rabbitmq-server
   ```

2. **Python Dependencies**:
   ```bash
   pip install -r requirements.txt
   ```

### Building and Running

1. **Build the simulation**:
   ```bash
   make
   ```

2. **Run the simulation** (in one terminal):
   ```bash
   ./bin/vr_telemetry_sim
   ```

3. **Run the consumer** (in another terminal):
   ```bash
   python python/vr_consumer.py --visualize
   ```

4. **Run system tests**:
   ```bash
   python test_system.py
   ```

## Usage Examples

### Basic Embedded System
```bash
# Run with default settings (1000Hz sensors, 60Hz telemetry, infinite duration)
./bin/vr_telemetry_sim

# Run for 60 seconds with 500Hz sensors and 30Hz telemetry
./bin/vr_telemetry_sim -f 500 -t 30 -d 60

# Run without RabbitMQ (console output only)
./bin/vr_telemetry_sim -n
```

### Embedded System Configuration
```bash
# High-performance mode (2000Hz sensors, 120Hz telemetry)
./bin/vr_telemetry_sim -f 2000 -t 120

# Power-saving mode with CPU sleep
./bin/vr_telemetry_sim --power-save --cpu-sleep-level 2

# Custom watchdog timeout (10 seconds)
./bin/vr_telemetry_sim --watchdog-timeout 10000

# Low-power mode (100Hz sensors, 10Hz telemetry)
./bin/vr_telemetry_sim -f 100 -t 10 --power-save
```

### Custom RabbitMQ Configuration
```bash
# Connect to remote RabbitMQ server
./bin/vr_telemetry_sim -h rabbitmq.example.com -p 5673 -u myuser -w mypass

# Custom exchange and routing key
./bin/vr_telemetry_sim -e my_exchange -r my.routing.key
```

### Python Consumer Options
```bash
# Run with real-time visualization
python python/vr_consumer.py --visualize

# Connect to remote RabbitMQ
python python/vr_consumer.py --host rabbitmq.example.com --port 5673

# Auto-export data every 60 seconds
python python/vr_consumer.py --export-interval 60
```

## Configuration

### Embedded System Parameters

The embedded system can be configured through command-line arguments:

| Parameter | Description | Default |
|-----------|-------------|---------|
| `-f, --frequency` | Sensor update frequency in Hz | 1000 |
| `-t, --telemetry-rate` | Telemetry transmission rate in Hz | 60 |
| `-d, --duration` | Duration in seconds (0 = infinite) | 0 |
| `-n, --no-rabbitmq` | Run without RabbitMQ | false |
| `--watchdog-timeout` | Watchdog timeout in milliseconds | 5000 |
| `--power-save` | Enable power saving mode | false |
| `--cpu-sleep-level` | CPU sleep level (0-3) | 1 |

### RabbitMQ Configuration

| Parameter | Description | Default |
|-----------|-------------|---------|
| `-h, --host` | RabbitMQ host | localhost |
| `-p, --port` | RabbitMQ port | 5672 |
| `-u, --username` | RabbitMQ username | guest |
| `-w, --password` | RabbitMQ password | guest |
| `-v, --vhost` | RabbitMQ vhost | / |
| `-e, --exchange` | RabbitMQ exchange | vr_telemetry |
| `-r, --routing-key` | RabbitMQ routing key | telemetry.data |

## Data Format

Telemetry data is published as JSON messages with the following structure:

```json
{
  "timestamp_us": 1234567890123,
  "frame_id": 42,
  "head_position": {"x": 0.1, "y": 1.7, "z": 0.2},
  "head_orientation": {"x": 0.0, "y": 0.0, "z": 0.0, "w": 1.0},
  "head_acceleration": {"x": 0.01, "y": 0.02, "z": 0.01},
  "head_angular_velocity": {"x": 0.1, "y": 0.2, "z": 0.1},
  "left_eye": {"x": 0.5, "y": 0.5, "pupil_diameter": 3.5, "is_blinking": false},
  "right_eye": {"x": 0.5, "y": 0.5, "pupil_diameter": 3.5, "is_blinking": false},
  "left_hand": {
    "x": 0.3, "y": 1.2, "z": 0.1,
    "orientation": {"x": 0.0, "y": 0.0, "z": 0.0, "w": 1.0},
    "grip_strength": 0.5,
    "is_tracking": true
  },
  "right_hand": {
    "x": -0.3, "y": 1.2, "z": 0.1,
    "orientation": {"x": 0.0, "y": 0.0, "z": 0.0, "w": 1.0},
    "grip_strength": 0.5,
    "is_tracking": true
  },
  "cpu_usage": 45.2,
  "gpu_usage": 60.1,
  "temperature": 35.5,
  "battery_level": 85,
  "is_connected": true
}
```

## Development

### Building from Source

```bash
# Clean build
make clean

# Debug build
make debug

# Release build
make release

# Install dependencies
make install-deps
make install-python-deps
```

### Testing

```bash
# Run system tests
make test

# Run Python consumer tests
python test_system.py

# Run with visualization
python python/vr_consumer.py --visualize
```

### Code Structure

```
vr_telemetry_sim/
├── include/
│   └── vr_telemetry.h          # Data structures and prototypes
├── src/
│   ├── main.c                  # Main simulation loop
│   ├── vr_simulation.c         # VR sensor simulation
│   └── vr_rabbitmq.c          # RabbitMQ integration
├── python/
│   └── vr_consumer.py          # Python consumer with visualization
├── bin/                        # Compiled binaries
├── obj/                        # Object files
├── Makefile                    # Build configuration
├── requirements.txt            # Python dependencies
├── test_system.py             # System testing
└── README.md                  # This file
```

## Troubleshooting

### Common Issues

1. **RabbitMQ Connection Failed**:
   - Ensure RabbitMQ server is running: `sudo systemctl status rabbitmq-server`
   - Check connection parameters (host, port, credentials)
   - Use `-n` flag to run without RabbitMQ for testing

2. **Build Errors**:
   - Install required dependencies: `make install-deps`
   - Check GCC compiler: `gcc --version`
   - Clean and rebuild: `make clean && make`

3. **Python Import Errors**:
   - Install Python dependencies: `pip install -r requirements.txt`
   - Check Python version (3.7+ required)

4. **Performance Issues**:
   - Reduce simulation frequency: `-f 30`
   - Use calm simulation mode: `-s calm`
   - Monitor system resources with `htop`

### Debug Mode

Run with debug information:
```bash
make debug
./bin/vr_telemetry_sim -f 10 -d 30
```

## Performance

### System Requirements

- **CPU**: 2+ cores recommended
- **RAM**: 4GB+ recommended
- **Storage**: 1GB+ free space
- **Network**: Local network for RabbitMQ

### Performance Metrics

- **Simulation Rate**: Up to 120Hz on modern systems
- **Message Throughput**: 1000+ messages/second
- **Memory Usage**: <100MB for simulation, <200MB for consumer
- **Latency**: <10ms end-to-end

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests for new functionality
5. Submit a pull request

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Support

For issues and questions:
1. Check the troubleshooting section
2. Run system tests: `python test_system.py`
3. Create an issue with system information and error logs
