#include "NvmStorage.hpp"
#include <cstdio>

NvmStorage::NvmStorage(const std::string& filePath)
    : m_filePath(filePath) {
    printf("[NvmStorage] File: %s\n", filePath.c_str());
}

Std_ReturnType NvmStorage::store(const DemCore& dem) {
    FILE* f = fopen(m_filePath.c_str(), "wb");
    if (!f) {
        printf("[NvmStorage] ERROR: Cannot open for write\n");
        return E_NOT_OK;
    }
    uint8_t count = dem.getEventMemoryCount();
    fwrite(&count, 1, 1, f);
    for (uint8_t i = 0; i < count; i++) {
        auto e = dem.getEventMemoryEntry(i);
        fwrite(&e, sizeof(e), 1, f);
    }
    fclose(f);
    printf("[NvmStorage] Stored %d entries\n", count);
    return E_OK;
}

Std_ReturnType NvmStorage::restore(DemCore& dem) {
    FILE* f = fopen(m_filePath.c_str(), "rb");
    if (!f) {
        printf("[NvmStorage] No existing file - fresh start\n");
        return E_NOT_OK;
    }
    uint8_t count = 0;
    fread(&count, 1, 1, f);
    if (count > DemCore::MAX_EVENT_MEMORY) count = DemCore::MAX_EVENT_MEMORY;
    for (uint8_t i = 0; i < count; i++) {
        DemCore::EventMemoryEntry e{};
        fread(&e, sizeof(e), 1, f);
        dem.restoreEntry(e);
    }
    fclose(f);
    printf("[NvmStorage] Restored %d entries\n", count);
    return E_OK;
}
