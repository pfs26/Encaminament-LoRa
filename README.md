# LoRa Mesh Network Protocol

## Overview
This project implements a **custom LoRa-based mesh network protocol** designed for **low-power, long-range communications** in remote environments. The protocol operates over **LoRa (SX1262 transceivers)** using the **RadioLib library**, ensuring efficient packet transmission, relay, and acknowledgment.

The system features a **static routing mechanism**, **adaptive retransmission strategies**, **CSMA-based channel access**, and **error detection** to enhance network reliability.

## Features
- **Custom LoRa MAC Protocol**
  - Layered protocol structure for modularity and easy extension
  - Unique node addressing and message relay
  - Static routing using **precomputed** routing tables, configurable during runtime
  - **CSMA** to avoid collisions, combined with **Binary Exponential Backoff (BEB)** if channel activity is detected
  - **Adaptive retransmission**, with dynamic transmission power adjustment
  - **TTL mechanism** to prevent routing loops
  - Simple **checksum-based error detection**
  - **Message deduplication** at the transport layer
  - **End-to-end acknowledgments** for reliable message delivery
  
- **LoRaWAN Relaying**
  - Nodes with LoRaWAN access can relay packets to the gateway
  - Handles end-to-end acknowledgment since LoRaWAN gateways cannot do this natively
  
- **Application Layer (I2C Support)**
  - Allows communication with **multiple sensors/devices** over I2C
  - Sensor data can be transmitted across multiple LoRa nodes until reaching a LoRaWAN-capable node for final transmission

## Repository Structure
```
├── firmware/          # ESP32 firmware for LoRa node
│   ├── src/           # LoRa protocol implementation
│   ├── include/       # Header files
│   ├── config/        # LoRa settings
│   └── main.cpp       # Entry point
├── hardware/          # Node PCB and enclosure design
├── docs/              # Project documentation
├── simulations/       # Network simulation scripts
└── README.md          # This file
```

## Protocol Architecture
The protocol is structured into multiple layers, each handling specific aspects of communication:

### LoRa Physical Layer
- Uses **SX1262 transceivers**
- Configurable modulation settings (SF, BW, TX power)
- Parameters can be set **at compile-time** or updated via a **web interface**, stored in **NVS**

### MAC Layer
- **CSMA** with **Listen Before Talk (LBT)** to reduce collisions
- **Binary Exponential Backoff (BEB)** if the channel is busy
- **Static routing with runtime updates** via API (`RoutingTable_*()` functions)
- **TTL enforcement** to prevent looping packets

### LoRaWAN Support
- Nodes with LoRaWAN access can forward packets to the gateway
- Uses LoRaWAN v1.1, using non-volatile storage to store nonces and keys
- OTAA for enhanced security
- Automatic LoRaWAN and raw LoRa mode swapping, storing connection information to avoid OTAA reactivations for each transmission 

### Transport Layer
- **Segment deduplication** using a unique **Segment ID**
- **End-to-end acknowledgment** ensures reliable delivery
- Automatic retransmissions if no acknowledgment is recevied, applying an exponential backoff after each attempt

### Application Layer (I2C Communication)
- Supports communication with **multiple sensors/devices** over I2C
- Any connected sensor can send data through the mesh network
- Data hops through multiple nodes until reaching a LoRaWAN-capable node

<!-- ## Installation
### Prerequisites
- **ESP32-S3** development board
- **SX1262 LoRa transceiver**
- **PlatformIO** for firmware development
- **RadioLib** library for LoRa communication

### Installation
1. Clone the repository:
   ```sh
   git clone https://github.com/pfs26/TFG
   ```
2. Install dependencies:
   ```sh
   platformio run
   ```
3. Flash the firmware:
   ```sh
   platformio run --target upload
   ```

## API
The protocol provides APIs for:
- **Routing Table Management** (`RoutingTable_addRoute(dst, nexthop)`, etc.)
- **Transmission & Acknowledgment Handling**
- **I2C Sensor Integration**

## Testing
Currently tested using:
- **ESP32-S3 nodes**
- **Real-world LoRa communication tests**
- **Packet loss & network performance monitoring**

## Future Improvements
- Implement **multi-packet segmentation** for large data transfers
- Enhance **dynamic routing strategies**
- Improve **security & encryption** for LoRa transmissions
 -->
