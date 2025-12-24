# run_auto_tests.ps1
# 自动运行所有测试并收集结果到 test_results\results.csv

$ErrorActionPreference = 'Stop'
Set-Location -Path (Split-Path -Parent $MyInvocation.MyCommand.Definition)

# Ensure results directory
if (-not (Test-Path "test_results")) {
    New-Item -ItemType Directory -Path "test_results" | Out-Null
}

# Output CSV header
$csvPath = "test_results\results.csv"
"test_id,ssthresh,window,loss,transmission_time_s,throughput_kb_s" | Out-File -FilePath $csvPath -Encoding utf8

$testId = 0

function Run-Test ($ssthresh, $window, $loss) {
    $global:testId++
    $id = $global:testId
    Write-Host ("`n=== Test {0}: ssthresh={1} window={2} loss={3}% ===" -f $id, $ssthresh, $window, $loss)

    # generate config
    & .\test_config_generator.exe $ssthresh $window $loss | Out-Null

    # compile
    g++ -o server.exe server.cpp -lws2_32 -std=c++11 2>$null
    g++ -o client.exe client.cpp -lws2_32 -std=c++11 2>$null

    if (-not (Test-Path '.\server.exe') -or -not (Test-Path '.\client.exe')) {
        Write-Host ("Compilation failed, skipping test {0}" -f $id) -ForegroundColor Yellow
        return
    }

    # prepare server input file (filename to save)
    $serverFilename = ("test_{0}_result.jpg" -f $id)
    $serverFilename | Out-File -FilePath server_input.txt -Encoding ascii

    # remove previous server.txt
    if (Test-Path 'server.txt') { Remove-Item 'server.txt' -Force }

    # start server in background (using cmd start /B)
    $arg = ("/c start /B server.exe < server_input.txt > server_run_{0}.log 2>&1" -f $id)
    Start-Process -FilePath cmd.exe -ArgumentList $arg -NoNewWindow

    # wait until server process appears (with timeout)
    $waitCount = 0
    while ($waitCount -lt 30) {
        Start-Sleep -Milliseconds 200
        $proc = Get-Process -Name server -ErrorAction SilentlyContinue
        if ($proc) { break }
        $waitCount++
    }

    Start-Sleep -Milliseconds 300

    # run client (pipe filename input)
    Write-Host "Starting client..."
    $clientCmd = ('cmd /c "echo 1.jpg | client.exe > client_run_{0}.log 2>&1"' -f $id)
    Invoke-Expression $clientCmd

    # wait for server to finish (timeout ~120s)
    $timeout = 120
    $elapsed = 0
    while ($elapsed -lt $timeout) {
        Start-Sleep -Seconds 1
        $proc = Get-Process -Name server -ErrorAction SilentlyContinue
        if (-not $proc) { break }
        $elapsed++
    }

    # After server finishes, parse server.txt
    Start-Sleep -Milliseconds 200
    $transTime = ''
    $throughput = ''
    if (Test-Path 'server.txt') {
        $lines = Get-Content -Path 'server.txt'
        foreach ($line in $lines) {
            if ($line -match 'Transmission Time: *([0-9\.]+)') {
                $transTime = $matches[1]
            }
            if ($line -match 'Average Throughput: *([0-9\.]+)') {
                $throughput = $matches[1]
            }
        }
    }

    if ($transTime -eq '') { $transTime = 'NA' }
    if ($throughput -eq '') { $throughput = 'NA' }

    "$id,$ssthresh,$window,$loss,$transTime,$throughput" | Out-File -FilePath $csvPath -Append -Encoding utf8

    Write-Host ("Test {0} result: time={1} s, throughput={2} KB/s" -f $id, $transTime, $throughput)
}

# Group1: ssthresh in 8,16,32 & window in 8,16,32 with loss 5%
$ssthreshValues = 8,16,32
$windowValues = 8,16,32
foreach ($ss in $ssthreshValues) {
    foreach ($w in $windowValues) {
        Run-Test $ss $w 5
    }
}

# Group2: ssthresh=16 window=16 loss in 0,5,10
foreach ($l in 0,5,10) {
    Run-Test 16 16 $l
}

Write-Host "\nAll automated tests finished. Results saved to $csvPath"
