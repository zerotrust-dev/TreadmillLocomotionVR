param(
    [string]$PortName = "COM4",
    [int]$BaudRate = 115200,
    [int]$Seconds = 60,
    [int]$PollMs = 100,
    [string]$Out = "",
    [switch]$AllowRealityRunnerRunning
)

$ErrorActionPreference = "Stop"

if ($Seconds -le 0) {
    throw "Seconds must be positive."
}
if ($PollMs -le 0) {
    throw "PollMs must be positive."
}

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
if ([string]::IsNullOrWhiteSpace($Out)) {
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $Out = Join-Path $repoRoot "captures\rr-api-$PortName-$stamp.csv"
}
$Out = [System.IO.Path]::GetFullPath($Out)
[System.IO.Directory]::CreateDirectory((Split-Path -Parent $Out)) | Out-Null

$running = Get-Process -Name "RealityRunner" -ErrorAction SilentlyContinue
if ($running -and -not $AllowRealityRunnerRunning) {
    $ids = ($running | Select-Object -ExpandProperty Id) -join ", "
    throw "RealityRunner is running (PID $ids). Close the RealityRunner desktop app before opening $PortName."
}

function Escape-Csv([string]$Value) {
    if ($null -eq $Value) {
        return ""
    }
    if ($Value.Contains('"') -or $Value.Contains(',') -or $Value.Contains("`r") -or $Value.Contains("`n")) {
        return '"' + $Value.Replace('"', '""') + '"'
    }
    return $Value
}

function Read-Exact([System.IO.Ports.SerialPort]$Port, [int]$Count, [datetime]$Deadline) {
    $buffer = New-Object byte[] $Count
    $offset = 0
    while ($offset -lt $Count) {
        if ([datetime]::UtcNow -ge $Deadline) {
            throw "Timed out reading $Count byte(s)."
        }
        try {
            $read = $Port.Read($buffer, $offset, $Count - $offset)
        } catch [System.TimeoutException] {
            continue
        }
        if ($read -gt 0) {
            $offset += $read
        }
    }
    return $buffer
}

function Read-RrFrame([System.IO.Ports.SerialPort]$Port, [int]$TimeoutMs = 2000) {
    $deadline = [datetime]::UtcNow.AddMilliseconds($TimeoutMs)
    $headerBytes = Read-Exact $Port 4 $deadline
    $header = [System.Text.Encoding]::ASCII.GetString($headerBytes)
    if ($header[3] -ne ':') {
        $Port.DiscardInBuffer()
        throw "Invalid RealityRunner frame header '$header'."
    }

    $lengthText = $header.Substring(0, 3)
    $length = 0
    if (-not [int]::TryParse($lengthText, [ref]$length)) {
        $Port.DiscardInBuffer()
        throw "Invalid RealityRunner frame length '$lengthText'."
    }

    $payloadBytes = Read-Exact $Port $length $deadline
    $newline = Read-Exact $Port 1 $deadline
    if ($newline[0] -ne 10) {
        $Port.DiscardInBuffer()
        throw "RealityRunner frame missing newline terminator."
    }
    return [System.Text.Encoding]::UTF8.GetString($payloadBytes)
}

function Send-RrCommand([System.IO.Ports.SerialPort]$Port, [string]$Command) {
    $Port.DiscardInBuffer()
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($Command)
    $Port.Write($bytes, 0, $bytes.Length)
    return Read-RrFrame $Port
}

function Parse-Joystick([string]$Payload) {
    $parts = $Payload.Trim().Split(',')
    if ($parts.Length -lt 2) {
        throw "Joystick response did not contain two fields: '$Payload'"
    }
    return [pscustomobject]@{
        JoystickValue = [int]$parts[0]
        SprintActive = ([int]$parts[1]) -ne 0
    }
}

$port = [System.IO.Ports.SerialPort]::new($PortName, $BaudRate, [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One)
$port.ReadTimeout = 100
$port.WriteTimeout = 1000

$writer = $null
try {
    Write-Host "RealityRunner API logger"
    Write-Host "  Port:    $PortName"
    Write-Host "  Baud:    $BaudRate"
    Write-Host "  Seconds: $Seconds"
    Write-Host "  Poll:    $PollMs ms"
    Write-Host "  Output:  $Out"
    Write-Host ""

    $port.Open()
    Start-Sleep -Milliseconds 250
    $port.DiscardInBuffer()

    $curve = Send-RrCommand $port "GET curve`n"
    $profile = Send-RrCommand $port "GET profiles`n"
    $bootMode = Send-RrCommand $port "GET bootmode`n"

    Write-Host "Curve:    $curve"
    Write-Host "Profile:  $profile"
    Write-Host "BootMode: $bootMode"
    Write-Host ""
    Write-Host "Polling joystick stream. Move the treadmill now."

    $writer = [System.IO.StreamWriter]::new($Out, $false, [System.Text.Encoding]::UTF8)
    $writer.WriteLine("sample_index,time_utc,elapsed_ms,joystick_value,sprint_active,raw_payload")

    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    $deadline = [datetime]::UtcNow.AddSeconds($Seconds)
    $sampleIndex = 0

    while ([datetime]::UtcNow -lt $deadline) {
        $payload = Send-RrCommand $port "SET stream true,WIRED`n"
        $data = Parse-Joystick $payload
        ++$sampleIndex

        $now = [datetime]::UtcNow.ToString("O", [System.Globalization.CultureInfo]::InvariantCulture)
        $elapsed = $stopwatch.ElapsedMilliseconds.ToString([System.Globalization.CultureInfo]::InvariantCulture)
        $sprint = if ($data.SprintActive) { "true" } else { "false" }
        $writer.WriteLine("$sampleIndex,$now,$elapsed,$($data.JoystickValue),$sprint,$(Escape-Csv $payload)")
        $writer.Flush()

        if (($sampleIndex % 10) -eq 0) {
            Write-Host "#$sampleIndex $($stopwatch.ElapsedMilliseconds) ms joystick=$($data.JoystickValue) sprint=$sprint"
        }
        Start-Sleep -Milliseconds $PollMs
    }

    Send-RrCommand $port "SET stream false,WIRED`n" | Out-Null
    Write-Host ""
    Write-Host "Done. Captured $sampleIndex sample(s)."
} finally {
    if ($writer) {
        $writer.Dispose()
    }
    if ($port.IsOpen) {
        $port.Close()
    }
    $port.Dispose()
}
