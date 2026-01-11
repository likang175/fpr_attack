This is the repo for the paper One Fell Swoop: A Single-Trace Key-Recovery Attack on the Falcon Signing Algorithm. (Link:https://eprint.iacr.org/2025/2159.pdf)

## Overview
There are two code files corresponding to the -O0 and -O3 compilation levels, respectively. Each code file follows a similar directory structure and includes scripts for power trace collection, preprocessing, and key-recovery attacks on Falcon-512 and Falcon-1024.

For example, in the O0_level directory:

- `collect_traces/`    stores the scripts for collecting power traces.

- `data/`    contains the collected power trace datasets.

- `data_k/`    stores the generated secret key data.

  

## Folder Structure
```
main_folder/
├── O0_level/
│   ├── collect_traces/                   # Scripts for collecting power traces
│   ├── data/                             # Collected power trace data
│   ├── data_k/                           # Generated key data
│   ├── attack_falcon512(1024)_O0.ipynb   # Attack script for Falcon-512 and Falcon-1024 (-O0)
│   ├── Solve_equation_Falcon512(1024).py # Script for recovering remaining key coefficients
│   ├── sca_preprocess.py                 # Preprocessing toolkit for side-channel attack scripts
│   └── Template_multiplication.py         # Template construction for mantissa multiplication
│
└── O3_level/
    ├── collect_traces/                   # Scripts for collecting power traces
    ├── data-O3-828/                      # Collected power trace data (-O3)
    ├── data_k/                           # Generated key data
    ├── attack_O3_ISD_Falcon512(1024).ipynb # Attack script for Falcon-512 and Falcon-1024 (-O3)
    ├── Solve_equation_Falcon512(1024).py # Script for recovering remaining key coefficients
    ├── sca_preprocess.py                 # Preprocessing toolkit for side-channel attack scripts
    └── Template_multiplication.py         # Template construction for mantissa multiplication
```



## Note

When collecting attack traces, since we only care about the multiplication operations in the first layer of FFT, we made a modification to the FFT code so that FFT only executes the first layer.

Due to equipment limitations, we collected the power traces of the three operations of interest (FP conversion, normalization procedure, and mantissa multiplication) separately during the attack phase.  Actually, we can collect a single long trace and then divide it into three segments of interest if we use more powerful equipments (such as an oscilloscope).


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

### Submodule

- [cw-firmware-mcu](https://github.com/OChicken/cw-firmware-mcu.git): ChipWhisperer firmware utilities providing convenient interfaces for data collection and analysis.

**Note:** This project uses cw-firmware-mcu pinned at commit `7f5879e` (not the latest version), which was the stable version used during the development of this attack.

To initialize the submodule:
``` shell
git submodule update --init
```
