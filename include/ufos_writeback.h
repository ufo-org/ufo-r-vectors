#pragma once

#include <stdint.h>

typedef enum UfoWriteListenerEvent_Tag {
  Writeback,
  Reset,
  UfoWBDestroy,
} UfoWriteListenerEvent_Tag;

typedef struct Writeback_Body {
  uintptr_t start_idx;
  uintptr_t end_idx;
  const uint8_t *data;
} Writeback_Body;

typedef struct UfoWriteListenerEvent {
  UfoWriteListenerEvent_Tag tag;
  union {
    Writeback_Body writeback;
  };
} UfoWriteListenerEvent;

typedef void (*UfoWritebackListener)(void *, struct UfoWriteListenerEvent);
