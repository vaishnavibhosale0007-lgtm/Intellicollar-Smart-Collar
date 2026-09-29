# IntelliCollar

## Smart Collar for Livestock Health Tracking and Monitoring

IntelliCollar is a lightweight, low-power embedded system developed for livestock health tracking and monitoring.

The system uses an ESP32-based architecture along with GPS, GSM communication and the Blynk Cloud Platform.

## Key Features

- Livestock health monitoring
- Location tracking
- ESP32-based embedded system
- GPS integration
- GSM communication
- Cloud-based monitoring
- Low-power embedded-system design

## Technologies Used

- ESP32
- GPS Module
- GSM Communication
- Blynk Cloud Platform
- Embedded C

## System Architecture

Livestock Monitoring
        ↓
     Sensors
        ↓
      ESP32
        ↓
 ┌──────┴──────┐
 ↓             ↓
GPS           GSM
 ↓             ↓
Location     Communication
        ↓
   Blynk Cloud
        ↓
 Remote Monitoring
 
Hardware
ESP32 Development Board
GPS Module
GSM Communication Module
Monitoring Sensors
Battery / Power Supply
Collar Prototype

Software
Embedded C
Arduino IDE
Blynk Cloud Platform

Working
Monitoring sensors acquire the required livestock-related data.
The ESP32 processes the collected information.
GPS provides location information.
GSM communication is used to transmit the required information.
Blynk Cloud provides remote monitoring of the system.
Blynk Cloud

The project uses the Blynk Cloud Platform for remote monitoring.

Dashboard screenshots and configuration information will be added to this repository.

Project Structure
firmware/       → ESP32 embedded firmware
hardware/       → Circuit diagrams and hardware information
blynk/          → Blynk dashboard information
docs/           → Project documentation
testing/        → Testing information
results/        → Project photographs and results

Recognition

The IntelliCollar project was presented at the AVISHKAR 2025 Research Competition and secured 1st place at the college level.

The project also received Copyright Registration.

Future Scope
Additional livestock health sensors
Improved location tracking
Advanced health-condition monitoring
Enhanced cloud analytics
Improved power optimization
Author

Vaishnavi Bhosale

Electronics and Telecommunication Engineering
D. Y. Patil College of Engineering, Pune
