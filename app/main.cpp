#include <cstdio>
#include <cstring>
#include <unistd.h>
#include "DemCore.hpp"
#include "DemEventConfig.hpp"
#include "NvmStorage.hpp"
#include "EvtLogger.hpp"
#include "KavachEth.hpp"
#include "KavachConditions.hpp"
#include "ModbusTcp.hpp"
#include "KavachUdp.hpp"
#include "KavachSensor.hpp"
#include "DCM_DSL.hpp"
#include "DCM_DSP.hpp"
#include "DCM_DSD.hpp"

static constexpr int CONFIRM_CYCLES = 4;

static void printDtcReport(DemCore& dem) {
    printf("\n========================================\n");
    printf("  DTC Report (UDS 0x19)\n");
    printf("========================================\n");
    uint8_t count = dem.getEventMemoryCount();
    printf("  Active failed DTCs: %d\n\n", count);
    if (count == 0) {
        printf("  No faults detected - all conditions normal.\n");
    } else {
        for (uint8_t i = 0; i < count; i++) {
            auto e = dem.getEventMemoryEntry(i);
            printf("  DTC: 0x%06X  UDS: 0x%02X\n",
                   e.dtc, e.udsStatusByte);
        }
    }
    printf("========================================\n");
}


// ── UDS Response Decoder ──────────────────────────────────────────
static void decodeResponse(const uint8_t* resp, uint16_t len) {
    if (len == 0) { printf("  [No response]\n"); return; }

    if (resp[0] == 0x7F) {
        // NEGATIVE RESPONSE
        const char* nrcName = "Unknown";
        switch(resp[2]) {
            case 0x10: nrcName="generalReject"; break;
            case 0x11: nrcName="serviceNotSupported"; break;
            case 0x12: nrcName="subFunctionNotSupported"; break;
            case 0x13: nrcName="incorrectMessageLength"; break;
            case 0x22: nrcName="conditionsNotCorrect"; break;
            case 0x24: nrcName="requestSequenceError"; break;
            case 0x25: nrcName="noResponseFromSubnetComponent"; break;
            case 0x31: nrcName="requestOutOfRange"; break;
            case 0x33: nrcName="securityAccessDenied"; break;
            case 0x35: nrcName="invalidKey"; break;
            case 0x36: nrcName="exceededNumberOfAttempts"; break;
            case 0x37: nrcName="requiredTimeDelayNotExpired"; break;
            case 0x78: nrcName="requestCorrectlyReceivedResponsePending"; break;
        }
        printf("  ┌─ NEGATIVE RESPONSE ─────────────────┐\n");
        printf("  │  0x7F = NegativeResponse            │\n");
        printf("  │  0x%02X = SID that was rejected        │\n", resp[1]);
        printf("  │  0x%02X = NRC: %-24s│\n", resp[2], nrcName);
        printf("  └─────────────────────────────────────┘\n");
        return;
    }

    // POSITIVE RESPONSE
    printf("  ┌─ POSITIVE RESPONSE ─────────────────┐\n");
    switch(resp[0]) {
        case 0x50:
            printf("  │  0x50 = DiagSessionControl +PR      │\n");
            printf("  │  0x%02X = Session granted              │\n", resp[1]);
            if (resp[1]==0x01) printf("  │         -> DEFAULT session           │\n");
            if (resp[1]==0x02) printf("  │         -> PROGRAMMING session       │\n");
            if (resp[1]==0x03) printf("  │         -> EXTENDED session          │\n");
            break;
        case 0x51:
            printf("  │  0x51 = ECUReset +PR                │\n");
            printf("  │  0x%02X = ResetType                   │\n", resp[1]);
            if (resp[1]==0x01) printf("  │         -> Hard Reset                │\n");
            if (resp[1]==0x02) printf("  │         -> KeyOffOn Reset            │\n");
            if (resp[1]==0x03) printf("  │         -> Soft Reset                │\n");
            break;
        case 0x54:
            printf("  │  0x54 = ClearDTC +PR                │\n");
            printf("  │         -> DTCs cleared OK           │\n");
            break;
        case 0x59:
            printf("  │  0x59 = ReadDTCInfo +PR             │\n");
            printf("  │  0x%02X = SubFunction                  │\n", resp[1]);
            if (resp[1]==0x01 && len>=6)
                printf("  │  DTC count = %d                     │\n",
                       (resp[4]<<8)|resp[5]);
            if (resp[1]==0x02) {
                int dtcs = (len-3)/4;
                printf("  │  DTCs found = %d                    │\n", dtcs);
                for (int i=0;i<dtcs&&i<4;i++) {
                    int o=3+(i*4);
                    printf("  │    DTC: 0x%02X%02X%02X  UDS: 0x%02X       │\n",
                           resp[o],resp[o+1],resp[o+2],resp[o+3]);
                }
            }
            break;
        case 0x62:
            printf("  │  0x62 = ReadDataById +PR            │\n");
            printf("  │  DID:  0x%02X%02X                       │\n", resp[1],resp[2]);
            if (resp[1]==0xF1 && resp[2]==0x90) {
                printf("  │  VIN:  ");
                for (int i=3;i<len&&i<20;i++)
                    printf("%c", resp[i]>0x20?resp[i]:'?');
                printf("                │\n");
            } else if (resp[1]==0xF1 && resp[2]==0x86) {
                printf("  │  Session: 0x%02X                     │\n", resp[3]);
            }
            break;
        case 0x67:
            printf("  │  0x67 = SecurityAccess +PR          │\n");
            if (resp[1]==0x01 && len>=6) {
                uint32_t seed=((uint32_t)resp[2]<<24)|((uint32_t)resp[3]<<16)|
                              ((uint32_t)resp[4]<<8)|resp[5];
                printf("  │  0x01 = SeedRequested               │\n");
                printf("  │  Seed: 0x%08X                  │\n", seed);
                printf("  │  Key:  0x%08X (seed^0xCAFEBABE)│\n", seed^0xCAFEBABEU);
            } else if (resp[1]==0x02) {
                printf("  │  0x02 = KeyAccepted -> UNLOCKED     │\n");
            }
            break;
        case 0x6E:
            printf("  │  0x6E = WriteDataById +PR           │\n");
            printf("  │  DID:  0x%02X%02X written OK            │\n", resp[1],resp[2]);
            break;
        case 0x71:
            printf("  │  0x71 = RoutineControl +PR          │\n");
            printf("  │  0x%02X = SubFunction                  │\n", resp[1]);
            printf("  │  Routine: 0x%02X%02X                    │\n", resp[2],resp[3]);
            if (len>4) printf("  │  Status:  0x%02X                     │\n", resp[4]);
            break;
        case 0x7E:
            printf("  │  0x7E = TesterPresent +PR           │\n");
            printf("  │         -> Session kept alive        │\n");
            break;
        case 0xC5:
            printf("  │  0xC5 = ControlDTCSetting +PR       │\n");
            printf("  │  0x%02X = %s                │\n", resp[1],
                   resp[1]==0x01?"DTC setting ENABLED ":"DTC setting DISABLED");
            break;
        default:
            printf("  │  0x%02X = Response                    │\n", resp[0]);
    }
    printf("  │  Raw: ");
    for (int i=0;i<len&&i<8;i++) printf("%02X ",resp[i]);
    if (len>8) printf("...");
    printf("│\n");
    printf("  └─────────────────────────────────────┘\n");
}
// ─────────────────────────────────────────────────────────────────
static void runMenu(EvtLogger& logger, DemCore& dem, DCM_DSD& dsd) {
    char buf[32]{};
    while (true) {
        printf("\n========================================\n");
        printf("  What would you like to do?\n");
        printf("----------------------------------------\n");
        printf("  1  or  all  -> Show all failed events\n");
        printf("  2  or  rbi  -> ReadByIdentifier query\n");
    printf("  3  or  dcm  -> DCM UDS Services (session based)\n");
        printf("  q           -> Quit\n");
        printf("========================================\n");
        printf("Choice: ");
        fflush(stdout);
        if (!fgets(buf, sizeof(buf), stdin)) break;
        for (int i = 0; buf[i]; i++)
            if (buf[i] == '\n') { buf[i] = '\0'; break; }

        if (strcmp(buf,"1")==0 || strcmp(buf,"all")==0) {
            printDtcReport(dem);

        } else if (strcmp(buf,"2")==0 || strcmp(buf,"rbi")==0) {
            printf("\n+-------+----------+------------------------+----------+--------+\n");
            printf("| Input | Event ID | Event Name             | DTC      | Sev    |\n");
            printf("+-------+----------+------------------------+----------+--------+\n");
            printf("|  A1   | 0x00A1   | Over_Speeding          | 0x00A101 | HIGH   |\n");
            printf("|  A2   | 0x00A2   | SPAD                   | 0x00A201 | HIGH   |\n");
            printf("|  A3   | 0x00A3   | SOS_Received           | 0x00A301 | HIGH   |\n");
            printf("|  A4   | 0x00A4   | Roll_Back              | 0x00A401 | HIGH   |\n");
            printf("|  A5   | 0x00A5   | Radio_Loss             | 0x00A501 | MEDIUM |\n");
            printf("|  A6   | 0x00A6   | Brake_Command          | 0x00A601 | HIGH   |\n");
            printf("|  B1   | 0x00B1   | RFID_Tag_Read          | 0x00B101 | LOW    |\n");
            printf("|  07   | 0x0007   | Trip                   | 0x010701 | MEDIUM |\n");
            printf("+-------+----------+------------------------+----------+--------+\n");
            printf("|  0000 = Query ALL  |  b or q = Back                           |\n");
            printf("+-------+----------+------------------------+----------+--------+\n");
            while (true) {
                printf("\nEnter Event ID: ");
                fflush(stdout);
                if (!fgets(buf, sizeof(buf), stdin)) break;
                for (int i = 0; buf[i]; i++)
                    if (buf[i] == '\n') { buf[i] = '\0'; break; }
                if (strcmp(buf,"b")==0 || strcmp(buf,"B")==0 ||
                    strcmp(buf,"q")==0 || strcmp(buf,"Q")==0) {
                    printf("Back to menu.\n"); break;
                }
                unsigned int parsed = 0;
                if (sscanf(buf, "%x", &parsed) != 1) {
                    printf("  Invalid. Enter hex like A1. b to go back.\n");
                    continue;
                }
                Dem_EventIdType evtId =
                    static_cast<Dem_EventIdType>(parsed);
                if (evtId == 0x0000U) {
                    Dem_EventIdType all[] = {
                        KAVACH_EVT_OVERSPEED, KAVACH_EVT_SPAD,
                        KAVACH_EVT_SOS,       KAVACH_EVT_RADIO_LOSS,
                        KAVACH_EVT_ROLLBACK,  KAVACH_EVT_RFID,
                        KAVACH_EVT_BRAKE_CMD, KAVACH_EVT_MODE_TR
                    };
                    for (auto id : all)
                        logger.readByIdentifier(id);
                } else {
                    logger.readByIdentifier(evtId);
                }
            }

        } else if (strcmp(buf,"3")==0 || strcmp(buf,"dcm")==0) {
            uint8_t req[20]{}, resp[64]{};

            // ── Step 1: Select Session ──
            printf("\n========================================\n");
            printf("  SELECT SESSION\n");
            printf("========================================\n");
            printf("  1 -> Default Session     (0x01)\n");
            printf("  2 -> Extended Session    (0x03)\n");
            printf("  3 -> Programming Session (0x02)\n");
            printf("  b -> Back to menu\n");
            printf("========================================\n");
            printf("Session: ");
            fflush(stdout);
            char sbuf[16]{};
            if (!fgets(sbuf, sizeof(sbuf), stdin)) continue;
            for (int i=0;sbuf[i];i++) if(sbuf[i]=='\n'){sbuf[i]='\0';break;}
            if (strcmp(sbuf,"b")==0) continue;

            uint8_t sessionId = 0x01;
            const char* sessionName = "DEFAULT";
            if (strcmp(sbuf,"1")==0) { sessionId=0x01; sessionName="DEFAULT"; }
            else if (strcmp(sbuf,"2")==0) { sessionId=0x03; sessionName="EXTENDED"; }
            else if (strcmp(sbuf,"3")==0) { sessionId=0x02; sessionName="PROGRAMMING"; }
            else { printf("  Invalid session.\n"); continue; }

            // Switch session
            req[0]=0x10; req[1]=sessionId;
            { uint16_t rlen = dsd.dispatch(req,2,resp,64); decodeResponse(resp, rlen); }
            printf("\n  >> Now in %s session\n\n", sessionName);

            // ── Step 2: Select Service ──
            while (true) {
                printf("========================================\n");
                printf("  DCM SERVICES  [Session: %s]\n", sessionName);
                printf("========================================\n");
                // 0x3E allowed in ALL sessions
                printf("  1  -> 0x3E TesterPresent        [ALL sessions]\n");
                // 0x22 allowed in DEFAULT + EXTENDED
                if (sessionId==0x01||sessionId==0x03)
                    printf("  3  -> 0x22 ReadDataById          [DEFAULT/EXTENDED]\n");
                // 0x19 allowed in DEFAULT + EXTENDED
                if (sessionId==0x01||sessionId==0x03)
                    printf("  4  -> 0x19 ReadDTCInfo           [DEFAULT/EXTENDED]\n");
                // 0x27 allowed in EXTENDED + PROGRAMMING
                if (sessionId==0x03||sessionId==0x02)
                    printf("  2  -> 0x27 SecurityAccess        [EXTENDED/PROG]\n");
                // 0x14 allowed in EXTENDED only
                if (sessionId==0x03)
                    printf("  5  -> 0x14 ClearDTC              [EXTENDED only]\n");
                // 0x31 allowed in EXTENDED + PROGRAMMING
                if (sessionId==0x03||sessionId==0x02)
                    printf("  6  -> 0x31 RoutineControl        [EXTENDED/PROG]\n");
                // 0x85 allowed in EXTENDED only
                if (sessionId==0x03)
                    printf("  7  -> 0x85 ControlDTCSetting     [EXTENDED only]\n");
                // 0x2E allowed in PROGRAMMING only
                if (sessionId==0x02)
                    printf("  8  -> 0x2E WriteDataById         [PROG only - needs unlock]\n");
                // 0x11 allowed in EXTENDED + PROGRAMMING
                if (sessionId==0x03||sessionId==0x02)
                    printf("  9  -> 0x11 ECUReset              [EXTENDED/PROG]\n");
                printf("  b  -> Back to menu\n");
                printf("========================================\n");
                printf("Service: ");
                fflush(stdout);
                char dbuf[16]{};
                if (!fgets(dbuf, sizeof(dbuf), stdin)) break;
                for (int i=0;dbuf[i];i++) if(dbuf[i]=='\n'){dbuf[i]='\0';break;}

                if (strcmp(dbuf,"b")==0) break;

                else if (strcmp(dbuf,"1")==0) {
                    req[0]=0x3E; req[1]=0x00;
                    { uint16_t rlen = dsd.dispatch(req,2,resp,64); decodeResponse(resp, rlen); }

                } else if (strcmp(dbuf,"2")==0) {
                    // Request seed
                    req[0]=0x27; req[1]=0x01;
                    uint16_t len = dsd.dispatch(req,2,resp,64);
                    decodeResponse(resp, len);
                    if (len >= 6 && resp[0]==0x67) {
                        // Auto compute key
                        uint32_t seed = ((uint32_t)resp[2]<<24)|((uint32_t)resp[3]<<16)|
                                        ((uint32_t)resp[4]<<8)|resp[5];
                        uint32_t key  = seed ^ 0xCAFEBABEU;
                        req[0]=0x27; req[1]=0x02;
                        req[2]=(key>>24)&0xFF; req[3]=(key>>16)&0xFF;
                        req[4]=(key>>8)&0xFF;  req[5]=key&0xFF;
                        { uint16_t rlen = dsd.dispatch(req,6,resp,64); decodeResponse(resp, rlen); }
                        printf("  >> Security UNLOCKED\n");
                    } else {
                        printf("  >> Seed request denied (wrong session?)\n");
                    }

                } else if (strcmp(dbuf,"3")==0) {
                    req[0]=0x22; req[1]=0xF1; req[2]=0x90;
                    { uint16_t rlen = dsd.dispatch(req,3,resp,64); decodeResponse(resp, rlen); }
                    if (resp[0]==0x62) {
                        printf("  VIN: ");
                        for (int i=3;i<20&&i<64;i++) printf("%c",resp[i]>0x20?resp[i]:'?');
                        printf("\n");
                    }
                    req[0]=0x22; req[1]=0xF1; req[2]=0x86;
                    { uint16_t rlen = dsd.dispatch(req,3,resp,64); decodeResponse(resp, rlen); }
                    req[0]=0x22; req[1]=0xF1; req[2]=0x00;
                    { uint16_t rlen = dsd.dispatch(req,3,resp,64); decodeResponse(resp, rlen); }

                } else if (strcmp(dbuf,"4")==0) {
                    req[0]=0x19; req[1]=0x01; req[2]=0x0F;
                    { uint16_t rlen = dsd.dispatch(req,3,resp,64); decodeResponse(resp, rlen); }
                    req[0]=0x19; req[1]=0x02; req[2]=0x0F;
                    { uint16_t rlen = dsd.dispatch(req,3,resp,64); decodeResponse(resp, rlen); }

                } else if (strcmp(dbuf,"5")==0) {
                    req[0]=0x14; req[1]=0xFF; req[2]=0xFF; req[3]=0xFF;
                    { uint16_t rlen = dsd.dispatch(req,4,resp,64); decodeResponse(resp, rlen); }
                    printf("  >> DTCs cleared\n");

                } else if (strcmp(dbuf,"6")==0) {
                    req[0]=0x31; req[1]=0x01; req[2]=0x02; req[3]=0x02;
                    { uint16_t rlen = dsd.dispatch(req,4,resp,64); decodeResponse(resp, rlen); }

                } else if (strcmp(dbuf,"7")==0) {
                    printf("  1=Enable  2=Disable: ");
                    fflush(stdout);
                    char xbuf[8]{};
                    if(fgets(xbuf,8,stdin)){
                        req[0]=0x85;
                        req[1]=(xbuf[0]=='2') ? 0x02 : 0x01;
                        { uint16_t rlen = dsd.dispatch(req,2,resp,64); decodeResponse(resp, rlen); }
                    }

                } else if (strcmp(dbuf,"8")==0) {
                    req[0]=0x2E; req[1]=0xF1; req[2]=0x90;
                    memcpy(&req[3],"KAVACH-NEW-VIN-01",17);
                    { uint16_t rlen = dsd.dispatch(req,20,resp,64); decodeResponse(resp, rlen); }

                } else if (strcmp(dbuf,"9")==0) {
                    printf("  1=Hard  2=KeyOffOn  3=Soft: ");
                    fflush(stdout);
                    char xbuf[8]{};
                    if(fgets(xbuf,8,stdin)){
                        req[0]=0x11;
                        req[1]=(xbuf[0]=='1')?0x01:(xbuf[0]=='2')?0x02:0x03;
                        { uint16_t rlen = dsd.dispatch(req,2,resp,64); decodeResponse(resp, rlen); }
                    }
                } else {
                    printf("  Unknown service.\n");
                }
            }

            // Switch to extended session
            req[0]=0x10; req[1]=0x03;

        } else if (strcmp(buf,"q")==0 || strcmp(buf,"Q")==0) {
            printf("Exiting.\n");
            break;
        } else {
            printf("  Unknown option.\n");
        }
    }
}

int main() {
    printf("\n========================================\n");
    printf("  Railway DEM/DCM - Kavach Live Mode\n");
    printf("  DMI:  192.168.0.110:1502\n");
    printf("  UDP:  192.168.0.110:5600\n");
    printf("  Confirmation: %d consecutive cycles\n",
           CONFIRM_CYCLES);
    printf("========================================\n\n");

    DemCore          dem{};
    NvmStorage       nvm{"dem_nvram.bin"};
    EvtLogger        logger{"logs"};
    KavachEth        eth{"192.168.0.110", 1502, 5601};
    ModbusTcp        modbus{"192.168.0.110", 1502, &dem};
    KavachUdp        udp{"192.168.0.110", KAVACH_UDP_PORT, &dem};
    KavachConditions cond{dem, logger, eth, modbus};
    KavachSensor     sensor{"192.168.0.110", 1502};

    DCM_DSL  dsl{};
    DCM_DSP  dsp{dem, logger, dsl};
    DCM_DSD  dsd{dsl, dsp};

    nvm.restore(dem);
    modbus.connect();

    if (!sensor.connect())
        printf("[WARN] Sensor not connected - will retry\n");

    // ---- Debounce counters (one per condition) ----
    int cnt_speed  = 0;
    int cnt_brake  = 0;
    int cnt_sos    = 0;
    int cnt_signal = 0;
    int cnt_radio  = 0;
    int cnt_rfid   = 0;

    // ---- Track which events already confirmed this session ----
    bool confirmed_speed  = false;
    bool confirmed_brake  = false;
    bool confirmed_sos    = false;
    bool confirmed_signal = false;
    bool confirmed_radio  = false;
    bool confirmed_rfid   = false;

    uint32_t nowMs    = 0;
    uint32_t lastRxMs = 0;

    printf("[MAIN] Reading live DMI data...\n");
    printf("[MAIN] Each condition must fail %d consecutive cycles"
           " to be confirmed.\n\n", CONFIRM_CYCLES);

    for (int cycle = 1; cycle <= CONFIRM_CYCLES; cycle++) {
        nowMs += 500;

        printf("--- Cycle %d / %d ---\n", cycle, CONFIRM_CYCLES);

        KavachSensorData data = sensor.readAll();

        if (!data.valid) {
            printf("[WARN] Sensor read failed - retrying...\n");
            sensor.disconnect();
            sleep(1);
            sensor.connect();
            cycle--;   // retry this cycle
            continue;
        }

        // Update radio timer
        if (data.rfSignalBars > 0) lastRxMs = nowMs;

        // ---- Count consecutive failures per condition ----

        // Overspeed
        if (data.speedActual > 0 &&
            data.speedPermitted > 0 &&
            data.speedActual > data.speedPermitted) {
            cnt_speed++;
            printf("  [SPEED]  %d > %d  cnt=%d/%d\n",
                   data.speedActual, data.speedPermitted,
                   cnt_speed, CONFIRM_CYCLES);
        } else {
            cnt_speed = 0;
        }

        // Brake pressure
        float bpBar = static_cast<float>(data.brakePipe) / 10.0f;
        if (data.brakePipe > 0 && bpBar < 2.0f) {
            cnt_brake++;
            printf("  [BRAKE]  %.1f bar < 2.0  cnt=%d/%d\n",
                   bpBar, cnt_brake, CONFIRM_CYCLES);
        } else {
            cnt_brake = 0;
        }

        // SOS
        if (data.sosState == 1) {
            cnt_sos++;
            printf("  [SOS]    pressed  cnt=%d/%d\n",
                   cnt_sos, CONFIRM_CYCLES);
        } else {
            cnt_sos = 0;
        }

        // Signal SPAD (aspect 3 = danger AND moving)
        if (data.signalAspect == 3 && data.speedActual > 0) {
            cnt_signal++;
            printf("  [SIGNAL] aspect=%d speed=%d  cnt=%d/%d\n",
                   data.signalAspect, data.speedActual,
                   cnt_signal, CONFIRM_CYCLES);
        } else {
            cnt_signal = 0;
        }

        // Radio loss
        if ((nowMs - lastRxMs) > 5000U) {
            cnt_radio++;
            printf("  [RADIO]  gap=%ums  cnt=%d/%d\n",
                   (nowMs - lastRxMs), cnt_radio, CONFIRM_CYCLES);
        } else {
            cnt_radio = 0;
        }

        // RFID — tag must be present (tagId > 0) AND hardware OK
        if (data.tagId == 0 || data.rfidStatus != 0) {
            cnt_rfid++;
            printf("  [RFID]   tagId=%d hw=%d  cnt=%d/%d\n",
                   data.tagId, data.rfidStatus,
                   cnt_rfid, CONFIRM_CYCLES);
        } else {
            cnt_rfid = 0;
        }

        // ---- Confirm events that hit 4 cycles ----
        if (cnt_speed >= CONFIRM_CYCLES && !confirmed_speed) {
            confirmed_speed = true;
            printf("\n  [CONFIRMED] OVERSPEED -> reporting to DEM\n\n");
            cond.setSpeed(static_cast<uint8_t>(data.speedActual),
                          static_cast<uint8_t>(data.speedPermitted));
            cond.checkAll();
        }
        if (cnt_brake >= CONFIRM_CYCLES && !confirmed_brake) {
            confirmed_brake = true;
            printf("\n  [CONFIRMED] BRAKE FAULT -> reporting to DEM\n\n");
            cond.setBrake(bpBar);
            cond.checkAll();
        }
        if (cnt_sos >= CONFIRM_CYCLES && !confirmed_sos) {
            confirmed_sos = true;
            printf("\n  [CONFIRMED] SOS -> reporting to DEM\n\n");
            cond.setSos(true);
            cond.checkAll();
        }
        if (cnt_signal >= CONFIRM_CYCLES && !confirmed_signal) {
            confirmed_signal = true;
            printf("\n  [CONFIRMED] SPAD -> reporting to DEM\n\n");
            cond.setSignal(static_cast<uint8_t>(data.signalAspect),
                           static_cast<uint8_t>(data.speedActual));
            cond.checkAll();
        }
        if (cnt_radio >= CONFIRM_CYCLES && !confirmed_radio) {
            confirmed_radio = true;
            printf("\n  [CONFIRMED] RADIO LOSS -> reporting to DEM\n\n");
            cond.setRadio(lastRxMs, nowMs);
            cond.checkAll();
        }
        if (cnt_rfid >= CONFIRM_CYCLES && !confirmed_rfid) {
            confirmed_rfid = true;
            printf("\n  [CONFIRMED] RFID FAULT -> reporting to DEM\n\n");
            cond.setRfid(data.tagId > 0, data.rfidStatus == 0);
            cond.checkAll();
        }

        // Send UDP snapshot every cycle
        uint8_t coils = KavachUdp::buildCoilFlags(dem);
        udp.sendSnapshot(dem,
                         data.kavachMode,
                         static_cast<uint8_t>(data.speedActual),
                         static_cast<uint8_t>(data.speedPermitted),
                         static_cast<uint8_t>(dsl.getCurrentSession()),
                         coils);

        usleep(500000);  // 500ms between cycles
    }

    // ---- Print final DTC report ----
    printf("\n========================================\n");
    printf("  4-Cycle Confirmation Complete\n");
    printf("========================================\n");
    printf("  Overspeed   : %s\n", confirmed_speed  ? "CONFIRMED FAILED" : "OK");
    printf("  Brake       : %s\n", confirmed_brake  ? "CONFIRMED FAILED" : "OK");
    printf("  SOS         : %s\n", confirmed_sos    ? "CONFIRMED FAILED" : "OK");
    printf("  SPAD        : %s\n", confirmed_signal ? "CONFIRMED FAILED" : "OK");
    printf("  Radio Loss  : %s\n", confirmed_radio  ? "CONFIRMED FAILED" : "OK");
    printf("  RFID        : %s\n", confirmed_rfid   ? "CONFIRMED FAILED" : "OK");
    printf("========================================\n");

    printDtcReport(dem);

    // Save NvM
    nvm.store(dem);

    // Interactive menu
    runMenu(logger, dem, dsd);

    printf("\n[MAIN] Done.\n");
    printf("  events_failed.csv -> current run\n");
    printf("  rbi_history.csv   -> all runs history\n\n");
    return 0;
}
