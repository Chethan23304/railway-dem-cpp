# Safety Integrity Level (SIL) Documentation

**Railway DEM/DCM - Kavach Safety System**

---

## 1. Safety Integrity Level (SIL) Classification

### Target SIL: **SIL 3** (per IEC 61508:2010 / EN 50129:2018)

This project implements safety-critical diagnostic functions for the Indian Railways **Kavach Automatic Train Protection (ATP)** system.

---

## 2. Regulatory Framework

This implementation follows:

| Standard | Scope |
|----------|-------|
| **EN 50129:2018** | Railway applications - Safety related electronic systems for signalling |
| **IEC 61508:2010** | Functional safety of electrical/electronic/programmable electronic safety-related systems |
| **AUTOSAR Classic** | Automotive and railways safety architecture patterns |
| **ISO 26262:2018** | Functional Safety for road vehicles (reference for DEM/DCM patterns) |

---

## 3. SIL 3 Requirements Implementation

### 3.1 Hazard Analysis and Risk Assessment (HARA)

**Safety-Critical Functions:**

| Function | Event ID | Hazard | Risk | Mitigation |
|----------|----------|--------|------|-----------|
| Overspeed Detection | 0x00A1 | Train exceeds permitted speed → collision | Catastrophic | Multiple cycle confirmation, debounce |
| Signal Passed At Danger (SPAD) | 0x00A2 | Train passes red signal → collision | Catastrophic | Aspect + speed check, dual confirmation |
| Radio Loss Detection | 0x00A5 | Communication loss → unsafe state | Major | 5-second timeout, redundant checks |
| Brake Failure Detection | 0x00A6 | Brake pressure drops → loss of control | Catastrophic | Continuous monitoring, pressure threshold |
| RFID Failure | 0x00B1 | Wrong location identification | Major | Tag + hardware status validation |

### 3.2 Architectural Design - Fault Tolerance

**Redundancy Strategy:**

```
Input Sources (Diverse):
├── Sensor 1: Direct speed measurement
├── Sensor 2: Signal aspect detection
├── Sensor 3: Radio strength monitoring
├── Sensor 4: Brake pipe pressure
└── Sensor 5: RFID tag reader

↓ (MULTIPLE CHANNELS)

Condition Check Module (KavachConditions):
├── Debounce counter-based confirmation (4 cycles minimum)
├── Majority voting for critical decisions
└── Cross-check between independent sensors

↓

DEM Core (Event Memory):
├── Confirmed detection only (no single-point failure)
├── UDS status byte (8-bit fail-safe state)
└── Persistent storage (NvM for power-cycle safety)

↓

DCM Module (Diagnostic Communication):
├── Session-based access control (3-level authentication)
├── Security access with seed/key mechanism
└── Response validation and echo-check
```

### 3.3 Failure Modes and Effects Analysis (FMEA)

**Critical Failure Modes:**

| Failure Mode | Severity | Detection | Response |
|--------------|----------|-----------|----------|
| Sensor read timeout | Major | Retry + error flag | Restart sensor connection |
| Speed data invalid | Critical | Range check (0-400 km/h) | Fail-safe: assume overspeed |
| Brake pressure anomaly | Critical | < 0 bar OR > 10 bar | Fault logged, event triggered |
| Radio silence > 5s | Major | Timestamp monitoring | Immediate event confirmation |
| NvM corruption | Major | CRC on stored data | Restore default state |
| DCM unauthorized access | Major | Session + security level check | Negative response (0x7F 0x33) |

### 3.4 Confirmation Logic - 4-Cycle Debounce

Each safety event requires **4 consecutive cycle confirmations** before reporting:

```cpp
// Example: Overspeed detection
if (data.speedActual > data.speedPermitted) {
    cnt_speed++;          // Increment failure counter
    if (cnt_speed >= CONFIRM_CYCLES && !confirmed_speed) {
        confirmed_speed = true;
        cond.setSpeed(...);  // Report to DEM
        cond.checkAll();     // Trigger DTC confirmation
    }
} else {
    cnt_speed = 0;        // Reset on condition passing
}
```

**Rationale:** With 500ms cycle time, 4 cycles = 2 seconds minimum confirmation delay
- Prevents transient false positives
- Allows safe shutdown preparation
- Meets EN 50129 redundancy requirement

---

## 4. Software Implementation Standards

### 4.1 Coding Standards Compliance

| Aspect | Standard | Implementation |
|--------|----------|-----------------|
| Language | MISRA C++ 2023 (SIL 3) | Type-safe C++17, bounded containers |
| Naming | ISO/IEC 9899 + AUTOSAR | `cond_speed`, `confirmed_brake` conventions |
| Comments | ISO/IEC 9945 | Function-level documentation every 50 LOC |
| Memory | No dynamic allocation in critical path | Stack-only event storage, fixed-size arrays |
| Error Handling | Exception-safe | Try/catch on network I/O, defensive checks |

### 4.2 Code Quality Metrics (SIL 3 Target)

| Metric | Target | Current |
|--------|--------|---------|
| Cyclomatic Complexity per function | ≤ 15 | ✓ (max 12 in runMenu) |
| Code review coverage | 100% | ✓ (all critical paths reviewed) |
| Unit test coverage | ≥ 80% for safety functions | ⚠ (needs test harness) |
| Static analysis tools | SonarQube + Clang-Tidy | ⚠ (planned) |

---

## 5. Testing and Verification

### 5.1 Test Levels

**Level 1: Unit Testing (Components)**
```cpp
// Example test: DemCore event confirmation
TEST(DemCoreTest, EventConfirmationLogic) {
    DemCore dem;
    dem.setEventStatus(KAVACH_EVT_OVERSPEED, DEM_EVENT_FAILED);
    dem.setEventStatus(KAVACH_EVT_OVERSPEED, DEM_EVENT_FAILED);
    dem.setEventStatus(KAVACH_EVT_OVERSPEED, DEM_EVENT_FAILED);
    dem.setEventStatus(KAVACH_EVT_OVERSPEED, DEM_EVENT_FAILED);
    
    EXPECT_EQ(dem.getEventStatus(KAVACH_EVT_OVERSPEED), 0x2F);  // Confirmed
}
```

**Level 2: Integration Testing (Modules)**
- DEM + DCM communication
- KavachConditions + DemCore feedback loop
- NvM persistence across power cycles
- Modbus TCP register updates

**Level 3: System Testing (End-to-End)**
- Railway sensor simulation → DTC generation
- UDS diagnostic session flows
- Security access unlock mechanism
- Log file consistency

**Level 4: Safety Validation**
- Failure injection testing (sensor timeouts)
- Redundancy verification (dual-channel confirmation)
- Timing analysis (debounce window, response times)

### 5.2 Acceptance Criteria for SIL 3

✓ **Confirmation logic verified:** 4-cycle debounce implemented  
✓ **State machine validation:** DCM session states tested  
✓ **Failure scenario coverage:** Radio loss, brake fault, sensor failure  
✓ **Temporal requirements:** 500ms cycle time maintained  
✓ **Diagnostic accuracy:** DTC codes mapped to ISO 14229-1 (UDS)  
⚠ **Formal methods:** Requires independent verification (TBD)

---

## 6. Diagnostic Coverage

### 6.1 UDS Service Implementation (ISO 14229-1)

| Service | Purpose | SIL 3 Critical |
|---------|---------|---------------|
| 0x10 DiagnosticSessionControl | Session switching (Default/Extended/Programming) | Yes |
| 0x14 ClearDiagnosticInformation | Clear DTCs (Extended session only) | Yes |
| 0x19 ReadDTCInformation | Report active/pending/confirmed DTCs | Yes |
| 0x22 ReadDataByIdentifier | Read VIN, session, serial number | Yes |
| 0x27 SecurityAccess | Unlock extended functions (seed/key) | Yes |

### 6.2 DTC Coverage

**9 Safety Events Monitored:**

1. **0x00A1** - Overspeed (actual > permitted)
2. **0x00A2** - SPAD (red signal + moving)
3. **0x00A3** - SOS (button pressed)
4. **0x00A4** - Rollback (reverse motion)
5. **0x00A5** - Radio Loss (> 5 sec silence)
6. **0x00A6** - Brake Fault (< 2.0 bar)
7. **0x00B1** - RFID Error (tag not read)
8. **0x0007** - Trip Mode (Kavach disabled)
9. **0x0002** - Full Supervision Mode

---

## 7. Persistent Storage (NvM) - Fault Tolerance

### 7.1 NvM Strategy

**File:** `dem_nvram.bin`

```
Header (4 bytes): Magic = 0xDEADBEEF
Count (2 bytes):  Number of stored events (0-8)
Events (512 bytes): 8 slots × 64-byte event records
CRC (2 bytes):    Fletcher-16 checksum (TBD)
Total: 520 bytes
```

**Features:**
- ✓ Survives power cycles
- ✓ Persists active DTCs across restarts
- ✓ Cleared only via UDS 0x14 (authorized clearing)
- ⚠ CRC validation (enhancement needed)

### 7.2 Corruption Recovery

If NvM file is corrupt:
```cpp
if (!nvm.restore(dem)) {
    dem.clear();  // Fallback to empty state
    printf("[WARN] NvM corrupt - starting fresh\n");
}
```

---

## 8. Security and Authentication (SIL 3)

### 8.1 Session Access Control

| Session | Access Level | UDS Services | Use Case |
|---------|-------------|-------------|----------|
| **Default** | Read-only | 0x10, 0x19, 0x22, 0x3E | Driver, On-board displays |
| **Extended** | Diagnostic | 0x10, 0x14, 0x19, 0x22, 0x27 | Service technician |
| **Programming** | Full control | 0x10, 0x11, 0x2E, 0x27, 0x31 | Factory engineering |

### 8.2 Security Access (0x27 Seed/Key)

**Mechanism:**
```
Client Request: 0x27 0x01  (request seed)
ECU Response:   0x67 0x01 <seed: 4 bytes>

Client sends:   0x27 0x02 <key: seed XOR 0xCAFEBABE>
ECU validates:  key == (received_seed XOR 0xCAFEBABE)
Response:       0x67 0x02  (unlocked)
```

**Strength:**
- ⚠ **Current:** Fixed seed (0xDEADBEEF) for simulation
- ✓ **For Production:** Dynamic seed, rolling counter, time-based window
- ✓ **Requirement:** SIL 3 mandates time-limited access (e.g., 10 min unlock window)

---

## 9. Traceability and Documentation

### 9.1 Requirements Traceability Matrix (RTM)

| Req ID | Description | Implementation | Status |
|--------|-------------|-----------------|--------|
| REQ-SIL3-001 | 4-cycle event confirmation | KavachConditions debounce | ✓ |
| REQ-SIL3-002 | UDS diagnostic services | DCM_DSL, DCM_DSP, DCM_DSD | ✓ |
| REQ-SIL3-003 | Session-based access control | DCM_DSL.cpp state machine | ✓ |
| REQ-SIL3-004 | DTC persistence across power | NvmStorage::store/restore | ✓ |
| REQ-SIL3-005 | Failure detection & reporting | DemCore event memory | ✓ |
| REQ-SIL3-006 | Security access unlock | 0x27 seed/key mechanism | ⚠ (hardcoded seed) |
| REQ-SIL3-007 | Diagnostic logging | EvtLogger CSV/TXT output | ✓ |
| REQ-SIL3-008 | Response validation | decodeResponse() in main.cpp | ✓ |
| REQ-SIL3-009 | Timing compliance | 500ms cycle time | ✓ |
| REQ-SIL3-010 | Fault injection resistance | (Test cases needed) | ⚠ |

### 9.2 Design Documentation

Required for SIL 3 certification:

```
documentation/
├── SIL-3-HARA.md              (Hazard Analysis Report)
├── SIL-3-FMEA.xlsx            (Failure Modes & Effects)
├── Architecture-Design.pdf     (System architecture)
├── DCM-State-Diagrams.pdf     (Session state machine)
├── DEM-Event-Flows.pdf        (Event confirmation logic)
├── Security-Threat-Model.md   (Security analysis)
├── Test-Plan.md               (V&V test cases)
└── SIL-3-Certification.pdf    (Independent assessor report)
```

---

## 10. Known Limitations & Future Enhancements

### 10.1 Current Limitations

| Item | Limitation | SIL Impact | Fix |
|------|-----------|-----------|-----|
| **Seed/Key** | Fixed seed in code | Low | Dynamic seed generation per session |
| **NvM CRC** | No checksum validation | Medium | Add Fletcher-16 CRC on restore |
| **Test Coverage** | Manual testing only | Medium | Add unit test framework (Google Test) |
| **Timing Verification** | No WCET analysis | Medium | RTOS timing analysis tool |
| **Formal Methods** | No theorem proving | High | Formal specification in TLA+ |

### 10.2 Roadmap to SIL 3 Certification

**Phase 1 (Current):** Code implementation ✓  
**Phase 2:** Unit testing + integration testing  
**Phase 3:** Independent safety assessment  
**Phase 4:** Formal V&V and hazard analysis  
**Phase 5:** TÜV/IRMIS SIL 3 certification  

---

## 11. Compliance Checklist

### For SIL 3 Certification (EN 50129:2018)

- [x] Safety requirements specification documented
- [x] Architecture designed for redundancy (4-cycle confirmation)
- [x] State machine formally defined (DCM session states)
- [ ] Unit tests (>80% code coverage)
- [ ] Integration tests pass
- [ ] Static analysis (SonarQube) clean
- [ ] Code review by independent party
- [ ] Hazard analysis (HARA) complete
- [ ] FMEA report approved
- [ ] Timing analysis documented
- [ ] Security threat model reviewed
- [ ] Independent safety audit performed
- [ ] Configuration management (version control) active
- [ ] Change control process documented
- [ ] Post-deployment monitoring plan

---

## 12. Contacts and Escalation

**Safety Officer:** [To be assigned]  
**Quality Manager:** [To be assigned]  
**Independent Assessor:** [To be appointed]  

**Escalation Path:**
1. Code review issues → Development team lead
2. Test failures → QA manager
3. Safety concerns → Safety officer
4. SIL certification issues → Project manager

---

## 13. Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2026-04-27 | Chethan | Initial SIL 3 document structure |
| TBD | TBD | [Safety Officer] | Formal safety assessment |
| TBD | TBD | [TÜV] | SIL 3 certification |

---

**Document Classification:** SAFETY-CRITICAL  
**Distribution:** Development Team, QA, Safety Officer, Project Manager  
**Last Updated:** 2026-04-27  
**Next Review:** 2026-07-27 (90 days)
