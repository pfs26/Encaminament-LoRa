# LoRa Static Routing Protocol for Monitoring Applications
This work presents the design and implementation of a modular and layered communication protocol based on LoRa and static routing, intended for monitoring device networks in extensive or isolated environments, where human presence is limited. The goal is to reduce the dependency on LoRaWAN gateways, taking advantage of the fact that devices are usually static and allow the use of predefined communication routes.

An application optimized for linear networks -a topology particularly useful in railway lines or tunnels- has been developed based on this protocol. It focuses on minimizing the energy consumption of the devices through a synchronization strategy between nodes, which allows coordinating transmissions and reducing the active time of the devices, without compromising reliability or the routing mechanisms provided by the protocol. The conducted tests have demonstrated that the proposed solution is viable, scalable, and efficient in environments where energy consumption and coverage are critical.

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

- **Transport Layer**
  - Allows for optional end-to-end reliability
  - Segment deduplication 

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
├── tests/             # Tests and results, done during validation
├── memoria/           # Design and code documentation (.tex format)
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
