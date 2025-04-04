/*
 * board/audioboard_v1/applications/passive/main.c
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "board_shared.h"
#include <halm/pm.h>
#include <xcore/interface.h>
/*----------------------------------------------------------------------------*/
static void showStatus(struct Interface *spi, struct Pin cs, uint8_t value)
{
  pinReset(cs);
  ifWrite(spi, &value, sizeof(value));
  pinSet(cs);
}
/*----------------------------------------------------------------------------*/
int main(void)
{
  boardSetupClock();

  const struct Pin led = pinInit(BOARD_LED_PIN);
  pinOutput(led, false);

  struct ChronoPackage chronoPackage = boardSetupChronoPackage();
  struct AmpPackage ampPackage = boardSetupAmpPackage();
  struct ControlPackage controlPackage = boardSetupControlPackage(
      chronoPackage.factory);

  /* Reset codec pins */
  boardSetupCodecPackage(NULL, false, false);

  /* Reset LEDs */
  showStatus(controlPackage.spi, controlPackage.csW, 0);

  /* Enable power amplifier */
  pinSet(ampPackage.gain0);
  pinSet(ampPackage.gain1);
  pinSet(ampPackage.power);

  while (1)
  {
    pmChangeState(PM_SLEEP);
  }

  return 0;
}
