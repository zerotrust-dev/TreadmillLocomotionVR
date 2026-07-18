#include "InputMux.h"

#include <Windows.h>

#include <algorithm>
#include <cstring>

namespace
{
    // These constants define the shared cross-DLL ABI. They MUST be identical
    // in every participating mod (see the shared spec).
    constexpr std::uint32_t muxMagic = 0x494D5558;  // 'IMUX'
    constexpr std::uint32_t muxVersion = 1;
    constexpr std::uint32_t muxSourceCount = 8;
    constexpr std::uint64_t muxFreshnessMs = 500;
    constexpr auto muxMappingName = L"Local\\SkyrimVRInputMux_v1";
    constexpr auto muxInitMutexName = L"Local\\SkyrimVRInputMux_v1_init";

    struct InputMuxSource
    {
        volatile LONG claimedId;
        volatile LONG thumbRX;
        volatile LONG thumbRY;
        volatile LONG thumbLX;
        volatile LONG thumbLY;
        volatile LONG buttons;
        volatile LONG leftTrigger;
        volatile LONG rightTrigger;
        volatile LONG64 heartbeatMs;
    };

    struct InputMuxShared
    {
        volatile LONG magic;
        volatile LONG version;
        volatile LONG ownerId;
        volatile LONG pad;
        InputMuxSource sources[muxSourceCount];
    };
}

namespace TLV
{
    InputMux& InputMux::GetSingleton()
    {
        static InputMux singleton;
        return singleton;
    }

    InputMux::~InputMux()
    {
        Shutdown();
    }

    bool InputMux::Initialize(std::uint32_t myId)
    {
        if (available_) {
            return true;
        }
        if (myId == 0) {
            return false;
        }
        myId_ = myId;

        const auto mapping = CreateFileMappingW(
            INVALID_HANDLE_VALUE,
            nullptr,
            PAGE_READWRITE,
            0,
            sizeof(InputMuxShared),
            muxMappingName);
        if (!mapping) {
            logger::warn("input-mux CreateFileMapping failed error={}", GetLastError());
            return false;
        }

        const auto view = MapViewOfFile(
            mapping,
            FILE_MAP_READ | FILE_MAP_WRITE,
            0,
            0,
            sizeof(InputMuxShared));
        if (!view) {
            logger::warn("input-mux MapViewOfFile failed error={}", GetLastError());
            CloseHandle(mapping);
            return false;
        }

        auto shared = static_cast<InputMuxShared*>(view);

        // First creator initializes the block under a named mutex so the
        // create/zero race between DLLs is closed.
        const auto initMutex = CreateMutexW(nullptr, FALSE, muxInitMutexName);
        if (initMutex) {
            WaitForSingleObject(initMutex, 5000);
        }
        if (static_cast<std::uint32_t>(shared->magic) != muxMagic) {
            std::memset(view, 0, sizeof(InputMuxShared));
            shared->version = static_cast<LONG>(muxVersion);
            shared->magic = static_cast<LONG>(muxMagic);
        }
        if (initMutex) {
            ReleaseMutex(initMutex);
            CloseHandle(initMutex);
        }

        if (static_cast<std::uint32_t>(shared->version) != muxVersion) {
            logger::error(
                "input-mux version mismatch: block={} expected={}; not participating",
                static_cast<std::uint32_t>(shared->version),
                muxVersion);
            UnmapViewOfFile(view);
            CloseHandle(mapping);
            return false;
        }

        // Election: first to swap ownerId from 0 wins. Same PID for all mods in
        // this process, so identity is myId, never GetCurrentProcessId().
        owner_ = InterlockedCompareExchange(
            &shared->ownerId,
            static_cast<LONG>(myId),
            0) == 0;

        // Claim the first free source slot.
        for (std::uint32_t i = 0; i < muxSourceCount; ++i) {
            if (InterlockedCompareExchange(
                    &shared->sources[i].claimedId,
                    static_cast<LONG>(myId),
                    0) == 0) {
                slotIndex_ = static_cast<int>(i);
                break;
            }
        }
        if (slotIndex_ < 0) {
            logger::error("input-mux no free contribution slot");
            UnmapViewOfFile(view);
            CloseHandle(mapping);
            return false;
        }

        mapping_ = mapping;
        view_ = view;
        available_ = true;
        logger::info(
            "input-mux ready id=0x{:08x} owner={} slot={}",
            myId,
            owner_,
            slotIndex_);
        return true;
    }

    void InputMux::Shutdown()
    {
        if (view_) {
            auto shared = static_cast<InputMuxShared*>(view_);
            if (slotIndex_ >= 0) {
                std::memset(
                    &shared->sources[slotIndex_],
                    0,
                    sizeof(InputMuxSource));
            }
            if (owner_) {
                InterlockedCompareExchange(
                    &shared->ownerId,
                    0,
                    static_cast<LONG>(myId_));
            }
            UnmapViewOfFile(view_);
            view_ = nullptr;
        }
        if (mapping_) {
            CloseHandle(mapping_);
            mapping_ = nullptr;
        }
        slotIndex_ = -1;
        owner_ = false;
        available_ = false;
    }

    void InputMux::SetContribution(
        std::int16_t rx,
        std::int16_t ry,
        std::int16_t lx,
        std::int16_t ly,
        std::uint16_t buttons,
        std::uint8_t leftTrigger,
        std::uint8_t rightTrigger)
    {
        if (!available_ || slotIndex_ < 0) {
            return;
        }
        auto& source = static_cast<InputMuxShared*>(view_)->sources[slotIndex_];
        InterlockedExchange(&source.thumbRX, rx);
        InterlockedExchange(&source.thumbRY, ry);
        InterlockedExchange(&source.thumbLX, lx);
        InterlockedExchange(&source.thumbLY, ly);
        InterlockedExchange(&source.buttons, buttons);
        InterlockedExchange(&source.leftTrigger, leftTrigger);
        InterlockedExchange(&source.rightTrigger, rightTrigger);
        InterlockedExchange64(
            &source.heartbeatMs,
            static_cast<LONG64>(GetTickCount64()));
    }

    InputMux::Merged InputMux::Merge() const
    {
        Merged merged{};
        if (!available_) {
            return merged;
        }

        const auto shared = static_cast<const InputMuxShared*>(view_);
        const auto now = GetTickCount64();
        int leftTrigger = 0;
        int rightTrigger = 0;
        for (std::uint32_t i = 0; i < muxSourceCount; ++i) {
            const auto& source = shared->sources[i];
            if (source.claimedId == 0) {
                continue;
            }
            const auto heartbeat =
                static_cast<std::uint64_t>(source.heartbeatMs);
            if (heartbeat != 0 && now - heartbeat > muxFreshnessMs) {
                continue;
            }
            merged.rx += source.thumbRX;
            merged.ry += source.thumbRY;
            merged.lx += source.thumbLX;
            merged.ly += source.thumbLY;
            merged.buttons |= static_cast<std::uint16_t>(source.buttons);
            leftTrigger = (std::max)(leftTrigger, static_cast<int>(source.leftTrigger));
            rightTrigger = (std::max)(rightTrigger, static_cast<int>(source.rightTrigger));
        }
        merged.leftTrigger = static_cast<std::uint8_t>(std::clamp(leftTrigger, 0, 255));
        merged.rightTrigger = static_cast<std::uint8_t>(std::clamp(rightTrigger, 0, 255));
        return merged;
    }
}
