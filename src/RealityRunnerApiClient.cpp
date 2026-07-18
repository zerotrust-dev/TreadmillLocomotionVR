#include "RealityRunnerApiClient.h"

#include "Settings.h"

namespace
{
    struct SerialHandle
    {
        HANDLE value{ INVALID_HANDLE_VALUE };

        ~SerialHandle()
        {
            if (value != INVALID_HANDLE_VALUE) {
                CloseHandle(value);
            }
        }

        SerialHandle() = default;
        SerialHandle(const SerialHandle&) = delete;
        SerialHandle& operator=(const SerialHandle&) = delete;
    };

    [[nodiscard]] std::wstring ToWide(std::string_view value)
    {
        return std::wstring(value.begin(), value.end());
    }

    [[nodiscard]] std::wstring DevicePath(std::string_view portName)
    {
        auto wide = ToWide(portName);
        if (wide.starts_with(LR"(\\.\)")) {
            return wide;
        }
        return LR"(\\.\)" + wide;
    }

    [[nodiscard]] bool ConfigureSerial(HANDLE handle)
    {
        DCB dcb{};
        dcb.DCBlength = sizeof(dcb);
        if (!GetCommState(handle, &dcb)) {
            logger::warn("RealityRunner GetCommState failed error={}", GetLastError());
            return false;
        }

        dcb.BaudRate = CBR_115200;
        dcb.ByteSize = 8;
        dcb.Parity = NOPARITY;
        dcb.StopBits = ONESTOPBIT;
        dcb.fBinary = TRUE;
        dcb.fOutxCtsFlow = FALSE;
        dcb.fOutxDsrFlow = FALSE;
        dcb.fDtrControl = DTR_CONTROL_ENABLE;
        dcb.fDsrSensitivity = FALSE;
        dcb.fOutX = FALSE;
        dcb.fInX = FALSE;
        dcb.fRtsControl = RTS_CONTROL_ENABLE;

        if (!SetCommState(handle, &dcb)) {
            logger::warn("RealityRunner SetCommState failed error={}", GetLastError());
            return false;
        }

        COMMTIMEOUTS timeouts{};
        timeouts.ReadIntervalTimeout = 50;
        timeouts.ReadTotalTimeoutConstant = 100;
        timeouts.ReadTotalTimeoutMultiplier = 0;
        timeouts.WriteTotalTimeoutConstant = 1000;
        timeouts.WriteTotalTimeoutMultiplier = 0;
        if (!SetCommTimeouts(handle, &timeouts)) {
            logger::warn("RealityRunner SetCommTimeouts failed error={}", GetLastError());
            return false;
        }

        SetupComm(handle, 4096, 4096);
        PurgeComm(handle, PURGE_RXCLEAR | PURGE_TXCLEAR);
        return true;
    }

    [[nodiscard]] bool WriteAll(HANDLE handle, std::string_view text)
    {
        DWORD written = 0;
        const auto* data = text.data();
        auto remaining = static_cast<DWORD>(text.size());
        while (remaining > 0) {
            DWORD chunk = 0;
            if (!WriteFile(handle, data + written, remaining, &chunk, nullptr)) {
                logger::warn("RealityRunner WriteFile failed error={}", GetLastError());
                return false;
            }
            if (chunk == 0) {
                logger::warn("RealityRunner WriteFile wrote 0 bytes");
                return false;
            }
            written += chunk;
            remaining -= chunk;
        }
        return true;
    }

    [[nodiscard]] bool ReadExact(
        HANDLE handle,
        char* destination,
        std::size_t count,
        std::atomic<bool>& stopRequested)
    {
        std::size_t offset = 0;
        while (offset < count && !stopRequested.load(std::memory_order_relaxed)) {
            DWORD read = 0;
            if (!ReadFile(
                    handle,
                    destination + offset,
                    static_cast<DWORD>(count - offset),
                    &read,
                    nullptr)) {
                logger::warn("RealityRunner ReadFile failed error={}", GetLastError());
                return false;
            }
            if (read == 0) {
                continue;
            }
            offset += read;
        }
        return offset == count;
    }

    [[nodiscard]] std::optional<std::string> ReadFrame(
        HANDLE handle,
        std::atomic<bool>& stopRequested)
    {
        char header[4]{};
        if (!ReadExact(handle, header, sizeof(header), stopRequested)) {
            return std::nullopt;
        }
        if (header[3] != ':' ||
            !std::isdigit(static_cast<unsigned char>(header[0])) ||
            !std::isdigit(static_cast<unsigned char>(header[1])) ||
            !std::isdigit(static_cast<unsigned char>(header[2]))) {
            logger::warn(
                "RealityRunner invalid frame header bytes={:02x} {:02x} {:02x} {:02x}",
                static_cast<unsigned char>(header[0]),
                static_cast<unsigned char>(header[1]),
                static_cast<unsigned char>(header[2]),
                static_cast<unsigned char>(header[3]));
            PurgeComm(handle, PURGE_RXCLEAR);
            return std::nullopt;
        }

        const auto length =
            (header[0] - '0') * 100 + (header[1] - '0') * 10 + (header[2] - '0');
        std::string payload(static_cast<std::size_t>(length), '\0');
        if (length > 0 &&
            !ReadExact(handle, payload.data(), payload.size(), stopRequested)) {
            return std::nullopt;
        }

        char newline = 0;
        if (!ReadExact(handle, &newline, 1, stopRequested) || newline != '\n') {
            logger::warn("RealityRunner frame missing newline");
            PurgeComm(handle, PURGE_RXCLEAR);
            return std::nullopt;
        }
        return payload;
    }

    [[nodiscard]] std::optional<std::string> SendCommand(
        HANDLE handle,
        std::string_view command,
        std::atomic<bool>& stopRequested)
    {
        PurgeComm(handle, PURGE_RXCLEAR);
        if (!WriteAll(handle, command)) {
            return std::nullopt;
        }
        return ReadFrame(handle, stopRequested);
    }

    [[nodiscard]] std::vector<std::string> Split(std::string_view text)
    {
        std::vector<std::string> parts;
        std::string current;
        std::istringstream stream{ std::string(text) };
        while (std::getline(stream, current, ',')) {
            parts.push_back(current);
        }
        return parts;
    }

    [[nodiscard]] bool ParseBool(std::string_view text)
    {
        return text == "true" || text == "1";
    }

    [[nodiscard]] std::optional<TLV::RealityRunnerCurve> ParseCurve(
        std::string_view payload)
    {
        const auto parts = Split(payload);
        if (parts.size() < 14) {
            return std::nullopt;
        }

        try {
            TLV::RealityRunnerCurve curve{};
            curve.sprintMode = std::stoi(parts[0]);
            curve.sprintButton = std::stoi(parts[1]);
            curve.invertYaxis = ParseBool(parts[2]);
            curve.deadzone = std::stoi(parts[3]);
            curve.sprintThreshold = std::stoi(parts[4]);
            curve.maxPulses = std::stof(parts[5]);
            curve.y1 = std::stoi(parts[6]);
            curve.y2 = std::stoi(parts[7]);
            curve.y3 = std::stoi(parts[8]);
            curve.y4 = std::stoi(parts[9]);
            curve.y5 = std::stoi(parts[10]);
            curve.reverseMode = std::stoi(parts[11]);
            curve.index = std::stoi(parts[12]);
            curve.name = parts[13];
            return curve;
        } catch (...) {
            return std::nullopt;
        }
    }

    [[nodiscard]] std::optional<std::pair<int, bool>> ParseJoystick(
        std::string_view payload)
    {
        const auto parts = Split(payload);
        if (parts.size() < 2) {
            return std::nullopt;
        }
        try {
            return std::pair{ std::stoi(parts[0]), std::stoi(parts[1]) != 0 };
        } catch (...) {
            return std::nullopt;
        }
    }
}

namespace TLV
{
    RealityRunnerApiClient& RealityRunnerApiClient::GetSingleton()
    {
        static RealityRunnerApiClient singleton;
        return singleton;
    }

    RealityRunnerApiClient::~RealityRunnerApiClient()
    {
        Stop();
    }

    bool RealityRunnerApiClient::Start()
    {
        if (running_.load(std::memory_order_relaxed)) {
            return true;
        }

        const auto& settings = Settings::GetSingleton();
        if (!settings.Enabled() || !settings.DirectApiEnabled()) {
            logger::info("RealityRunner direct API reader disabled");
            return false;
        }

        stopRequested_.store(false, std::memory_order_relaxed);
        running_.store(true, std::memory_order_relaxed);
        thread_ = std::thread(
            &RealityRunnerApiClient::ThreadMain,
            this,
            settings.ComPort());
        return true;
    }

    void RealityRunnerApiClient::Stop()
    {
        stopRequested_.store(true, std::memory_order_relaxed);
        if (thread_.joinable()) {
            thread_.join();
        }
        running_.store(false, std::memory_order_relaxed);
        MarkDisconnected();
    }

    RealityRunnerSnapshot RealityRunnerApiClient::Latest() const
    {
        std::scoped_lock lock(mutex_);
        return latest_;
    }

    RealityRunnerCurve RealityRunnerApiClient::Curve() const
    {
        std::scoped_lock lock(mutex_);
        return curve_;
    }

    void RealityRunnerApiClient::Publish(
        int joystickValue,
        bool sprintActive,
        std::string_view raw)
    {
        std::scoped_lock lock(mutex_);
        ++latest_.sequence;
        latest_.tickMs = GetTickCount64();
        latest_.joystickValue = joystickValue;
        latest_.sprintActive = sprintActive;
        latest_.connected = true;
        std::memset(latest_.rawPayload, 0, sizeof(latest_.rawPayload));
        const auto copyLength =
            (std::min)(raw.size(), sizeof(latest_.rawPayload) - 1);
        std::memcpy(latest_.rawPayload, raw.data(), copyLength);
    }

    void RealityRunnerApiClient::MarkDisconnected()
    {
        std::scoped_lock lock(mutex_);
        latest_.connected = false;
    }

    void RealityRunnerApiClient::ThreadMain(std::string portName)
    {
        logger::info("RealityRunner API reader opening {}", portName);

        SerialHandle handle;
        const auto path = DevicePath(portName);
        handle.value = CreateFileW(
            path.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (handle.value == INVALID_HANDLE_VALUE) {
            logger::error(
                "RealityRunner API reader could not open {} error={}; close the desktop app",
                portName,
                GetLastError());
            running_.store(false, std::memory_order_relaxed);
            MarkDisconnected();
            return;
        }

        if (!ConfigureSerial(handle.value)) {
            running_.store(false, std::memory_order_relaxed);
            MarkDisconnected();
            return;
        }

        Sleep(250);
        PurgeComm(handle.value, PURGE_RXCLEAR | PURGE_TXCLEAR);

        const auto curvePayload =
            SendCommand(handle.value, "GET curve\n", stopRequested_);
        if (!curvePayload) {
            logger::error("RealityRunner API reader failed to read curve");
            running_.store(false, std::memory_order_relaxed);
            MarkDisconnected();
            return;
        }
        if (auto curve = ParseCurve(*curvePayload)) {
            const auto& parsedCurve = *curve;
            std::scoped_lock lock(mutex_);
            curve_ = parsedCurve;
            logger::info(
                "RealityRunner curve name={} deadzone={} sprintThreshold={} "
                "invertY={} y=[{},{},{},{},{}]",
                parsedCurve.name,
                parsedCurve.deadzone,
                parsedCurve.sprintThreshold,
                parsedCurve.invertYaxis,
                parsedCurve.y1,
                parsedCurve.y2,
                parsedCurve.y3,
                parsedCurve.y4,
                parsedCurve.y5);
        } else {
            logger::warn("RealityRunner curve parse failed: {}", *curvePayload);
        }

        const auto profilePayload =
            SendCommand(handle.value, "GET profiles\n", stopRequested_);
        if (profilePayload) {
            logger::info("RealityRunner profile payload: {}", *profilePayload);
        } else {
            logger::warn("RealityRunner profile read failed");
        }

        const auto bootModePayload =
            SendCommand(handle.value, "GET bootmode\n", stopRequested_);
        if (bootModePayload) {
            logger::info("RealityRunner boot mode payload: {}", *bootModePayload);
        } else {
            logger::warn("RealityRunner boot mode read failed");
        }

        // `SET stream true,WIRED` is a one-shot enable: the device then pushes
        // joystick frames continuously. Re-sending it per iteration would purge
        // the receive buffer (SendCommand starts with PURGE_RXCLEAR) and throw
        // away every frame pushed since the last poll, adding latency and
        // discarding data. Enable it once, then only read.
        //
        // ReadFrame blocks inside ReadExact until a frame arrives or a stop is
        // requested, so the loop is paced by the device itself and needs no
        // sleep. A silent device simply blocks here, publishes nothing, and the
        // consumer's staleness watchdog falls back to stopped.
        const auto streamAck =
            SendCommand(handle.value, "SET stream true,WIRED\n", stopRequested_);
        if (!streamAck) {
            if (!stopRequested_.load(std::memory_order_relaxed)) {
                logger::error("RealityRunner stream enable failed; stopping reader");
            }
            running_.store(false, std::memory_order_relaxed);
            MarkDisconnected();
            return;
        }
        logger::info("RealityRunner joystick stream enabled");

        // The reply to the enable may already be the first joystick frame.
        if (const auto firstFrame = ParseJoystick(*streamAck); firstFrame) {
            Publish(firstFrame->first, firstFrame->second, *streamAck);
        }

        // ReadFrame returns nullopt only on a read error or a framing fault
        // (both of which resync by purging), never on mere silence. A run of
        // them means the link is unusable, so give up rather than spin.
        constexpr std::uint32_t maximumConsecutiveFrameFaults = 50;
        std::uint32_t consecutiveFrameFaults = 0;
        while (!stopRequested_.load(std::memory_order_relaxed)) {
            const auto payload = ReadFrame(handle.value, stopRequested_);
            if (!payload) {
                if (stopRequested_.load(std::memory_order_relaxed)) {
                    break;
                }
                if (++consecutiveFrameFaults >= maximumConsecutiveFrameFaults) {
                    logger::warn(
                        "RealityRunner stream hit {} consecutive frame faults; "
                        "stopping reader",
                        consecutiveFrameFaults);
                    break;
                }
                continue;
            }
            consecutiveFrameFaults = 0;

            const auto joystick = ParseJoystick(*payload);
            if (!joystick) {
                logger::debug("Ignoring non-joystick stream payload: {}", *payload);
                continue;
            }
            Publish(joystick->first, joystick->second, *payload);
        }

        (void)SendCommand(handle.value, "SET stream false,WIRED\n", stopRequested_);
        running_.store(false, std::memory_order_relaxed);
        MarkDisconnected();
        logger::info("RealityRunner API reader stopped");
    }
}
