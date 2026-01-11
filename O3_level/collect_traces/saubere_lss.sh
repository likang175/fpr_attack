#!/usr/bin/env bash
# saubere_lss.sh
# Usage: saubere_lss.sh input.lss output.lss unwanted-list.txt
# Strips the bodies of all functions listed (one per line) in unwanted-list.txt but leaves calls intact.

if [ "$#" -ne 2 ]; then
  echo "Usage: $0 input.lss output.lss"
  exit 1
fi

input="$1"
output="$2"

unwanted_list=(
    strlen
    memchr
    memcpy
    __aeabi_drsub
    __aeabi_dsub
    __adddf3
    __aeabi_ui2d
    __aeabi_i2d
    __aeabi_f2d
    __aeabi_ul2d
    __aeabi_l2d
    __aeabi_dmul
    __aeabi_ddiv
    __gedf2
    __ledf2
    __cmpdf2
    __aeabi_cdrcmple
    __aeabi_cdcmpeq
    __aeabi_dcmpeq
    __aeabi_dcmplt
    __aeabi_dcmple
    __aeabi_dcmpge
    __aeabi_dcmpgt
    __aeabi_dcmpun
    __aeabi_d2iz
    __aeabi_uldivmod
    __aeabi_idiv0
    __do_global_dtors_aux
    frame_dummy
    PQCLEAN_FALCON512_CLEAN_fpr_scaled
    PQCLEAN_FALCON512_CLEAN_fpr_add
    PQCLEAN_FALCON512_CLEAN_fpr_mul
    hex_decode
    check_version
    ss_num_commands
    ss_get_commands
    simpleserial_init
    simpleserial_addcmd
    simpleserial_addcmd_flags
    simpleserial_get
    simpleserial_put
    setreg
    getreg
    enable_trace
    set_pcsample_params
    trigger_high_pcsamp
    trigger_low_pcsamp
    info
    hal_send_str
    __wrap__sbrk
    platform_init
    init_uart
    trigger_setup
    trigger_high
    trigger_low
    getch
    putch
    HAL_GetTick
    HAL_RNG_Init
    HAL_RNG_MspInit
    HAL_GPIO_Init
    HAL_GPIO_WritePin
    HAL_RCC_ClockConfig
    HAL_RCC_GetPCLK1Freq
    HAL_RCC_GetPCLK2Freq
    HAL_RCC_OscConfig
    UART_SetConfig
    HAL_UART_Init
    UART_WaitOnFlagForever
    HAL_UART_Transmit
    HAL_UART_Receive
    Reset_Handler
    CopyDataInit
    LoopCopyDataInit
    FillZerobss
    LoopFillZerobss
    LoopForever
    BusFault_Handler
    atexit
    atoll
    stdio_exit_handler
    cleanup_stdio
    global_stdio_init.part.0
    __sinit
    __sfp_lock_acquire
    __sfp_lock_release
    _fwalk_sglue
    sprintf
    __sread
    __swrite
    __sseek
    __sclose
    memset
    _close_r
    _lseek_r
    _read_r
    _write_r
    __errno
    __libc_init_array
    __libc_fini_array
    __retarget_lock_acquire_recursive
    __retarget_lock_release_recursive
    __register_exitproc
    register_fini
    _malloc_trim_r
    _free_r
    _malloc_r
    __malloc_lock
    __malloc_unlock
    _strtoll_l.isra.0
    strtoll
    _svfprintf_r
    __ssprint_r
    _fclose_r
    __sflush_r
    _fflush_r
    strncpy
    _localeconv_r
    _sbrk_r
    sysconf
    frexp
    quorem
    _dtoa_r
    _Balloc
    _Bfree
    __multadd
    __hi0bits
    __lo0bits
    __i2b
    __multiply
    __pow5mult
    __lshift
    __mcmp
    __mdiff
    __d2b
    __ssputs_r
    memmove
    __assert_func
    _calloc_r
    __ascii_mbtowc
    _realloc_r
    __ascii_wctomb
    fiprintf
    _vfiprintf_r
    __sbprintf
    __sprint_r
    __sfvwrite_r
    __swsetup_r
    abort
    __smakebuf_r
    raise
    _fstat_r
    _isatty_r
    _kill_r
    _getpid_r
    __udivmoddi4
    _init
    _fini
)

# Join array into a single string
unwanted_str="${unwanted_list[*]}"

# Use awk with the list passed in a variable
awk -v list="$unwanted_str" '
  BEGIN {
    # build lookup table of unwanted names
    split(list, arr, " ")
    for (i in arr) unwanted[arr[i]] = 1
  }
  # if line is a header like "08002748 <foo>:"
  /^[0-9A-Fa-f]{8} <[^>]+>:$/ {
    # extract the name
    if (match($0, /^[0-9A-Fa-f]{8} <([^>]+)>:$/, m)) {
      name = m[1]
    } else {
      name = ""
    }
    if (name in unwanted) {
      in_skip = 1
      print           # print the header
      print ""        # blank line
      next
    }
    # otherwise turn off skip-mode and print it
    in_skip = 0
    print
    next
  }
  # for all other lines, print only if not in_skip
  !in_skip
' "$input" > "$output"
