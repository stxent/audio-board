/*
 * core/settings.h
 * Copyright (C) 2023 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef CORE_SETTINGS_H_
#define CORE_SETTINGS_H_
/*----------------------------------------------------------------------------*/
#include <stdbool.h>
#include <stdint.h>
/*----------------------------------------------------------------------------*/
struct Interface;

struct [[gnu::packed]] Settings
{
  uint8_t magic;

  uint8_t codecInputAGCEnabled;
  uint8_t codecInputLevel;
  uint8_t codecInputPath;
  uint8_t codecOutputLevel;
  uint8_t codecOutputPath;

  uint8_t checksum;
};
/*----------------------------------------------------------------------------*/
bool loadSettings(struct Interface *, uint32_t, struct Settings *);
void saveSettings(struct Interface *, uint32_t, const struct Settings *);
/*----------------------------------------------------------------------------*/
#endif /* CORE_SETTINGS_H_ */
