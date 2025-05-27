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
  
- **Application Layer**
  - User-defined and fully customizable
  - Includes built-in low-power management, to be run on port `0x01`
  - Supports any logic needed, such as I2C sensor communication, dynamic configuration, etc.

## Repository Structure
```
├── firmware/          # ESP32 firmware for LoRa node
│   ├── src/           # LoRa protocol implementation
│   ├── include/       # Header files
│   ├── config/        # LoRa settings
│   └── main.cpp       # Entry point
├── memoria/           # Design and code documentation
└── README.md          # This file
```

## Protocol Architecture
The protocol is structured into multiple layers, each handling specific aspects of communication:

### LoRa Physical Layer
- Uses **SX1262 transceivers**
- Configurable modulation settings (SF, BW, TX power)

### MAC Layer
- **CSMA** with **Listen Before Talk (LBT)** to reduce collisions
- **Binary Exponential Backoff (BEB)** if the channel is busy

### Routing Layer
- **Static routing with runtime updates** via API (`RoutingTable_*()` functions)
- **TTL enforcement** to prevent looping packets
- Multiple LoRa interfaces support, for different configurations (raw LoRa vs LoRaWAN, or multiple raw LoRa transceivers)

### LoRaWAN Support
- Nodes with LoRaWAN access can forward packets to the gateway
- Uses LoRaWAN v1.1, using non-volatile storage to store nonces and keys
- OTAA for enhanced security
- Automatic LoRaWAN and raw LoRa mode swapping, storing connection information to avoid OTAA reactivations for each transmission 

### Transport Layer
- **Segment deduplication** using a unique **Segment ID**
- **End-to-end acknowledgment** ensures reliable delivery
- Automatic retransmissions if no acknowledgment is recevied, applying an exponential backoff after each attempt
- Unrealiable segments for quick end-to-end messaging

### Application Layer
- Customisable by the user, depending on requirements
- Uses the full protocol stack underneath
- **Built-in low-power** management application
