/*
 * board/audioboard_v1/tests/wakeup/main.c
 * Copyright (C) 2023 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "board_shared.h"
#include <halm/delay.h>
#include <halm/interrupt.h>
#include <halm/pm.h>
#include <halm/watchdog.h>
#include <assert.h>
/*----------------------------------------------------------------------------*/
static void onWakeupEvent(void *argument)
{
  *(bool *)argument = true;
}
/*----------------------------------------------------------------------------*/
int main(void)
{
  boardSetupClock();
  bool event = false;

  const struct Pin led = pinInit(BOARD_LED_PIN);
  pinOutput(led, false);

  struct Watchdog * const watchdog = boardMakeWatchdog();
  assert(watchdog != NULL);

  struct Interrupt * const wakeup = boardMakeWakeupInt();
  assert(wakeup != NULL);
  interruptSetCallback(wakeup, onWakeupEvent, &event);
  interruptEnable(wakeup);

  /* Delay for debugger connection */
  for (unsigned int i = 0; i < 10; ++i)
  {
    mdelay(100);
    watchdogReload(watchdog);
  }

  while (1)
  {
    pinSet(led);
    mdelay(100);
    pinReset(led);

    boardResetClock();
    pmChangeState(PM_SUSPEND);
    boardSetupClock();

    watchdogReload(watchdog);
  }

  return 0;
}
