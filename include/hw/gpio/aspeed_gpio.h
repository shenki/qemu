/*
 *  ASPEED GPIO Controller
 *
 *  Copyright (C) 2017-2018 IBM Corp.
 *
 * This code is licensed under the GPL version 2 or later.  See
 * the COPYING file in the top-level directory.
 */

#ifndef ASPEED_GPIO_H
#define ASPEED_GPIO_H

#include "hw/sysbus.h"

#define TYPE_ASPEED_GPIO "aspeed.gpio"
#define ASPEED_GPIO(obj) OBJECT_CHECK(AspeedGPIOState, (obj), TYPE_ASPEED_GPIO)
#define ASPEED_GPIO_CLASS(klass) \
     OBJECT_CLASS_CHECK(AspeedGPIOClass, (klass), TYPE_ASPEED_GPIO)
#define ASPEED_GPIO_GET_CLASS(obj) \
     OBJECT_GET_CLASS(AspeedGPIOClass, (obj), TYPE_ASPEED_GPIO)

#define ASPEED_GPIO_MAX_NR_SETS 8
#define ASPEED_REGS_PER_BANK 14
#define ASPEED_GPIO_MAX_NR_REGS (ASPEED_REGS_PER_BANK * ASPEED_GPIO_MAX_NR_SETS)
#define ASPEED_GPIO_NR_PINS 228

typedef struct GPIORegs GPIORegs;
typedef const struct GPIOSetProperties {
    uint32_t input;
    uint32_t output;
    char set[4][3];
} GPIOSetProperties;

typedef const struct AspeedGPIOController {
    const char *name;
    GPIOSetProperties *props;
    uint32_t nr_gpio_pins;
    uint32_t nr_gpio_sets;
    uint32_t gap;
} AspeedGPIOController;

typedef struct  AspeedGPIOClass {
    SysBusDevice parent_obj;
    AspeedGPIOController *ctrl;
}  AspeedGPIOClass;

typedef struct AspeedGPIOState {
    /* <private> */
    SysBusDevice parent;

    /*< public >*/
    MemoryRegion iomem;
    qemu_irq output[ASPEED_GPIO_NR_PINS];
    qemu_irq irq;
    AspeedGPIOController *ctrl;

/* Parallel GPIO Registers */
    struct GPIORegs {
        uint32_t data_value; /* Reflects pin values */
        uint32_t data_read; /* Contains last value written to data value */
        uint32_t direction;
        uint32_t int_enable;
        uint32_t int_sens_0;
        uint32_t int_sens_1;
        uint32_t int_sens_2;
        uint32_t int_status;
        uint32_t reset_tol;
        uint32_t cmd_source_0;
        uint32_t cmd_source_1;
        uint32_t debounce_1;
        uint32_t debounce_2;
        uint32_t input_mask;
    } sets[ASPEED_GPIO_MAX_NR_SETS];
} AspeedGPIOState;

#endif /* _ASPEED_GPIO_H_ */
