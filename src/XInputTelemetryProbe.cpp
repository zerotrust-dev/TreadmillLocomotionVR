#include "XInputTelemetryProbe.h"

#include "Settings.h"

namespace
{
    struct XInputGamepad
    {
        std::uint16_t buttons;
        std::uint8_t leftTrigger;
        std::uint8_t rightTrigger;
        std::int16_t thumbLX;
        std::int16_t thumbLY;
        std::int16_t thumbRX;
        std::int16_t thumbRY;
    };

    struct XInputState
    {
        std::uint32_t packetNumber;
        XInputGamepad gamepad;
    };

    static_assert(sizeof(XInputGamepad) == 12);
    static_assert(sizeof(XInputState) == 16);

    enum class XInputSource
    {
        named,
        ordinal100
    };

    struct PatchedImportSlot
    {
        std::uintptr_t* slot{ nullptr };
        std::uintptr_t original{ 0 };
    };

    using XInputGetStateFn =
        std::uint32_t(WINAPI*)(std::uint32_t, XInputState*);

    constexpr std::uint32_t maximumPatchedSlots = 16;
    constexpr std::uint16_t xinputGetStateOrdinal = 2;
    constexpr std::uint16_t xinputGetStateExOrdinal = 100;

    XInputGetStateFn g_realNamedGetState{ nullptr };
    XInputGetStateFn g_realOrdinal100GetState{ nullptr };
    std::array<PatchedImportSlot, maximumPatchedSlots> g_patchedSlots{};
    std::uint32_t g_patchedSlotCount{ 0 };
    std::atomic<std::uint64_t> g_sequence{ 0 };
    std::mutex g_hookMutex;

    [[nodiscard]] bool IsXInputModuleName(const char* moduleName)
    {
        static constexpr const char* candidates[] = {
            "XInput1_4.dll",
            "XInput1_3.dll",
            "XInput9_1_0.dll",
            "XInput1_2.dll",
            "XInput1_1.dll"
        };

        if (!moduleName) {
            return false;
        }

        return std::any_of(
            std::begin(candidates),
            std::end(candidates),
            [moduleName](const char* candidate) {
                return _stricmp(moduleName, candidate) == 0;
            });
    }

    void PublishSample(
        std::uint32_t userIndex,
        std::uint32_t result,
        const XInputState* state)
    {
        const auto& settings = TLV::Settings::GetSingleton();
        if (!settings.Enabled() || !settings.Telemetry()) {
            return;
        }
        if (userIndex != settings.UserIndex() || !state) {
            return;
        }

        TLV::XInputTelemetrySample sample{};
        sample.sequence = g_sequence.fetch_add(1, std::memory_order_relaxed) + 1;
        sample.tickMs = GetTickCount64();
        sample.userIndex = userIndex;
        sample.result = result;
        sample.packetNumber = state->packetNumber;
        sample.buttons = state->gamepad.buttons;
        sample.leftTrigger = state->gamepad.leftTrigger;
        sample.rightTrigger = state->gamepad.rightTrigger;
        sample.thumbLX = state->gamepad.thumbLX;
        sample.thumbLY = state->gamepad.thumbLY;
        sample.thumbRX = state->gamepad.thumbRX;
        sample.thumbRY = state->gamepad.thumbRY;

        TLV::Telemetry::GetSingleton().WriteSample(sample, settings.Analysis());

        if (settings.DebugLogging() && (sample.sequence % 120) == 1) {
            logger::debug(
                "xinput treadmill sample seq={} result={} packet={} lx={} ly={} rx={} ry={}",
                sample.sequence,
                result,
                sample.packetNumber,
                sample.thumbLX,
                sample.thumbLY,
                sample.thumbRX,
                sample.thumbRY);
        }
    }

    std::uint32_t WINAPI XInputGetStateHook(
        std::uint32_t userIndex,
        XInputState* state)
    {
        const auto result = g_realNamedGetState ?
            g_realNamedGetState(userIndex, state) :
            static_cast<std::uint32_t>(ERROR_DEVICE_NOT_CONNECTED);
        PublishSample(userIndex, result, state);
        return result;
    }

    std::uint32_t WINAPI XInputGetStateExHook(
        std::uint32_t userIndex,
        XInputState* state)
    {
        const auto result = g_realOrdinal100GetState ?
            g_realOrdinal100GetState(userIndex, state) :
            static_cast<std::uint32_t>(ERROR_DEVICE_NOT_CONNECTED);
        PublishSample(userIndex, result, state);
        return result;
    }

    [[nodiscard]] bool ProtectSlot(
        std::uintptr_t* slot,
        std::uintptr_t replacement,
        std::uintptr_t& original)
    {
        if (g_patchedSlotCount >= maximumPatchedSlots) {
            logger::warn("xinput treadmill probe patch table is full");
            return false;
        }

        DWORD oldProtect = 0;
        if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &oldProtect)) {
            logger::warn("xinput treadmill probe VirtualProtect failed");
            return false;
        }

        original = *slot;
        *slot = replacement;
        DWORD ignoredProtect = 0;
        VirtualProtect(slot, sizeof(*slot), oldProtect, &ignoredProtect);

        g_patchedSlots[g_patchedSlotCount++] = PatchedImportSlot{ slot, original };
        return true;
    }

    [[nodiscard]] bool PatchXInputImports()
    {
        const auto base =
            reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        if (base == 0) {
            logger::warn("xinput treadmill probe could not resolve main module base");
            return false;
        }

        const auto dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
        if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
            logger::warn("xinput treadmill probe found invalid DOS header");
            return false;
        }

        const auto ntHeaders =
            reinterpret_cast<IMAGE_NT_HEADERS*>(base + dosHeader->e_lfanew);
        if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) {
            logger::warn("xinput treadmill probe found invalid NT header");
            return false;
        }

        const auto& importDir =
            ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (importDir.VirtualAddress == 0) {
            logger::warn("xinput treadmill probe found no main-module imports");
            return false;
        }

        std::uint32_t patchedNamed = 0;
        std::uint32_t patchedOrdinal100 = 0;

        auto descriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(
            base + importDir.VirtualAddress);
        for (; descriptor->Name != 0; ++descriptor) {
            const auto moduleName =
                reinterpret_cast<const char*>(base + descriptor->Name);
            if (!IsXInputModuleName(moduleName)) {
                continue;
            }

            logger::info(
                "xinput treadmill probe inspecting import descriptor {}",
                moduleName);

            auto thunk = reinterpret_cast<IMAGE_THUNK_DATA*>(
                base + descriptor->FirstThunk);
            auto importThunk = reinterpret_cast<IMAGE_THUNK_DATA*>(
                base + (descriptor->OriginalFirstThunk != 0 ?
                        descriptor->OriginalFirstThunk :
                        descriptor->FirstThunk));

            for (; thunk->u1.Function != 0; ++thunk, ++importThunk) {
                const auto current =
                    static_cast<std::uintptr_t>(thunk->u1.Function);
                if (current == reinterpret_cast<std::uintptr_t>(&XInputGetStateHook) ||
                    current == reinterpret_cast<std::uintptr_t>(&XInputGetStateExHook)) {
                    continue;
                }

                auto slot = reinterpret_cast<std::uintptr_t*>(&thunk->u1.Function);
                const auto import = importThunk->u1.AddressOfData;
                const auto ordinalImport = IMAGE_SNAP_BY_ORDINAL(import);
                const auto ordinal = ordinalImport ? IMAGE_ORDINAL(import) : 0;
                const char* importedName = nullptr;
                if (!ordinalImport) {
                    const auto byName = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(
                        base + import);
                    importedName = reinterpret_cast<const char*>(byName->Name);
                }

                if (ordinalImport && ordinal == xinputGetStateExOrdinal) {
                    std::uintptr_t original = 0;
                    if (ProtectSlot(
                            slot,
                            reinterpret_cast<std::uintptr_t>(&XInputGetStateExHook),
                            original)) {
                        if (!g_realOrdinal100GetState) {
                            g_realOrdinal100GetState =
                                reinterpret_cast<XInputGetStateFn>(original);
                        }
                        ++patchedOrdinal100;
                        logger::info(
                            "xinput treadmill probe patched {} ordinal{} slot; next={}",
                            moduleName,
                            ordinal,
                            reinterpret_cast<void*>(original));
                    }
                } else if (ordinalImport && ordinal == xinputGetStateOrdinal) {
                    std::uintptr_t original = 0;
                    if (ProtectSlot(
                            slot,
                            reinterpret_cast<std::uintptr_t>(&XInputGetStateHook),
                            original)) {
                        if (!g_realNamedGetState) {
                            g_realNamedGetState =
                                reinterpret_cast<XInputGetStateFn>(original);
                        }
                        ++patchedNamed;
                        logger::info(
                            "xinput treadmill probe patched {} ordinal{} slot; next={}",
                            moduleName,
                            ordinal,
                            reinterpret_cast<void*>(original));
                    }
                } else if (!ordinalImport && importedName &&
                    std::strcmp(importedName, "XInputGetState") == 0) {
                    std::uintptr_t original = 0;
                    if (ProtectSlot(
                            slot,
                            reinterpret_cast<std::uintptr_t>(&XInputGetStateHook),
                            original)) {
                        if (!g_realNamedGetState) {
                            g_realNamedGetState =
                                reinterpret_cast<XInputGetStateFn>(original);
                        }
                        ++patchedNamed;
                        logger::info(
                            "xinput treadmill probe patched {}!{} slot; next={}",
                            moduleName,
                            importedName,
                            reinterpret_cast<void*>(original));
                    }
                }
            }
        }

        if (patchedNamed == 0 && patchedOrdinal100 == 0) {
            logger::warn("xinput treadmill probe found no XInput state IAT slots");
            return false;
        }

        logger::info(
            "xinput treadmill probe installed: namedSlots={} ordinal100Slots={}",
            patchedNamed,
            patchedOrdinal100);
        return true;
    }

    void RestorePatchedImports()
    {
        std::scoped_lock lock(g_hookMutex);
        for (std::uint32_t i = 0; i < g_patchedSlotCount; ++i) {
            auto& patched = g_patchedSlots[i];
            if (!patched.slot || patched.original == 0) {
                continue;
            }

            DWORD oldProtect = 0;
            if (VirtualProtect(
                    patched.slot,
                    sizeof(*patched.slot),
                    PAGE_READWRITE,
                    &oldProtect)) {
                *patched.slot = patched.original;
                DWORD ignoredProtect = 0;
                VirtualProtect(
                    patched.slot,
                    sizeof(*patched.slot),
                    oldProtect,
                    &ignoredProtect);
            }
        }

        if (g_patchedSlotCount > 0) {
            logger::info(
                "xinput treadmill probe restored {} IAT slot(s)",
                g_patchedSlotCount);
        }
        g_patchedSlotCount = 0;
        g_realNamedGetState = nullptr;
        g_realOrdinal100GetState = nullptr;
    }
}

namespace TLV
{
    XInputTelemetryProbe& XInputTelemetryProbe::GetSingleton()
    {
        static XInputTelemetryProbe singleton;
        return singleton;
    }

    XInputTelemetryProbe::~XInputTelemetryProbe()
    {
        Shutdown();
    }

    bool XInputTelemetryProbe::Initialize()
    {
        if (installed_) {
            return true;
        }

        const auto& settings = Settings::GetSingleton();
        if (!settings.Enabled()) {
            logger::info("Treadmill telemetry probe is disabled by settings");
            return false;
        }
        if (!settings.PatchXInput()) {
            logger::info("Treadmill telemetry XInput patching is disabled by settings");
            return false;
        }
        if (!settings.LogOnly()) {
            logger::warn(
                "TreadmillLocomotionVR first build supports log-only mode only; "
                "no input mutation will occur");
        }

        installed_ = PatchXInputImports();
        return installed_;
    }

    void XInputTelemetryProbe::Shutdown()
    {
        if (installed_) {
            RestorePatchedImports();
            installed_ = false;
        }
    }

    bool XInputTelemetryProbe::IsInstalled() const
    {
        return installed_;
    }
}
