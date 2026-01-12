#!/bin/bash

# hal/CMSIS/core/
SRC=$HOME/arm/CMSIS_6/CMSIS/Core/Include/
DEST=hal/CMSIS/core/
cp -p $SRC/*.h $DEST
cp -rp $SRC/m-profile/ $DEST

# hal/CMSIS/device/
SRC=$HOME/arm/cmsis-device-f4/Include
DEST=hal/CMSIS/device/stm32f4/
cp -p $SRC/stm32f405xx.h $DEST
cp -p $SRC/stm32f415xx.h $DEST
cp -p $SRC/stm32f4xx.h $DEST
cp -p $SRC/system_stm32f4xx.h $DEST

# hal/driver/stm32f4/Inc/
# SRC=$HOME/arm/stm32f4xx-hal-driver/Inc/
SRC=$HOME/sca/chipwhisperer-fw-extra/stm32f4/
DEST=hal/driver/stm32f4/Inc/
# cp -p $SRC/Legacy/stm32_hal_legacy.h $DEST/Legacy
# cp -p $SRC/stm32f4xx_hal.h $DEST
cp -p $SRC/stm32f4xx_hal_cryp.h $DEST
cp -p $SRC/stm32f4xx_hal_cryp_ex.h $DEST
cp -p $SRC/stm32f4xx_hal_def.h $DEST
cp -p $SRC/stm32f4xx_hal_dma.h $DEST
cp -p $SRC/stm32f4xx_hal_dma_ex.h $DEST
cp -p $SRC/stm32f4xx_hal_flash.h $DEST
cp -p $SRC/stm32f4xx_hal_flash_ex.h $DEST
cp -p $SRC/stm32f4xx_hal_flash_ramfunc.h $DEST
cp -p $SRC/stm32f4xx_hal_gpio.h $DEST
cp -p $SRC/stm32f4xx_hal_gpio_ex.h $DEST
cp -p $SRC/stm32f4xx_hal_rcc.h $DEST
cp -p $SRC/stm32f4xx_hal_rcc_ex.h $DEST
cp -p $SRC/stm32f4xx_hal_rng.h $DEST
cp -p $SRC/stm32f4xx_hal_uart.h $DEST
cp -p $SRC/stm32f4xx_hal_usart.h $DEST

# hal/driver/stm32f4/Src/
# SRC=$HOME/arm/stm32f4xx-hal-driver/Src/
SRC=$HOME/sca/chipwhisperer-fw-extra/stm32f4/
DEST=hal/driver/stm32f4/Src/
cp -p $SRC/stm32f4xx_hal_cryp.c $DEST
cp -p $SRC/stm32f4xx_hal_rng.c $DEST
