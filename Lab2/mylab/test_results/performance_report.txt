# Transmission Performance Report

Test file: 1.jpg

## Table 1: Effect of ssthresh and window size (loss=5%)
| ssthresh | window | transmission_time_s | avg_throughput_KB_s |
|----------|:------:|:------------------:|:------------------:|
| 8 | 8 | 7.01 | 258.748 |
| 8 | 16 | 3.527 | 514.267 |
| 8 | 32 | 6.035 | 300.55 |
| 16 | 8 | 5.051 | 359.101 |
| 16 | 16 | 4.834 | 375.222 |
| 16 | 32 | 6.145 | 295.17 |
| 32 | 8 | 5.62 | 322.744 |
| 32 | 16 | 5.686 | 318.998 |
| 32 | 32 | 4.349 | 417.066 |

## Table 2: Effect of loss rate (ssthresh=16, window=16)
| loss(%) | transmission_time_s | avg_throughput_KB_s |
|:-------:|:------------------:|:------------------:|
| 0 | 3.528 | 514.122 |
| 5 | 4.834 | 375.222 |
| 10 | 11.667 | 155.466 |
