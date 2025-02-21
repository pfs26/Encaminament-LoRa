# LoRa Mesh Network Protocol

## Overview
This project implements a **custom LoRa-based mesh network protocol** designed for **low-power, long-range communications** in remote environments. The protocol operates over **LoRa (SX1276 and SX126x transceivers)** using the **RadioHead library**, ensuring efficient packet transmission, relay, and acknowledgment.

The system is built to be **transceiver-agnostic**, allowing easy adaptation to different LoRa modules supported by RadioHead. It features a **static routing mechanism**, **implicit acknowledgment**, and **adaptive retransmission strategies** for reliability.

## Features
- **Custom LoRa MAC Protocol**
  - Unique node addressing and message relay
  - Static routing using **Dijkstra’s algorithm** with link cost based on distance squared
  - Implicit acknowledgments inferred from relayed packets
  - **Binary Exponential Backoff (BEB)** for retransmissions
  - **Adaptive retransmission** (increasing TX power or using redundant nodes)
  - TTL mechanism to prevent routing loops
  - Simple checksum-based error detection
  - Optional message deduplication for reliability
<!---
- **Configurable LoRa Parameters**
  - Frequency: **EU868** MHz
  - Spreading Factor: **SF7–SF12** (adaptive)
  - Bandwidth: **125 kHz**
  - Coding Rate: **4/5**
  - Transmission Power: **Dynamically adjustable**
- **Hardware & Low-Power Optimization**
  - Based on **ESP32-S3**
  - Deep sleep consumption: **~10µA**
  - Estimated battery life: **30+ days** (2500mAh LiPo, 15-min interval TX)
- **Task Scheduling**
  - Uses a **custom task scheduler** (main loop only calls `scheduler_run()`) for non-blocking execution
  - `yield()` ensures background tasks are not blocked by LoRa operations
-->
## Repository Structure
```
├── firmware/		  # ESP32 firmware for LoRa node
│   ├── src/		  # LoRa protocol implementation
│   ├── include/	  # Header files
│   ├── config/		  # LoRa settings
│   └── main.cpp	  # Entry point
├── hardware/		  # Node PCB and enclosure design
├── docs/		      # Project documentation
├── simulations/	# Network simulation scripts
└── README.md	    # This file
```

## API
### LoRa
```cpp
bool LoRa_init(); 
void LoRa_deinit();

lora_tx_error_t LoRa_send(const lora_data_t* data); 
bool LoRa_receive(lora_data_t* data, uint8_t& length);
bool LoRa_isAvailable();
bool LoRa_isBusy();
int16_t LoRa_getLastRSSI();
int16_t LoRa_getLastSNR();
bool LoRa_sleep();
bool LoRa_setFrequency(float frequency);
bool LoRa_setTxPower(int power);
void LoRa_printDebug();

void LoRa_onReceive(lora_callback_t cb);
void LoRa_onSend(lora_callback_t cb);
```
### SHop (Single Hop)
### Routing
### Reliability
### I2C
<!---
### Additional Features
- **Dynamic configuration**: Change frequency, SF, coding rate at runtime
- **Sleep & wakeup**: `LoRa_sleep()` and `LoRa_wakeup()` for power savings

## Getting Started
### Prerequisites
- **ESP32-S3** development board
- **SX1276/SX126x** LoRa module
- **PlatformIO** (recommended) or Arduino IDE
### Installation
1. Clone the repository:
   ```sh
   git clone https://github.com/your-repo/lora-mesh.git
   cd lora-mesh
   ```
2. Install dependencies:
   ```sh
   pio lib install
   ```
3. Configure LoRa settings in `config/lora_config.h`
4. Build & upload firmware:
   ```sh
   pio run --target upload
   ```

## Simulation & Testing
To test network performance and congestion:
```sh
python simulations/simulate_network.py
```
The simulation models random node transmissions, relay probability, and packet loss.

## License

## Contact
-->
