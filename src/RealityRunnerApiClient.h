#pragma once

namespace TLV
{
    struct RealityRunnerCurve
    {
        int sprintMode{ 0 };
        int sprintButton{ 0 };
        bool invertYaxis{ false };
        int deadzone{ 0 };
        int sprintThreshold{ 0 };
        float maxPulses{ 0.0F };
        int y1{ 0 };
        int y2{ 0 };
        int y3{ 0 };
        int y4{ 0 };
        int y5{ 0 };
        int reverseMode{ 0 };
        int index{ 0 };
        std::string name;
    };

    struct RealityRunnerSnapshot
    {
        std::uint64_t sequence{ 0 };
        std::uint64_t tickMs{ 0 };
        int joystickValue{ 0 };
        bool sprintActive{ false };
        bool connected{ false };
        char rawPayload[64]{};
    };

    class RealityRunnerApiClient
    {
    public:
        static RealityRunnerApiClient& GetSingleton();

        [[nodiscard]] bool Start();
        void Stop();
        [[nodiscard]] RealityRunnerSnapshot Latest() const;
        [[nodiscard]] RealityRunnerCurve Curve() const;

    private:
        RealityRunnerApiClient() = default;
        ~RealityRunnerApiClient();
        RealityRunnerApiClient(const RealityRunnerApiClient&) = delete;
        RealityRunnerApiClient& operator=(const RealityRunnerApiClient&) = delete;

        void ThreadMain(std::string portName);
        void Publish(int joystickValue, bool sprintActive, std::string_view raw);
        void MarkDisconnected();

        mutable std::mutex mutex_;
        RealityRunnerSnapshot latest_{};
        RealityRunnerCurve curve_{};
        std::thread thread_;
        std::atomic<bool> stopRequested_{ false };
        std::atomic<bool> running_{ false };
    };
}
