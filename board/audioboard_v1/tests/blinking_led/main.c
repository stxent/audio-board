/*
 * board/audioboard_v1/tests/blinking_led/main.c
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "board_shared.h"
#include <halm/timer.h>
#include <xcore/memory.h>
#include <assert.h>
/*----------------------------------------------------------------------------*/
static void onTimerOverflow(void *argument)
{
  *(bool *)argument = true;
}
/*----------------------------------------------------------------------------*/
int main(void)
{
  bool event = false;

  boardSetupClock();

  const struct Pin led = pinInit(BOARD_LED_PIN);
  pinOutput(led, true);

  struct ChronoPackage chronoPackage = boardSetupChronoPackage();

  timerSetOverflow(chronoPackage.load,
      timerGetFrequency(chronoPackage.load) / 2);
  timerSetCallback(chronoPackage.load, onTimerOverflow, &event);
  timerEnable(chronoPackage.load);

  while (1)
  {
    while (!event)
      barrier();
    event = false;

    pinToggle(led);
  }

  return 0;
}
