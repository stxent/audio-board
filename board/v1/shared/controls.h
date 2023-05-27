/*
 * board/v1/shared/controls.h
 * Copyright (C) 2023 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef BOARD_V1_SHARED_CONTROLS_H_
#define BOARD_V1_SHARED_CONTROLS_H_
/*----------------------------------------------------------------------------*/
#include <xcore/bits.h>
/*----------------------------------------------------------------------------*/
enum
{
  /* Enable active codec configuration mode */
  SW_ACTIVE             = 0x01,
  /* Load predefined configuration instead of runtime configuration */
  SW_LOAD_CONFIG        = 0x02,
  /* Enable external MCLK signal */
  SW_EXT_CLOCK          = 0x04,
  /* Enable 48 kHz sample rate */
  SW_SAMPLE_RATE        = 0x08,
  /* Enable AGC */
  SW_INPUT_GAIN_AUTO    = 0x10,
  /* Set maximum gain for external amplifier */
  SW_OUTPUT_GAIN_BOOST  = 0x20
};

#define SW_MASK           MASK(6)
#define VOLTAGE_THRESHOLD 4900
/*----------------------------------------------------------------------------*/
#endif /* BOARD_V1_SHARED_CONTROLS_H_ */
