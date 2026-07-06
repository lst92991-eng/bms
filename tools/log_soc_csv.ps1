param(
    [string]$Port = "COM10",
    [int]$Baud = 115200,
    [int]$DurationSec = 0,
    [string]$OutDir = "logs",
    [string]$Prefix = "bms_soc"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $OutDir)) {
    New-Item -ItemType Directory -Path $OutDir | Out-Null
}

$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$csvPath = Join-Path $OutDir "$Prefix`_$stamp.csv"
$rawPath = Join-Path $OutDir "$Prefix`_$stamp.raw.log"

$serial = [System.IO.Ports.SerialPort]::new($Port, $Baud, [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One)
$serial.ReadTimeout = 250
$serial.WriteTimeout = 1000
$serial.DtrEnable = $false
$serial.RtsEnable = $false

$buffer = ""
$csvHeaderWritten = $false
$start = Get-Date

function Write-BmsLine {
    param([string]$Line)

    $pcDate = Get-Date -Format "yyyy-MM-dd_HH:mm:ss.fff"

    if ($Line.StartsWith("BMSCSV,MS,")) {
        if (-not $script:csvHeaderWritten) {
            Add-Content -LiteralPath $script:csvPath -Encoding ascii -Value ("PC_DATE," + $Line)
            $script:csvHeaderWritten = $true
        }
        return
    }

    if ($Line.StartsWith("BMSCSV,")) {
        if ($script:csvHeaderWritten) {
            Add-Content -LiteralPath $script:csvPath -Encoding ascii -Value ($pcDate + "," + $Line)
        }
        return
    }

    Add-Content -LiteralPath $script:rawPath -Encoding utf8 -Value ($pcDate + " " + $Line)
}

try {
    $serial.Open()
    Start-Sleep -Milliseconds 300
    $serial.DiscardInBuffer()
    # 先送一个换行清掉固件 CLI 里可能残留的半行输入，再重新开启 CSV。
    $serial.Write("`r`n")
    Start-Sleep -Milliseconds 100
    $serial.Write("csv off`r`n")
    Start-Sleep -Milliseconds 100
    $serial.Write("csv`r`n")
    Write-Host "SOC CSV logging started."
    Write-Host "CSV: $csvPath"
    Write-Host "RAW: $rawPath"
    Write-Host "Press Ctrl+C to stop."

    while ($true) {
        if (($DurationSec -gt 0) -and (((Get-Date) - $start).TotalSeconds -ge $DurationSec)) {
            break
        }

        try {
            $chunk = $serial.ReadExisting()
            if ($chunk.Length -gt 0) {
                $buffer += $chunk
                while ($true) {
                    $idxR = $buffer.IndexOf("`r")
                    $idxN = $buffer.IndexOf("`n")
                    $indexes = @()
                    if ($idxR -ge 0) { $indexes += $idxR }
                    if ($idxN -ge 0) { $indexes += $idxN }
                    if ($indexes.Count -eq 0) { break }

                    $idx = ($indexes | Measure-Object -Minimum).Minimum
                    $line = $buffer.Substring(0, $idx).Trim()
                    $buffer = $buffer.Substring($idx + 1)
                    if ($line.Length -gt 0) {
                        Write-BmsLine -Line $line
                    }
                }
            }
        } catch [System.TimeoutException] {
        }

        Start-Sleep -Milliseconds 50
    }
}
finally {
    if ($serial.IsOpen) {
        try {
            $serial.Write("csv off`r`n")
            Start-Sleep -Milliseconds 100
        } catch {
        }
        $serial.Close()
    }
    Write-Host "SOC CSV logging stopped."
    Write-Host "CSV: $csvPath"
    Write-Host "RAW: $rawPath"
}
