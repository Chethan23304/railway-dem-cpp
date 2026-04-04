# Railway DEM/DCM - Kavach DMI (C++)

A complete Diagnostic Event Manager (DEM) and Diagnostic Communication Manager (DCM) implementation for the Indian Railways Kavach safety system, built in C++ with simplified OOP principles following AUTOSAR architecture.

---

## Table of Contents

- [Overview](#overview)
- [Project Structure](#project-structure)
- [Architecture](#architecture)
- [Modules](#modules)
- [Event IDs and DTC Map](#event-ids-and-dtc-map)
- [UDS Services](#uds-services)
- [Modbus TCP Integration](#modbus-tcp-integration)
- [Log Files](#log-files)
- [Build Instructions](#build-instructions)
- [Run Instructions](#run-instructions)
- [ReadByIdentifier Usage](#readbyidentifier-usage)
- [Kavach DMI Qt Integration](#kavach-dmi-qt-integration)
- [Dependencies](#dependencies)

---

## Overview

This project simulates the DEM/DCM layer of the Kavach Train Collision Avoidance System. It detects safety-critical events (overspeed, SPAD, SOS, radio loss, rollback, RFID errors, brake faults, trip mode), logs them to structured CSV files, exposes UDS diagnostic services, and transmits live data to the Kavach DMI simulator over **Modbus TCP** and **Ethernet UDP**.

**Data Flow:**
Kavach Sensors
|
v
KavachConditions  ---- checks 9 safety conditions
|
v
DemCore           ---- manages UDS status byte, event memory, debounce
|
+---> EvtLogger    ---- writes events_failed.csv + per-event history
|
+---> NvmStorage   ---- saves/restores DTCs across power cycles
|
+---> KavachEth    ---- sends UDP frames to Kavach simulator
|
+---> ModbusTcp    ---- sends live DTC data to Kavach DMI (Qt)
|
+---> DCM (DSD/DSP/DSL)  ---- handles UDS requests
---

## Project Structure
railway-dem-cpp/
|
+-- app/
|   +-- diag_app/
|       +-- main.cpp                  # Entry point, scenario, menu
|
+-- bsw/
|   +-- dem/
|   |   +-- include/
|   |   |   +-- StdTypes.hpp          # AUTOSAR base types
|   |   |   +-- DemEventConfig.hpp    # Event IDs, DTC codes, names
|   |   |   +-- DemCore.hpp           # Event memory, UDS status
|   |   |   +-- EvtLogger.hpp         # CSV/TXT log writer
|   |   +-- src/
|   |       +-- DemCore.cpp
|   |       +-- EvtLogger.cpp
|   |
|   +-- dcm/
|   |   +-- include/
|   |   |   +-- DCM_DSL.hpp           # Session layer
|   |   |   +-- DCM_DSP.hpp           # Service processor
|   |   |   +-- DCM_DSD.hpp           # Service dispatcher
|   |   +-- src/
|   |       +-- DCM_DSL.cpp
|   |       +-- DCM_DSP.cpp
|   |       +-- DCM_DSD.cpp
|   |
|   +-- nvm/
|   |   +-- include/
|   |   |   +-- NvmStorage.hpp        # NvM save/restore
|   |   +-- src/
|   |       +-- NvmStorage.cpp
|   |
|   +-- kavach/
|       +-- include/
|       |   +-- KavachEth.hpp         # UDP Ethernet transport
|       |   +-- KavachConditions.hpp  # Safety condition checks
|       |   +-- ModbusTcp.hpp         # Modbus TCP to DMI
|       +-- src/
|           +-- KavachEth.cpp
|           +-- KavachConditions.cpp
|           +-- ModbusTcp.cpp
|
+-- build/
|   +-- logs/
|       +-- events_failed.csv         # Current run failed events
|       +-- rbi_history.csv           # All RBI queries all runs
|       +-- history/
|           +-- 0x00A1_Over_Speeding.csv
|           +-- 0x00A2_SPAD.csv
|           +-- 0x00A3_SOS_Received.csv
|           +-- 0x00A4_Roll_Back.csv
|           +-- 0x00A5_Radio_Loss.csv
|           +-- 0x00A6_Brake_Command.csv
|           +-- 0x00B1_RFID_Tag_Read.csv
|           +-- 0x0007_Trip.csv
|
+-- CMakeLists.txt
+-- .gitignore
+-- README.md
---

## Architecture

### AUTOSAR-Inspired Layered Design
+-----------------------------------------------------+
|                   Application Layer                 |
|              app/diag_app/main.cpp                  |
|         (scenario, menu, RBI prompt)                |
+------------------------+----------------------------+
|
+------------------------v----------------------------+
|               Basic Software (BSW)                  |
|                                                     |
|  +----------+  +----------------------------------+ |
|  |   DEM    |  |             DCM                  | |
|  | DemCore  |  |  DSD -> DSL check -> DSP execute | |
|  | EvtLogger|  |  0x10 / 0x14 / 0x19 / 0x22/0x27 | |
|  +----+-----+  +----------------------------------+ |
|       |                                             |
|  +----v-----+  +----------+  +------------------+  |
|  |   NvM    |  |  Kavach  |  |   ModbusTcp      |  |
|  | Storage  |  |   Eth    |  | 192.168.0.110:    |  |
|  +----------+  +----------+  |    1502           |  |
|                               +------------------+  |
+-----------------------------------------------------+
### DCM Sub-Module Flow
Raw UDS bytes
|
v
DCM_DSD  ---> extract SID
|
v
DCM_DSL  ---> check session access (Default/Extended/Programming)
|
v
DCM_DSP  ---> execute service, build response bytes
|
v
Response bytes returned to caller
---

## Modules

### DemCore
Manages UDS status byte, event memory (8 slots), counter-based debounce, and DTC filtering.

| Method | Description |
|---|---|
| `setEventStatus()` | Report FAILED / PASSED / PREFAILED |
| `getEventStatus()` | Read UDS status byte for an event |
| `clearDTC()` | Clear one DTC or all (0xFFFFFF) |
| `setDTCFilter()` | Set filter mask for 0x19 queries |
| `getNextFilteredDTC()` | Iterate filtered DTCs |
| `mainFunction()` | Periodic tick for time-based debounce |

### EvtLogger
Writes structured logs to CSV and TXT. Maintains per-event segregated history files.

| File | Content | Mode |
|---|---|---|
| `events_failed.csv` | Current run FAILED events | Overwrite each run |
| `events_failed.txt` | Current run formatted | Overwrite each run |
| `rbi_history.csv` | All RBI queries all runs | Append forever |
| `history/0xXXXX_Name.csv` | Per-event query history | Append forever |

### DCM Sub-Modules

| Module | Role |
|---|---|
| **DCM_DSL** | Session state (Default/Extended/Programming), security access seed/key |
| **DCM_DSP** | Executes UDS services, builds response bytes |
| **DCM_DSD** | Entry point: receives raw bytes, checks session via DSL, routes to DSP |

### NvmStorage
Saves all event memory entries to `dem_nvram.bin` on shutdown. Restores on next startup so DTCs survive power cycles.

### KavachEth
Sends 16-byte UDP frames to the Kavach simulator. Each frame carries MsgType, EventId, DTC, Severity, Status, and 8 bytes of payload.

### KavachConditions - 9 Safety Checks

| Condition | Trigger |
|---|---|
| Over Speeding | actual speed > permitted speed |
| SPAD | RED signal (aspect=0) + train moving |
| SOS | SOS button pressed |
| Radio Loss | No packet received for > 5000 ms |
| Roll Back | Backward movement detected |
| RFID Error | Tag not read or wrong location |
| Brake Fault | Brake pressure < 2.0 bar |
| Mode TRIP | Kavach mode = 0x07 |
| Full Supervision | Kavach mode = 0x02 |

### ModbusTcp
Sends live DEM state to the Kavach DMI Qt application over Modbus TCP on 192.168.0.110:1502.

---

## Event IDs and DTC Map

| Event ID | Event Name | DTC Code | Severity |
|---|---|---|---|
| 0x0001 | Stand_By | 0x010101 | MEDIUM |
| 0x0002 | Full_Supervision | 0x010201 | MEDIUM |
| 0x0003 | Limited_Supervision | 0x010301 | MEDIUM |
| 0x0004 | Staff_Responsible | 0x010401 | MEDIUM |
| 0x0005 | Shunting | 0x010501 | MEDIUM |
| 0x0006 | On_Sight | 0x010601 | MEDIUM |
| 0x0007 | Trip | 0x010701 | MEDIUM |
| 0x0008 | Post_Trip | 0x010801 | MEDIUM |
| 0x0009 | Reverse | 0x010901 | MEDIUM |
| 0x000F | System_Failure | 0x010F01 | MEDIUM |
| 0x00A1 | Over_Speeding | 0x00A101 | HIGH |
| 0x00A2 | SPAD | 0x00A201 | HIGH |
| 0x00A3 | SOS_Received | 0x00A301 | HIGH |
| 0x00A4 | Roll_Back | 0x00A401 | HIGH |
| 0x00A5 | Radio_Loss | 0x00A501 | MEDIUM |
| 0x00A6 | Brake_Command | 0x00A601 | HIGH |
| 0x00B1 | RFID_Tag_Read | 0x00B101 | LOW |
| 0x00B2 | Aspect_Change | 0x00B201 | LOW |
| 0x00B3 | MA_Update | 0x00B301 | LOW |

### UDS Status Byte Bits (0x2F = all bits set after FAILED)

| Bit | Mask | Name |
|---|---|---|
| 0 | 0x01 | testFailed (TF) |
| 1 | 0x02 | testFailedThisOpCycle (TFTOC) |
| 2 | 0x04 | pendingDTC (PDTC) |
| 3 | 0x08 | confirmedDTC (CDTC) |
| 5 | 0x20 | testFailedSinceLastClear (TFSLC) |

---

## UDS Services

| SID | Service | Sessions Allowed |
|---|---|---|
| 0x10 | DiagnosticSessionControl | All |
| 0x14 | ClearDiagnosticInformation | Extended only |
| 0x19 | ReadDTCInformation | Default + Extended |
| 0x22 | ReadDataByIdentifier | Default + Extended |
| 0x27 | SecurityAccess | Extended + Programming |

### 0x19 Sub-functions Supported

| SubFunc | Name | Description |
|---|---|---|
| 0x01 | reportNumberByStatusMask | Count DTCs matching mask |
| 0x02 | reportDTCByStatusMask | List all DTCs matching mask |
| 0x06 | reportDTCExtendedDataRecord | Detail for one DTC |

### 0x22 Data Identifiers Supported

| DataId | Name | Value |
|---|---|---|
| 0xF190 | VIN | KAVACH0001234567 |
| 0xF186 | ActiveDiagnosticSession | Current session byte |
| 0xF18C | ECU Serial Number | RAIL-DEM-CPP-001 |
| 0xF100 | Event Memory Count | Number of stored DTCs |

### Security Access (0x27)

- Seed: `0xDEADBEEF` (fixed for simulation)
- Key: `seed XOR 0xCAFEBABE` = `0x14530451`
- Once unlocked: ClearDTC and WriteDataByIdentifier are permitted

---

## Modbus TCP Integration

### Holding Registers

| Register | Name | Description |
|---|---|---|
| 0 | DTC Count | Number of active failed DTCs |
| 1 | DTC1 High | DTC1 upper 16 bits |
| 2 | DTC1 Low | DTC1 lower 16 bits |
| 3 | DTC1 UDS | DTC1 UDS status byte |
| 4-6 | DTC2 | Same layout as DTC1 |
| 7-9 | DTC3 | Same layout as DTC1 |
| 10 | Kavach Mode | Current operating mode |
| 11 | Speed Actual | Actual train speed km/h |
| 12 | Speed Permitted | Permitted speed km/h |
| 13 | DCM Session | Active diagnostic session |

### Coils (Event Flags)

| Coil | Event |
|---|---|
| 0 | Overspeed active |
| 1 | SPAD active |
| 2 | SOS active |
| 3 | Brake fault |
| 4 | Radio loss |
| 5 | Rollback |
| 6 | RFID error |
| 7 | Trip mode |

### Qt DMI Side Requirements

Add `QT += serialbus` to the `.pro` file and implement `QModbusTcpServer` listening on `0.0.0.0:1502`. Connect the `dataWritten` signal to update UI labels whenever the C++ program writes to any register or coil.

---

## Log Files
build/logs/
+-- events_failed.csv        <- current run only (overwritten each run)
+-- events_failed.txt        <- current run formatted
+-- rbi_history.csv          <- all RBI queries across all runs
+-- history/
+-- 0x00A1_Over_Speeding.csv    <- only Over_Speeding queries
+-- 0x00A2_SPAD.csv             <- only SPAD queries
+-- 0x00A3_SOS_Received.csv
+-- 0x00A4_Roll_Back.csv
+-- 0x00A5_Radio_Loss.csv
+-- 0x00A6_Brake_Command.csv
+-- 0x00B1_RFID_Tag_Read.csv
+-- 0x0007_Trip.csv
**View logs in terminal:**
```bash
column -t -s',' build/logs/events_failed.csv
column -t -s',' build/logs/history/0x00A1_Over_Speeding.csv
```

**Open folder in Windows Explorer (WSL):**
```bash
explorer.exe "$(wslpath -w ~/railway-dem-cpp/build/logs)"
```

---

## Build Instructions
```bash
# Install dependencies
sudo apt-get install -y cmake g++ libmodbus-dev

# Clone and build
git clone https://github.com/Chethan23304/railway-dem-cpp.git
cd railway-dem-cpp
mkdir build && cd build
cmake ..
make -j4
```

Expected output:
[100%] Built target rail_kavach_cpp
---

## Run Instructions
```bash
cd build
rm -f dem_nvram.bin logs/events_failed.*
./rail_kavach_cpp
```

After the 9-step scenario completes, a menu appears:
========================================
What would you like to do?
1  or  all  -> Show all failed events
2  or  rbi  -> ReadByIdentifier query
q           -> Quit
Choice:
---

## ReadByIdentifier Usage

Select `2` or type `rbi` at the menu, then enter any Event ID:
Enter Event ID: A1
Output shows current run result plus full history across all previous runs:
+-----------------------------------------------------------------+
|  ReadByIdentifier  EventId: 0x00A1  Over_Speeding              |
+-----------------------------------------------------------------+
|  CURRENT RUN (#3)                                               |
|  DTC Code   : 0x00A101                                          |
|  UDS Status : 0x2F                                              |
|  Severity   : HIGH                                              |
|  Occurrences: 1                                                  |
|  Logged At  : 2026-04-04 09:45:12                               |
|  Status     : FAILED                                            |
+-----------------------------------------------------------------+
|  FULL HISTORY (all runs)                                        |
+-----------------------------------------------------------------+
Run  Timestamp             EventName       DTC        UDS   Occ
1    2026-04-04 09:43:01  Over_Speeding   0x00A101   0x2F   1
2    2026-04-04 09:44:05  Over_Speeding   0x00A101   0x2F   1
3    2026-04-04 09:45:12  Over_Speeding   0x00A101   0x2F   1
Total records found: 3 (across all runs)
+-----------------------------------------------------------------+
Enter `0000` to query all events at once. Enter `b` or `q` to return to menu.

---

## Kavach DMI Qt Integration

When the physical Kavach DMI is available:

**Step 1 - Update IP address:**
```bash
sed -i 's/127.0.0.1/192.168.0.110/g' \
    app/diag_app/main.cpp \
    bsw/kavach/src/ModbusTcp.cpp
cd build && make -j4
```

**Step 2 - Verify network:**
```bash
ping 192.168.0.110
nc -zv 192.168.0.110 1502
```

**Step 3 - Run and verify:**
```bash
./rail_kavach_cpp
python3 ~/verify_modbus.py
```

**Test without real DMI (local simulator):**
```bash
pip3 install pymodbus --break-system-packages
python3 ~/kavach_simulator.py
```
Then in a second terminal run `./rail_kavach_cpp`.

---

## Dependencies

| Dependency | Purpose | Install |
|---|---|---|
| CMake 3.16+ | Build system | `apt-get install cmake` |
| GCC 13+ C++17 | Compiler | `apt-get install g++` |
| libmodbus | Modbus TCP client | `apt-get install libmodbus-dev` |
| pymodbus (optional) | Local DMI simulator | `pip3 install pymodbus` |

---

## Author

**Chethan** - Railway DEM/DCM implementation for Kavach DMI  
GitHub: [Chethan23304](https://github.com/Chethan23304/railway-dem-cpp)
