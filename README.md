This is the repo for the paper One Fell Swoop: A Single-Trace Key-Recovery Attack on the Falcon Signing Algorithm. (Link:https://eprint.iacr.org/2025/2159.pdf)

#Overview
There are two code files corresponding to the -O0 and -O3 compilation levels, respectively. 
Each code file follows a similar directory structure and includes scripts for power trace collection, preprocessing, and key-recovery attacks on Falcon-512 and Falcon-1024.
For example, in the O0_level directory:
collect_traces/ stores the scripts for collecting power traces.
data/ contains the collected power trace datasets.
data_k/ stores the generated secret key data.

#Folder Structure
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

