#include "XInputLocomotionOutput.h"

#include "InputMux.h"
#include "Settings.h"

namespace
{
    // Unique per-mod id for the shared XInput mux.
    constexpr std::uint32_t muxModId = 0x544D4C31;  // 'TML1'

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

    struct XInputVibration
    {
        std::uint16_t leftMotorSpeed;
        std::uint16_t rightMotorSpeed;
    };

    struct XInputCapabilities
    {
        std::uint8_t type;
        std::uint8_t subType;
        std::uint16_t flags;
        XInputGamepad gamepad;
        XInputVibration vibration;
    };

    static_assert(sizeof(XInputGamepad) == 12);
    static_assert(sizeof(XInputState) == 16);
    static_assert(sizeof(XInputVibration) == 4);
    static_assert(sizeof(XInputCapabilities) == 20);

    enum class XInputSource
    {
        named,
        ordinal100
    };

    struct XInputReturnedSlotState
    {
        XInputGamepad lastGamepad{};
        std::uint32_t packetNumber{ 0 };
        bool initialized{ false };
    };

    struct PatchedImportSlot
    {
        std::uintptr_t* slot{ nullptr };
        std::uintptr_t original{ 0 };
    };

    using XInputGetStateFn = std::uint32_t(WINAPI*)(
        std::uint32_t,
        XInputState*);
    using XInputGetCapabilitiesFn = std::uint32_t(WINAPI*)(
        std::uint32_t,
        std::uint32_t,
        XInputCapabilities*);

    constexpr std::uint32_t maximumPatchedSlots = 16;
    constexpr std::uint32_t sourceCount = 2;
    constexpr std::uint32_t userCount = 4;
    constexpr std::uint32_t xinputGamepadType = 0x01;
    constexpr std::uint32_t xinputGamepadSubType = 0x01;
    constexpr std::uint16_t xinputGetStateOrdinal = 2;
    constexpr std::uint16_t xinputGetCapabilitiesOrdinal = 4;
    constexpr std::uint16_t xinputGetStateExOrdinal = 100;

    XInputGetStateFn g_realNamedGetState{ nullptr };
    XInputGetStateFn g_realOrdinal100GetState{ nullptr };
    XInputGetCapabilitiesFn g_realGetCapabilities{ nullptr };
    std::array<XInputReturnedSlotState, sourceCount * userCount>
        g_returnedSlotStates{};
    std::array<PatchedImportSlot, maximumPatchedSlots> g_patchedSlots{};
    std::uint32_t g_patchedSlotCount{ 0 };
    std::atomic<int> g_lastLy{ 0 };
    std::atomic<int> g_lastButtons{ 0 };
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

    [[nodiscard]] std::uint32_t StateIndex(
        XInputSource source,
        std::uint32_t userIndex)
    {
        return static_cast<std::uint32_t>(source) * userCount + userIndex;
    }

    [[nodiscard]] std::int16_t ClampStickValue(int value)
    {
        return static_cast<std::int16_t>(
            std::clamp(value, -32768, 32767));
    }

    [[nodiscard]] bool ShouldUseOutput()
    {
        const auto& settings = TLV::Settings::GetSingleton();
        return settings.Enabled() &&
            settings.DirectApiEnabled() &&
            settings.EnableOutput();
    }

    [[nodiscard]] bool ShouldFakePresenceForSlot(
        std::uint32_t userIndex,
        std::uint32_t result)
    {
        return ShouldUseOutput() && userIndex == 0 && result != ERROR_SUCCESS;
    }

    [[nodiscard]] bool GamepadChanged(
        const XInputGamepad& current,
        const XInputGamepad& previous)
    {
        return current.buttons != previous.buttons ||
            current.leftTrigger != previous.leftTrigger ||
            current.rightTrigger != previous.rightTrigger ||
            current.thumbLX != previous.thumbLX ||
            current.thumbLY != previous.thumbLY ||
            current.thumbRX != previous.thumbRX ||
            current.thumbRY != previous.thumbRY;
    }

    void FinalizeReturnedPacketNumber(
        XInputSource source,
        std::uint32_t userIndex,
        std::uint32_t originalPacketNumber,
        XInputState* state)
    {
        if (!state || userIndex >= userCount) {
            return;
        }

        std::scoped_lock lock(g_hookMutex);
        auto& returnedState = g_returnedSlotStates[
            StateIndex(source, userIndex)];
        if (!returnedState.initialized) {
            returnedState.packetNumber = originalPacketNumber;
        }
        if (!returnedState.initialized ||
            GamepadChanged(state->gamepad, returnedState.lastGamepad)) {
            returnedState.packetNumber = (std::max)(
                originalPacketNumber,
                returnedState.packetNumber + 1);
            returnedState.lastGamepad = state->gamepad;
            returnedState.initialized = true;
        }
        state->packetNumber = returnedState.packetNumber;
    }

    [[nodiscard]] bool InjectLocomotionOutput(
        std::uint32_t userIndex,
        std::uint32_t result,
        XInputState* state,
        std::int16_t& beforeLY)
    {
        beforeLY = state ? state->gamepad.thumbLY : 0;
        if (!ShouldUseOutput() ||
            userIndex != 0 ||
            result != ERROR_SUCCESS ||
            !state) {
            return false;
        }

        auto& pad = state->gamepad;
        auto& mux = TLV::InputMux::GetSingleton();
        if (mux.IsAvailable()) {
            const auto merged = mux.Merge();
            pad.thumbRX = ClampStickValue(static_cast<int>(pad.thumbRX) + merged.rx);
            pad.thumbRY = ClampStickValue(static_cast<int>(pad.thumbRY) + merged.ry);
            pad.thumbLX = ClampStickValue(static_cast<int>(pad.thumbLX) + merged.lx);
            pad.thumbLY = ClampStickValue(static_cast<int>(pad.thumbLY) + merged.ly);
            pad.buttons = static_cast<std::uint16_t>(pad.buttons | merged.buttons);
            pad.leftTrigger = static_cast<std::uint8_t>((std::max)(
                static_cast<int>(pad.leftTrigger),
                static_cast<int>(merged.leftTrigger)));
            pad.rightTrigger = static_cast<std::uint8_t>((std::max)(
                static_cast<int>(pad.rightTrigger),
                static_cast<int>(merged.rightTrigger)));
            return merged.rx != 0 || merged.ry != 0 || merged.lx != 0 ||
                merged.ly != 0 || merged.buttons != 0 ||
                merged.leftTrigger != 0 || merged.rightTrigger != 0 ||
                pad.thumbLY != beforeLY;
        }

        const auto injectedLy = g_lastLy.load(std::memory_order_relaxed);
        const auto injectedButtons =
            static_cast<std::uint16_t>(g_lastButtons.load(std::memory_order_relaxed));
        pad.thumbLY = ClampStickValue(
            static_cast<int>(pad.thumbLY) + injectedLy);
        pad.buttons = static_cast<std::uint16_t>(pad.buttons | injectedButtons);
        return injectedLy != 0 ||
            injectedButtons != 0 ||
            pad.thumbLY != beforeLY;
    }

    void FillFakeGamepadState(XInputState* state)
    {
        if (!state) {
            return;
        }

        std::memset(state, 0, sizeof(*state));
    }

    void FillFakeCapabilities(XInputCapabilities* capabilities)
    {
        if (!capabilities) {
            return;
        }

        std::memset(capabilities, 0, sizeof(*capabilities));
        capabilities->type = static_cast<std::uint8_t>(xinputGamepadType);
        capabilities->subType =
            static_cast<std::uint8_t>(xinputGamepadSubType);
    }

    std::uint32_t WINAPI XInputGetStateHook(
        std::uint32_t userIndex,
        XInputState* state)
    {
        const auto result = g_realNamedGetState ?
            g_realNamedGetState(userIndex, state) :
            static_cast<std::uint32_t>(ERROR_DEVICE_NOT_CONNECTED);
        if (ShouldFakePresenceForSlot(userIndex, result)) {
            FillFakeGamepadState(state);
            const auto originalPacketNumber = state ? state->packetNumber : 0;
            std::int16_t beforeLY = 0;
            const auto injected = InjectLocomotionOutput(
                userIndex,
                ERROR_SUCCESS,
                state,
                beforeLY);
            FinalizeReturnedPacketNumber(
                XInputSource::named,
                userIndex,
                originalPacketNumber,
                state);
            if (injected && TLV::Settings::GetSingleton().DebugLogging()) {
                logger::debug(
                    "xinput locomotion output forced slot 0 named beforeLY={} afterLY={}",
                    beforeLY,
                    state ? state->gamepad.thumbLY : 0);
            }
            return ERROR_SUCCESS;
        }

        const auto originalPacketNumber = state ? state->packetNumber : 0;
        std::int16_t beforeLY = 0;
        const auto injected = InjectLocomotionOutput(
            userIndex,
            result,
            state,
            beforeLY);
        if (ShouldUseOutput() && result == ERROR_SUCCESS && state) {
            FinalizeReturnedPacketNumber(
                XInputSource::named,
                userIndex,
                originalPacketNumber,
                state);
        }
        if (injected && TLV::Settings::GetSingleton().DebugLogging()) {
            logger::debug(
                "xinput locomotion output named user={} beforeLY={} afterLY={}",
                userIndex,
                beforeLY,
                state ? state->gamepad.thumbLY : 0);
        }
        return result;
    }

    std::uint32_t WINAPI XInputGetStateExHook(
        std::uint32_t userIndex,
        XInputState* state)
    {
        const auto result = g_realOrdinal100GetState ?
            g_realOrdinal100GetState(userIndex, state) :
            static_cast<std::uint32_t>(ERROR_DEVICE_NOT_CONNECTED);
        if (ShouldFakePresenceForSlot(userIndex, result)) {
            FillFakeGamepadState(state);
            const auto originalPacketNumber = state ? state->packetNumber : 0;
            std::int16_t beforeLY = 0;
            const auto injected = InjectLocomotionOutput(
                userIndex,
                ERROR_SUCCESS,
                state,
                beforeLY);
            FinalizeReturnedPacketNumber(
                XInputSource::ordinal100,
                userIndex,
                originalPacketNumber,
                state);
            if (injected && TLV::Settings::GetSingleton().DebugLogging()) {
                logger::debug(
                    "xinput locomotion output forced slot 0 ordinal100 beforeLY={} afterLY={}",
                    beforeLY,
                    state ? state->gamepad.thumbLY : 0);
            }
            return ERROR_SUCCESS;
        }

        const auto originalPacketNumber = state ? state->packetNumber : 0;
        std::int16_t beforeLY = 0;
        const auto injected = InjectLocomotionOutput(
            userIndex,
            result,
            state,
            beforeLY);
        if (ShouldUseOutput() && result == ERROR_SUCCESS && state) {
            FinalizeReturnedPacketNumber(
                XInputSource::ordinal100,
                userIndex,
                originalPacketNumber,
                state);
        }
        if (injected && TLV::Settings::GetSingleton().DebugLogging()) {
            logger::debug(
                "xinput locomotion output ordinal100 user={} beforeLY={} afterLY={}",
                userIndex,
                beforeLY,
                state ? state->gamepad.thumbLY : 0);
        }
        return result;
    }

    std::uint32_t WINAPI XInputGetCapabilitiesHook(
        std::uint32_t userIndex,
        std::uint32_t flags,
        XInputCapabilities* capabilities)
    {
        const auto result = g_realGetCapabilities ?
            g_realGetCapabilities(userIndex, flags, capabilities) :
            static_cast<std::uint32_t>(ERROR_DEVICE_NOT_CONNECTED);
        if (ShouldFakePresenceForSlot(userIndex, result)) {
            FillFakeCapabilities(capabilities);
            return ERROR_SUCCESS;
        }

        return result;
    }

    [[nodiscard]] bool ProtectSlot(
        std::uintptr_t* slot,
        std::uintptr_t replacement,
        std::uintptr_t& original)
    {
        if (g_patchedSlotCount >= maximumPatchedSlots) {
            logger::warn("xinput locomotion output patch table is full");
            return false;
        }

        DWORD oldProtect = 0;
        if (!VirtualProtect(
                slot,
                sizeof(*slot),
                PAGE_READWRITE,
                &oldProtect)) {
            logger::warn("xinput locomotion output VirtualProtect failed");
            return false;
        }

        original = *slot;
        *slot = replacement;
        DWORD ignoredProtect = 0;
        VirtualProtect(slot, sizeof(*slot), oldProtect, &ignoredProtect);

        g_patchedSlots[g_patchedSlotCount++] = PatchedImportSlot{
            slot,
            original
        };
        return true;
    }

    [[nodiscard]] bool PatchXInputImports()
    {
        const auto base =
            reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        if (base == 0) {
            logger::warn("xinput locomotion output could not resolve main module base");
            return false;
        }

        const auto dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
        if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
            logger::warn("xinput locomotion output found invalid DOS header");
            return false;
        }

        const auto ntHeaders =
            reinterpret_cast<IMAGE_NT_HEADERS*>(base + dosHeader->e_lfanew);
        if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) {
            logger::warn("xinput locomotion output found invalid NT header");
            return false;
        }

        const auto& importDir =
            ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (importDir.VirtualAddress == 0) {
            logger::warn("xinput locomotion output found no main-module imports");
            return false;
        }

        std::uint32_t patchedNamed = 0;
        std::uint32_t patchedOrdinal = 0;
        std::uint32_t patchedCapabilities = 0;

        auto descriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(
            base + importDir.VirtualAddress);
        for (; descriptor->Name != 0; ++descriptor) {
            const auto moduleName =
                reinterpret_cast<const char*>(base + descriptor->Name);
            if (!IsXInputModuleName(moduleName)) {
                continue;
            }

            logger::info(
                "xinput locomotion output inspecting import descriptor {}",
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
                if (current == reinterpret_cast<std::uintptr_t>(
                        &XInputGetStateHook) ||
                    current == reinterpret_cast<std::uintptr_t>(
                        &XInputGetStateExHook) ||
                    current == reinterpret_cast<std::uintptr_t>(
                        &XInputGetCapabilitiesHook)) {
                    continue;
                }

                auto slot = reinterpret_cast<std::uintptr_t*>(
                    &thunk->u1.Function);
                const auto import = importThunk->u1.AddressOfData;
                const auto ordinalImport = IMAGE_SNAP_BY_ORDINAL(import);
                const auto ordinal = ordinalImport ?
                    IMAGE_ORDINAL(import) :
                    0;
                const char* importedName = nullptr;
                if (!ordinalImport) {
                    const auto byName = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(
                        base + import);
                    importedName =
                        reinterpret_cast<const char*>(byName->Name);
                }

                if (!ordinalImport &&
                    importedName &&
                    std::strcmp(importedName, "XInputGetCapabilities") == 0) {
                    std::uintptr_t original = 0;
                    if (ProtectSlot(
                            slot,
                            reinterpret_cast<std::uintptr_t>(
                                &XInputGetCapabilitiesHook),
                            original)) {
                        if (!g_realGetCapabilities) {
                            g_realGetCapabilities =
                                reinterpret_cast<XInputGetCapabilitiesFn>(
                                    original);
                        }
                        ++patchedCapabilities;
                    }
                } else if (
                    ordinalImport &&
                    ordinal == xinputGetCapabilitiesOrdinal) {
                    std::uintptr_t original = 0;
                    if (ProtectSlot(
                            slot,
                            reinterpret_cast<std::uintptr_t>(
                                &XInputGetCapabilitiesHook),
                            original)) {
                        if (!g_realGetCapabilities) {
                            g_realGetCapabilities =
                                reinterpret_cast<XInputGetCapabilitiesFn>(
                                    original);
                        }
                        ++patchedCapabilities;
                    }
                } else if (
                    ordinalImport &&
                    ordinal == xinputGetStateExOrdinal) {
                    std::uintptr_t original = 0;
                    if (ProtectSlot(
                            slot,
                            reinterpret_cast<std::uintptr_t>(
                                &XInputGetStateExHook),
                            original)) {
                        if (!g_realOrdinal100GetState) {
                            g_realOrdinal100GetState =
                                reinterpret_cast<XInputGetStateFn>(
                                    original);
                        }
                        ++patchedOrdinal;
                    }
                } else if (
                    ordinalImport &&
                    ordinal == xinputGetStateOrdinal) {
                    std::uintptr_t original = 0;
                    if (ProtectSlot(
                            slot,
                            reinterpret_cast<std::uintptr_t>(
                                &XInputGetStateHook),
                            original)) {
                        if (!g_realNamedGetState) {
                            g_realNamedGetState =
                                reinterpret_cast<XInputGetStateFn>(
                                    original);
                        }
                        ++patchedNamed;
                    }
                } else if (!ordinalImport &&
                    importedName &&
                    std::strcmp(importedName, "XInputGetState") == 0) {
                    std::uintptr_t original = 0;
                    if (ProtectSlot(
                            slot,
                            reinterpret_cast<std::uintptr_t>(
                                &XInputGetStateHook),
                            original)) {
                        if (!g_realNamedGetState) {
                            g_realNamedGetState =
                                reinterpret_cast<XInputGetStateFn>(
                                    original);
                        }
                        ++patchedNamed;
                    }
                }
            }
        }

        if (patchedNamed == 0 &&
            patchedOrdinal == 0 &&
            patchedCapabilities == 0) {
            logger::warn("xinput locomotion output found no XInput IAT slots");
            return false;
        }

        logger::info(
            "xinput locomotion output installed: namedSlots={} ordinal100Slots={} capabilitySlots={}",
            patchedNamed,
            patchedOrdinal,
            patchedCapabilities);
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
                "xinput locomotion output restored {} IAT slot(s)",
                g_patchedSlotCount);
        }
        g_patchedSlotCount = 0;
        g_realNamedGetState = nullptr;
        g_realOrdinal100GetState = nullptr;
        g_realGetCapabilities = nullptr;
        g_returnedSlotStates = {};
        g_lastLy.store(0, std::memory_order_relaxed);
        g_lastButtons.store(0, std::memory_order_relaxed);
    }
}

namespace TLV
{
    XInputLocomotionOutput& XInputLocomotionOutput::GetSingleton()
    {
        static XInputLocomotionOutput singleton;
        return singleton;
    }

    XInputLocomotionOutput::~XInputLocomotionOutput()
    {
        Shutdown();
    }

    bool XInputLocomotionOutput::Initialize()
    {
        if (installed_) {
            return true;
        }

        if (!ShouldUseOutput()) {
            return false;
        }

        auto& mux = InputMux::GetSingleton();
        if (mux.Initialize(muxModId) && !mux.IsOwner()) {
            logger::info(
                "treadmill deferring to existing XInput hook owner; "
                "contributing locomotion via the mux only");
            return true;
        }

        installed_ = PatchXInputImports();
        return installed_;
    }

    void XInputLocomotionOutput::Shutdown()
    {
        SetLocomotion(0, 0);
        if (installed_) {
            RestorePatchedImports();
            installed_ = false;
        }
        InputMux::GetSingleton().Shutdown();
    }

    bool XInputLocomotionOutput::IsInstalled() const
    {
        return installed_;
    }

    bool XInputLocomotionOutput::HasStateHook() const
    {
        return g_realNamedGetState || g_realOrdinal100GetState;
    }

    bool XInputLocomotionOutput::IsOutputPathReady() const
    {
        return HasStateHook() || InputMux::GetSingleton().IsAvailable();
    }

    void XInputLocomotionOutput::SetLocomotion(
        std::int16_t leftY,
        std::uint16_t buttons)
    {
        g_lastLy.store(leftY, std::memory_order_relaxed);
        g_lastButtons.store(buttons, std::memory_order_relaxed);
        InputMux::GetSingleton().SetContribution(
            0,
            0,
            0,
            leftY,
            buttons,
            0,
            0);
    }

    float XInputLocomotionOutput::LastAppliedLeftY() const
    {
        return static_cast<float>(
            g_lastLy.load(std::memory_order_relaxed)) / 32767.0F;
    }

    std::uint16_t XInputLocomotionOutput::LastAppliedButtons() const
    {
        return static_cast<std::uint16_t>(
            g_lastButtons.load(std::memory_order_relaxed));
    }
}
