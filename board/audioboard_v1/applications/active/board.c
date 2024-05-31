/*
 * board/audioboard_v1/applications/active/board.c
 * Copyright (C) 2023 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "board.h"
#include <halm/generic/work_queue.h>
#include <assert.h>
/*----------------------------------------------------------------------------*/
static const struct WorkQueueConfig workQueueConfig = {
    .size = 4
};
/*----------------------------------------------------------------------------*/
void appBoardInit(struct Board *board)
{
  bool ready = boardSetupClock();

#ifdef ENABLE_WDT
  /* Enable watchdog prior to all other peripherals */
  if ((board->system.watchdog = boardMakeWatchdog()) == NULL)
    ready = false;
#else
  board->system.watchdog = NULL;
#endif

#ifdef ENABLE_DBG
  if ((board->debug.serial = boardMakeSerial()) == NULL)
    ready = false;
  if ((board->debug.timer = boardMakeLoadTimer()) == NULL)
    ready = false;
#else
  board->debug.serial = NULL;
  board->debug.timer = NULL;
#endif

  ready = ready && boardSetupAmpPackage(&board->ampPackage);
  ready = ready && boardSetupAdcPackage(&board->adcPackage);
  ready = ready && boardSetupButtonPackage(&board->buttonPackage);
  ready = ready && boardSetupControlPackage(&board->controlPackage);

  /* Initialize Deep-Sleep wake-up logic */
  if ((board->system.wakeup = boardMakeWakeupInt()) == NULL)
    ready = false;

  /* Initialize Work Queue */
  if ((WQ_DEFAULT = init(WorkQueue, &workQueueConfig)) == NULL)
    ready = false;

  assert(ready);
  (void)ready;

  board->indication.red = pinInit(BOARD_LED_PIN);
  pinOutput(board->indication.red, true);

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

  board->system.slave = NULL;
  board->system.sw = 0;
  board->system.timeout = 0;
  board->system.autosuspend = false;
  board->system.powered = false;

  board->debug.loops = 0;
}
/*----------------------------------------------------------------------------*/
int appBoardStart(struct Board *)
{
  wqStart(WQ_DEFAULT);
  return 0;
}
