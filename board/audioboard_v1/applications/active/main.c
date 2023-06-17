/*
 * board/audioboard_v1/applications/active/main.c
 * Copyright (C) 2023 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "board.h"
#include "tasks.h"
#include <stdlib.h>
/*----------------------------------------------------------------------------*/
int main(void)
{
  struct Board * const board = malloc(sizeof(struct Board));
  appBoardInit(board);

#ifdef ENABLE_DBG
  /*
   * 1090898 for 12 MHz
   *  363625 for  4 MHz
   */
  board->debug.idle = 363625;
#endif

  invokeStartupTask(board);
  return appBoardStart(board);
}
