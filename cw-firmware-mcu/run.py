#!/usr/bin/python

"""
Usage:
    python run.py <filename> [program]
where [program] if is some char (no matter what), would write to target;
                if it's not given, it will read from target.
"""

import os
import sys
import argparse
import re
from datetime import datetime
from setup_generic import hardware_setup, scope_reset

def str_to_bool(value):
    if value.lower() in ('true', '1'):
        return True
    elif value.lower() in ('false', '0'):
        return False
    else:
        raise argparse.ArgumentTypeError(
            f"Invalid boolean value: {value}. Use 'True' or 'False'.")

def parse_arg():
    parser = argparse.ArgumentParser(description="cw-firmware-mcu run")
    parser.add_argument(
        'filename', type=str, help='The hex file to be programmed.')
    parser.add_argument(
        '-p', '--program',
        type=str_to_bool, default=True, help='Program flag (default: True)')
    parser.add_argument(
        '-s', '--ss_ver',
        type=str, default='SS_VER_1_1', help='SS version to be set')
    args = parser.parse_args()
    filename = args.filename
    platform = re.match(r'.*build-(.*?)/.*\.hex', filename).group(1)
    program = args.program
    ss_ver = args.ss_ver

    if ss_ver == '1.1':
        ss_ver = "SS_VER_1_1"
    elif ss_ver == '2.1':
        ss_ver = "SS_VER_2_1"
    else:
        print(f"Invalid ss_ver value: {ss_ver}. Only 1.1 and 2.1 are accepted.")
        sys.exit(1)

    if not os.path.exists(filename):
        print(f"File '{filename}' does not exist. Exiting.")
        sys.exit(1)

    return filename, platform, program, ss_ver

if __name__ == '__main__':
    filename, platform, program, ss_ver = parse_arg()
    scope, target = hardware_setup(filename, platform, ss_ver, program)
    print(datetime.now().strftime("%Y-%m-%d %H:%M:%S"))

    target.flush()
    scope_reset(scope, platform)
    data = ''
    i = 0
    while '' not in data:
        data += target.read()
    while '' not in data:
        i+=1
        data += target.read()
    print(data)
    print("How many times to call target.read()?:", i)

    scope.dis()
    target.dis()
