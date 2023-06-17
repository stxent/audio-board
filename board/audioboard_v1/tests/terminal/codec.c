/*
 * board/audioboard_v1/tests/terminal/codec.c
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the MIT License
 */

#include "codec.h"
#include <halm/delay.h>
#include <halm/generic/i2c.h>
#include <assert.h>
#include <string.h>
/*----------------------------------------------------------------------------*/
#define BUFFER_LENGTH 9
#define CODEC_ADDRESS 0x18
#define CODEC_RATE    400000
/*----------------------------------------------------------------------------*/
static void setupInterface(struct Codec *, bool);
/*----------------------------------------------------------------------------*/
static enum Result codecInit(void *, const void *);
static void codecDeinit(void *);
/*----------------------------------------------------------------------------*/
const struct EntityClass * const Codec =
    &(const struct EntityClass){
    .size = sizeof(struct Codec),
    .init = codecInit,
    .deinit = codecDeinit
};
/*----------------------------------------------------------------------------*/
static void setupInterface(struct Codec *codec, bool read)
{
  ifSetParam(codec->interface, IF_RATE, &codec->rate);
  ifSetParam(codec->interface, IF_ADDRESS, &codec->address);

  if (read)
    ifSetParam(codec->interface, IF_I2C_REPEATED_START, 0);
}
/*----------------------------------------------------------------------------*/
static enum Result codecInit(void *object, const void *arguments)
{
  const struct CodecConfig * const config = arguments;
  assert(config);

  struct Codec * const codec = object;

  codec->amp = pinInit(config->amp);
  assert(pinValid(codec->amp));
  pinOutput(codec->amp, false);

  codec->gain0 = pinInit(config->gain0);
  assert(pinValid(codec->gain0));
  pinOutput(codec->gain0, false);

  codec->gain1 = pinInit(config->gain1);
  assert(pinValid(codec->gain1));
  pinOutput(codec->gain1, false);

  codec->reset = pinInit(config->reset);
  assert(pinValid(codec->reset));
  pinOutput(codec->reset, true);

  codec->select = pinInit(config->select);
  assert(pinValid(codec->select));
  pinOutput(codec->select, true);

  codec->interface = config->interface;
  codec->address = config->address ? config->address : CODEC_ADDRESS;
  codec->rate = config->rate ? config->rate : CODEC_RATE;

  pinReset(codec->reset);
  mdelay(10);
  pinSet(codec->reset);

  return E_OK;
}
/*----------------------------------------------------------------------------*/
static void codecDeinit(void *object)
{
  struct Codec * const codec = object;

  pinOutput(codec->amp, false);
  pinOutput(codec->reset, false);
}
/*----------------------------------------------------------------------------*/
void codecReadBuffer(struct Codec *codec, uint16_t address, void *buffer,
    size_t length, bool *ok)
{
  const uint8_t pageBuffer[2] = {0x00, (address & 0x100) ? 1 : 0};
  size_t count;
  bool result = false;

  setupInterface(codec, false);
  count = ifWrite(codec->interface, &pageBuffer, sizeof(pageBuffer));

  if (count == sizeof(pageBuffer))
  {
    const uint8_t addressBuffer = (uint8_t)address;

    setupInterface(codec, true);
    count = ifWrite(codec->interface, &addressBuffer, sizeof(addressBuffer));

    if (count == sizeof(addressBuffer))
    {
      count = ifRead(codec->interface, buffer, length);
      result = (count == length + 1);
    }
  }

  if (ok)
    *ok = result;
}
/*----------------------------------------------------------------------------*/
uint8_t codecReadReg(struct Codec *codec, uint16_t address, bool *ok)
{
  uint8_t buffer = 0;

  codecReadBuffer(codec, address, &buffer, sizeof(buffer), ok);
  return buffer;
}
/*----------------------------------------------------------------------------*/
uint16_t codecReadReg16(struct Codec *codec, uint16_t address, bool *ok)
{
  uint16_t buffer = 0;

  codecReadBuffer(codec, address, &buffer, sizeof(buffer), ok);
  return buffer;
}
/*----------------------------------------------------------------------------*/
void codecWriteBuffer(struct Codec *codec, uint16_t address, const void *buffer,
    size_t length, bool *ok)
{
  if (length > BUFFER_LENGTH - 1)
  {
    *ok = false;
    return;
  }

  const uint8_t pageBuffer[2] = {0x00, (address & 0x100) ? 1 : 0};
  size_t count;
  bool result = false;

  setupInterface(codec, false);
  count = ifWrite(codec->interface, &pageBuffer, sizeof(pageBuffer));

  if (count == sizeof(pageBuffer))
  {
    uint8_t dataBuffer[BUFFER_LENGTH];

    dataBuffer[0] = (uint8_t)address;
    memcpy(dataBuffer + 1, buffer, length);

    setupInterface(codec, false);
    count = ifWrite(codec->interface, &dataBuffer, length + 1);

    result = (count == length + 1);
  }

  if (ok)
    *ok = result;
}
/*----------------------------------------------------------------------------*/
void codecWriteReg(struct Codec *codec, uint16_t address, uint8_t value,
    bool *ok)
{
  codecWriteBuffer(codec, address, &value, sizeof(value), ok);
}
/*----------------------------------------------------------------------------*/
void codecWriteReg16(struct Codec *codec, uint16_t address, uint16_t value,
    bool *ok)
{
  codecWriteBuffer(codec, address, &value, sizeof(value), ok);
}
