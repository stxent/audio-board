/*
 * board/v1/application/tasks.h
 * Copyright (C) 2023 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef BOARD_V1_APPLICATION_TASKS_H_
#define BOARD_V1_APPLICATION_TASKS_H_
/*----------------------------------------------------------------------------*/
#include <xcore/helpers.h>
/*----------------------------------------------------------------------------*/
struct Board;
/*----------------------------------------------------------------------------*/
BEGIN_DECLS

void invokeStartupTask(struct Board *);

END_DECLS
/*----------------------------------------------------------------------------*/
#endif /* BOARD_V1_APPLICATION_TASKS_H_ */
