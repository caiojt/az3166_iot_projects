# AZ3166 IoT Sensor Station

A low-power IoT sensor station project for the MXChip AZ3166 (STM32F4) development board that reads environmental sensors and publishes data via MQTT.

## Features

- **Pure STM32 Implementation**: Bypasses Azure IoT framework for direct hardware control
- **Multi-Sensor Support**: Temperature, humidity, pressure, accelerometer, gyroscope, and magnetometer
- **MQTT Integration**: Publishes sensor data in JSON format compatible with Home Assistant
- **Low Power Mode**: Implements sleep cycles (60-second intervals) to minimize power consumption
- **Serial Configuration**: Interactive configuration via serial interface with Flash storage persistence
- **Visual Feedback**: OLED display status updates and RGB LED indicators

## Hardware Requirements

- MXChip AZ3166 DevKit (STM32F4-based)
- WiFi network
- MQTT broker (e.g., Mosquitto, Home Assistant)

## Sensors Included

- **HTS221**: Temperature and humidity
- **LPS22HB**: Barometric pressure
- **LSM6DSL**: 6-axis accelerometer and gyroscope
- **LIS2MDL**: 3-axis magnetometer

## Configuration

### First-Time Setup

1. Connect the board via USB
2. Open serial monitor at 115200 baud
3. Press 'C' within 5 seconds of boot to enter configuration mode
4. Enter your WiFi credentials and MQTT broker details:
   - WiFi SSID
   - WiFi Password
   - MQTT Server IP/hostname
   - MQTT Port (default: 1883)
   - MQTT Topic
   - Device ID, Model, and Location

Configuration is saved to Flash memory and persists across reboots.

### Default Configuration

If no valid configuration is found, the device uses placeholder defaults. You **must** configure via serial interface before first use:

```cpp
// In main.cpp - Update these defaults or use serial configuration
strcpy(config.ssid, "YOUR_WIFI_SSID");
strcpy(config.password, "YOUR_WIFI_PASSWORD");
strcpy(config.mqttServer, "YOUR_MQTT_BROKER_IP");
config.mqttPort = 1883;
strcpy(config.mqttTopic, "homeassistant/sensor/az3166/state");
```

## Building and Uploading

### Using PlatformIO

```bash
# Build the project
pio run

# Upload to board
pio run --target upload

# Monitor serial output
pio device monitor
```

### Build Flags

The project disables Azure IoT services via build flags in `platformio.ini`:

- `DISABLE_AZURE_IOT_HUB_TELEMETRY`
- `DISABLE_TELEMETRY`
- `ARDUINO_MAIN_OVERRIDE`
- Additional flags to prevent Azure background services

## MQTT Data Format

The device publishes JSON payloads to the configured MQTT topic:

```json
{
  "temperature": 22.5,
  "humidity": 45.0,
  "pressure": 1013.2,
  "device_id": "SensorStation_01",
  "model": "az3166",
  "location": "Garage",
  "timestamp": 1234567890,
  "last_seen": 1234567890,
  "accel_x": 0.001,
  "accel_y": 0.002,
  "accel_z": 1.0,
  "gyro_x": 0.1,
  "gyro_y": 0.2,
  "gyro_z": 0.3,
  "mag_x": 0.123,
  "mag_y": 0.456,
  "mag_z": 0.789,
  "rssi": -45,
  "uptime": 123456
}
```

## Power Optimization

The device implements a low-power cycle:

1. Wake up and initialize display/LED
2. Connect to MQTT (if not already connected)
3. Read all sensors
4. Publish data with LED feedback (3 green blinks)
5. Turn off LED and clear display
6. Sleep for 60 seconds
7. Repeat

## Temperature Calibration

A -2.5°C correction factor is applied to temperature readings:

```cpp
temperature = temperature - 2.5;
```

Adjust this value in the code if needed for your specific sensor.

## LED Indicators

- **Blue**: Idle/connecting state
- **Green blinks (3x)**: Sending MQTT data
- **Off**: Sleep mode (power saving)

## OLED Display

The OLED shows:

- **Line 0**: Device name or status
- **Line 1**: Temperature and humidity
- **Line 2**: Pressure or connection status
- **Line 3**: MQTT status messages

## Troubleshooting

### WiFi Connection Issues

- Verify SSID and password via serial configuration
- Check that 2.4GHz WiFi is available (AZ3166 doesn't support 5GHz)

### MQTT Connection Issues

- Verify broker IP/hostname is correct
- Check that broker port is accessible
- Verify MQTT credentials if authentication is enabled
- Update `connectMQTT()` function with your broker's username/password

### Serial Configuration Not Working

- Ensure serial monitor baud rate is set to 115200
- Press 'C' immediately after reset/power-on

## Home Assistant Integration

This project is designed for easy integration with Home Assistant. The JSON format is compatible with MQTT sensors.

Example Home Assistant configuration:

```yaml
mqtt:
  sensor:
    - name: "AZ3166 Temperature"
      state_topic: "homeassistant/sensor/az3166/state"
      value_template: "{{ value_json.temperature }}"
      unit_of_measurement: "°C"

    - name: "AZ3166 Humidity"
      state_topic: "homeassistant/sensor/az3166/state"
      value_template: "{{ value_json.humidity }}"
      unit_of_measurement: "%"
```

## License

This project is open source. Feel free to modify and adapt for your needs.

## Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues.

## Credits

- Built for MXChip AZ3166 DevKit
- Uses STM32 HAL libraries for Flash persistence
- Compatible with PlatformIO build system
