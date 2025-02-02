# fast.c
Prepare for DeekSeek R1 inference: Benchmark CPU, DRAM, SSD, iGPU, GPU, ... with efficient code.

Current optimization target:

CPU = AMD 7700 (Zen 4)

DRAM = DDR5-6000 dual channel

SSD = 4 x PCIE5.0x4

Current results:

NVME raw speed = 53+ GB/s

DRAM processing = 60.8 GB/s

SSD processing = 29.4 GB/s (huge room for improvements, such as ovelapping io & compute, raw fs)

GEMV int8/32 = 75 ~ 147 GB/s (?!)
