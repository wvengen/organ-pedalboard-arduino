/*
 * Organ pedal board MIDIfication
 * For: Arduino Mega 2560 + MIDI shield
 *
 * Consists of:
 * - MIDI handling
 * - pedal key handling
 * - swell pedal (digital or analog)
 * - stops (to emit program change messages)
 *
 * Each section has its own variables configuring MIDI notes,
 * channels and CC number, as well pin numbers on the Arduino.
 *
 * Requires the MIDI library from.
 * Download the latest release zip, and install it as a library:
 * https://github.com/FortySevenEffects/arduino_midi_library/releases
 *
 *
 * Changelog:
 * 2022-01-26 wvengen tested with Arduino + MIDI shield
 * 2022-01-14 wvengen initial version
 */
#include <MIDI.h>

/*
 * MIDI handling
 */

MIDI_CREATE_DEFAULT_INSTANCE();

void setup_midi() {
  // Don't process any incoming midi messages, we only do output.
  MIDI.begin(MIDI_CHANNEL_OFF);
  // But copy in to out, so another device can be daisy chained
  MIDI.turnThruOn();
}

void loop_midi() {
  // Call MIDI read for MIDI thru handling
  // See: https://github.com/FortySevenEffects/arduino_midi_library/wiki#midi-thru
  MIDI.read();
}

/*
 * Key handling
 *
 * The keys we have are make contacts, floating open when disconnected.
 * Therefore the pins are configured with pullup, and need to be connected
 * to GND when closed.
 */

// Pin numbers of each key/switch
// this is an array so we can rearrange order later if needed
static const uint8_t KEY_PINS[] = {
  22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
  32, 33, 34, 35, 36, 37, 38, 39, 40, 41,
  42, 43, 44, 45, 46, 47, 48, 49, 50, 51,
  52, 53
};
static const size_t KEY_COUNT = sizeof(KEY_PINS) / sizeof(uint8_t);

// MIDI Output channel for keys.
static const uint8_t KEY_OUT_CHANNEL = 3;
// MIDI Start note of first key.
static const uint8_t KEY_START_NOTE = 24; // note C0
// MIDI Velocity of key
static const uint8_t KEY_VELOCITY = 127;

// Last read state of each key
static uint8_t key_states[KEY_COUNT];

void setup_keys() {
  for (uint8_t i = 0; i < KEY_COUNT; i++) {
    // Configure as digital inputs
    pinMode(KEY_PINS[i], INPUT_PULLUP);
    // Assume keys are not pressed to start with
    key_states[i] = HIGH;
  }
}

void loop_keys() {
  for (uint8_t i=0; i < KEY_COUNT; i++) {
    uint8_t old_state = key_states[i];
    uint8_t new_state = digitalRead(KEY_PINS[i]);

    if (old_state != new_state) {
      key_states[i] = new_state;

      uint8_t note = KEY_START_NOTE + i;

      if (new_state == LOW) {
        // Key pressed
        MIDI.sendNoteOn(note, KEY_VELOCITY, KEY_OUT_CHANNEL);
      } else {
        // Key released
        MIDI.sendNoteOff(note, KEY_VELOCITY, KEY_OUT_CHANNEL);
      }
    }
  }
}

/*
 * Digital pedal handling (currently unused, we prefered an analog pedal after all)
 *
 * The encoder has a digital non-floating output (bits low or high).
 */

// Pin numbers for 7-bit encoder (MSB to LSB) (CC messages are 7 bits)
static const size_t DPEDAL_ENCODER_PIN_COUNT = 7;
static const uint8_t DPEDAL_ENCODER_PINS[DPEDAL_ENCODER_PIN_COUNT] = {
  A9, A10, A11, A12, A13, A14, A15
};

// MIDI Output channel for digital pedal.
static const uint8_t DPEDAL_OUT_CHANNEL = 3;
// MIDI Controller number for digital pedal.
static const uint8_t DPEDAL_CC = 7; // Volume

// Last read value
static uint8_t dpedal_value = 0;

void setup_dpedal() {
  for (uint8_t i = 0; i < DPEDAL_ENCODER_PIN_COUNT; i++) {
    pinMode(DPEDAL_ENCODER_PINS[i], INPUT);
  }
}

void loop_dpedal() {
  uint8_t cur_value = 0;
  for (uint8_t i = 0; i < DPEDAL_ENCODER_PIN_COUNT; i++) {
    cur_value = cur_value << 1;
    if (digitalRead(DPEDAL_ENCODER_PINS[i])) {
      cur_value |= 1;
    }
  }

  if (cur_value != dpedal_value) {
    dpedal_value = cur_value;
    MIDI.sendControlChange(DPEDAL_CC, cur_value, DPEDAL_OUT_CHANNEL);
  }
}


/*
 * Analog pedal handling
 */

// Analog pedal pin number
static const uint8_t APEDAL_PIN = A0;

// MIDI Output channel for analog pedal.
static const uint8_t APEDAL_OUT_CHANNEL = 3;
// MIDI Controller number for analog pedal.
static const uint8_t APEDAL_CC = 7; // Volume

// Last read value
static uint8_t apedal_value = 0;

void setup_apedal() {
  // nothing to do
}

void loop_apedal() {
  int cur_value = analogRead(APEDAL_PIN);
  // Reduce resolution from 10 to 7 bits, suitable for MIDI.
  // See also: https://docs.arduino.cc/learn/microcontrollers/analog-input
  cur_value = cur_value >> (10 - 7);

  if (cur_value != apedal_value) {
    apedal_value = cur_value;
    MIDI.sendControlChange(APEDAL_CC, cur_value, APEDAL_OUT_CHANNEL);
  }
}

/*
 * Stops
 *
 * Stop keys are connecting to GND on close, floating open.
 */

// Pin numbers of each stop key
// this is an array so we can rearrange order later if needed
static const uint8_t STOP_PINS[] = {
  5, 4, 3, 2, 14, 15, 16, 17, 18, 19, 20, 21
};
static const size_t STOP_COUNT = sizeof(STOP_PINS) / sizeof(uint8_t);

// MIDI Program Change numbers, one for each stop key
static const uint8_t STOP_PC_NUMBERS[STOP_COUNT] = {
  1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12
};

// MIDI Output channel for stops.
static const uint8_t STOP_OUT_CHANNEL = 3;

// Last read state of each stop key
static uint8_t stop_states[STOP_COUNT];

void setup_stops() {
  for (uint8_t i = 0; i < STOP_COUNT; i++) {
    // Configure as digital inputs
    pinMode(STOP_PINS[i], INPUT_PULLUP);
    // Assume keys are not pressed to start with
    stop_states[i] = HIGH;
  }
}

void loop_stops() {
  for (uint8_t i=0; i < STOP_COUNT; i++) {
    uint8_t old_state = stop_states[i];
    uint8_t new_state = digitalRead(STOP_PINS[i]);

    // do something on stop press
    if (old_state != new_state) {
      stop_states[i] = new_state;

      if (new_state == LOW) {
        uint8_t pc_number = STOP_PC_NUMBERS[i];

        MIDI.sendProgramChange(pc_number, STOP_OUT_CHANNEL);
      }
    }
  }
}

/*
 * Main
 */

void setup() {
  setup_midi();
  setup_keys();
  setup_apedal(); // change to loop_dpedal for digital pedal
  setup_stops();
}

void loop() {
  loop_midi();
  loop_keys();
  loop_apedal(); // change to loop_dpedal for digital pedal
  loop_stops();
}
