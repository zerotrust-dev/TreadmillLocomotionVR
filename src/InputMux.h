#pragma once

#include <cstdint>

namespace TLV
{
    // Shared cross-DLL XInput contribution mux. Exactly one participant (the
    // election winner) owns the XInput hook; every participant writes its axis
    // contribution to its own slot, and the owner merges them. This client is
    // vendored identically across every mod that participates -- the shared
    // block ABI (struct layout, magic, version, names) must match byte for byte.
    // Contract: HeadDirectedTurning/.research/xinput-mux/SPEC.md.
    class InputMux
    {
    public:
        struct Merged
        {
            int rx{ 0 };
            int ry{ 0 };
            int lx{ 0 };
            int ly{ 0 };
            std::uint16_t buttons{ 0 };
            std::uint8_t leftTrigger{ 0 };
            std::uint8_t rightTrigger{ 0 };
        };

        static InputMux& GetSingleton();

        // myId: unique non-zero id for this mod. Returns true if the shared
        // block is usable (magic/version ok, slot claimed).
        [[nodiscard]] bool Initialize(std::uint32_t myId);
        void Shutdown();

        [[nodiscard]] bool IsAvailable() const { return available_; }
        [[nodiscard]] bool IsOwner() const { return owner_; }

        // Write this mod's contribution and stamp the heartbeat. Pass 0 for
        // axes/buttons this mod does not drive this frame.
        void SetContribution(
            std::int16_t rx,
            std::int16_t ry,
            std::int16_t lx,
            std::int16_t ly,
            std::uint16_t buttons,
            std::uint8_t leftTrigger,
            std::uint8_t rightTrigger);

        // Owner-side: sum all fresh contributors.
        [[nodiscard]] Merged Merge() const;

    private:
        InputMux() = default;
        ~InputMux();
        InputMux(const InputMux&) = delete;
        InputMux& operator=(const InputMux&) = delete;

        void* mapping_{ nullptr };
        void* view_{ nullptr };
        std::uint32_t myId_{ 0 };
        int slotIndex_{ -1 };
        bool owner_{ false };
        bool available_{ false };
    };
}
