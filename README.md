# SilentSense

![ESP32](https://img.shields.io/badge/ESP32-Embedded-blue)
![TinyML](https://img.shields.io/badge/TinyML-Edge%20AI-green)
![Edge Impulse](https://img.shields.io/badge/Edge%20Impulse-ML-orange)
![IoT](https://img.shields.io/badge/Domain-IoT-blueviolet)
![Status](https://img.shields.io/badge/Status-In%20Progress-yellow)
![Assistive Tech](https://img.shields.io/badge/Application-Assistive%20Tech-success)

Wearable ESP32-based environmental sound classification system using TinyML and Edge Impulse.

## Features
- Real-time environmental sound classification
- RGB LED feedback system
- Vibration motor alerts
- Blynk IoT notifications
- Adaptive detection thresholds

## Hardware
- ESP32 DevKit V1
- INMP441 I2S Microphone
- RGB LED
- Coin vibration motor
- LiPo battery integration (in progress)

## Project Structure
- `firmware/` → ESP32 code + deployed model
- `hardware/` → wiring, PCB, enclosure design
- `docs/` → development notes and logs

## Performance
- Validation Accuracy: 94.5%
- Test Accuracy: 87.14%
- Inference Latency: ~665 ms

## Status
Working prototype 🚧

## Future Scope
- Improved hardware reliability
- Compact wearable enclosure
- Enhanced real-time responsiveness
- Expanded environmental sound categories