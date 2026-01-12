This is the repo for the paper "One Fell Swoop: A Single-Trace Key-Recovery Attack on the Falcon Signing Algorithm". (Link:https://eprint.iacr.org/2025/2159.pdf)

## Overview

This repository contains implementations of the single-trace key-recovery attack under -O0 and the multi-trace attack under -O3 on Falcon.
Each implementation includes scripts for power trace collection, preprocessing, and complete key recovery.


## Project Structure

```
fpr_attack/
├── README.md                             # Project documentation
├── sca_preprocess.py                     # Preprocessing toolkit for side-channel attack scripts
│
├── PQClean/                              # Submodule: PQClean cryptographic library
│   └── crypto_sign/falcon-*/             # Falcon signature implementation (provides fpr.c)
│
├── cw-firmware-mcu/                      # Submodule: ChipWhisperer firmware utilities (pinned at 7f5879e)
│
├── O0_level/                             # Attack implementation for -O0 optimization level
│   ├── collect_traces/                   # Scripts for collecting power traces
│   ├── data/                             # The file for storing the collected power trace data
│   ├── data_k/                           # Generated key data
│   ├── attack_falcon512(1024)_O0.ipynb   # Attack script for Falcon-512 and Falcon-1024 (-O0)
│   ├── Solve_equation_Falcon512(1024).py # Script for recovering remaining key coefficients
│   └── Template_multiplication.py        # Template construction for mantissa multiplication
│
└── O3_level/                             # Attack implementation for -O3 optimization level
    ├── collect_traces/                   # Scripts for collecting power traces
    ├── data/                             # The file for storing collected power trace data (-O3)
    ├── data_k/                           # Generated key data
    ├── e2e/                              # Efficient scripts for recovering remaining key coefficients
    ├── attack_O3_ISD_Falcon512(1024).ipynb # Attack script for Falcon-512 and Falcon-1024 (-O3)
    ├── TA_discriminative.py              # Deep learning-based discriminative attack utilities
```

## Package Dependencies

### Python Packages

Required for data analysis and attack implementation:

- chipwhisperer, numpy, scipy, matplotlib, scikit-learn, pandas, tqdm
- torch, torchinfo (required for deep learning-based attacks in O3_level)

### C/Embedded Development

Required for firmware compilation and deployment:

- ARM GCC toolchain for STM32F4 microcontrollers
- CMSIS_6 and STM32F3/F4 device libraries
- ChipWhisperer firmware libraries

### Submodules

- [PQClean](https://github.com/PQClean/PQClean.git): Post-quantum cryptographic implementations. This project uses the Falcon signature scheme implementation (provides `fpr.c` and related floating-point arithmetic functions).

- [cw-firmware-mcu](https://github.com/OChicken/cw-firmware-mcu.git): ChipWhisperer firmware utilities providing convenient interfaces for data collection and analysis.

**Note:** This project uses cw-firmware-mcu pinned at commit `7f5879e` (not the latest version), which was the stable version used during the development of this attack.

To initialize the submodules:
``` shell
git submodule update --init
```


## Note

Our attack targets the multiplication operations in the first layer of FFT. To facilitate trace collection, we modified the FFT implementation to execute only this first layer. An example of this modification can be found at line 173 in [O3_level/collect_traces/fpr_smallint_and_FFT_f_one_layer.c](O3_level/collect_traces/fpr_smallint_and_FFT_f_one_layer.c).

Due to equipment limitations, we collected the power traces of the three operations of interest (FP conversion, normalization procedure, and mantissa multiplication) separately during the attack phase.  Actually, we can collect a single long trace and then divide it into three segments of interest if we use more powerful equipment.
