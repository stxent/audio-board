/*
 * board/audioboard_v1/applications/active/board.c
 * Copyright (C) 2023 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "board.h"
#include <halm/delay.h>
#include <halm/wq.h>
/*----------------------------------------------------------------------------*/
static void panic(struct Pin);
/*----------------------------------------------------------------------------*/
static void panic(struct Pin led)
{
  while (1)
  {
    mdelay(500);
    pinToggle(led);
  }
}
/*----------------------------------------------------------------------------*/
void appBoardInit(struct Board *board)
{
  const bool ready = boardSetupClock();

  board->indication.red = pinInit(BOARD_LED_PIN);
  pinOutput(board->indication.red, !BOARD_LED_INV);

  if (!ready)
    panic(board->indication.red);

#ifdef ENABLE_WDT
  /* Enable watchdog prior to all other peripherals */
  board->system.watchdog = boardMakeWatchdog();
#else
  board->system.watchdog = NULL;
#endif

  /* Initialize Work Queue */
  boardSetupDefaultWQ();

#ifdef ENABLE_DBG
  board->debug.serial = boardMakeSerial();
#else
  board->debug.serial = NULL;
#endif

  board->chronoPackage = boardSetupChronoPackage();
  board->ampPackage = boardSetupAmpPackage();
  board->adcPackage = boardSetupAdcPackage();
  board->buttonPackage = boardSetupButtonPackage(board->chronoPackage.factory);
  board->controlPackage = boardSetupControlPackage(
      board->chronoPackage.factory);

  /* Initialize Deep-Sleep wake-up logic */
  board->system.wakeup = boardMakeWakeupInt();

  board->indication.active = 0;
  board->indication.blink = 0;
  board->indication.state = 0;

  board->config.memory = boardMakeMemory();
  board->config.mode = MODE_NONE;

  board->event.codec = false;
  board->event.read = false;
  board->event.show = false;
  board->event.slave = false;
  board->event.suspend = false;

  board->system.retries = 0;
  board->system.slave = NULL;
  board->system.sw = 0;
  board->system.timeout = 0;
  board->system.autosuspend = false;
  board->system.powered = false;

  board->debug.idle = 0;
  board->debug.loops = 0;
}
/*----------------------------------------------------------------------------*/
int appBoardStart(struct Board *)
{
  wqStart(WQ_DEFAULT);
  return 0;
}
