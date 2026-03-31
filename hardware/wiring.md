# Wiring

## ESP32 Connections

### I2S Microphone (INMP441)
- WS → GPIO 25
- SCK → GPIO 27
- SD → GPIO 33
- VCC → 3.3V
- GND → GND

### RGB LED
- Red → GPIO 13
- Green → GPIO 12
- Blue → GPIO 14

### Vibration Motor
- + → GPIO 4 (via transistor recommended)
- - → GND

## Notes
- Add capacitor across motor to reduce noise
- Boost converter required for battery use