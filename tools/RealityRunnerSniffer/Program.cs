using System.Diagnostics;
using System.Globalization;
using System.Text;
using NetMQ;
using NetMQ.Sockets;

var options = Options.Parse(args);
Directory.CreateDirectory(Path.GetDirectoryName(options.OutputPath)!);

Console.WriteLine("RealityRunner ZMQ sniffer");
Console.WriteLine($"  Endpoints: {string.Join(", ", options.Endpoints)}");
Console.WriteLine($"  Seconds:   {options.Seconds}");
Console.WriteLine($"  Output:    {options.OutputPath}");
Console.WriteLine($"  Max bytes: {options.MaxBytesPerFrame}");
Console.WriteLine($"  Progress:  every {options.ProgressEvery} message(s)");
Console.WriteLine();

using var writer = new StreamWriter(options.OutputPath, append: false, Encoding.UTF8);
writer.WriteLine(
    "message_index,time_utc,elapsed_ms,endpoint_count,frame_index,frame_count," +
    "frame_size,frame_text,frame_hex");

var messageIndex = 0L;
var stopwatch = Stopwatch.StartNew();
var deadline = DateTime.UtcNow + TimeSpan.FromSeconds(options.Seconds);

{
    using var subscriber = new SubscriberSocket();
    subscriber.Options.ReceiveHighWatermark = 100000;
    subscriber.SubscribeToAnyTopic();

    foreach (var endpoint in options.Endpoints) {
        Console.WriteLine($"Connecting {endpoint}");
        subscriber.Connect(endpoint);
    }

    Console.WriteLine("Listening. Move the treadmill or interact with RealityRunner now.");

    while (DateTime.UtcNow < deadline) {
        var message = new NetMQMessage();
        if (!subscriber.TryReceiveMultipartMessage(
                TimeSpan.FromMilliseconds(100),
                ref message)) {
            continue;
        }

        ++messageIndex;
        var now = DateTime.UtcNow;
        var elapsedMs = stopwatch.ElapsedMilliseconds;
        if (options.ProgressEvery > 0 &&
            messageIndex % options.ProgressEvery == 0) {
            Console.WriteLine(
                $"#{messageIndex} {elapsedMs,8} ms frames={message.FrameCount}");
        }

        for (var frameIndex = 0; frameIndex < message.FrameCount; ++frameIndex) {
            var bytes = message[frameIndex].ToByteArray();
            var text = DecodeText(bytes, options.MaxBytesPerFrame);
            var hex = ToHex(bytes, options.MaxBytesPerFrame);
            writer.Write(messageIndex.ToString(CultureInfo.InvariantCulture));
            writer.Write(',');
            writer.Write(EscapeCsv(now.ToString("O", CultureInfo.InvariantCulture)));
            writer.Write(',');
            writer.Write(elapsedMs.ToString(CultureInfo.InvariantCulture));
            writer.Write(',');
            writer.Write(options.Endpoints.Count.ToString(CultureInfo.InvariantCulture));
            writer.Write(',');
            writer.Write(frameIndex.ToString(CultureInfo.InvariantCulture));
            writer.Write(',');
            writer.Write(message.FrameCount.ToString(CultureInfo.InvariantCulture));
            writer.Write(',');
            writer.Write(bytes.Length.ToString(CultureInfo.InvariantCulture));
            writer.Write(',');
            writer.Write(EscapeCsv(text));
            writer.Write(',');
            writer.Write(EscapeCsv(hex));
            writer.WriteLine();
        }

        writer.Flush();
    }
}

NetMQConfig.Cleanup(block: false);

Console.WriteLine();
Console.WriteLine($"Done. Captured {messageIndex} message(s).");

static string DecodeText(byte[] bytes, int maxBytes)
{
    var length = Math.Min(bytes.Length, maxBytes);
    if (length == 0) {
        return "";
    }

    try {
        var text = Encoding.UTF8.GetString(bytes, 0, length);
        var printable = text.All(ch =>
            ch == '\r' ||
            ch == '\n' ||
            ch == '\t' ||
            !char.IsControl(ch));
        if (!printable) {
            return "";
        }
        return length < bytes.Length ? text + "...<truncated>" : text;
    }
    catch (DecoderFallbackException) {
        return "";
    }
}

static string ToHex(byte[] bytes, int maxBytes)
{
    var length = Math.Min(bytes.Length, maxBytes);
    if (length == 0) {
        return "";
    }

    var builder = new StringBuilder(length * 3);
    for (var i = 0; i < length; ++i) {
        if (i > 0) {
            builder.Append(' ');
        }
        builder.Append(bytes[i].ToString("X2", CultureInfo.InvariantCulture));
    }
    if (length < bytes.Length) {
        builder.Append(" ...<truncated>");
    }
    return builder.ToString();
}

static string EscapeCsv(string value)
{
    if (value.Contains('"') ||
        value.Contains(',') ||
        value.Contains('\r') ||
        value.Contains('\n')) {
        return "\"" + value.Replace("\"", "\"\"") + "\"";
    }
    return value;
}

sealed class Options
{
    public List<string> Endpoints { get; } = new();
    public int Seconds { get; private set; } = 60;
    public int MaxBytesPerFrame { get; private set; } = 4096;
    public int ProgressEvery { get; private set; } = 100;
    public string OutputPath { get; private set; } = Path.Combine(
        "captures",
        $"realityrunner-zmq-{DateTime.Now:yyyyMMdd-HHmmss}.csv");

    public static Options Parse(string[] args)
    {
        var options = new Options();
        for (var i = 0; i < args.Length; ++i) {
            var arg = args[i];
            switch (arg) {
            case "--endpoint":
            case "-e":
                options.Endpoints.Add(RequireValue(args, ref i, arg));
                break;
            case "--seconds":
            case "-s":
                options.Seconds = int.Parse(
                    RequireValue(args, ref i, arg),
                    CultureInfo.InvariantCulture);
                break;
            case "--out":
            case "-o":
                options.OutputPath = RequireValue(args, ref i, arg);
                break;
            case "--max-bytes":
                options.MaxBytesPerFrame = int.Parse(
                    RequireValue(args, ref i, arg),
                    CultureInfo.InvariantCulture);
                break;
            case "--progress-every":
                options.ProgressEvery = int.Parse(
                    RequireValue(args, ref i, arg),
                    CultureInfo.InvariantCulture);
                break;
            case "--help":
            case "-h":
                PrintUsageAndExit();
                break;
            default:
                throw new ArgumentException($"Unknown argument: {arg}");
            }
        }

        if (options.Endpoints.Count == 0) {
            options.Endpoints.Add("tcp://127.0.0.1:52237");
            options.Endpoints.Add("tcp://127.0.0.1:52239");
        }
        if (options.Seconds <= 0) {
            throw new ArgumentOutOfRangeException(
                nameof(options.Seconds),
                "Seconds must be positive.");
        }
        if (options.MaxBytesPerFrame <= 0) {
            throw new ArgumentOutOfRangeException(
                nameof(options.MaxBytesPerFrame),
                "Max bytes must be positive.");
        }
        if (options.ProgressEvery < 0) {
            throw new ArgumentOutOfRangeException(
                nameof(options.ProgressEvery),
                "Progress interval must be zero or positive.");
        }

        options.OutputPath = Path.GetFullPath(options.OutputPath);
        return options;
    }

    private static string RequireValue(string[] args, ref int index, string name)
    {
        if (index + 1 >= args.Length) {
            throw new ArgumentException($"{name} requires a value.");
        }
        ++index;
        return args[index];
    }

    private static void PrintUsageAndExit()
    {
        Console.WriteLine(
            """
            Usage:
              RealityRunnerSniffer [options]

            Options:
              -e, --endpoint <zmq-endpoint>  Endpoint to connect. May repeat.
                                            Default: tcp://127.0.0.1:52237 and :52239
              -s, --seconds <seconds>        Capture duration. Default: 60
              -o, --out <csv-path>           Output CSV path.
              --max-bytes <count>            Max text/hex bytes per frame. Default: 4096
              --progress-every <count>        Console update interval. 0 disables. Default: 100
              -h, --help                     Show this help.
            """);
        Environment.Exit(0);
    }
}
