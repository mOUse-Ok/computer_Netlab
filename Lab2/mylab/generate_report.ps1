$csv = Import-Csv -Path 'test_results\results.csv'

# Table 1: different ssthresh and window with loss=5
$table1 = $csv | Where-Object { $_.loss -eq '5' } | Sort-Object {[int]$_.ssthresh}, {[int]$_.window}

# Table 2: loss rates for ssthresh=16 window=16
$table2 = $csv | Where-Object { $_.ssthresh -eq '16' -and $_.window -eq '16' } | Sort-Object {[int]$_.loss}

$md = @()
$md += "# Transmission Performance Report"
$md += ""
$md += "Test file: 1.jpg"
$md += ""
$md += "## Table 1: Effect of ssthresh and window size (loss=5%)"
$md += "| ssthresh | window | transmission_time_s | avg_throughput_KB_s |"
$md += "|----------|:------:|:------------------:|:------------------:|"
foreach ($r in $table1) {
    $md += ("| {0} | {1} | {2} | {3} |" -f $r.ssthresh, $r.window, $r.transmission_time_s, $r.throughput_kb_s)
}
$md += ""
$md += "## Table 2: Effect of loss rate (ssthresh=16, window=16)"
$md += "| loss(%) | transmission_time_s | avg_throughput_KB_s |"
$md += "|:-------:|:------------------:|:------------------:|"
foreach ($r in $table2) {
    $md += ("| {0} | {1} | {2} |" -f $r.loss, $r.transmission_time_s, $r.throughput_kb_s)
}

$mdPath = 'test_results\performance_report.md'
$txtPath = 'test_results\performance_report.txt'
$md -join "`n" | Out-File -FilePath $mdPath -Encoding utf8
$md -join "`n" | Out-File -FilePath $txtPath -Encoding utf8

Write-Host "Reports generated: $mdPath and $txtPath"