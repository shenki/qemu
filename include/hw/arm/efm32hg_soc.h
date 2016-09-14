/*
 * Silabs EFM32HG SoC
 *
 * Copyright 2016 Joel Stanley <joel@jms.id.au>
 *
 * This code is licensed under the GPL version 2 or later.  See
 * the COPYING file in the top-level directory.
 */

#ifndef HW_ARM_EFM32HG_SOC_H
#define HW_ARM_EFM32HG_SOC_H

#include "hw/timer/efm32hg_timer.h"
#include "hw/char/efm32hg_leuart.h"
#include "hw/misc/efm32hg_cmu.h"

#define TYPE_EFM32HG_SOC "efm32hg-soc"
#define EFM32HG_SOC(obj) \
    OBJECT_CHECK(EFM32HGState, (obj), TYPE_EFM32HG_SOC)

#define EFM32HG_NUM_USARTS 2
#define EFM32HG_NUM_TIMERS 3

#define FLASH_BASE		0x00000000
#define FLASH_SIZE		(64 * 1024)

#define SRAM_BASE		0x20000000
#define SRAM_BASE_ALIAS		0x10000000
#define SRAM_SIZE		(8 * 1024)

#define CMU_BASE		0x400c8000

#define WDOG_BASE		0x40088000
#define PCNT0_BASE		0x40086000
#define LEUART0_BASE		0x40084000
#define RTC_BASE		0x40080000

#define USART0_BASE		0x4000C000
#define USART1_BASE		0x4000C400

#define TIMER0_BASE		0x40010000
#define TIMER1_BASE		0x40010400
#define TIMER2_BASE		0x40010800

typedef struct EFM32HGState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    char *kernel_filename;

    MemoryRegion iomem;
    EFM32HGLeuartState leuart;
    EFM32HGCmuState cmu;

} EFM32HGState;

#endif
