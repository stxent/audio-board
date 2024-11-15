/*
 * core/slave.h
 * Copyright (C) 2023 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef CORE_SLAVE_H_
#define CORE_SLAVE_H_
/*----------------------------------------------------------------------------*/
#include <xcore/bits.h>
/*----------------------------------------------------------------------------*/
#define SLAVE_ADDRESS   0x15
#define SLAVE_REG_COUNT 9

enum
{
  SLAVE_REG_RESET   = 0x00,
  SLAVE_REG_SYS     = 0x01,
  SLAVE_REG_CTL     = 0x02,
  SLAVE_REG_LED     = 0x03,
  SLAVE_REG_STATUS  = 0x04,
  SLAVE_REG_SW      = 0x05,
  SLAVE_REG_PATH    = 0x06,
  SLAVE_REG_MIC     = 0x07,
  SLAVE_REG_SPK     = 0x08
};

struct [[gnu::packed]] SlaveRegOverlay
{
  uint8_t reset;
  uint8_t sys;
  uint8_t ctl;
  uint8_t led;
  uint8_t status;
  uint8_t sw;

  /*
   * Bits 0..1 - output path
   * Bits 2..3 - input path
   * Bit 4     - input AGC
   */
  uint8_t path;
  /* MIC level from 0 to 255 */
  uint8_t mic;
  /* SPK level from 0 to 255 */
  uint8_t spk;
};
/*------------------Reset control register------------------------------------*/
#define SLAVE_RESET_RESET               BIT(0)
/*------------------System control register-----------------------------------*/
#define SLAVE_SYS_EXT_CLOCK             BIT(0)
#define SLAVE_SYS_SUSPEND               BIT(1)
#define SLAVE_SYS_SUSPEND_AUTO          BIT(2)
#define SLAVE_SYS_SAVE_CONFIG           BIT(7)
#define SLAVE_SYS_MASK                  (BIT_FIELD(MASK(3), 0) | BIT(7))
/*------------------Amplifier control register--------------------------------*/
#define SLAVE_CTL_POWER                 BIT(0)
#define SLAVE_CTL_GAIN0                 BIT(1)
#define SLAVE_CTL_GAIN1                 BIT(2)
#define SLAVE_CTL_MASK                  MASK(3)
/*------------------Status register-------------------------------------------*/
#define SLAVE_STATUS_POWER_READY        BIT(0)
/*------------------Path control register-------------------------------------*/
enum
{
  SLAVE_PATH_INT = 1,
  SLAVE_PATH_EXT = 2
};

#define SLAVE_PATH_OUTPUT(value)        BIT_FIELD((value), 0)
#define SLAVE_PATH_OUTPUT_MASK          BIT_FIELD(MASK(2), 0)
#define SLAVE_PATH_OUTPUT_VALUE(reg) \
    FIELD_VALUE((reg), SLAVE_PATH_OUTPUT_MASK, 0)

#define SLAVE_PATH_INPUT(value)         BIT_FIELD((value), 2)
#define SLAVE_PATH_INPUT_MASK           BIT_FIELD(MASK(2), 2)
#define SLAVE_PATH_INPUT_VALUE(reg) \
    FIELD_VALUE((reg), SLAVE_PATH_INPUT_MASK, 2)

#define SLAVE_PATH_INPUT_AGC            BIT(4)
#define SLAVE_PATH_MASK                 MASK(5)
/*----------------------------------------------------------------------------*/
#endif /* CORE_SLAVE_H_ */
