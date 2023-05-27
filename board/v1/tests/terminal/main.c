/*
 * board/v1/tests/blinking_led/main.c
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "board_shared.h"
#include "codec.h"
#include <xcore/interface.h>
#include <xcore/memory.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/*----------------------------------------------------------------------------*/
struct Command
{
  const char *text;
  void (*callback)(struct Codec *, uint32_t);
  uint32_t control;
};
/*----------------------------------------------------------------------------*/
static void commandAmplifierControl(struct Codec *codec, uint32_t control)
{
  pinWrite(codec->amp, control != 0);
}
/*----------------------------------------------------------------------------*/
static void commandGain0Control(struct Codec *codec, uint32_t control)
{
  pinWrite(codec->gain0, control != 0);
}
/*----------------------------------------------------------------------------*/
static void commandGain1Control(struct Codec *codec, uint32_t control)
{
  pinWrite(codec->gain1, control != 0);
}
/*----------------------------------------------------------------------------*/
static void commandClockSelectControl(struct Codec *codec, uint32_t control)
{
  pinWrite(codec->select, control != 0);
}
/*----------------------------------------------------------------------------*/
struct Command COMMANDS[] = {
    {"amp on", commandAmplifierControl, 1},
    {"amp 1", commandAmplifierControl, 1},
    {"amp off", commandAmplifierControl, 0},
    {"amp 0", commandAmplifierControl, 0},
    {"g0 on", commandGain0Control, 1},
    {"g0 1", commandGain0Control, 1},
    {"g0 off", commandGain0Control, 0},
    {"g0 0", commandGain0Control, 0},
    {"g1 on", commandGain1Control, 1},
    {"g1 1", commandGain1Control, 1},
    {"g1 off", commandGain1Control, 0},
    {"g1 0", commandGain1Control, 0},
    {"clk int", commandClockSelectControl, 1},
    {"clk 1", commandClockSelectControl, 1},
    {"clk ext", commandClockSelectControl, 0},
    {"clk 0", commandClockSelectControl, 0}
};
/*----------------------------------------------------------------------------*/
static void onSerialEvent(void *argument)
{
  *(bool *)argument = true;
}
/*----------------------------------------------------------------------------*/
static void processCommand(struct Interface *serial, struct Codec *codec,
    const char *command)
{
  for (size_t i = 0; i < ARRAY_SIZE(COMMANDS); ++i)
  {
    if (!strcmp(COMMANDS[i].text, command))
    {
      COMMANDS[i].callback(codec, COMMANDS[i].control);

      ifWrite(serial, "\r\n> ", 4);
      ifWrite(serial, COMMANDS[i].text, strlen(COMMANDS[i].text));
      ifWrite(serial, "\r\n", 2);

      break;
    }
  }

  uint16_t value = 0;
  uint16_t address = UINT16_MAX;
  size_t count = 1;
  char *position;
  bool write = false;

  address = (uint16_t)strtol(command, &position, 0);

  if (*position != '\0')
  {
    value = (uint16_t)strtol(position, &position, 0);
    write = true;
  }

  if (*position != '\0')
  {
    count = (size_t)strtol(position, 0, 0);
  }

  if (address == UINT16_MAX)
    return;
  if (count < 1 || count > 2)
    return;

  if (write)
  {
    codecWriteBuffer(codec, address, &value, count, 0);
  }

  char response[24];
  codecReadBuffer(codec, address, &value, count, 0);

  sprintf(response, "\r\n> %u 0x%02X\r\n",
      (unsigned int)address, (unsigned int)value);
  ifWrite(serial, response, strlen(response));
}
/*----------------------------------------------------------------------------*/
int main(void)
{
  bool event = false;

  boardSetupClock();

  struct Interface * const i2c = boardMakeI2CMaster();
  assert(i2c != NULL);

  struct Interface * const serial = boardMakeSerial();
  assert(serial != NULL);
  ifSetCallback(serial, onSerialEvent, &event);

  const struct CodecConfig codecConfig = {
      .interface = i2c,
      .address = 0x18,
      .rate = 0,
      .amp = BOARD_AMP_POWER_PIN,
      .gain0 = BOARD_AMP_GAIN0_PIN,
      .gain1 = BOARD_AMP_GAIN1_PIN,
      .reset = BOARD_I2S_RST_PIN,
      .select = BOARD_I2S_MUX_PIN
  };
  struct Codec * const codec = init(Codec, &codecConfig);
  assert(codec != NULL);

  char command[32];
  size_t position = 0;

  while (1)
  {
    while (!event)
      barrier();
    event = false;

    char buffer[16];
    size_t count;

    while ((count = ifRead(serial, buffer, sizeof(buffer))) > 0)
    {
      ifWrite(serial, buffer, count);

      for (size_t i = 0; i < count; ++i)
      {
        char c = buffer[i];

        if (c == '\r' || c == '\n')
        {
          if (position > 0)
          {
            command[position] = '\0';
            position = 0;

            processCommand(serial, codec, command);
          }
        }
        else if (position < sizeof(command) - 1)
        {
          command[position++] = c;
        }
      }
    }
  }

  return 0;
}
