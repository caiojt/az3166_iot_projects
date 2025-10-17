# Debugging Guide for Memory Issues

## Quick Memory Check

Connect to serial monitor (115200 baud) and look for these indicators:

### Healthy Operation:
```
Free RAM: 45000 bytes    <- Initial
Free RAM: 44800 bytes    <- After first loop
Free RAM: 44800 bytes    <- Should stay stable
Free RAM: 44800 bytes    <- Not decreasing
```

### Memory Leak Detected:
```
Free RAM: 45000 bytes    <- Initial
Free RAM: 44500 bytes    <- After first loop
Free RAM: 44000 bytes    <- Decreasing!
Free RAM: 43500 bytes    <- Problem!
```

## Common Issues and Solutions

### Issue: Device Freezes After X Hours

**Symptoms:**
- Serial output stops
- OLED display frozen
- LED stuck in one state

**Check:**
1. Last memory reading before freeze
2. Was WiFi connected?
3. Was MQTT connected?

**Solution Applied:**
- Watchdog timer now auto-resets within 90s
- Check watchdog reset counter in logs

### Issue: WiFi Disconnects

**Symptoms:**
```
WiFi status: 6  (WL_DISCONNECTED)
MQTT connection failed!
```

**Check:**
- Router stability
- Signal strength (RSSI value in JSON)
- Network congestion

**Solution Applied:**
- Automatic WiFi reconnection (up to 20 attempts)
- DNS cache reset on reconnect

### Issue: MQTT Connection Fails

**Symptoms:**
```
TCP connection failed
WiFi status after failed connect: 3
Client connected status: 0
```

**Check:**
1. MQTT broker running? `sudo systemctl status mosquitto`
2. Port 1883 accessible? `telnet 192.168.1.105 1883`
3. Firewall blocking? `sudo ufw status`

**Solution Applied:**
- Connection cleanup before retry
- Proper socket closure
- mqttIPResolved reset on WiFi reconnect

### Issue: Out of Memory

**Symptoms:**
```
Free RAM: 5000 bytes    <- Very low!
Free RAM: 3000 bytes    <- Critical!
```

**Check:**
- Memory trending downward = leak
- Memory stable but low = normal operation

**Solution Applied:**
- Eliminated String heap fragmentation
- Static allocation for JSON buffer
- WiFiClient leak fixes

## Serial Monitor Commands

### Normal Boot Sequence:
```
=== PURE STM32 CODE ===
Wake counter: 0
Watchdog timer initialized (90s timeout)
Loading configuration from Flash...
Configuration loaded from Flash storage
Free RAM: 45234 bytes
Connecting to WiFi...
WiFi connected!
IP: 192.168.1.XXX
```

### Configuration Mode:
```
=== AZ3166 Sensor Station ===
Press 'C' within 5 seconds to enter configuration mode...
```
Press 'C' or 'c' to reconfigure device

### Memory Monitoring Points:
```
Free RAM: XXXXX bytes      <- After config load
Free RAM: XXXXX bytes      <- Before sensor read
Free RAM: XXXXX bytes      <- After MQTT publish
```

## LED Indicators

- **Blue:** Normal operation / connecting
- **Green blinking (3x):** Sending MQTT message
- **Off:** Power saving mode / sleeping

## OLED Display States

```
AZ3166 Sensor      <- Line 0: Device name
T:22.5C H:45%      <- Line 1: Temperature & Humidity
P:1013mbar         <- Line 2: Pressure
MQTT sent!         <- Line 3: Status
```

## Watchdog Behavior

The watchdog timer prevents infinite freezes:

1. **Timeout:** 90 seconds
2. **Refresh Points:**
   - Start of each main loop
   - Before MQTT operations
   - During WiFi reconnection

If device freezes and watchdog expires:
```
(System resets automatically)
=== PURE STM32 CODE ===
Wake counter: X    <- Incremented
```

## Memory Budget (256KB Total RAM)

| Component | Size | Type |
|-----------|------|------|
| System/Heap | ~200KB | Dynamic |
| Stack | ~8KB | Stack |
| Global Variables | ~2KB | Static |
| **Available** | **~40-50KB** | **App** |

### Critical Thresholds:
- **> 40KB:** Healthy
- **20-40KB:** Normal operation
- **10-20KB:** Warning
- **< 10KB:** Critical (may crash)

## Testing Checklist

### Short-term Test (1 hour):
- [ ] Initial free RAM recorded
- [ ] RAM stable after 10 loops
- [ ] RAM stable after 30 loops
- [ ] RAM stable after 60 loops
- [ ] No watchdog resets
- [ ] All MQTT messages sent successfully

### Long-term Test (24 hours):
- [ ] Initial free RAM: ______ bytes
- [ ] RAM after 1 hour: ______ bytes
- [ ] RAM after 6 hours: ______ bytes
- [ ] RAM after 12 hours: ______ bytes
- [ ] RAM after 24 hours: ______ bytes
- [ ] Total watchdog resets: ______
- [ ] Total WiFi reconnects: ______
- [ ] Total MQTT failures: ______

### Expected Results:
- Free RAM should vary by < 1KB over 24 hours
- Zero watchdog resets (unless intentional freeze)
- Occasional WiFi reconnects OK (network dependent)
- MQTT failures should auto-recover

## Troubleshooting Commands

### Check MQTT Broker:
```bash
# On MQTT broker machine
sudo systemctl status mosquitto
sudo tail -f /var/log/mosquitto/mosquitto.log
```

### Monitor MQTT Messages:
```bash
# Subscribe to see messages
mosquitto_sub -h 192.168.1.105 -t "homeassistant/sensor/az3166/state" -v
```

### Check Network:
```bash
# Ping device
ping 192.168.1.XXX  # (device IP from serial)

# Check port
telnet 192.168.1.105 1883
```

## Emergency Recovery

### Device Completely Frozen:
1. Wait 90 seconds (watchdog should reset)
2. If no reset, power cycle device
3. Check serial output for cause

### Configuration Corrupted:
1. Hold 'C' key during boot (first 5 seconds)
2. Reconfigure all settings
3. Settings saved to Flash automatically

### Flash Memory Issues:
1. Re-upload firmware
2. Device will use default config
3. Configure via serial if needed

## Log Analysis

### Good Pattern:
```
Free RAM: 45234 bytes
WiFi connected!
MQTT connected!
Free RAM: 44987 bytes
MQTT sent!
Free RAM: 44981 bytes
Entering deep sleep...
Wake counter: 1
Free RAM: 44985 bytes
MQTT sent!
Free RAM: 44979 bytes
```
**Analysis:** Stable RAM (~250 byte variation is normal)

### Bad Pattern:
```
Free RAM: 45234 bytes
MQTT sent!
Free RAM: 43100 bytes  <- Large drop
MQTT sent!
Free RAM: 41200 bytes  <- Continuing to drop
MQTT sent!
Free RAM: 39100 bytes  <- Memory leak!
```
**Analysis:** Clear memory leak, ~2KB per cycle

## Contact & Support

If issues persist after applying these fixes:

1. Capture full serial log (from boot to freeze)
2. Note free RAM values over time
3. Document WiFi/MQTT environment
4. Check Flash configuration integrity
5. Verify MQTT broker logs

Key data to collect:
- Initial free RAM: ______
- RAM when frozen: ______
- Time to freeze: ______
- Wake counter value: ______
- Last successful operation: ______
