param(
    [int]$id,
    [int]$ssthresh,
    [int]$window,
    [int]$loss
)

Set-Location -Path (Split-Path -Parent $MyInvocation.MyCommand.Definition)

Write-Host ("Running single test {0}: ssthresh={1} window={2} loss={3}%" -f $id,$ssthresh,$window,$loss)
# generate config
& .\test_config_generator.exe $ssthresh $window $loss | Out-Null
# compile
g++ -o server.exe server.cpp -lws2_32 -std=c++11 2>$null
g++ -o client.exe client.cpp -lws2_32 -std=c++11 2>$null

if (-not (Test-Path '.\server.exe') -or -not (Test-Path '.\client.exe')) {
    Write-Host "Compilation failed for test $id" -ForegroundColor Yellow
    exit 1
}

# prepare server input
$serverFilename = "test_${id}_result.jpg"
$serverFilename | Out-File -FilePath server_input.txt -Encoding ascii
if (Test-Path 'server.txt') { Remove-Item 'server.txt' -Force }

# start server
$arg = "/c start /B server.exe < server_input.txt > server_run_${id}.log 2>&1"
Start-Process -FilePath cmd.exe -ArgumentList $arg -NoNewWindow

# wait for server to be ready
$wait=0
while ($wait -lt 30) {
    Start-Sleep -Milliseconds 200
    $p = Get-Process -Name server -ErrorAction SilentlyContinue
    if ($p) { break }
    $wait++
}
Start-Sleep -Milliseconds 300

# run client
$clientCmd = ('cmd /c "echo 1.jpg | client.exe > client_run_{0}.log 2>&1"' -f $id)
Invoke-Expression $clientCmd

# wait server finish
$timeout = 120
$elapsed = 0
while ($elapsed -lt $timeout) {
    Start-Sleep -Seconds 1
    $p = Get-Process -Name server -ErrorAction SilentlyContinue
    if (-not $p) { break }
    $elapsed++
}

# parse server.txt
$trans='NA'; $tp='NA'
if (Test-Path 'server.txt') {
    $lines = Get-Content 'server.txt'
    foreach ($l in $lines) {
        if ($l -match 'Transmission Time: *([0-9\.]+)') { $trans = $matches[1] }
        if ($l -match 'Average Throughput: *([0-9\.]+)') { $tp = $matches[1] }
    }
}

# append to CSV
$csv = 'test_results\results.csv'
"$id,$ssthresh,$window,$loss,$trans,$tp" | Out-File -FilePath $csv -Append -Encoding utf8
Write-Host "Test $id result: time=$trans s, throughput=$tp KB/s"
