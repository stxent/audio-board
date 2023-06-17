/*
 * board/audioboard_v1/tests/slave/main.c
 * Copyright (C) 2023 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "board_shared.h"
#include "slave.h"
#include <halm/core/cortex/nvic.h>
#include <halm/generic/work_queue.h>
#include <halm/pm.h>
#include <xcore/interface.h>
#include <assert.h>
/*----------------------------------------------------------------------------*/
struct MockBoard
{
  struct AmpPackage amp;
  struct Interface *slave;

  struct Pin led;
  struct Pin mux;
  struct Pin rst;
};
/*----------------------------------------------------------------------------*/
static bool mockBoardInit(struct MockBoard *);
static void onSlaveUpdateEvent(void *);
static void slaveUpdateTask(void *);
/*----------------------------------------------------------------------------*/
static const struct WorkQueueConfig workQueueConfig = {
    .size = 4
};
/*----------------------------------------------------------------------------*/
static bool mockBoardInit(struct MockBoard *board)
{
  board->led = pinInit(BOARD_LED_PIN);
  if (!pinValid(board->led))
    return false;
  pinOutput(board->led, false);

  board->mux = pinInit(BOARD_I2S_MUX_PIN);
  if (!pinValid(board->mux))
    return false;
  pinOutput(board->mux, true);

  board->rst = pinInit(BOARD_I2S_RST_PIN);
  if (!pinValid(board->rst))
    return false;
  pinOutput(board->rst, true);

  if (!boardSetupAmpPackage(&board->amp))
    return false;

  return true;
}
/*----------------------------------------------------------------------------*/
static void onSlaveUpdateEvent(void *argument)
{
  wqAdd(WQ_DEFAULT, slaveUpdateTask, argument);
}
/*----------------------------------------------------------------------------*/
static void slaveUpdateTask(void *argument)
{
  static_assert(sizeof(struct SlaveRegOverlay) == SLAVE_REG_COUNT,
      "Incorrect slave structure");

  struct MockBoard * const board = argument;
  struct SlaveRegOverlay overlay;
  bool suspend = false;

  ifRead(board->slave, &overlay, sizeof(overlay));

  if (overlay.reset & SLAVE_RESET_RESET)
  {
    nvicResetCore();
    /* Unreachable code */
  }

  /* System control */
  if ((overlay.sys & SLAVE_SYS_SUSPEND) && (overlay.sys & SLAVE_SYS_EXT_CLOCK))
    suspend = true;
  pinWrite(board->mux, (overlay.sys & SLAVE_SYS_EXT_CLOCK) == 0);

  /* Amplifier control */
  pinWrite(board->amp.power, (overlay.ctl & SLAVE_CTL_POWER) != 0);
  pinWrite(board->amp.gain0, (overlay.ctl & SLAVE_CTL_GAIN0) != 0);
  pinWrite(board->amp.gain1, (overlay.ctl & SLAVE_CTL_GAIN1) != 0);

  /* Reset state */
  overlay.reset = 0;
  overlay.sys &= SLAVE_SYS_MASK;
  overlay.ctl &= SLAVE_CTL_MASK;
  overlay.status = SLAVE_STATUS_POWER_READY;
  overlay.sw = 0;

  ifWrite(board->slave, &overlay, sizeof(overlay));

  if (suspend)
  {
    boardResetClock();
    pmChangeState(PM_SUSPEND);
    boardSetupClock();
  }
}
/*----------------------------------------------------------------------------*/
int main(void)
{
  struct MockBoard board;
  bool ready;

  ready = mockBoardInit(&board);
  assert(ready);
  (void)ready;

  /* Initialize Work Queues */
  WQ_DEFAULT = init(WorkQueue, &workQueueConfig);
  assert(WQ_DEFAULT != NULL);

  board.slave = boardMakeI2CSlave();
  assert(board.slave != NULL);
  ifSetCallback(board.slave, onSlaveUpdateEvent, &board);

  pinSet(board.led);
  wqStart(WQ_DEFAULT);
  return 0;
}
