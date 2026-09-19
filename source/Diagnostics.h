#pragma once
#include "Version.h"

// Worker-thread-only diagnostics. The UI callbacks only publish POD observations.
// No game pointers are dereferenced here; process paths and heap addresses are
// deliberately omitted. Both files describe one immutable snapshot and carry
// the same session/revision, so stale or partially replaced pairs are detectable.
namespace crimson {
struct DiagnosticSnapshot {
    Observation observation;
    bool available = false;
    U32 uiThread = 0;
    U32 offThreadCalls = 0;
};

struct Diagnostics {
    wchar_t directory[2048]{};
    wchar_t path[2304]{};
    wchar_t temporary[2368]{};
    char buffer[8192]{};
    char session[64]{};
    U32 directoryLength = 0;
    U32 process = 0;
    U32 length = 0;
    U64 revision = 0;
    U64 writeErrors = 0;
    DWORD lastWriteError = 0;
    bool retryPending = false;

    void add(const char* text) {
        for (U32 i = 0; text[i] && length + 1 < sizeof(buffer); ++i)
            buffer[length++] = text[i];
        buffer[length] = 0;
    }
    void number(U64 value) {
        char digits[24];
        U32 n = 0;
        do { digits[n++] = char('0' + value % 10); value /= 10; } while (value);
        while (n) { char digit[2] = {digits[--n], 0}; add(digit); }
    }
    void fixedNumber(U32 value, U32 width) {
        char digits[11]{};
        if (width > 10) return;
        for (U32 i = width; i; --i) { digits[i - 1] = char('0' + value % 10); value /= 10; }
        add(digits);
    }
    void hex(U64 value) {
        add("0x");
        char digits[16];
        U32 n = 0;
        do { digits[n++] = "0123456789ABCDEF"[value & 15]; value >>= 4; } while (value);
        while (n) { char digit[2] = {digits[--n], 0}; add(digit); }
    }
    void field(const char* name, U64 value, bool comma = true) {
        add("    \""); add(name); add("\": "); number(value); add(comma ? ",\n" : "\n");
    }
    void state(const char* name, I32 value, bool comma = true) {
        add("    \""); add(name); add("\": ");
        if (value < 0) add("null"); else number(U32(value));
        add(comma ? ",\n" : "\n");
    }
    void initialize(HMODULE module) {
        process = GetCurrentProcessId();
        SystemTime now{};
        GetSystemTime(&now);
        length = 0;
        fixedNumber(now.year, 4); fixedNumber(now.month, 2); fixedNumber(now.day, 2);
        add("T"); fixedNumber(now.hour, 2); fixedNumber(now.minute, 2); fixedNumber(now.second, 2);
        add("."); fixedNumber(now.millisecond, 3); add("Z-"); number(process);
        for (U32 i = 0; i <= length && i < sizeof(session); ++i) session[i] = buffer[i];
        DWORD n = GetModuleFileNameW(module, directory, 2048);
        if (!n || n >= 2048) return;
        while (n && directory[n - 1] != L'\\' && directory[n - 1] != L'/') --n;
        directory[n] = 0;
        directoryLength = n;
    }
    bool failure(DWORD error) {
        ++writeErrors;
        lastWriteError = error;
        return false;
    }
    bool writeFile(const wchar_t* name) {
        if (!directoryLength) return failure(3);
        U32 n = 0;
        for (; n < directoryLength; ++n) path[n] = directory[n];
        for (U32 i = 0; name[i]; ++i) {
            if (n + 1 >= 2304) return failure(206);
            path[n++] = name[i];
        }
        path[n] = 0;
        U32 t = 0;
        for (; t < n; ++t) temporary[t] = path[t];
        temporary[t++] = L'.';
        for (U32 i = 0; session[i]; ++i) temporary[t++] = (wchar_t)session[i];
        const wchar_t suffix[] = L".tmp";
        for (U32 i = 0; i < 5; ++i) temporary[t++] = suffix[i];
        HANDLE file = CreateFileW(temporary, 0x40000000, 1, nullptr, 2, 0x80, nullptr);
        if (file == (HANDLE)(I64)-1) return failure(GetLastError());
        DWORD error = 0;
        U32 sent = 0;
        while (sent < length) {
            DWORD written = 0;
            if (!WriteFile(file, buffer + sent, length - sent, &written, nullptr)) {
                error = GetLastError(); if (!error) error = 29; break;
            }
            if (!written || written > length - sent) { error = 29; break; }
            sent += written;
        }
        if (!CloseHandle(file) && !error) { error = GetLastError(); if (!error) error = 6; }
        if (!error && !MoveFileExW(temporary, path, 1 | 8)) { error = GetLastError(); if (!error) error = 29; }
        if (error) { DeleteFileW(temporary); return failure(error); }
        return true;
    }
    void write(const char* status, const char* reason, const Image& image,
               const Resolved& resolved, const DiagnosticSnapshot& snap) {
        ++revision;
        const U64 priorErrors = writeErrors;
        const DWORD priorError = lastWriteError;
        const Observation& s = snap.observation;
        length = 0;
        add("{\n  \"schema_version\": 1,\n  \"mod\": \"ShutUpAndLetMePlay\",\n  \"version\": \"" SULMP_VERSION "\",\n  \"session_id\": \"");
        add(session); add("\",\n  \"process_id\": "); number(process);
        add(",\n  \"revision\": "); number(revision);
        add(",\n  \"status\": \""); add(status); add("\",\n  \"reason\": \""); add(reason ? reason : "");
        add("\",\n  \"visual_verification\": \"not measured; counters verify native style state, not pixels\",\n  \"pe_timestamp\": \"");
        hex(image.timestamp); add("\",\n  \"image_size\": \""); hex(image.size);
        add("\",\n  \"resolver\": {\n");
        field("interaction_rva", resolved.interaction);
        field("cinema_appearance_rva", resolved.appearance);
        field("set_keyguide_appearance_rva", resolved.setAppearance);
        field("native_skip_semantic_rva", resolved.skipSemantic);
        field("appear_style_global_rva", resolved.symbols[Sym_AppearStyle]);
        field("disappear_style_global_rva", resolved.symbols[Sym_DisappearStyle], false);
        add("  },\n  \"runtime\": {\n");
        field("snapshot_available", snap.available);
        field("ui_thread_id", snap.uiThread);
        field("off_thread_passthrough_calls", snap.offThreadCalls);
        field("interaction_calls", s.interactionCalls);
        field("appearance_calls", s.appearanceCalls);
        field("preactivation_unlocks", s.preUnlocks);
        field("initial_input_shows", s.inputShows);
        field("appearance_repairs", s.appearanceRepairs);
        field("native_style_state_verified", s.appearanceVerified);
        field("safety_gate_blocks", s.blocks);
        field("interaction_disable_cleanup", s.cleanupCalls);
        state("last_cinema_mode", s.lastMode == 255 ? -1 : I32(s.lastMode));
        state("last_input_family", s.lastInputFamily == 255 ? -1 : I32(s.lastInputFamily));
        state("last_input_subtype", s.lastInputSubtype == 255 ? -1 : I32(s.lastInputSubtype));
        state("last_appear_before", s.lastAppearBefore);
        state("last_disappear_before", s.lastDisappearBefore);
        state("last_appear_after", s.lastAppearAfter);
        state("last_disappear_after", s.lastDisappearAfter, false);
        add("  },\n  \"diagnostics\": {\n");
        field("previous_write_errors", priorErrors);
        field("last_write_error", priorError, false);
        add("  }\n}\n");
        const bool jsonOK = writeFile(L"ShutUpAndLetMePlay_UpdateReport.json");

        length = 0;
        add("ShutUpAndLetMePlay " SULMP_VERSION "\nSession: "); add(session);
        add("\nProcess ID: "); number(process); add("\nRevision: "); number(revision);
        add("\nStatus: "); add(status); add("\nReason: "); add(reason ? reason : "");
        add("\nExecutable PE timestamp: "); hex(image.timestamp);
        add("\nExecutable image size: "); hex(image.size);
        add("\nAppearance repairs: "); number(s.appearanceRepairs);
        add("\nNative style-state checks passed: "); number(s.appearanceVerified);
        add("\nSafety-gate blocks: "); number(s.blocks);
        add("\nPrevious diagnostic write errors: "); number(priorErrors);
        add("\nLast diagnostic write error: "); number(priorError);
        add("\nNative action, hold timing and bindings are unchanged. No custom overlay.\n");
        add("Style-state checks do not measure rendered pixels.\n");
        add("The JSON report has the full snapshot. Compare session and revision before combining files.\n");
        const bool logOK = writeFile(L"ShutUpAndLetMePlay.log");
        retryPending = !jsonOK || !logOK;
    }
};
} // namespace crimson
