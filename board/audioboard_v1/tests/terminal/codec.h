/*
 * board/audioboard_v1/tests/terminal/codec.h
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the MIT License
 */

#ifndef BOARD_AUDIOBOARD_V1_TESTS_TERMINAL_H_
#define BOARD_AUDIOBOARD_V1_TESTS_TERMINAL_H_
/*----------------------------------------------------------------------------*/
#include <halm/pin.h>
#include <xcore/entity.h>
/*----------------------------------------------------------------------------*/
extern const struct EntityClass * const Codec;

struct CodecConfig
{
  /** Mandatory: management interface. */
  struct Interface *interface;
  /** Optional: codec address. */
  uint32_t address;
  /** Optional: codec management interface rate. */
  uint32_t rate;
  /** Mandatory: amplifier power enable pin. */
  PinNumber amp;
  /** Mandatory: amplifier GAIN0 control pin. */
  PinNumber gain0;
  /** Mandatory: amplifier GAIN1 control pin. */
  PinNumber gain1;
  /** Mandatory: codec power enable pin. */
  PinNumber reset;
  /** Mandatory: codec clock selection pin. */
  PinNumber select;
};

struct Codec
{
  struct Entity base;

  struct Interface *interface;
  struct Pin amp;
  struct Pin gain0;
  struct Pin gain1;
  struct Pin reset;
  struct Pin select;
  uint32_t address;
  uint32_t rate;
};
/*----------------------------------------------------------------------------*/
BEGIN_DECLS

void codecReadBuffer(struct Codec *, uint16_t, void *, size_t, bool *);
uint8_t codecReadReg(struct Codec *, uint16_t, bool *);
uint16_t codecReadReg16(struct Codec *, uint16_t, bool *);
void codecWriteBuffer(struct Codec *, uint16_t, const void *, size_t, bool *);
void codecWriteReg(struct Codec *, uint16_t, uint8_t, bool *);
void codecWriteReg16(struct Codec *, uint16_t, uint16_t, bool *);

END_DECLS
/*----------------------------------------------------------------------------*/
#endif /* BOARD_AUDIOBOARD_V1_TESTS_TERMINAL_H_ */
