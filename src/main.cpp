// Completely bypass Azure framework - pure STM32 + WiFi + MQTT + Sensors
#include <Arduino.h>
#include "AZ3166WiFi.h"
#include "AZ3166WiFiUdp.h"
#include "OledDisplay.h"
#include "RGB_LED.h"
#include "Sensor.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_flash.h"
#include "stm32f4xx_hal_rtc.h"

// Flash storage for configuration (using STM32 internal flash)
#define CONFIG_FLASH_SECTOR FLASH_SECTOR_10 // Use sector 10 for config (128KB sector)
#define CONFIG_FLASH_ADDRESS 0x080C0000     // Start of sector 10

// Configuration structure (stored in Flash memory)
struct DeviceConfig
{
  char magic[4];       // "AZ31" - magic bytes to verify valid config
  char deviceId[32];   // Device name/ID
  char model[16];      // Device model (e.g., "az3166")
  char location[32];   // Device location (e.g., "Garage")
  char ssid[32];       // WiFi SSID
  char password[64];   // WiFi password
  char mqttServer[64]; // MQTT server hostname
  int mqttPort;        // MQTT port
  char mqttTopic[64];  // MQTT topic
  uint8_t checksum;    // Simple checksum
  uint8_t padding[3];  // Explicit padding to ensure word alignment
} __attribute__((packed));

// Default configuration
DeviceConfig config = {
    {'A', 'Z', '3', '1'},                // magic
    "Garage",                            // deviceId
    "az3166",                            // model
    "Garage",                            // location
    "BRZ",                               // ssid - YOUR WIFI NETWORK
    "caio0099",                          // password - YOUR WIFI PASSWORD
    "192.168.1.105",                     // mqttServer - YOUR MQTT BROKER IP
    1883,                                // mqttPort
    "homeassistant/sensor/az3166/state", // mqttTopic - Home Assistant compatible topic
    0,                                   // checksum (calculated later)
    {0, 0, 0}                            // padding
};

// Flash storage functions for persistent configuration
// Note: This implementation uses STM32 HAL Flash functions for real persistence
bool saveConfigToFlash()
{
  // Clear checksum and padding before calculating
  config.checksum = 0;
  memset(config.padding, 0, sizeof(config.padding));

  // Calculate checksum over specific fields manually to avoid struct padding issues
  uint8_t calculatedChecksum = 0;

  // Hash magic bytes (4 bytes)
  for (int i = 0; i < 4; i++)
  {
    calculatedChecksum ^= config.magic[i];
  }

  // Hash device ID (32 bytes)
  for (int i = 0; i < 32; i++)
  {
    calculatedChecksum ^= config.deviceId[i];
  }

  // Hash model (16 bytes)
  for (int i = 0; i < 16; i++)
  {
    calculatedChecksum ^= config.model[i];
  }

  // Hash location (32 bytes)
  for (int i = 0; i < 32; i++)
  {
    calculatedChecksum ^= config.location[i];
  }

  // Hash SSID (32 bytes)
  for (int i = 0; i < 32; i++)
  {
    calculatedChecksum ^= config.ssid[i];
  }

  // Hash password (64 bytes)
  for (int i = 0; i < 64; i++)
  {
    calculatedChecksum ^= config.password[i];
  }

  // Hash MQTT server (64 bytes)
  for (int i = 0; i < 64; i++)
  {
    calculatedChecksum ^= config.mqttServer[i];
  }

  // Hash MQTT port (4 bytes)
  uint8_t *portBytes = (uint8_t *)&config.mqttPort;
  for (int i = 0; i < 4; i++)
  {
    calculatedChecksum ^= portBytes[i];
  }

  // Hash MQTT topic (64 bytes)
  for (int i = 0; i < 64; i++)
  {
    calculatedChecksum ^= config.mqttTopic[i];
  }

  config.checksum = calculatedChecksum;

  Serial.println("Saving configuration to Flash...");
  Serial.print("Calculated checksum: 0x");
  Serial.println(config.checksum, 16);
  Serial.print("Config size: ");
  Serial.println(sizeof(DeviceConfig));

  // Unlock the Flash memory
  HAL_FLASH_Unlock();

  // Erase the sector
  FLASH_EraseInitTypeDef EraseInitStruct;
  uint32_t SectorError = 0;

  EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
  EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
  EraseInitStruct.Sector = CONFIG_FLASH_SECTOR;
  EraseInitStruct.NbSectors = 1;

  if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK)
  {
    Serial.println("Flash erase failed!");
    HAL_FLASH_Lock();
    return false;
  }

  // Write the configuration data word by word
  uint32_t *configData = (uint32_t *)&config;
  uint32_t address = CONFIG_FLASH_ADDRESS;
  int numWords = (sizeof(DeviceConfig) + 3) / 4; // Round up to nearest word

  Serial.print("Writing ");
  Serial.print(numWords);
  Serial.println(" words to Flash...");

  for (int i = 0; i < numWords; i++)
  {
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, configData[i]) != HAL_OK)
    {
      Serial.print("Flash write failed at word ");
      Serial.println(i);
      HAL_FLASH_Lock();
      return false;
    }
    address += 4;
  }

  // Lock the Flash memory
  HAL_FLASH_Lock();

  Serial.println("Configuration saved successfully to Flash!");
  return true;
}

bool loadConfigFromFlash()
{
  Serial.println("Loading configuration from Flash...");

  // Read configuration from Flash
  DeviceConfig *flashConfig = (DeviceConfig *)CONFIG_FLASH_ADDRESS;

  // Check magic bytes
  Serial.print("Magic bytes from Flash: ");
  for (int i = 0; i < 4; i++)
  {
    Serial.print("0x");
    Serial.print(flashConfig->magic[i], 16);
    Serial.print(" ");
  }
  Serial.println();

  if (flashConfig->magic[0] != 'A' || flashConfig->magic[1] != 'Z' ||
      flashConfig->magic[2] != '3' || flashConfig->magic[3] != '1')
  {
    Serial.println("No valid magic bytes found in Flash");
    return false;
  }

  // Calculate checksum using the same field-by-field method as save
  uint8_t calculatedChecksum = 0;

  // Hash magic bytes (4 bytes)
  for (int i = 0; i < 4; i++)
  {
    calculatedChecksum ^= flashConfig->magic[i];
  }

  // Hash device ID (32 bytes)
  for (int i = 0; i < 32; i++)
  {
    calculatedChecksum ^= flashConfig->deviceId[i];
  }

  // Hash model (16 bytes)
  for (int i = 0; i < 16; i++)
  {
    calculatedChecksum ^= flashConfig->model[i];
  }

  // Hash location (32 bytes)
  for (int i = 0; i < 32; i++)
  {
    calculatedChecksum ^= flashConfig->location[i];
  }

  // Hash SSID (32 bytes)
  for (int i = 0; i < 32; i++)
  {
    calculatedChecksum ^= flashConfig->ssid[i];
  }

  // Hash password (64 bytes)
  for (int i = 0; i < 64; i++)
  {
    calculatedChecksum ^= flashConfig->password[i];
  }

  // Hash MQTT server (64 bytes)
  for (int i = 0; i < 64; i++)
  {
    calculatedChecksum ^= flashConfig->mqttServer[i];
  }

  // Hash MQTT port (4 bytes)
  uint8_t *portBytes = (uint8_t *)&flashConfig->mqttPort;
  for (int i = 0; i < 4; i++)
  {
    calculatedChecksum ^= portBytes[i];
  }

  // Hash MQTT topic (64 bytes)
  for (int i = 0; i < 64; i++)
  {
    calculatedChecksum ^= flashConfig->mqttTopic[i];
  }

  Serial.print("Stored checksum: 0x");
  Serial.println(flashConfig->checksum, 16);
  Serial.print("Calculated checksum: 0x");
  Serial.println(calculatedChecksum, 16);

  if (calculatedChecksum != flashConfig->checksum)
  {
    Serial.println("Flash configuration checksum mismatch");
    return false;
  }

  // Copy valid configuration from Flash to RAM
  memcpy(&config, flashConfig, sizeof(DeviceConfig));
  Serial.println("Valid configuration loaded from Flash!");
  return true;
}

// Legacy variables for compatibility
// DNS and MQTT globals
WiFiClient mqttWifiClient;
IPAddress cachedMqttIP;
bool mqttIPResolved = false;

// Sensor variables
DevI2C *i2c;
HTS221Sensor *ht_sensor;
LPS22HBSensor *pressure_sensor;
LSM6DSLSensor *acc_gyro;
LIS2MDLSensor *magnetometer;

// RGB LED instance
RGB_LED rgbLED;

// RTC handle for backup register access
RTC_HandleTypeDef hrtc;

// Wake counter management using RTC backup register
#define WAKE_COUNTER_REGISTER RTC_BKP_DR0
#define MAX_WAKE_COUNT_BEFORE_CONFIG 5

// Function to initialize RTC for backup register access
void initRTC()
{
  __HAL_RCC_PWR_CLK_ENABLE();
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_RTC_ENABLE();
}

// Function to read wake counter from backup register
uint32_t getWakeCounter()
{
  return HAL_RTCEx_BKUPRead(&hrtc, WAKE_COUNTER_REGISTER);
}

// Function to increment and store wake counter
void incrementWakeCounter()
{
  uint32_t count = getWakeCounter() + 1;
  HAL_RTCEx_BKUPWrite(&hrtc, WAKE_COUNTER_REGISTER, count);
}

// Function to reset wake counter
void resetWakeCounter()
{
  HAL_RTCEx_BKUPWrite(&hrtc, WAKE_COUNTER_REGISTER, 0);
}

// Function to perform system reset
void performSystemReset()
{
  Serial.println("Performing system reset to refresh all states...");
  Serial.flush(); // Ensure message is sent before reset
  delay(100);
  NVIC_SystemReset();
}

// Configuration management functions
uint8_t calculateChecksum(const DeviceConfig *cfg)
{
  uint8_t checksum = 0;
  const uint8_t *data = (const uint8_t *)cfg;
  for (int i = 0; i < sizeof(DeviceConfig) - 1; i++)
  { // -1 to exclude checksum field
    checksum ^= data[i];
  }
  return checksum;
}

void loadDefaultConfig()
{
  Serial.println("Loading default configuration...");
  config.magic[0] = 'A';
  config.magic[1] = 'Z';
  config.magic[2] = '3';
  config.magic[3] = '1';
  strcpy(config.deviceId, "Garage");
  strcpy(config.model, "az3166");
  strcpy(config.location, "Garage");
  strcpy(config.ssid, "BRZ");
  strcpy(config.password, "caio0099");
  strcpy(config.mqttServer, "192.168.1.105");
  config.mqttPort = 1883;
  strcpy(config.mqttTopic, "homeassistant/sensor/az3166/state");
  config.checksum = calculateChecksum(&config);
}

String readSerialString(const char *prompt, const char *defaultValue, int maxLength)
{
  Serial.print(prompt);
  Serial.print(" [");
  Serial.print(defaultValue);
  Serial.print("]: ");

  String input = "";
  unsigned long startTime = millis();
  const unsigned long timeout = 30000; // 30 second timeout

  while (millis() - startTime < timeout)
  {
    if (Serial.available())
    {
      char c = Serial.read();
      if (c == '\n' || c == '\r')
      {
        break;
      }
      else if (c == '\b' || c == 127)
      { // Backspace
        if (input.length() > 0)
        {
          input.remove(input.length() - 1);
          Serial.print("\b \b");
        }
      }
      else if (input.length() < maxLength - 1 && c >= 32 && c <= 126)
      {
        input += c;
        Serial.print(c);
      }
    }
    delay(10);
  }

  Serial.println();
  return input.length() > 0 ? input : String(defaultValue);
}

void configureDevice()
{
  Serial.println("\n=== DEVICE CONFIGURATION ===");
  Serial.println("Configure your device settings via serial interface.");
  Serial.println("Press ENTER to keep default values, or type new values.");
  Serial.println("Configuration will timeout in 30 seconds per field.\n");

  // Device ID
  String newDeviceId = readSerialString("Device ID", config.deviceId, sizeof(config.deviceId));
  strcpy(config.deviceId, newDeviceId.c_str());

  // Device Model
  String newModel = readSerialString("Device Model", config.model, sizeof(config.model));
  strcpy(config.model, newModel.c_str());

  // Device Location
  String newLocation = readSerialString("Device Location", config.location, sizeof(config.location));
  strcpy(config.location, newLocation.c_str());

  // WiFi SSID
  String newSSID = readSerialString("WiFi SSID", config.ssid, sizeof(config.ssid));
  strcpy(config.ssid, newSSID.c_str());

  // WiFi Password
  String newPassword = readSerialString("WiFi Password", config.password, sizeof(config.password));
  strcpy(config.password, newPassword.c_str());

  // MQTT Server
  String newMqttServer = readSerialString("MQTT Server", config.mqttServer, sizeof(config.mqttServer));
  strcpy(config.mqttServer, newMqttServer.c_str());

  // MQTT Port
  String newMqttPort = readSerialString("MQTT Port", String(config.mqttPort).c_str(), 8);
  config.mqttPort = newMqttPort.toInt();
  if (config.mqttPort <= 0 || config.mqttPort > 65535)
  {
    config.mqttPort = 1883;
  }

  // MQTT Topic
  String newMqttTopic = readSerialString("MQTT Topic", config.mqttTopic, sizeof(config.mqttTopic));
  strcpy(config.mqttTopic, newMqttTopic.c_str());

  // Update checksum
  config.checksum = calculateChecksum(&config);

  Serial.println("\n=== CONFIGURATION SUMMARY ===");
  Serial.print("Device ID: ");
  Serial.println(config.deviceId);
  Serial.print("Device Model: ");
  Serial.println(config.model);
  Serial.print("Device Location: ");
  Serial.println(config.location);
  Serial.print("WiFi SSID: ");
  Serial.println(config.ssid);
  Serial.print("WiFi Password: ");
  Serial.println("***hidden***");
  Serial.print("MQTT Server: ");
  Serial.println(config.mqttServer);
  Serial.print("MQTT Port: ");
  Serial.println(config.mqttPort);
  Serial.print("MQTT Topic: ");
  Serial.println(config.mqttTopic);

  // Save configuration to Flash memory
  if (saveConfigToFlash())
  {
    Serial.println("Configuration saved to Flash memory!");
  }
  else
  {
    Serial.println("Warning: Failed to save configuration to Flash!");
  }
}

bool checkForConfigurationMode()
{
  Serial.println("\n=== AZ3166 Sensor Station ===");
  Serial.println("Press 'C' within 5 seconds to enter configuration mode...");

  unsigned long startTime = millis();
  while (millis() - startTime < 5000)
  {
    if (Serial.available())
    {
      char c = Serial.read();
      if (c == 'C' || c == 'c')
      {
        Serial.println("Entering configuration mode...");
        return true;
      }
    }
    delay(100);
  }

  Serial.println("Starting with current configuration...");
  return false;
}

// Simplified DNS Resolution - use direct IP for now, add full DNS later
bool resolveHostname(const char *hostname, IPAddress &ip)
{
  Serial.print("Resolving hostname: ");
  Serial.println(hostname);

  // Check if it's already an IP address - support both old and new IP
  if (strcmp(hostname, "192.168.1.105") == 0)
  {
    ip = IPAddress(192, 168, 1, 105);
    Serial.print("Using direct IP: ");
    Serial.println(ip);
    return true;
  }

  if (strcmp(hostname, "192.168.1.111") == 0)
  {
    ip = IPAddress(192, 168, 1, 111);
    Serial.print("Using direct IP (old): ");
    Serial.println(ip);
    return true;
  }

  // For other known hosts, add them here
  if (strcmp(hostname, "mqtt.dcasati.net") == 0)
  {
    ip = IPAddress(172, 16, 5, 241);
    Serial.print("Using cached IP: ");
    Serial.println(ip);
    return true;
  }

  Serial.println("Hostname not in cache");
  return false;
}

// Simple MQTT Connect
bool connectMQTT()
{
  // Use cached IP if available
  IPAddress targetIP;
  if (mqttIPResolved)
  {
    targetIP = cachedMqttIP;
    Serial.print("Using cached MQTT IP: ");
    Serial.println(targetIP);
  }
  else
  {
    Serial.println("Resolving MQTT server IP...");
    if (resolveHostname(config.mqttServer, targetIP))
    {
      cachedMqttIP = targetIP;
      mqttIPResolved = true;
    }
    else
    {
      Serial.println("Failed to resolve MQTT server");
      return false;
    }
  }

  // Test basic connectivity first
  Serial.println("Testing basic network connectivity...");
  WiFiClient testClient;
  if (testClient.connect(targetIP, config.mqttPort))
  {
    Serial.println("Basic connectivity test passed (port 1883)");
    testClient.stop();
    delay(100); // Small delay before main connection
  }
  else
  {
    Serial.println("Basic connectivity test failed (port 1883)");
  }

  Serial.print("Connecting to MQTT broker at ");
  Serial.print(targetIP);
  Serial.print(":");
  Serial.println(config.mqttPort);

  Serial.println("Attempting TCP connection...");
  Serial.print("WiFi status before connect: ");
  Serial.println(WiFi.status());

  if (!mqttWifiClient.connect(targetIP, config.mqttPort))
  {
    Serial.println("TCP connection failed");
    Serial.print("WiFi status after failed connect: ");
    Serial.println(WiFi.status());
    Serial.print("Client connected status: ");
    Serial.println(mqttWifiClient.connected());
    return false;
  }

  Serial.println("TCP connected, sending MQTT CONNECT...");

  // Build MQTT CONNECT packet
  uint8_t packet[128];
  int pos = 0;

  // Fixed header
  packet[pos++] = 0x10; // CONNECT packet type
  int lenPos = pos++;   // Length placeholder

  // Variable header
  packet[pos++] = 0x00;
  packet[pos++] = 0x06; // Protocol name length
  packet[pos++] = 'M';
  packet[pos++] = 'Q';
  packet[pos++] = 'I';
  packet[pos++] = 's';
  packet[pos++] = 'd';
  packet[pos++] = 'p';
  packet[pos++] = 0x03; // Protocol version (MQTT 3.1)
  packet[pos++] = 0xC2; // Connect flags (clean session + username + password)
  packet[pos++] = 0x00;
  packet[pos++] = 0x3C; // Keep alive (60 seconds)

  // Payload - Client ID
  int idLen = strlen(config.deviceId);
  packet[pos++] = 0x00;
  packet[pos++] = idLen;
  memcpy(&packet[pos], config.deviceId, idLen);
  pos += idLen;

  // Payload - Username (azure_iot)
  const char *username = "azure_iot";
  int usernameLen = strlen(username);
  packet[pos++] = 0x00;
  packet[pos++] = usernameLen;
  memcpy(&packet[pos], username, usernameLen);
  pos += usernameLen;

  // Payload - Password (Zelda646464$$$)
  const char *password = "Zelda646464$$$";
  int passwordLen = strlen(password);
  packet[pos++] = 0x00;
  packet[pos++] = passwordLen;
  memcpy(&packet[pos], password, passwordLen);
  pos += passwordLen;

  // Set remaining length
  packet[lenPos] = pos - 2;

  // Send packet
  Serial.print("Sending MQTT CONNECT packet (");
  Serial.print(pos);
  Serial.print(" bytes): ");
  for (int i = 0; i < pos; i++)
  {
    Serial.print("0x");
    if (packet[i] < 16)
      Serial.print("0");
    Serial.print(packet[i], 16); // 16 for hex format
    Serial.print(" ");
  }
  Serial.println();

  mqttWifiClient.write(packet, pos);
  Serial.println("Packet sent, waiting for CONNACK...");

  // Wait for CONNACK
  unsigned long timeout = millis() + 5000;
  Serial.println("Waiting for CONNACK response...");

  // Wait for at least some data
  while (millis() < timeout && mqttWifiClient.available() == 0)
  {
    delay(10);
  }

  if (mqttWifiClient.available() > 0)
  {
    Serial.print("Received ");
    Serial.print(mqttWifiClient.available());
    Serial.println(" bytes from broker");

    // Give it a moment for the complete packet to arrive
    delay(50);

    Serial.print("Total bytes available: ");
    Serial.println(mqttWifiClient.available());
  }

  if (mqttWifiClient.available() >= 4)
  {
    uint8_t response[4];
    mqttWifiClient.read(response, 4);

    Serial.print("CONNACK response: ");
    for (int i = 0; i < 4; i++)
    {
      Serial.print("0x");
      if (response[i] < 16)
        Serial.print("0");
      Serial.print(response[i], 16);
      Serial.print(" ");
    }
    Serial.println();

    if (response[0] == 0x20 && response[3] == 0x00)
    {
      Serial.println("MQTT connected successfully!");
      return true;
    }
    else
    {
      Serial.print("MQTT CONNACK failed, return code: ");
      Serial.println(response[3]);
    }
  }
  else if (mqttWifiClient.available() >= 1)
  {
    // Try to read whatever we got
    uint8_t partialResponse[4] = {0};
    int bytesRead = mqttWifiClient.read(partialResponse, mqttWifiClient.available());
    Serial.print("Received CONNACK (");
    Serial.print(bytesRead);
    Serial.println(" bytes): ");
    for (int i = 0; i < bytesRead; i++)
    {
      Serial.print("0x");
      if (partialResponse[i] < 16)
        Serial.print("0");
      Serial.print(partialResponse[i], 16);
      Serial.print(" ");
    }
    Serial.println();

    // If we got 0x20, that's the CONNACK message type - treat as success
    if (partialResponse[0] == 0x20)
    {
      Serial.println("MQTT connection successful (broker sent CONNACK 0x20)!");
      return true;
    }
  }
  else
  {
    Serial.println("MQTT CONNACK timeout - insufficient data");
  }

  mqttWifiClient.stop();
  return false;
}

// Simple MQTT Publish
bool publishMQTT(const char *topic, const char *payload)
{
  if (!mqttWifiClient.connected())
  {
    Serial.println("MQTT not connected");
    return false;
  }

  int topicLen = strlen(topic);
  int payloadLen = strlen(payload);

  // Calculate remaining length (topic length field + topic + payload)
  int remainingLength = 2 + topicLen + payloadLen;

  // Build PUBLISH packet
  uint8_t packet[1024]; // Increased buffer size for large JSON payloads
  int pos = 0;

  // Fixed header
  packet[pos++] = 0x30; // PUBLISH packet type (QoS 0, no retain, no dup)

  // Variable length encoding for remaining length
  if (remainingLength < 128)
  {
    packet[pos++] = remainingLength;
  }
  else if (remainingLength < 16384)
  {
    packet[pos++] = (remainingLength % 128) | 0x80;
    packet[pos++] = remainingLength / 128;
  }
  else
  {
    // For very large packets, we'd need more bytes, but this should be sufficient
    Serial.println("MQTT payload too large");
    return false;
  }

  // Topic length (MSB, LSB)
  packet[pos++] = (topicLen >> 8) & 0xFF;
  packet[pos++] = topicLen & 0xFF;

  // Topic
  memcpy(&packet[pos], topic, topicLen);
  pos += topicLen;

  // Payload (no length field for payload in PUBLISH packets)
  memcpy(&packet[pos], payload, payloadLen);
  pos += payloadLen;

  // Debug output
  Serial.print("MQTT PUBLISH packet (");
  Serial.print(pos);
  Serial.print(" bytes): Topic=");
  Serial.print(topic);
  Serial.print(", Payload size=");
  Serial.println(payloadLen);

  mqttWifiClient.write(packet, pos);
  Serial.println("MQTT message published");
  return true;
}

// Custom main function to bypass Azure framework
int main()
{
  // Initialize hardware
  Serial.begin(115200);
  delay(1000);

  Serial.println("=== PURE STM32 CODE ===");

  // Initialize RTC for backup register access
  initRTC();

  // Check wake counter to determine if we just woke up from sleep
  uint32_t wakeCount = getWakeCounter();
  Serial.print("Wake counter: ");
  Serial.println(wakeCount);

  // If this is not a fresh boot (wake count > 0), increment the counter
  if (wakeCount > 0)
  {
    Serial.println("Waking up from sleep cycle...");
    incrementWakeCounter();
  }

  // Try to load configuration from Flash first
  if (!loadConfigFromFlash())
  {
    // If no valid config found, load defaults
    loadDefaultConfig();
    Serial.println("Using default configuration (Flash storage empty/invalid)");
  }
  else
  {
    Serial.println("Configuration loaded from Flash storage");

    // Force update if Flash has old IP address
    if (strcmp(config.mqttServer, "192.168.1.111") == 0)
    {
      Serial.println("Detected old MQTT server IP in Flash - updating to 192.168.1.105");
      strcpy(config.mqttServer, "192.168.1.105");
      config.checksum = calculateChecksum(&config);
      saveConfigToFlash();
      Serial.println("Configuration updated in Flash!");
    }
  }

  // Check if user wants to configure the device
  // Only allow configuration mode during fresh boot or if wake count exceeds threshold
  if (wakeCount == 0 || wakeCount >= MAX_WAKE_COUNT_BEFORE_CONFIG)
  {
    if (checkForConfigurationMode())
    {
      resetWakeCounter(); // Reset counter after configuration
      configureDevice();
    }
  }
  else
  {
    Serial.println("Skipping configuration check (wake from sleep)");
  }

  // Show current configuration
  Serial.println("\n=== CURRENT CONFIGURATION ===");
  Serial.print("Device ID: ");
  Serial.println(config.deviceId);
  Serial.print("WiFi SSID: ");
  Serial.println(config.ssid);
  Serial.print("MQTT Server: ");
  Serial.println(config.mqttServer);
  Serial.print("MQTT Port: ");
  Serial.println(config.mqttPort);
  Serial.print("MQTT Topic: ");
  Serial.println(config.mqttTopic);
  Serial.println();

  // Aggressively disable any Azure background services
  Serial.println("Disabling Azure background services...");

// Disable any HTTP/telemetry services that might auto-start
#ifdef DISABLE_AZURE_HTTP
  Serial.println("Azure HTTP disabled via build flag");
#endif

  // Initialize OLED display
  Serial.println("Initializing OLED display");
  Screen.init();

  // Initialize I2C and sensors
  Serial.println("Initializing sensors...");
  i2c = new DevI2C(D14, D15);

  ht_sensor = new HTS221Sensor(*i2c);
  ht_sensor->init(NULL);
  ht_sensor->enable();

  pressure_sensor = new LPS22HBSensor(*i2c);
  pressure_sensor->init(NULL);

  acc_gyro = new LSM6DSLSensor(*i2c, D4, D5);
  acc_gyro->init(NULL);
  acc_gyro->enableAccelerator();
  acc_gyro->enableGyroscope();

  magnetometer = new LIS2MDLSensor(*i2c);
  magnetometer->init(NULL);

  Serial.println("All sensors initialized: temp, humidity, pressure, accelerometer, gyroscope, magnetometer!");

  // Simple WiFi connection without Azure framework
  Serial.println("Connecting to WiFi...");
  Screen.print(3, "WiFi connecting...");
  WiFi.begin(config.ssid, config.password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20)
  {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("\nWiFi connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Screen.print(3, "WiFi connected!");
  }
  else
  {
    Serial.println("\nWiFi failed!");
    Screen.print(3, "WiFi failed!");
  }

  // Main loop with sensor reading and MQTT publishing
  int counter = 0;
  bool mqttConnected = false;

  while (1)
  {
    // Wake up display and LED
    Screen.init();
    Screen.print(0, "AZ3166 Sensor");
    Screen.print(1, "Waking up...");
    Screen.print(2, "Connecting...");

    // Set LED to blue
    rgbLED.setColor(0, 0, 255);

    unsigned long now = millis();

    // Try to connect MQTT if not connected
    if (!mqttConnected && WiFi.status() == WL_CONNECTED)
    {
      Serial.println("Attempting MQTT connection...");
      Screen.print(2, "MQTT connecting...");
      mqttConnected = connectMQTT();
      if (mqttConnected)
      {
        Serial.println("MQTT connected!");
        Screen.print(2, "MQTT connected!");
      }
      else
      {
        Serial.println("MQTT connection failed!");
        Screen.print(2, "MQTT failed!");
        delay(2000);
      }
    }

    // Read sensors and publish immediately
    {
      Serial.print("\n=== Sensor Reading #");
      Serial.print(counter);
      Serial.println(" ===");

      // Read Temperature and Humidity
      float temperature, humidity;
      ht_sensor->getTemperature(&temperature);
      ht_sensor->getHumidity(&humidity);

      // Apply temperature correction factor
      temperature = temperature - 2.5;

      // Read Pressure
      float pressure;
      pressure_sensor->getPressure(&pressure);

      // Read Accelerometer
      int axes[3];
      acc_gyro->getXAxes(axes);
      float accel_x = axes[0] / 1000.0f; // Convert to g
      float accel_y = axes[1] / 1000.0f;
      float accel_z = axes[2] / 1000.0f;

      // Read Gyroscope
      int gyro_axes[3];
      acc_gyro->getGAxes(gyro_axes);
      float gyro_x = gyro_axes[0] / 1000.0f; // Convert to dps
      float gyro_y = gyro_axes[1] / 1000.0f;
      float gyro_z = gyro_axes[2] / 1000.0f;

      // Read Magnetometer
      int mag_axes[3];
      magnetometer->getMAxes(mag_axes);
      float mag_x = mag_axes[0] / 1000.0f; // Convert to gauss
      float mag_y = mag_axes[1] / 1000.0f;
      float mag_z = mag_axes[2] / 1000.0f;

      // Update display
      char tempStr[32];
      sprintf(tempStr, "T:%.1fC H:%.0f%%", temperature, humidity);
      Screen.print(1, tempStr);

      char pressStr[32];
      sprintf(pressStr, "P:%.0fmbar", pressure);
      Screen.print(2, pressStr);

      Screen.print(0, "Garage");

      // Create JSON payload optimized for Home Assistant MQTT
      char jsonPayload[512];

      // Create ISO 8601 timestamp for Home Assistant compatibility
      unsigned long timestamp_seconds = now / 1000;
      char iso_timestamp[32];
      sprintf(iso_timestamp, "%lu", timestamp_seconds);

      sprintf(jsonPayload,
              "{"
              "\"temperature_02\":%.2f,"
              "\"humidity_02\":%.2f,"
              "\"pressure_02\":%.2f,"
              "\"device_id\":\"%s\","
              "\"model\":\"%s\","
              "\"location\":\"%s\","
              "\"timestamp\":%s,"
              "\"last_seen\":%lu,"
              "\"accel_x\":%.3f,"
              "\"accel_y\":%.3f,"
              "\"accel_z\":%.3f,"
              "\"gyro_x\":%.2f,"
              "\"gyro_y\":%.2f,"
              "\"gyro_z\":%.2f,"
              "\"mag_x\":%.3f,"
              "\"mag_y\":%.3f,"
              "\"mag_z\":%.3f,"
              "\"rssi\":%d,"
              "\"uptime\":%lu"
              "}",
              temperature, humidity, pressure,
              config.deviceId, config.model, config.location, iso_timestamp, now,
              accel_x, accel_y, accel_z,
              gyro_x, gyro_y, gyro_z,
              mag_x, mag_y, mag_z,
              WiFi.RSSI(), now);

      // Only print the JSON payload that will be sent
      Serial.println("Sending JSON payload:");
      Serial.println(jsonPayload);

      // Publish to MQTT if connected
      if (mqttConnected)
      {
        // Blink green LED 3 times before sending
        for (int i = 0; i < 3; i++)
        {
          rgbLED.setColor(0, 255, 0); // Green
          delay(200);
          rgbLED.setColor(0, 0, 0); // Off
          delay(200);
        }

        bool publishSuccess = publishMQTT(config.mqttTopic, jsonPayload);

        if (publishSuccess)
        {
          Screen.print(3, "MQTT sent!");
          delay(2000); // Show success message for 2 seconds
        }
        else
        {
          Screen.print(3, "MQTT failed!");
          mqttConnected = false; // Reconnect next time
          delay(2000);           // Show error message for 2 seconds
        }

        // Turn off LED to save power
        rgbLED.setColor(0, 0, 0);
      }
      else
      {
        Screen.print(3, "MQTT not connected");
        delay(2000);
      }

      counter++;
    }

    // Enter deep sleep mode to save power
    Serial.println("Entering deep sleep for 60 seconds...");
    Screen.print(0, "Sleeping...");
    Screen.print(1, "60 seconds");
    Screen.print(2, "Power saving");
    Screen.print(3, "Will reset after");
    delay(1000); // Give time to display message

    // Turn off LED
    rgbLED.setColor(0, 0, 0);

    // Clean up display
    Screen.clean();

    // Set wake counter to indicate we're entering sleep mode
    if (wakeCount == 0)
    {
      // First time entering sleep, set counter to 1
      HAL_RTCEx_BKUPWrite(&hrtc, WAKE_COUNTER_REGISTER, 1);
    }

    // Deep sleep for 60 seconds (60000 milliseconds)
    // After sleep, perform system reset to refresh all states and prevent MQTT issues
    delay(60000);

    Serial.println("Waking up from sleep - performing system reset...");

    // Perform system reset to restart fresh (this fixes the MQTT connection issues)
    performSystemReset();
  }

  return 0;
}

// Override Arduino setup/loop to prevent them from running
void setup()
{
  // This should never be called
}

void loop()
{
  // This should never be called
}
