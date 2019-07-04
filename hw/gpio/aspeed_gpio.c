/*
 *  ASPEED GPIO Controller
 *
 *  Copyright (C) 2017-2019 IBM Corp.
 *
 * This code is licensed under the GPL version 2 or later.  See
 * the COPYING file in the top-level directory.
 *
 *
 * This models the ast2400 and ast2500 GPIO controllers.
 *
 * GPIO pins are arranged in groups of 8 pins labeled A,B,..,Y,Z,AA,AB,AC.
 * (Note that the ast2400 controller only goes up to group AB).
 * A set has four groups (except set AC which only has one) and is
 * referred to by the groups it is composed of (eg ABCD,EFGH,...,YZAAAB).
 * Each set is accessed and controlled by a bank of 14 registers.
 *
 * These registers operate on a per pin level where each bit in the register
 * corresponds to a pin, except for the command source registers. The command
 * source registers operate on a per group level where bits 24, 16, 8 and 0
 * correspond to each group in the set.
 *
 *  eg. registers for set ABCD:
 *  |D7...D0|C7...C0|B7...B0|A7...A0| <- GPIOs
 *  |31...24|23...16|15....8|7.....0| <- bit position
 *
 * Note that there are a couple of groups that only have 4 pins.

 * There are three ways that this model deviates from the behaviour of the
 * actual controller:
 * (1) There are three debounce registers which aren't modeled and so the per
 * set debounce setting registers don't affect anything.
 *
 * (2) The only control source driving the GPIO pins in the model is the ARM
 * model (as there currently aren't models for the LPC or Coprocessor [TODO]).
 *
 * (3) None of the registers in the model are reset tolerant. [TODO]
 *
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "hw/gpio/aspeed_gpio.h"
#include "include/hw/misc/aspeed_scu.h"
#include "qapi/error.h"
#include "qapi/visitor.h"

#define ASPEED_GPIOS_PER_REG 32
#define GPIO_REG_ARRAY_SIZE 0x3d8/4

/* GPIO Source Types */
#define ASPEED_CMD_SRC_MASK         0x01010101
#define ASPEED_SOURCE_ARM           0
#define ASPEED_SOURCE_LPC           1
#define ASPEED_SOURCE_COPROCESSOR   2
#define ASPEED_SOURCE_RESERVED      3

/* GPIO Interrupt Triggers */
#define ASPEED_FALLING_EDGE 0
#define ASPEED_RISING_EDGE  1
#define ASPEED_LEVEL_LOW    2
#define ASPEED_LEVEL_HIGH   3
#define ASPEED_DUAL_EDGE    4

/* GPIO Register Address Offsets */
#define GPIO_ABCD_DATA_VALUE     0x000 >> 2
#define GPIO_ABCD_DIRECTION      0x004 >> 2
#define GPIO_ABCD_INT_ENABLE     0x008 >> 2
#define GPIO_ABCD_INT_SENS_0     0x00C >> 2
#define GPIO_ABCD_INT_SENS_1     0x010 >> 2
#define GPIO_ABCD_INT_SENS_2     0x014 >> 2
#define GPIO_ABCD_INT_STATUS     0x018 >> 2
#define GPIO_ABCD_RESET_TOLERANT 0x01C >> 2
#define GPIO_EFGH_DATA_VALUE     0x020 >> 2
#define GPIO_EFGH_DIRECTION      0x024 >> 2
#define GPIO_EFGH_INT_ENABLE     0x028 >> 2
#define GPIO_EFGH_INT_SENS_0     0x02C >> 2
#define GPIO_EFGH_INT_SENS_1     0x030 >> 2
#define GPIO_EFGH_INT_SENS_2     0x034 >> 2
#define GPIO_EFGH_INT_STATUS     0x038 >> 2
#define GPIO_EFGH_RESET_TOL      0x03C >> 2
#define GPIO_ABCD_DEBOUNCE_1     0x040 >> 2
#define GPIO_ABCD_DEBOUNCE_2     0x044 >> 2
#define GPIO_EFGH_DEBOUNCE_1     0x048 >> 2
#define GPIO_EFGH_DEBOUNCE_2     0x04C >> 2
#define GPIO_DEBOUNCE_TIME_1     0x050 >> 2
#define GPIO_DEBOUNCE_TIME_2     0x054 >> 2
#define GPIO_DEBOUNCE_TIME_3     0x058 >> 2
#define GPIO_ABCD_COMMAND_SRC_0  0x060 >> 2
#define GPIO_ABCD_COMMAND_SRC_1  0x064 >> 2
#define GPIO_EFGH_COMMAND_SRC_0  0x068 >> 2
#define GPIO_EFGH_COMMAND_SRC_1  0x06C >> 2
#define GPIO_IJKL_DATA_VALUE     0x070 >> 2
#define GPIO_IJKL_DIRECTION      0x074 >> 2
#define GPIO_MNOP_DATA_VALUE     0x078 >> 2
#define GPIO_MNOP_DIRECTION      0x07C >> 2
#define GPIO_QRST_DATA_VALUE     0x080 >> 2
#define GPIO_QRST_DIRECTION      0x084 >> 2
#define GPIO_UVWX_DATA_VALUE     0x088 >> 2
#define GPIO_UWVX_DIRECTION      0x08C >> 2
#define GPIO_IJKL_COMMAND_SRC_0  0x090 >> 2
#define GPIO_IJKL_COMMAND_SRC_1  0x094 >> 2
#define GPIO_IJKL_INT_ENABLE     0x098 >> 2
#define GPIO_IJKL_INT_SENS_0     0x09C >> 2
#define GPIO_IJKL_INT_SENS_1     0x0A0 >> 2
#define GPIO_IJKL_INT_SENS_2     0x0A4 >> 2
#define GPIO_IJKL_INT_STATUS     0x0A8 >> 2
#define GPIO_IJKL_RESET_TOLERANT 0x0AC >> 2
#define GPIO_IJKL_DEBOUNCE_1     0x0B0 >> 2
#define GPIO_IJKL_DEBOUNCE_2     0x0B4 >> 2
#define GPIO_IJKL_INPUT_MASK     0x0B8 >> 2
#define GPIO_ABCD_DATA_READ      0x0C0 >> 2
#define GPIO_EFGH_DATA_READ      0x0C4 >> 2
#define GPIO_IJKL_DATA_READ      0x0C8 >> 2
#define GPIO_MNOP_DATA_READ      0x0CC >> 2
#define GPIO_QRST_DATA_READ      0x0D0 >> 2
#define GPIO_UVWX_DATA_READ      0x0D4 >> 2
#define GPIO_YZAAAB_DATA_READ    0x0D8 >> 2
#define GPIO_AC_DATA_READ        0x0DC >> 2
#define GPIO_MNOP_COMMAND_SRC_0  0x0E0 >> 2
#define GPIO_MNOP_COMMAND_SRC_1  0x0E4 >> 2
#define GPIO_MNOP_INT_ENABLE     0x0E8 >> 2
#define GPIO_MNOP_INT_SENS_0     0x0EC >> 2
#define GPIO_MNOP_INT_SENS_1     0x0F0 >> 2
#define GPIO_MNOP_INT_SENS_2     0x0F4 >> 2
#define GPIO_MNOP_INT_STATUS     0x0F8 >> 2
#define GPIO_MNOP_RESET_TOLERANT 0x0FC >> 2
#define GPIO_MNOP_DEBOUNCE_1     0x100 >> 2
#define GPIO_MNOP_DEBOUNCE_2     0x104 >> 2
#define GPIO_MNOP_INPUT_MASK     0x108 >> 2
#define GPIO_QRST_COMMAND_SRC_0  0x110 >> 2
#define GPIO_QRST_COMMAND_SRC_1  0x114 >> 2
#define GPIO_QRST_INT_ENABLE     0x118 >> 2
#define GPIO_QRST_INT_SENS_0     0x11C >> 2
#define GPIO_QRST_INT_SENS_1     0x120 >> 2
#define GPIO_QRST_INT_SENS_2     0x124 >> 2
#define GPIO_QRST_INT_STATUS     0x128 >> 2
#define GPIO_QRST_RESET_TOLERANT 0x12C >> 2
#define GPIO_QRST_DEBOUNCE_1     0x130 >> 2
#define GPIO_QRST_DEBOUNCE_2     0x134 >> 2
#define GPIO_QRST_INPUT_MASK     0x138 >> 2
#define GPIO_UVWX_COMMAND_SRC_0  0x140 >> 2
#define GPIO_UVWX_COMMAND_SRC_1  0x144 >> 2
#define GPIO_UVWX_INT_ENABLE     0x148 >> 2
#define GPIO_UVWX_INT_SENS_0     0x14C >> 2
#define GPIO_UVWX_INT_SENS_1     0x150 >> 2
#define GPIO_UVWX_INT_SENS_2     0x154 >> 2
#define GPIO_UVWX_INT_STATUS     0x158 >> 2
#define GPIO_UVWX_RESET_TOLERANT 0x15C >> 2
#define GPIO_UVWX_DEBOUNCE_1     0x160 >> 2
#define GPIO_UVWX_DEBOUNCE_2     0x164 >> 2
#define GPIO_UVWX_INPUT_MASK     0x168 >> 2
#define GPIO_YZAAAB_COMMAND_SRC_0 0x170 >> 2
#define GPIO_YZAAAB_COMMAND_SRC_1 0x174 >> 2
#define GPIO_YZAAAB_INT_ENABLE   0x178 >> 2
#define GPIO_YZAAAB_INT_SENS_0   0x17C >> 2
#define GPIO_YZAAAB_INT_SENS_1   0x180 >> 2
#define GPIO_YZAAAB_INT_SENS_2   0x184 >> 2
#define GPIO_YZAAAB_INT_STATUS   0x188 >> 2
#define GPIO_YZAAAB_RESET_TOLERANT 0x18C >> 2
#define GPIO_YZAAAB_DEBOUNCE_1   0x190 >> 2
#define GPIO_YZAAAB_DEBOUNCE_2   0x194 >> 2
#define GPIO_YZAAAB_INPUT_MASK   0x198 >> 2
#define GPIO_AC_COMMAND_SRC_0    0x1A0 >> 2
#define GPIO_AC_COMMAND_SRC_1    0x1A4 >> 2
#define GPIO_AC_INT_ENABLE       0x1A8 >> 2
#define GPIO_AC_INT_SENS_0       0x1AC >> 2
#define GPIO_AC_INT_SENS_1       0x1B0 >> 2
#define GPIO_AC_INT_SENS_2       0x1B4 >> 2
#define GPIO_AC_INT_STATUS       0x1B8 >> 2
#define GPIO_AC_RESET_TOLERANT   0x1BC >> 2
#define GPIO_AC_DEBOUNCE_1       0x1C0 >> 2
#define GPIO_AC_DEBOUNCE_2       0x1C4 >> 2
#define GPIO_AC_INPUT_MASK       0x1C8 >> 2
#define GPIO_ABCD_INPUT_MASK     0x1D0 >> 2
#define GPIO_EFGH_INPUT_MASK     0x1D4 >> 2
#define GPIO_YZAAAB_DATA_VALUE   0x1E0 >> 2
#define GPIO_YZAAAB_DIRECTION    0x1E4 >> 2
#define GPIO_AC_DATA_VALUE       0x1E8 >> 2
#define GPIO_AC_DIRECTION        0x1EC >> 2

/* 1.8V */
// HACK
// Actual values are same as ABCD and E above + 0x800
#define GPIO_18_ABCD_DATA_VALUE     (0x800-0x600) >> 2
#define GPIO_18_ABCD_DIRECTION      (0x804-0x600) >> 2
#define GPIO_18_ABCD_INT_ENABLE     (0x808-0x600) >> 2
#define GPIO_18_ABCD_INT_SENS_0     (0x80C-0x600) >> 2
#define GPIO_18_ABCD_INT_SENS_1     (0x810-0x600) >> 2
#define GPIO_18_ABCD_INT_SENS_2     (0x814-0x600) >> 2
#define GPIO_18_ABCD_INT_STATUS     (0x818-0x600) >> 2
#define GPIO_18_ABCD_RESET_TOLERANT (0x81C-0x600) >> 2
#define GPIO_18_E_DATA_VALUE        (0x820-0x600) >> 2
#define GPIO_18_E_DIRECTION         (0x824-0x600) >> 2
#define GPIO_18_E_INT_ENABLE        (0x828-0x600) >> 2
#define GPIO_18_E_INT_SENS_0        (0x82C-0x600) >> 2
#define GPIO_18_E_INT_SENS_1        (0x830-0x600) >> 2
#define GPIO_18_E_INT_SENS_2        (0x834-0x600) >> 2
#define GPIO_18_E_INT_STATUS        (0x838-0x600) >> 2
#define GPIO_18_E_RESET_TOL         (0x83C-0x600) >> 2
#define GPIO_18_ABCD_DEBOUNCE_1     (0x840-0x600) >> 2
#define GPIO_18_ABCD_DEBOUNCE_2     (0x844-0x600) >> 2
#define GPIO_18_E_DEBOUNCE_1        (0x848-0x600) >> 2
#define GPIO_18_E_DEBOUNCE_2        (0x84C-0x600) >> 2
#define GPIO_18_DEBOUNCE_TIME_1     (0x850-0x600) >> 2
#define GPIO_18_DEBOUNCE_TIME_2     (0x854-0x600) >> 2
#define GPIO_18_DEBOUNCE_TIME_3     (0x858-0x600) >> 2
#define GPIO_18_ABCD_COMMAND_SRC_0  (0x860-0x600) >> 2
#define GPIO_18_ABCD_COMMAND_SRC_1  (0x864-0x600) >> 2
#define GPIO_18_E_COMMAND_SRC_0     (0x868-0x600) >> 2
#define GPIO_18_E_COMMAND_SRC_1     (0x86C-0x600) >> 2
#define GPIO_18_ABCD_DATA_READ      (0x8C0-0x600) >> 2
#define GPIO_18_E_DATA_READ         (0x8C4-0x600) >> 2
#define GPIO_18_ABCD_INPUT_MASK     (0x9D0-0x600) >> 2
#define GPIO_18_E_INPUT_MASK        (0x9D4-0x600) >> 2

struct AspeedGPIO {
    uint16_t set_idx;
    uint32_t (*get)(GPIORegs *regs);
    void (*set)(AspeedGPIOState *s, GPIORegs *regs,
                GPIOSetProperties *props, uint32_t val);
};

static int aspeed_evaluate_irq(GPIORegs *regs, int prev_value_high, int bit)
{
    uint32_t falling_edge = 0, rising_edge = 0;
    uint32_t int_trigger = extract32(regs->int_sens_0, bit, 1)
                           | extract32(regs->int_sens_1, bit, 1) << 1
                           | extract32(regs->int_sens_2, bit, 1) << 2 ;
    uint32_t curr_pin_high = extract32(regs->data_value, bit, 1);

    /* Detect edges */
    if (curr_pin_high && !prev_value_high) {
            rising_edge = 1;
    } else if (!curr_pin_high && prev_value_high) {
            falling_edge = 1;
    }

    if (((int_trigger == ASPEED_FALLING_EDGE)  && falling_edge)  ||
        ((int_trigger == ASPEED_RISING_EDGE)  && rising_edge)  ||
        ((int_trigger == ASPEED_LEVEL_LOW)  && !curr_pin_high)  ||
        ((int_trigger == ASPEED_LEVEL_HIGH)  && curr_pin_high)  ||
        ((int_trigger >= ASPEED_DUAL_EDGE)  && (rising_edge || falling_edge)))
    {
        regs->int_status = deposit32(regs->int_status, bit, 1, 1);
        return 1;
    }
    return 0;
}

static void aspeed_gpio_update(AspeedGPIOState *s, GPIORegs *regs)
{
    uint32_t input_mask = regs->input_mask;
    uint32_t direction = regs->direction;
    uint32_t old = regs->data_value;
    uint32_t new = regs->data_read;
    uint32_t diff;
    int bit;

    diff = old ^ new;
    if (!diff) {
        return;
    }

    if (!direction) {
        return;
    }

    for (bit = 0; bit < ASPEED_GPIOS_PER_REG; bit++) {
        uint32_t mask = 1 << bit;
        /* If the bit needs to be updated... */
        if (!(diff & mask)) {
            continue;
        }
        /* ...and corresponds to an output pin...*/
        if (!(direction & mask)) {
            continue;
        }
        /* ...and isn't masked...  */
        if (input_mask & mask) {
            continue;
        }
        /* ... then update it*/
        if (mask & new) {
            regs->data_value |= mask;
        } else {
            regs->data_value &= ~mask;
        }

        if (aspeed_evaluate_irq(regs, old & mask, bit)) {
            qemu_set_irq(s->output[bit], 1);
        }
    }
}

static uint32_t aspeed_adjust_pin(AspeedGPIOState *s, uint32_t pin)
{
    if (s->ctrl->gap && pin >= s->ctrl->gap) {
        pin += 4;
    }

    return pin;
}

static uint32_t aspeed_get_set_idx_from_pin(AspeedGPIOState *s, uint32_t pin)
{
    /*
     * For most pins, dividing by 32 gets the set index.
     *
     * However the 2500 has a 4 pin gap in group AB and the 2400 has a 4 pin
     * gap in group Y (and only four pins in AB but this is the last group so
     * it doesn't matter).
     */
    return aspeed_adjust_pin(s, pin) >> 5;
}

static bool aspeed_gpio_get_pin_level(AspeedGPIOState *s, uint32_t pin)
{
    uint32_t reg_val;
    uint32_t mask = 1 << (pin);
    uint32_t set_idx = aspeed_get_set_idx_from_pin(s, pin);

    reg_val = s->sets[set_idx].data_value;

    return !!(reg_val & mask);
}

static void aspeed_gpio_set_pin_level(AspeedGPIOState *s, uint32_t pin,
                                      bool level)
{
    uint32_t mask  = 1 << (pin);
    uint32_t set_idx = aspeed_get_set_idx_from_pin(s, pin);

    if (level) {
        s->sets[set_idx].data_read |= mask;
    } else {
        s->sets[set_idx].data_read &= !mask;
    }

    aspeed_gpio_update(s, &s->sets[set_idx]);
}

/*
 *  | src_1 | src_2 |  source     |
 *  |-----------------------------|
 *  |   0   |   0   |  ARM        |
 *  |   0   |   1   |  LPC        |
 *  |   1   |   0   |  Coprocessor|
 *  |   1   |   1   |  Reserved   |
 *
 *  Once the source of a set is programmed, corresponding bits in the
 *  data_value, direction, interrupt [enable, sens[0-2]], reset_tol and
 *  debounce registers can only be written by the source.
 *
 *  Source is ARM by default
 *  only bits 24, 16, 8, and 0 can be set
 *
 *  we don't currently have a model for the LPC or Coprocessor
 */
static uint32_t update_value_control_source(GPIORegs *regs, uint32_t old_value,
                                            uint32_t value)
{
    int i;
    int cmd_source;

    /* assume the source is always ARM for now */
    int source = ASPEED_SOURCE_ARM;

    uint32_t new_value = 0;

    /* for each group in set */
    for (i = 0; i < ASPEED_GPIOS_PER_REG; i += 8) {
        cmd_source = extract32(regs->cmd_source_0, i, 1)
                | (extract32(regs->cmd_source_1, i, 1) << 1);

        if (source == cmd_source) {
            new_value |= (0xff << i) & value;
        } else {
            new_value |= (0xff << i) & old_value;
        }
    }
    return new_value;
}

/* Reader helper functions */
static uint32_t read_direction(GPIORegs *regs)
{
    return regs->direction;
}

static uint32_t read_data_value(GPIORegs *regs)
{
    return regs->data_value;
}

static uint32_t read_int_enable(GPIORegs *regs)
{
    return regs->int_enable;
}

static uint32_t read_int_sens_0(GPIORegs *regs)
{
    return regs->int_sens_0;
}

static uint32_t read_int_sens_1(GPIORegs *regs)
{
    return regs->int_sens_1;
}

static uint32_t read_int_sens_2(GPIORegs *regs)
{
    return regs->int_sens_2;
}

static uint32_t read_int_status(GPIORegs *regs)
{
    return regs->int_status;
}

static uint32_t read_reset_tol(GPIORegs *regs)
{
    return regs->reset_tol;
}

static uint32_t read_debounce_1(GPIORegs *regs)
{
    return regs->debounce_1;
}

static uint32_t read_debounce_2(GPIORegs *regs)
{
    return regs->debounce_2;
}

static uint32_t read_cmd_source_0(GPIORegs *regs)
{
    return regs->cmd_source_0;
}

static uint32_t read_cmd_source_1(GPIORegs *regs)
{
    return regs->cmd_source_1;
}

static uint32_t read_data(GPIORegs *regs)
{
    return regs->data_read;
}

static uint32_t read_input_mask(GPIORegs *regs)
{
    return regs->input_mask;
}

/* Write helper functions */

static void _write_data_value(AspeedGPIOState *s, GPIORegs *regs,
                              GPIOSetProperties *props, uint32_t val)
{
    val &= props->output | ~props->input;
    regs->data_read = update_value_control_source(regs, regs->data_read, val);
    aspeed_gpio_update(s, regs);
}

static void _write_direction(AspeedGPIOState *s, GPIORegs *regs,
                             GPIOSetProperties *props, uint32_t val)
{
    val &= props->output | ~props->input;
    regs->direction = update_value_control_source(regs, regs->direction, val);
    aspeed_gpio_update(s, regs);
}

static void _write_int_enable(AspeedGPIOState *s, GPIORegs *regs,
                              GPIOSetProperties *props, uint32_t val)
{
    regs->int_enable = update_value_control_source(regs, regs->int_enable, val);
    aspeed_gpio_update(s, regs);
}

static void _write_int_sens_0(AspeedGPIOState *s, GPIORegs *regs,
                              GPIOSetProperties *props, uint32_t val)
{
    regs->int_sens_0 = update_value_control_source(regs, regs->int_sens_0, val);
    aspeed_gpio_update(s, regs);
}

static void _write_int_sens_1(AspeedGPIOState *s, GPIORegs *regs,
                              GPIOSetProperties *props, uint32_t val)
{
    regs->int_sens_1 = update_value_control_source(regs, regs->int_sens_1, val);
    aspeed_gpio_update(s, regs);
}

static void _write_int_sens_2(AspeedGPIOState *s, GPIORegs *regs,
                              GPIOSetProperties *props, uint32_t val)
{
    regs->int_sens_2 = update_value_control_source(regs, regs->int_sens_2, val);
    aspeed_gpio_update(s, regs);
}

static void _write_int_status(AspeedGPIOState *s, GPIORegs *regs,
                              GPIOSetProperties *props, uint32_t val)
{
    regs->int_status = val;
    aspeed_gpio_update(s, regs);
}

static void _write_reset_tol(AspeedGPIOState *s, GPIORegs *regs,
                             GPIOSetProperties *props, uint32_t val)
{
    regs->reset_tol = update_value_control_source(regs, regs->reset_tol, val);
}

static void _write_debounce_1(AspeedGPIOState *s, GPIORegs *regs,
                              GPIOSetProperties *props, uint32_t val)
{
    regs->debounce_1 = update_value_control_source(regs, regs->debounce_1, val);
}

static void _write_debounce_2(AspeedGPIOState *s, GPIORegs *regs,
                              GPIOSetProperties *props, uint32_t val)
{
    regs->debounce_2 = update_value_control_source(regs, regs->debounce_2, val);
}

static void _write_cmd_source_0(AspeedGPIOState *s, GPIORegs *regs,
                                GPIOSetProperties *props, uint32_t val)
{
    regs->cmd_source_0 = val & ASPEED_CMD_SRC_MASK;
}

static void _write_cmd_source_1(AspeedGPIOState *s, GPIORegs *regs,
                                GPIOSetProperties *props, uint32_t val)
{
    regs->cmd_source_1 = val & ASPEED_CMD_SRC_MASK;
}

/*
 * feeds into interrupt generation
 * 0: read from data value reg will be updated
 * 1: read from data value reg will not be updated
 */
static void _write_input_mask(AspeedGPIOState *s, GPIORegs *regs,
                              GPIOSetProperties *props, uint32_t val)
{
    regs->input_mask = val & props->input;
    aspeed_gpio_update(s, regs);
}

static const struct AspeedGPIO gpios[GPIO_REG_ARRAY_SIZE] = {
    /* Set ABCD */
    [GPIO_ABCD_DATA_VALUE] = {0, read_data_value, _write_data_value},
    [GPIO_ABCD_DIRECTION] = {0, read_direction, _write_direction},
    [GPIO_ABCD_INT_ENABLE] = {0, read_int_enable, _write_int_enable},
    [GPIO_ABCD_INT_SENS_0] = {0, read_int_sens_0, _write_int_sens_0},
    [GPIO_ABCD_INT_SENS_1] = {0, read_int_sens_1, _write_int_sens_1},
    [GPIO_ABCD_INT_SENS_2] = {0, read_int_sens_2, _write_int_sens_2},
    [GPIO_ABCD_INT_STATUS] = {0, read_int_status, _write_int_status},
    [GPIO_ABCD_RESET_TOLERANT] = {0, read_reset_tol, _write_reset_tol},
    [GPIO_ABCD_DEBOUNCE_1] = {0, read_debounce_1, _write_debounce_1},
    [GPIO_ABCD_DEBOUNCE_2] = {0, read_debounce_2, _write_debounce_2},
    [GPIO_ABCD_COMMAND_SRC_0] = {0, read_cmd_source_0, _write_cmd_source_0},
    [GPIO_ABCD_COMMAND_SRC_1] = {0, read_cmd_source_1, _write_cmd_source_1},
    [GPIO_ABCD_DATA_READ] = {0, read_data, NULL},
    [GPIO_ABCD_INPUT_MASK] = {0, read_input_mask, _write_input_mask},
    /* Set EFGH */
    [GPIO_EFGH_DATA_VALUE] = {1, read_data_value, _write_data_value},
    [GPIO_EFGH_DIRECTION] = {1, read_direction, _write_direction },
    [GPIO_EFGH_INT_ENABLE] = {1, read_int_enable, _write_int_enable},
    [GPIO_EFGH_INT_SENS_0] = {1, read_int_sens_0, _write_int_sens_0},
    [GPIO_EFGH_INT_SENS_1] = {1, read_int_sens_1, _write_int_sens_1},
    [GPIO_EFGH_INT_SENS_2] = {1, read_int_sens_2, _write_int_sens_2},
    [GPIO_EFGH_INT_STATUS] = {1, read_int_status, _write_int_status},
    [GPIO_EFGH_RESET_TOL] = {1, read_reset_tol,   _write_reset_tol},
    [GPIO_EFGH_DEBOUNCE_1] = {1, read_debounce_1,  _write_debounce_1},
    [GPIO_EFGH_DEBOUNCE_2] = {1, read_debounce_2,  _write_debounce_2},
    [GPIO_EFGH_COMMAND_SRC_0] = {1, read_cmd_source_0,  _write_cmd_source_0},
    [GPIO_EFGH_COMMAND_SRC_1] = {1, read_cmd_source_1,  _write_cmd_source_1},
    [GPIO_EFGH_DATA_READ] = {1, read_data, NULL},
    [GPIO_EFGH_INPUT_MASK] = {1, read_input_mask,  _write_input_mask},
    /* Set IJKL */
    [GPIO_IJKL_DATA_VALUE] = {2, read_data_value, _write_data_value},
    [GPIO_IJKL_DIRECTION] = {2, read_direction, _write_direction},
    [GPIO_IJKL_INT_ENABLE] = {2, read_int_enable, _write_int_enable},
    [GPIO_IJKL_INT_SENS_0] = {2, read_int_sens_0, _write_int_sens_0},
    [GPIO_IJKL_INT_SENS_1] = {2, read_int_sens_1, _write_int_sens_1},
    [GPIO_IJKL_INT_SENS_2] = {2, read_int_sens_2, _write_int_sens_2},
    [GPIO_IJKL_INT_STATUS] = {2, read_int_status, _write_int_status},
    [GPIO_IJKL_RESET_TOLERANT] = {2, read_reset_tol, _write_reset_tol},
    [GPIO_IJKL_DEBOUNCE_1] = {2, read_debounce_1, _write_debounce_1},
    [GPIO_IJKL_DEBOUNCE_2] = {2, read_debounce_2, _write_debounce_2},
    [GPIO_IJKL_COMMAND_SRC_0] = {2, read_cmd_source_0, _write_cmd_source_0},
    [GPIO_IJKL_COMMAND_SRC_1] = {2, read_cmd_source_1, _write_cmd_source_1},
    [GPIO_IJKL_DATA_READ] = {2, read_data, NULL},
    [GPIO_IJKL_INPUT_MASK] = {2, read_input_mask, _write_input_mask},
    /* Set MNOP */
    [GPIO_MNOP_DATA_VALUE] = {3, read_data_value, _write_data_value},
    [GPIO_MNOP_DIRECTION] = {3, read_direction, _write_direction},
    [GPIO_MNOP_INT_ENABLE] = {3, read_int_enable, _write_int_enable},
    [GPIO_MNOP_INT_SENS_0] = {3, read_int_sens_0, _write_int_sens_0},
    [GPIO_MNOP_INT_SENS_1] = {3, read_int_sens_1, _write_int_sens_1},
    [GPIO_MNOP_INT_SENS_2] = {3, read_int_sens_2, _write_int_sens_2},
    [GPIO_MNOP_INT_STATUS] = {3, read_int_status, _write_int_status},
    [GPIO_MNOP_RESET_TOLERANT] = {3, read_reset_tol,  _write_reset_tol},
    [GPIO_MNOP_DEBOUNCE_1] = {3, read_debounce_1, _write_debounce_1},
    [GPIO_MNOP_DEBOUNCE_2] = {3, read_debounce_2, _write_debounce_2},
    [GPIO_MNOP_COMMAND_SRC_0] = {3, read_cmd_source_0, _write_cmd_source_0},
    [GPIO_MNOP_COMMAND_SRC_1] = {3, read_cmd_source_1, _write_cmd_source_1},
    [GPIO_MNOP_DATA_READ] = {3, read_data, NULL},
    [GPIO_MNOP_INPUT_MASK] = {3, read_input_mask, _write_input_mask},
    /* Set QRST */
    [GPIO_QRST_DATA_VALUE] = {4, read_data_value, _write_data_value},
    [GPIO_QRST_DIRECTION] = {4, read_direction, _write_direction},
    [GPIO_QRST_INT_ENABLE] = {4, read_int_enable, _write_int_enable},
    [GPIO_QRST_INT_SENS_0] = {4, read_int_sens_0, _write_int_sens_0},
    [GPIO_QRST_INT_SENS_1] = {4, read_int_sens_1, _write_int_sens_1},
    [GPIO_QRST_INT_SENS_2] = {4, read_int_sens_2, _write_int_sens_2},
    [GPIO_QRST_INT_STATUS] = {4, read_int_status, _write_int_status},
    [GPIO_QRST_RESET_TOLERANT] = {4, read_reset_tol, _write_reset_tol},
    [GPIO_QRST_DEBOUNCE_1] = {4, read_debounce_1, _write_debounce_1},
    [GPIO_QRST_DEBOUNCE_2] = {4, read_debounce_2, _write_debounce_2},
    [GPIO_QRST_COMMAND_SRC_0] = {4, read_cmd_source_0, _write_cmd_source_0},
    [GPIO_QRST_COMMAND_SRC_1] = {4, read_cmd_source_1, _write_cmd_source_1},
    [GPIO_QRST_DATA_READ] = {4, read_data, NULL},
    [GPIO_QRST_INPUT_MASK] = {4, read_input_mask,  _write_input_mask},
    /* Set UVWX */
    [GPIO_UVWX_DATA_VALUE] = {5, read_data_value, _write_data_value},
    [GPIO_UWVX_DIRECTION] = {5, read_direction, _write_direction},
    [GPIO_UVWX_INT_ENABLE] = {5, read_int_enable, _write_int_enable},
    [GPIO_UVWX_INT_SENS_0] = {5, read_int_sens_0, _write_int_sens_0},
    [GPIO_UVWX_INT_SENS_1] = {5, read_int_sens_1, _write_int_sens_1},
    [GPIO_UVWX_INT_SENS_2] = {5, read_int_sens_2, _write_int_sens_2},
    [GPIO_UVWX_INT_STATUS] = {5, read_int_status, _write_int_status},
    [GPIO_UVWX_RESET_TOLERANT] = {5, read_reset_tol, _write_reset_tol},
    [GPIO_UVWX_DEBOUNCE_1] = {5, read_debounce_1, _write_debounce_1},
    [GPIO_UVWX_DEBOUNCE_2] = {5, read_debounce_2, _write_debounce_2},
    [GPIO_UVWX_COMMAND_SRC_0] = {5, read_cmd_source_0, _write_cmd_source_0},
    [GPIO_UVWX_COMMAND_SRC_1] = {5, read_cmd_source_1, _write_cmd_source_1},
    [GPIO_UVWX_DATA_READ] = {5, read_data, NULL},
    [GPIO_UVWX_INPUT_MASK] = {5, read_input_mask, _write_input_mask},
    /* Set YZAAAB */
    [GPIO_YZAAAB_DATA_VALUE] = {6, read_data_value, _write_data_value},
    [GPIO_YZAAAB_DIRECTION] = {6, read_direction, _write_direction},
    [GPIO_YZAAAB_INT_ENABLE] = {6, read_int_enable, _write_int_enable},
    [GPIO_YZAAAB_INT_SENS_0] = {6, read_int_sens_0, _write_int_sens_0},
    [GPIO_YZAAAB_INT_SENS_1] = {6, read_int_sens_1, _write_int_sens_1},
    [GPIO_YZAAAB_INT_SENS_2] = {6, read_int_sens_2, _write_int_sens_2},
    [GPIO_YZAAAB_INT_STATUS] = {6, read_int_status, _write_int_status},
    [GPIO_YZAAAB_RESET_TOLERANT] = {6, read_reset_tol, _write_reset_tol},
    [GPIO_YZAAAB_DEBOUNCE_1] = {6, read_debounce_1, _write_debounce_1},
    [GPIO_YZAAAB_DEBOUNCE_2] = {6, read_debounce_2, _write_debounce_2},
    [GPIO_YZAAAB_COMMAND_SRC_0] = {6, read_cmd_source_0, _write_cmd_source_0},
    [GPIO_YZAAAB_COMMAND_SRC_1] = {6, read_cmd_source_1, _write_cmd_source_1},
    [GPIO_YZAAAB_DATA_READ] = {6, read_data, NULL},
    [GPIO_YZAAAB_INPUT_MASK] = {6, read_input_mask, _write_input_mask},
    /* Set AC */
    [GPIO_AC_DATA_VALUE] = {7, read_data_value, _write_data_value},
    [GPIO_AC_DIRECTION] = {7, read_direction, _write_direction},
    [GPIO_AC_INT_ENABLE] = {7, read_int_enable, _write_int_enable},
    [GPIO_AC_INT_SENS_0] = {7, read_int_sens_0, _write_int_sens_0},
    [GPIO_AC_INT_SENS_1] = {7, read_int_sens_1, _write_int_sens_1},
    [GPIO_AC_INT_SENS_2] = {7, read_int_sens_2, _write_int_sens_2},
    [GPIO_AC_INT_STATUS] = {7, read_int_status, _write_int_status},
    [GPIO_AC_RESET_TOLERANT] = {7, read_reset_tol, _write_reset_tol},
    [GPIO_AC_DEBOUNCE_1] = {7, read_debounce_1, _write_debounce_1},
    [GPIO_AC_DEBOUNCE_2] = {7, read_debounce_2, _write_debounce_2},
    [GPIO_AC_COMMAND_SRC_0] = {7, read_cmd_source_0, _write_cmd_source_0},
    [GPIO_AC_COMMAND_SRC_1] = {7, read_cmd_source_1, _write_cmd_source_1},
    [GPIO_AC_DATA_READ] = {7, read_data, NULL},
    [GPIO_AC_INPUT_MASK] = {7, read_input_mask,     _write_input_mask},
    /* Debounce registers */
    [GPIO_DEBOUNCE_TIME_1] = {-1, NULL, NULL},
    /* 1.8V Set ABCD */
    [GPIO_18_ABCD_DATA_VALUE] = {0, read_data_value, _write_data_value},
    [GPIO_18_ABCD_DIRECTION] = {0, read_direction, _write_direction},
    [GPIO_18_ABCD_INT_ENABLE] = {0, read_int_enable, _write_int_enable},
    [GPIO_18_ABCD_INT_SENS_0] = {0, read_int_sens_0, _write_int_sens_0},
    [GPIO_18_ABCD_INT_SENS_1] = {0, read_int_sens_1, _write_int_sens_1},
    [GPIO_18_ABCD_INT_SENS_2] = {0, read_int_sens_2, _write_int_sens_2},
    [GPIO_18_ABCD_INT_STATUS] = {0, read_int_status, _write_int_status},
    [GPIO_18_ABCD_RESET_TOLERANT] = {0, read_reset_tol, _write_reset_tol},
    [GPIO_18_ABCD_DEBOUNCE_1] = {0, read_debounce_1, _write_debounce_1},
    [GPIO_18_ABCD_DEBOUNCE_2] = {0, read_debounce_2, _write_debounce_2},
    [GPIO_18_ABCD_COMMAND_SRC_0] = {0, read_cmd_source_0, _write_cmd_source_0},
    [GPIO_18_ABCD_COMMAND_SRC_1] = {0, read_cmd_source_1, _write_cmd_source_1},
    [GPIO_18_ABCD_DATA_READ] = {0, read_data, NULL},
    [GPIO_18_ABCD_INPUT_MASK] = {0, read_input_mask, _write_input_mask},
    /* 1.8V Set E */
    [GPIO_18_E_DATA_VALUE] = {1, read_data_value, _write_data_value},
    [GPIO_18_E_DIRECTION] = {1, read_direction, _write_direction },
    [GPIO_18_E_INT_ENABLE] = {1, read_int_enable, _write_int_enable},
    [GPIO_18_E_INT_SENS_0] = {1, read_int_sens_0, _write_int_sens_0},
    [GPIO_18_E_INT_SENS_1] = {1, read_int_sens_1, _write_int_sens_1},
    [GPIO_18_E_INT_SENS_2] = {1, read_int_sens_2, _write_int_sens_2},
    [GPIO_18_E_INT_STATUS] = {1, read_int_status, _write_int_status},
    [GPIO_18_E_RESET_TOL] = {1, read_reset_tol,   _write_reset_tol},
    [GPIO_18_E_DEBOUNCE_1] = {1, read_debounce_1,  _write_debounce_1},
    [GPIO_18_E_DEBOUNCE_2] = {1, read_debounce_2,  _write_debounce_2},
    [GPIO_18_E_COMMAND_SRC_0] = {1, read_cmd_source_0,  _write_cmd_source_0},
    [GPIO_18_E_COMMAND_SRC_1] = {1, read_cmd_source_1,  _write_cmd_source_1},
    [GPIO_18_E_DATA_READ] = {1, read_data, NULL},
    [GPIO_18_E_INPUT_MASK] = {1, read_input_mask,  _write_input_mask},
};

static uint64_t aspeed_offset_to_idx(hwaddr offset)
{
    if (offset > GPIO_REG_ARRAY_SIZE) {
        /* HACK */
        if (offset >= 0x800 && offset < 0x9D8) {
            return (offset - 600) >> 2;
        } else {
            qemu_log_mask(LOG_GUEST_ERROR, "offset %lx out of bounds", offset);
            return -1;
        }
    }
    return offset >> 2;
}

static uint64_t aspeed_gpio_read(void *opaque, hwaddr offset, uint32_t size)
{
    AspeedGPIOState *s = ASPEED_GPIO(opaque);
    uint64_t idx = -1;
    uint32_t val = 0;

    if (size != 4) {
        return 0;
    }

    idx = aspeed_offset_to_idx(offset);

    if ((idx == -1) || (gpios[offset].get == NULL)) {
        qemu_log_mask(LOG_GUEST_ERROR, "no getter for offset %lx", offset);
        return 0;
    }

    val = gpios[offset].get(&s->sets[gpios[offset].set_idx]);
    return (uint64_t) val;
}

static void aspeed_gpio_write(void *opaque, hwaddr offset, uint64_t data,
                              uint32_t size)
{
    AspeedGPIOState *s = ASPEED_GPIO(opaque);
    GPIOSetProperties *props = &s->ctrl->props[gpios[offset].set_idx];
    uint64_t idx = -1;

    idx = aspeed_offset_to_idx(offset);

    if ((idx == -1) || (gpios[offset].set == NULL)) {
        qemu_log_mask(LOG_GUEST_ERROR, "no setter for offset %lx", offset);
        return;
    }

    uint32_t mask = props->input | props->output;

    gpios[offset].set(s, &s->sets[gpios[offset].set_idx],
                      props, data & mask);
}

static void aspeed_gpio_get_pin(Object *obj, Visitor *v, const char *name,
                                void *opaque, Error **errp)
{
    int pin = 0xfff;
    bool level = true;
    char group[3];
    AspeedGPIOState *s = ASPEED_GPIO(obj);
    if (sscanf(name, "gpio%2[A-Z]%1d", group, &pin) != 2) {
        qemu_log_mask(LOG_GUEST_ERROR, "error reading %s", name);
        return;
    }
    level = aspeed_gpio_get_pin_level(s, pin);
    visit_type_bool(v, name, &level, errp);
}

static void aspeed_gpio_set_pin(Object *obj, Visitor *v, const char *name,
                               void *opaque, Error **errp)
{
    Error *local_err = NULL;
    bool level;
    int pin = 0xfff;
    char group[3];
    AspeedGPIOState *s = ASPEED_GPIO(obj);
    visit_type_bool(v, name, &level, &local_err);
    if (sscanf(name, "gpio%2[A-Z]%1d", group, &pin) != 2) {
        qemu_log_mask(LOG_GUEST_ERROR, "error reading %s", name);
        return;
    }
    aspeed_gpio_set_pin_level(s, pin, level);
}


/* Setup functions */
static const MemoryRegionOps aspeed_gpio_ops = {
    .read = aspeed_gpio_read,
    .write = aspeed_gpio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
};

static void aspeed_gpio_reset(DeviceState *dev)
{
    struct AspeedGPIOState *s = ASPEED_GPIO(dev);

    /* TODO: respect the reset tolerance registers */
    memset(s->sets, 0, sizeof(s->sets));
}

static void aspeed_gpio_realize(DeviceState *dev, Error **errp)
{
    AspeedGPIOState *s = ASPEED_GPIO(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

    sysbus_init_irq(sbd, &s->irq);

    memory_region_init_io(&s->iomem, OBJECT(s), &aspeed_gpio_ops, s,
            TYPE_ASPEED_GPIO, 0x1000);

    sysbus_init_mmio(sbd, &s->iomem);
}

static const VMStateDescription vmstate_gpio_regs = {
    .name = TYPE_ASPEED_GPIO"/regs",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(data_value,   GPIORegs),
        VMSTATE_UINT32(data_read,    GPIORegs),
        VMSTATE_UINT32(direction,    GPIORegs),
        VMSTATE_UINT32(int_enable,   GPIORegs),
        VMSTATE_UINT32(int_sens_0,   GPIORegs),
        VMSTATE_UINT32(int_sens_1,   GPIORegs),
        VMSTATE_UINT32(int_sens_2,   GPIORegs),
        VMSTATE_UINT32(int_status,   GPIORegs),
        VMSTATE_UINT32(reset_tol,    GPIORegs),
        VMSTATE_UINT32(cmd_source_0, GPIORegs),
        VMSTATE_UINT32(cmd_source_1, GPIORegs),
        VMSTATE_UINT32(debounce_1,   GPIORegs),
        VMSTATE_UINT32(debounce_2,   GPIORegs),
        VMSTATE_UINT32(input_mask,   GPIORegs),
        VMSTATE_END_OF_LIST(),
    }
};
static const VMStateDescription vmstate_aspeed_gpio = {
    .name = TYPE_ASPEED_GPIO,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_STRUCT_ARRAY(sets, AspeedGPIOState, ASPEED_GPIO_MAX_NR_SETS,
                             1, vmstate_gpio_regs, GPIORegs),
        VMSTATE_END_OF_LIST(),
   }
};

static void aspeed_gpio_init(Object *obj)
{
    AspeedGPIOState *s = ASPEED_GPIO(obj);
    AspeedGPIOClass *agc = ASPEED_GPIO_GET_CLASS(s);
    int pin;

    s->ctrl = agc->ctrl;

    for (pin = 0; pin < agc->ctrl->nr_gpio_pins; pin++) {
        char *name;
        int set_idx = aspeed_get_set_idx_from_pin(s, pin);
        int _pin = aspeed_adjust_pin(s, pin);
        int pin_idx = _pin - (set_idx * 32);
        int group_idx = pin_idx >> 3;

        name = g_strdup_printf("gpio%s%d",
                               agc->ctrl->props[set_idx].set[group_idx],
                               pin_idx % 8);
        object_property_add(obj, name, "bool",
                            aspeed_gpio_get_pin,
                            aspeed_gpio_set_pin,
                            NULL, NULL, NULL);
    }
}

static void aspeed_gpio_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    AspeedGPIOClass *agc = ASPEED_GPIO_CLASS(klass);

    dc->realize = aspeed_gpio_realize;
    dc->reset = aspeed_gpio_reset;
    dc->desc = "Aspeed GPIO Controller";
    dc->vmsd = &vmstate_aspeed_gpio;
    agc->ctrl = data;
}

static GPIOSetProperties ast2400_set_props[] = {
    [0] = {0xffffffff,  0xffffffff,  {"A", "B", "C", "D"} },
    [1] = {0xffffffff,  0xffffffff,  {"E", "F", "G", "H"} },
    [2] = {0xffffffff,  0xffffffff,  {"I", "J", "K", "L"} },
    [3] = {0xffffffff,  0xffffffff,  {"M", "N", "O", "P"} },
    [4] = {0xffffffff,  0xffffffff,  {"Q", "R", "S", "T"} },
    [5] = {0xffffffff,  0x0000ffff,  {"U", "V", "W", "X"} },
    [6] = {0x0000000f,  0x0fffff0f,  {"Y", "Z", "AA", "AB"} },
};

static GPIOSetProperties ast2500_set_props[] = {
    [0] = {0xffffffff,  0xffffffff,  {"A", "B", "C", "D"} },
    [1] = {0xffffffff,  0xffffffff,  {"E", "F", "G", "H"} },
    [2] = {0xffffffff,  0xffffffff,  {"I", "J", "K", "L"} },
    [3] = {0xffffffff,  0xffffffff,  {"M", "N", "O", "P"} },
    [4] = {0xffffffff,  0xffffffff,  {"Q", "R", "S", "T"} },
    [5] = {0xffffffff,  0x0000ffff,  {"U", "V", "W", "X"} },
    [6] = {0xffffff0f,  0x0fffff0f,  {"Y", "Z", "AA", "AB"} },
    [7] = {0x000000ff,  0x000000ff,  {"AC"} },
};

static GPIOSetProperties ast2600_set_props[] = {
    [0] = {0xffffffff,  0xffffffff,  {"A", "B", "C", "D"} },
    [1] = {0xffffffff,  0xffffffff,  {"E", "F", "G", "H"} },
    [2] = {0xffffffff,  0xffffffff,  {"I", "J", "K", "L"} },
    [3] = {0xffffffff,  0xffffffff,  {"M", "N", "O", "P"} },
    [4] = {0xffffffff,  0xffffffff,  {"Q", "R", "S", "T"} },
    [5] = {0xffffffff,  0x0000ffff,  {"U", "V", "W", "X"} },
    [6] = {0xffff0000,  0x0fff0000,  {"Y", "Z", "", ""} },
    /* 1.8V */
    [7] = {0x000000ff,  0x000000ff,  {"A", "B", "C", "D"} },
    [8] = {0x000000ff,  0x000000ff,  {"E"} },
};

static AspeedGPIOController controllers[] = {
    {
        .name           = TYPE_ASPEED_GPIO "-ast2600",
        .props          = ast2600_set_props,
        .nr_gpio_pins   = 208,
        .nr_gpio_sets   = 7,
    }, {
        .name           = TYPE_ASPEED_GPIO "-ast2500",
        .props          = ast2500_set_props,
        .nr_gpio_pins   = 228,
        .nr_gpio_sets   = 8,
        .gap            = 220,
    }, {
        .name           = TYPE_ASPEED_GPIO "-ast2400",
        .props          = ast2400_set_props,
        .nr_gpio_pins   = 216,
        .nr_gpio_sets   = 7,
        .gap            = 196,
    }
};

static const TypeInfo aspeed_gpio_info = {
    .name = TYPE_ASPEED_GPIO,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(AspeedGPIOState),
    .class_size     = sizeof(AspeedGPIOClass),
    .instance_init = aspeed_gpio_init,
    .abstract       = true,
};

static void aspeed_gpio_register_types(void)
{
    int i;

    type_register_static(&aspeed_gpio_info);
    for (i = 0; i < ARRAY_SIZE(controllers); i++) {
        TypeInfo t = {
            .name = controllers[i].name,
            .parent = TYPE_ASPEED_GPIO,
            .class_init = aspeed_gpio_class_init,
            .class_data = (void *)&controllers[i],
        };
        type_register(&t);
    };
}

type_init(aspeed_gpio_register_types);
