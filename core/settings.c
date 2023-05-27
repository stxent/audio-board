/*
 * settings.c
 * Copyright (C) 2023 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "settings.h"
#include <halm/generic/flash.h>
#include <xcore/crc/crc8_dallas.h>
#include <xcore/interface.h>
#include <string.h>
/*----------------------------------------------------------------------------*/
#define MAGIC_NUMBER 0x61
#define PAGE_SIZE    256
/*----------------------------------------------------------------------------*/
bool loadSettings(struct Interface *memory, uint32_t address,
    struct Settings *settings)
{
  uint8_t buffer[sizeof(struct Settings)];

  if (ifSetParam(memory, IF_POSITION, &address) != E_OK)
    return false;
  if (ifRead(memory, buffer, sizeof(buffer)) != sizeof(buffer))
    return false;
  if (buffer[0] != MAGIC_NUMBER)
    return false;

  const uint8_t checksum = crc8DallasUpdate(0, buffer, sizeof(buffer) - 1);

  if (buffer[sizeof(buffer) - 1] == checksum)
  {
    memcpy(settings, buffer, sizeof(buffer));
    return true;
  }
  else
    return false;
}
/*----------------------------------------------------------------------------*/
void saveSettings(struct Interface *memory, uint32_t address,
    const struct Settings *settings)
{
  uint8_t buffer[PAGE_SIZE];

  memset(buffer, 0xFF, sizeof(buffer));
  memcpy(buffer, settings, sizeof(struct Settings));
  buffer[0] = MAGIC_NUMBER;
  buffer[sizeof(struct Settings) - 1] =
      crc8DallasUpdate(0, buffer, sizeof(struct Settings) - 1);

  if (ifSetParam(memory, IF_FLASH_ERASE_SECTOR, &address) != E_OK)
    return;
  if (ifSetParam(memory, IF_POSITION, &address) != E_OK)
    return;
  ifWrite(memory, buffer, sizeof(buffer));
}
