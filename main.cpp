#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <cstring>
#include <iostream>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <format>
#include <vector>
#include <memory>
#include <functional>
#include <queue>
#include <cmath>
#include <thread>
#include <chrono>
#include <bitset> 

// MIDI-File spec v1.1 parser
// best effort: if errors exist, will print warning and ignore

// base class for callbacks to handle parsed data
// by default, just prints the events to stdout
struct Handler {
  const static bool DEFAULT_PRINT = true;
  virtual void on_header(uint16_t format, uint16_t ntrks, uint16_t division) {
    if constexpr (DEFAULT_PRINT) std::cout << "Header: format=" << format << ", ntrks=" << ntrks << ", division=" << division << std::endl;
  }

  std::string prefix(uint16_t track_index, uint64_t track_tick) {
    return "[" + std::to_string(track_index) + ":" + std::to_string(track_tick) + "] ";
  }

  virtual void on_start_of_track(uint16_t track_index) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, 0) << "Start of Track" << std::endl;
  }

  virtual void on_end_of_track(uint16_t track_index, uint64_t track_tick) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "End of Track" << std::endl;
  }

  virtual void on_end_of_file() {
    if constexpr (DEFAULT_PRINT) std::cout << "End of File" << std::endl;
  }

  virtual void on_undefined(uint16_t track_index, uint64_t track_tick, const std::vector<uint8_t> &data) {
    if constexpr (!DEFAULT_PRINT) return;
    std::cout << prefix(track_index, track_tick) << "Undefined: data=";
    for (uint8_t byte : data) {
      std::cout << std::hex << (int)byte << " ";
    }
    std::cout << std::dec << std::endl;
  }

  virtual void on_note_off(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t note, uint8_t velocity) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "Note Off: channel=" << (int)channel << ", note=" << (int)note << ", velocity=" << (int)velocity << std::endl;
  }

  virtual void on_note_on(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t note, uint8_t velocity) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "Note On: channel=" << (int)channel << ", note=" << (int)note << ", velocity=" << (int)velocity << std::endl;
  }

  virtual void on_polyphonic_key_pressure(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t note, uint8_t pressure) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "Polyphonic Key Pressure: channel=" << (int)channel << ", note=" << (int)note << ", pressure=" << (int)pressure << std::endl;
  }

  virtual void on_program_change(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t program) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "Program Change: channel=" << (int)channel << ", program=" << (int)program << std::endl;
  }

  virtual void on_channel_pressure(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t pressure) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "Channel Pressure: channel=" << (int)channel << ", pressure=" << (int)pressure << std::endl;
  }

  virtual void on_pitch_wheel_change(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint16_t value) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "Pitch Wheel Change: channel=" << (int)channel << ", value=" << (int)value << std::endl;
  }

  virtual void on_sysex(uint16_t track_index, uint64_t track_tick, const std::vector<uint8_t> &data) {
    if constexpr (!DEFAULT_PRINT) return;
    std::cout << prefix(track_index, track_tick) << "SysEx: data=";
    for (uint8_t byte : data) {
      std::cout << std::hex << (int)byte << " ";
    }
    std::cout << std::dec << std::endl;
  }

  virtual void on_song_position_pointer(uint16_t track_index, uint64_t track_tick, uint16_t midi_beats) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "Song Position Pointer: midi_beats=" << midi_beats << std::endl;
  }

  virtual void on_song_select(uint16_t track_index, uint64_t track_tick, uint8_t song_number) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "Song Select: song_number=" << (int)song_number << std::endl;
  }

  virtual void on_tune_request(uint16_t track_index, uint64_t track_tick) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "Tune Request" << std::endl;
  }

  virtual void on_realtime_message(uint16_t track_index, uint64_t track_tick, uint8_t status) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "Realtime Message: status=" << std::hex << (int)status << std::dec << std::endl;
  }

  virtual void set_undefined_control(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t controller, uint8_t value, bool msb) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "Undefined Control: channel=" << (int)channel << ", controller=" << (int)controller << ", value=" << (int)value << ", msb=" << msb << std::endl;
  }

  virtual void set_local_control(uint16_t track_index, uint64_t track_tick, bool on) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "Local Control: " << (on ? "on" : "off") << std::endl;
  }

  virtual void set_omni_mode(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t controller, bool on) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "Omni Mode: channel=" << (int)channel << ", controller=" << (int)controller << ", " << (on ? "on" : "off") << std::endl;
  }

  virtual void set_mono_mode(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t controller, bool on, uint8_t channels) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "Mono Mode: channel=" << (int)channel << ", controller=" << (int)controller << ", " << (on ? "on" : "off") << ", channels=" << (int)channels << std::endl;
  }

  virtual void all_notes_off(uint16_t track_index, uint64_t track_tick) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "All Notes Off" << std::endl;
  }

  virtual void set_general_purpose_controller(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t controller, uint8_t value, bool msb, uint8_t index) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "General Purpose Controller: channel=" << (int)channel << ", controller=" << (int)controller << ", value=" << (int)value << ", msb=" << msb << ", index=" << (int)index << std::endl;
  }

  virtual void set_bank_select(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t value, bool msb) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "Bank Select: channel=" << (int)channel << ", value=" << (int)value << ", msb=" << msb << std::endl;
  }

  virtual void set_modulation_wheel(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t value, bool msb) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "Modulation Wheel: channel=" << (int)channel << ", value=" << (int)value << ", msb=" << msb << std::endl;
  }

  virtual void set_breath_control(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t value, bool msb) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "Breath Control: channel=" << (int)channel << ", value=" << (int)value << ", msb=" << msb << std::endl;
  }

  virtual void set_foot_controller(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t value, bool msb) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "Foot Controller: channel=" << (int)channel << ", value=" << (int)value << ", msb=" << msb << std::endl;
  }

  virtual void set_portamento_time(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t value, bool msb) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "Portamento Time: channel=" << (int)channel << ", value=" << (int)value << ", msb=" << msb << std::endl;
  }

  virtual void set_data_entry(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t value, bool msb) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "Data Entry: channel=" << (int)channel << ", value=" << (int)value << ", msb=" << msb << std::endl;
  }

  virtual void set_channel_volume(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t value, bool msb) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "Channel Volume: channel=" << (int)channel << ", value=" << (int)value << ", msb=" << msb << std::endl;
  }

  virtual void set_balance(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t value, bool msb) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "Balance: channel=" << (int)channel << ", value=" << (int)value << ", msb=" << msb << std::endl;
  }

  virtual void set_pan(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t value, bool msb) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "Pan: channel=" << (int)channel << ", value=" << (int)value << ", msb=" << msb << std::endl;
  }

  virtual void set_expression_controller(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t value, bool msb) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "Expression Controller: channel=" << (int)channel << ", value=" << (int)value << ", msb=" << msb << std::endl;
  }

  virtual void set_effect_control(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t value, bool msb, uint8_t index) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "Effect Control: channel=" << (int)channel << ", value=" << (int)value << ", msb=" << msb << ", index=" << (int)index << std::endl;
  }

  virtual void set_sound_controller(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t controller, uint8_t value, uint8_t index) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "Sound Controller: channel=" << (int)channel << ", controller=" << (int)controller << ", value=" << (int)value << ", index=" << (int)index << std::endl;
  }

  virtual void set_effects_depth(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t controller, uint8_t value, uint8_t index) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "Effects Depth: channel=" << (int)channel << ", controller=" << (int)controller << ", value=" << (int)value << ", index=" << (int)index << std::endl;
  }

  virtual void set_damper_pedal(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t value) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "Damper Pedal: channel=" << (int)channel << ", value=" << (int)value << std::endl;
  }

  virtual void set_portamento(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t value) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "Portamento: channel=" << (int)channel << ", value=" << (int)value << std::endl;
  }

  virtual void set_sostenuto(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t value) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "Sostenuto: channel=" << (int)channel << ", value=" << (int)value << std::endl;
  }

  virtual void set_soft_pedal(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t value) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "Soft Pedal: channel=" << (int)channel << ", value=" << (int)value << std::endl;
  }

  virtual void set_legato_footswitch(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t value) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "Legato Footswitch: channel=" << (int)channel << ", value=" << (int)value << std::endl;
  }

  virtual void set_hold_2(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t value) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "Hold 2: channel=" << (int)channel << ", value=" << (int)value << std::endl;
  }

  virtual void set_portamento_control(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t value) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "Portamento Control: channel=" << (int)channel << ", value=" << (int)value << std::endl;
  }

  virtual void change_data_entry(uint16_t track_index, uint64_t track_tick, uint8_t channel, int8_t delta) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "Change Data Entry: channel=" << (int)channel << ", delta=" << (int)delta << std::endl;
  }

  virtual void set_non_registered_parameter_number(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t value, bool msb) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "Non-Registered Parameter Number: channel=" << (int)channel << ", value=" << (int)value << ", msb=" << msb << std::endl;
  }

  virtual void set_registered_parameter_number(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t value, bool msb) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "Registered Parameter Number: channel=" << (int)channel << ", value=" << (int)value << ", msb=" << msb << std::endl;
  }

  virtual void set_sequence_number(uint16_t track_index, uint64_t track_tick, uint16_t sequence_number) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "Sequence Number: " << sequence_number << std::endl;
  }

  virtual void set_generic_text_event(uint16_t track_index, uint64_t track_tick, const std::string &text) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "Generic Text Event: " << text << std::endl;
  }

  virtual void set_copyright_notice(uint16_t track_index, uint64_t track_tick, const std::string &text) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "Copyright Notice: " << text << std::endl;
  }

  virtual void set_sequence_or_track_name(uint16_t track_index, uint64_t track_tick, const std::string &text) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "Sequence/Track Name: " << text << std::endl;
  }

  virtual void set_instrument_name(uint16_t track_index, uint64_t track_tick, const std::string &text) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "Instrument Name: " << text << std::endl;
  }

  virtual void on_lyric(uint16_t track_index, uint64_t track_tick, const std::string &text) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "Lyric: " << text << std::endl;
  }

  virtual void on_marker(uint16_t track_index, uint64_t track_tick, const std::string &text) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "Marker: " << text << std::endl;
  }

  virtual void on_cue_point(uint16_t track_index, uint64_t track_tick, const std::string &text) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "Cue Point: " << text << std::endl;
  }

  virtual void set_midi_channel_prefix(uint16_t track_index, uint64_t track_tick, uint8_t channel) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "MIDI Channel Prefix: channel=" << (int)channel << std::endl;
  }

  virtual void on_tempo_change(uint16_t track_index, uint64_t track_tick, uint32_t microseconds_per_quarter_note) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "Tempo Change: microseconds_per_quarter_note=" << microseconds_per_quarter_note << std::endl;
  }

  virtual void on_smpte_offset(uint16_t track_index, uint64_t track_tick, uint8_t smpte_format, uint8_t hour, uint8_t minute, uint8_t second, uint8_t frame, uint8_t fractional_frame) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "SMPTE Offset: format=" << (int)smpte_format << ", hour=" << (int)hour << ", minute=" << (int)minute << ", second=" << (int)second << ", frame=" << (int)frame << ", fractional_frame=" << (int)fractional_frame << std::endl;
  }

  virtual void on_time_signature(uint16_t track_index, uint64_t track_tick, uint8_t numerator, uint8_t denominator, uint8_t midi_clocks_per_click, uint8_t thirty_second_notes_per_24_midi_clocks) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "Time Signature: numerator=" << (int)numerator << ", denominator=" << (int)denominator << ", midi_clocks_per_click=" << (int)midi_clocks_per_click << ", thirty_second_notes_per_24_midi_clocks=" << (int)thirty_second_notes_per_24_midi_clocks << std::endl;
  }

  virtual void on_key_signature(uint16_t track_index, uint64_t track_tick, int8_t sharps_flats, bool minor) {
    if constexpr (DEFAULT_PRINT) std::cout << prefix(track_index, track_tick) << "Key Signature: sharps_flats=" << (int)sharps_flats << ", minor=" << minor << std::endl;
  }

  virtual void on_sequencer_specific_meta_event(uint16_t track_index, uint64_t track_tick, const std::vector<uint8_t> &data) {
    if constexpr (!DEFAULT_PRINT) return;
    std::cout << prefix(track_index, track_tick) << "Sequencer-Specific Meta Event: data=";
    for (uint8_t byte : data) {
      std::cout << std::hex << (int)byte << " ";
    }
    std::cout << std::dec << std::endl;
  }

  virtual void on_undefined_meta_event(uint16_t track_index, uint64_t track_tick, uint8_t type, const std::vector<uint8_t> &data) {
    if constexpr (!DEFAULT_PRINT) return;
    std::cout << prefix(track_index, track_tick) << "Undefined Meta Event: type=" << (int)type << ", data=";
    for (uint8_t byte : data) {
      std::cout << std::hex << (int)byte << " ";
    }
    std::cout << std::dec << std::endl;
  }
};

// handles conversion between ticks, quarter notes, and absolute time
// for format 1 (simultaneous tracks) track 0 should contain all tempo changes
struct TimeTracker {
  bool smpte_div = false;

  // smpte division
  double frames_per_sec = 0.;
  uint8_t ticks_per_frame = 0;

  // quarter note division
  uint16_t ticks_per_quarter_note = 0;

  // represent changes to tempo using tempo sections, which apply to a range of track ticks
  // at start, tempo is 120bpm (500000 microseconds per quarter note)
  struct TempoSection {
    uint64_t tick_offset;
    double sec_offset;
    uint32_t us_per_qnote;
    TempoSection(uint64_t tick_offset = 0, double sec_offset = 0., uint32_t us_per_qnote = 500000) : tick_offset(tick_offset), sec_offset(sec_offset), us_per_qnote(us_per_qnote) {}
  };
  std::vector<TempoSection> tempo_sections = { TempoSection() };

  // calculate the absolute time in seconds of a track tick based on tempo sections
  double get_absolute_time(uint64_t track_tick) {
    // find the tempo section that applies to this track tick
    size_t section_index = tempo_sections.size()-1;
    if (tempo_sections.back().tick_offset > track_tick) {
      // if tick before last section, use binary search to find the correct section
      size_t left = 0;
      size_t right = tempo_sections.size()-2;
      while (left != right) {
        size_t mid = (left + right) >> 1;
        if (tempo_sections[mid].tick_offset > track_tick) {
          left = mid + 1;
        } else {
          right = mid;
        }
      }
      section_index = left;
    }
    const TempoSection &section = tempo_sections[section_index];
    track_tick -= section.tick_offset;

    // convert track ticks to seconds based on the tempo section
    if (smpte_div) {
      return section.sec_offset + (double)track_tick / (frames_per_sec * (double)ticks_per_frame);
    } else {
      double sec_per_tick = (double)section.us_per_qnote / ((double)ticks_per_quarter_note * 1000000.);
      return section.sec_offset + track_tick * sec_per_tick;
    }
  }

  // division given by header chunk
  // either ticks per quarter note (if bit 15 is 0) or SMPTE frames per second + ticks per frame (if bit 15 is 1)
  void set_division(uint16_t division) {
    if (division >> 15) {
      // SMPTE format: 0, negative SMPTE format (7 bits), ticks per frame (8 bits)
      smpte_div = true;
      // since smpte format given as negative, need to flip with 2s complement to get positive value
      uint8_t smpte_format = ~((division >> 8) & 0x7F) + 1;

      // frames per second = smpte_format aside from 29, which is 29.97 (30 drop frame)
      frames_per_sec = smpte_format == 29 ? 30. * 1000. / 1001. : (double)smpte_format;
      ticks_per_frame = (uint8_t)(division & 0xFF);
    } else {
      // Quarter Note format: ticks per quarter note (15 bits)
      smpte_div = false;
      frames_per_sec = 0;
      ticks_per_quarter_note = division;
    }
  }

  // sets the absolute offset of the track in SMPTE time
  // for format 1, must be in track 0
  // must occur on track tick 0
  void set_smpte_offset(uint8_t smpte_format, uint8_t hour, uint8_t minute, uint8_t second, uint8_t frame, uint8_t fractional_frame) {
    if (tempo_sections.size() > 1) {
      std::cout << "Warning: SMPTE offset set after tempo changes, ignored" << std::endl;
      return;
    }

    // smpte format: 0=24 fps, 1=25 fps, 2=29.97 fps (30 drop), 3=30 fps
    // note: does not use division set in header, is a standalone timestamp
    // a fractional frame is 1/100 of a frame
    double fps = smpte_format == 0 ? 24. : smpte_format == 1 ? 25. : smpte_format == 2 ? 30. * 1000. / 1001. : 30.;
    tempo_sections[0].sec_offset = (double)hour * 3600. + (double)minute * 60. + (double)second + (double)frame / fps + (double)fractional_frame / (fps * 100.);
  }

  // for format 2, each track can have its own SMPTE offset
  // call this before parsing each track to reset the timing
  void reset_timing() {
    tempo_sections.clear();
    tempo_sections.emplace_back();
  }

  // updates the tempo at a track tick
  // note: MIDI format requires it be specified in increasing track tick
  void add_tempo_change(uint64_t track_tick, uint32_t microseconds_per_quarter_note) {
    if (tempo_sections.back().tick_offset > track_tick) {
      std::cout << "Warning: tempo change at tick " << track_tick << " is before last tempo change at tick " << tempo_sections.back().tick_offset << ", change ignored" << std::endl;
      return;
    }
    if (tempo_sections.back().tick_offset == track_tick) {
      tempo_sections.back().us_per_qnote = microseconds_per_quarter_note;
    } else {
      tempo_sections.emplace_back(track_tick, get_absolute_time(track_tick), microseconds_per_quarter_note);
    }
  }
};

// context of the file being parsed
struct FileContext {
  // byte access
  std::vector<uint8_t> bytes;
  std::shared_ptr<Handler> handler;
  size_t offset = 0;

  // header chunk
  uint16_t format = 0;
  uint16_t ntrks = 0;
  uint16_t division = 0;

  // MIDI events
  uint16_t track_index = 0;
  uint64_t track_tick = 0;
  uint8_t running_status = 0;

  FileContext(size_t size, std::shared_ptr<Handler> h) : bytes(size), handler(h) {}

  inline void read(uint8_t *dest, size_t size) {
    std::copy(bytes.begin() + offset, bytes.begin() + offset + size, dest);
    offset += size;
  }
  inline void read(char *dest, size_t size) {
    return read(reinterpret_cast<uint8_t *>(dest), size);
  }
  inline uint8_t read() {
    return bytes[offset++];
  }
  inline uint16_t read16() {
    uint16_t value = (bytes[offset] << 8) | bytes[offset + 1];
    offset += 2;
    return value;
  }
  inline uint32_t read32() {
    uint32_t value = (bytes[offset] << 24) | (bytes[offset + 1] << 16) | (bytes[offset + 2] << 8) | bytes[offset + 3];
    offset += 4;
    return value;
  }
  inline uint32_t read_varlen() {
    uint32_t value = 0;

    // at most 4 bytes
    for (int i = 0; i < 4; i++) {
      uint8_t byte = read();
      value = (value << 7) | (byte & 0x7F);
      if ((byte & 0x80) == 0) {
        break;
      }
    }

    return value;
  }

  inline uint8_t operator[](size_t index) const {
    return bytes[index];
  }
  inline uint8_t &operator[](size_t index) {
    return bytes[index];
  }

  inline bool eof() const {
    return offset >= bytes.size();
  }

  inline size_t remaining() const {
    return eof() ? 0 : bytes.size() - offset;
  }
};

uint64_t validate_chunk_type(FileContext &ctx, const char *expected) {
  if (ctx.remaining() < 4) {
    std::cerr << "Not enough bytes remaining to read chunk type" << std::endl;
    return false;
  }

  char chunk_type[4];
  ctx.read(chunk_type, 4);

  if (std::strcmp(chunk_type, expected) != 0) {
    std::cerr << "Invalid chunk type: expected " << expected << ", got " << chunk_type << std::endl;
    return false;
  }

  return true;
}

// <Header Chunk 14> = <chunk type 4 = MThd><length 4 = 6><format 2 <= 2><ntrks 2><division 2>
bool parse_header_chunk(FileContext &ctx) {
  if (!validate_chunk_type(ctx, "MThd")) {
    return false;
  }

  if (ctx.remaining() < 10) {
    std::cerr << "Not enough bytes remaining to read header chunk" << std::endl;
    return false;
  }
  uint32_t length = ctx.read32();
  if (length != 6) {
    std::cerr << "Invalid header chunk length: expected 6, got " << length << std::endl;
    return false;
  }
  ctx.format = ctx.read16();
  if (ctx.format > 2) {
    std::cerr << "Invalid MIDI format: expected 0, 1, or 2, got " << ctx.format << std::endl;
    return false;
  }
  ctx.ntrks = ctx.read16();
  ctx.division = ctx.read16();

  ctx.handler->on_header(ctx.format, ctx.ntrks, ctx.division);

  return true;
}

// control change messages
bool parse_control_change_message(FileContext &ctx, uint8_t channel, uint8_t controller, uint8_t value) {
  // handle channel mode messages (controller 122-127)
  if (controller == 122) { // local control on/off
    ctx.handler->set_local_control(ctx.track_index, ctx.track_tick, (bool)value);
    return true;
  }
  if (controller >= 123 && controller <= 127) {
    switch (controller) {
      case 124: ctx.handler->set_omni_mode(ctx.track_index, ctx.track_tick, channel, controller, false); break; // omni mode off
      case 125: ctx.handler->set_omni_mode(ctx.track_index, ctx.track_tick, channel, controller, true); break; // omni mode on
      case 126: { // mono mode on
        uint8_t channels = value == 0 ? 1 : value;
        ctx.handler->set_mono_mode(ctx.track_index, ctx.track_tick, channel, controller, true, channels);
        ctx.handler->set_omni_mode(ctx.track_index, ctx.track_tick, channel, controller, channels == 0); // omni mode on if channels is 0
      } break;
      case 127: ctx.handler->set_mono_mode(ctx.track_index, ctx.track_tick, channel, controller, false, 0); break; // poly mode on
    }
    ctx.handler->all_notes_off(ctx.track_index, ctx.track_tick); // all these messages trigger all notes off (controller 123 is just all notes off)
    return true;
  }

  // handle controller messages (controller 0-119)
  if (controller <= 0b00111111) {
    // 2 byte values with msb and lsb variants (bit 5)
    bool msb = (controller >> 5) ^ 1;
    uint8_t type = controller & 0b00011111;
    if (type >= 16 && type < 20) { // general controller 5-8 (2 byte)
      ctx.handler->set_general_purpose_controller(ctx.track_index, ctx.track_tick, channel, controller, value, true, type - 11);
    } else {
      switch (type) { // bits 0-4 are value type
        case 0: ctx.handler->set_bank_select(ctx.track_index, ctx.track_tick, channel, value, msb); break;
        case 1: ctx.handler->set_modulation_wheel(ctx.track_index, ctx.track_tick, channel, value, msb); break;
        case 2: ctx.handler->set_breath_control(ctx.track_index, ctx.track_tick, channel, value, msb); break;
        case 4: ctx.handler->set_foot_controller(ctx.track_index, ctx.track_tick, channel, value, msb); break;
        case 5: ctx.handler->set_portamento_time(ctx.track_index, ctx.track_tick, channel, value, msb); break;
        case 6: ctx.handler->set_data_entry(ctx.track_index, ctx.track_tick, channel, value, msb); break;
        case 7: ctx.handler->set_channel_volume(ctx.track_index, ctx.track_tick, channel, value, msb); break;
        case 8: ctx.handler->set_balance(ctx.track_index, ctx.track_tick, channel, value, msb); break;
        case 10: ctx.handler->set_pan(ctx.track_index, ctx.track_tick, channel, value, msb); break;
        case 11: ctx.handler->set_expression_controller(ctx.track_index, ctx.track_tick, channel, value, msb); break;
        case 12: case 13: ctx.handler->set_effect_control(ctx.track_index, ctx.track_tick, channel, value, msb, type - 12); break;
        default: ctx.handler->set_undefined_control(ctx.track_index, ctx.track_tick, channel, controller, value, msb); break;
      }
    }
  } else {
    // single byte values
    uint8_t type = controller & 0b00111111;
    if (type >= 6 && type < 16) { // sound controller 1-10 (1 byte)
      ctx.handler->set_sound_controller(ctx.track_index, ctx.track_tick, channel, controller, value, type - 5);
    } else if (type >= 16 && type < 20) { // general controller 5-8 (1 byte)
      ctx.handler->set_general_purpose_controller(ctx.track_index, ctx.track_tick, channel, controller, value, false, type - 11);
    } else if (type >= 27 && type < 32) { // effects 1-5 depth
      ctx.handler->set_effects_depth(ctx.track_index, ctx.track_tick, channel, controller, value, type - 26);
    } else {
      switch (type) {
        case 0b000000: ctx.handler->set_damper_pedal(ctx.track_index, ctx.track_tick, channel, value); break;
        case 0b000001: ctx.handler->set_portamento(ctx.track_index, ctx.track_tick, channel, value); break;
        case 0b000010: ctx.handler->set_sostenuto(ctx.track_index, ctx.track_tick, channel, value); break;
        case 0b000011: ctx.handler->set_soft_pedal(ctx.track_index, ctx.track_tick, channel, value); break;
        case 0b000100: ctx.handler->set_legato_footswitch(ctx.track_index, ctx.track_tick, channel, value); break;
        case 0b000101: ctx.handler->set_hold_2(ctx.track_index, ctx.track_tick, channel, value); break;
        case 0b010100: ctx.handler->set_portamento_control(ctx.track_index, ctx.track_tick, channel, value); break;
        case 0b100000: ctx.handler->change_data_entry(ctx.track_index, ctx.track_tick, channel, 1); break;
        case 0b100001: ctx.handler->change_data_entry(ctx.track_index, ctx.track_tick, channel, -1); break;
        case 0b100010: ctx.handler->set_non_registered_parameter_number(ctx.track_index, ctx.track_tick, channel, value, false); break;
        case 0b100011: ctx.handler->set_non_registered_parameter_number(ctx.track_index, ctx.track_tick, channel, value, true); break;
        case 0b100100: ctx.handler->set_registered_parameter_number(ctx.track_index, ctx.track_tick, channel, value, false); break;
        case 0b100101: ctx.handler->set_registered_parameter_number(ctx.track_index, ctx.track_tick, channel, value, true); break;
        default: ctx.handler->set_undefined_control(ctx.track_index, ctx.track_tick, channel, controller, value, false); break;
      }
    }
  }
  return true;
}

// <MTrk event> = <delta-time varlen><event>
// <event (MIDI message)> = <MIDI event> | <SysEx event> | <Meta event>
// first byte of event signifies the type
// status byte has bit 7 set, data bytes have bit 7 clear
//   sysex event: 0xF0 or 0xF7
//   meta event: 0xFF
//   midi event: anything else under 0x80-0xFF

// midi events: a MIDI message

// sysex events: events to encode data intended for hardware devices
// <sysex event> = (0xF0 | 0xF7) <length varlen> <message data>
// single-packet sysex event: F0 message that ends in F7
// multi-packet sysex event: F0 message followed by F7 messages with last one ending in F7 (F0 included in data)
// escape sequence: F7 message that is not preceded by F0 message (F7 omitted from data), used for truly arbitrary message (no F0 - F7 framing)

// meta events: events to encode data intended for software sequencers
// <meta event> = 0xFF <type 1 < 128> <length varlen> <data bytes>

// MIDI Messages
// type depends on status byte (N = channel number 0-15)
//   CHANNEL MESSAGES
//   0x8N: note off, 2 data bytes (note, velocity aka: how fast key released)
//   0x9N: note on, 2 data bytes (note, velocity aka: how fast key pressed)
//   0xAN: polyphonic key pressure, 2 data bytes (note, pressure)
//   0xBN: control change, 2 data bytes (controller number, value)
//         channel mode changes use certain controller numbers to change channel behavior
//   0xCN: program change, 1 data byte (program number aka: instrument type)
//   0xDN: channel pressure, 1 data byte (pressure aka: how hard key held down)
//   0xEN: pitch wheel change, 2 data bytes (low and high bits for pitch change)
//   SYSTEM COMMON MESSAGES
//   0xF0: start of system exclusive
//   0xF1: undefined
//   0xF2: song position pointer, 2 data bytes (low and high bits for position)
//   0xF3: song select, 1 data byte (song number)
//   0xF4: undefined
//   0xF5: undefined
//   0xF6: tune request, 0 data bytes
//   0xF7: end of system exclusive
//   SYSTEM REALTIME MESSAGES (IGNORED BY SEQUENCERS)
//   0xF8 to 0xFF: 0 data bytes

// Meta Events
//   0xFF 0x00 0x02: Sequence Number of a track
//                   if exists, must be at begginning of track
//   0xFF 0x01 len text: Generic Text Event
//   0xFF 0x02 len text: Copyright Notice
//   0xFF 0x03 len text: Sequence/Track Name
//   0xFF 0x04 len text: Instrument Name
//   0xFF 0x05 len text: Lyric
//   0xFF 0x06 len text: Marker
//   0xFF 0x07 len text: Cue Point
//   0xFF 0x20 0x01 cc: MIDI Channel Prefix
//                      which channel sysex messages/meta events refer to
//                      otherwise channel of the last channel message will be used
//   0xFF 0x2F 0x00: End of Track (required)
//   0xFF 0x51 0x03 tt tt tt: Set Tempo
//                            microseconds per quarter note
//                            default 500000 = 120 bpm
//   0xFF 0x54 0x05 hr mn sc fr ff: SMPTE Offset
//                            if exists, must be at begginning of track
//                            specifies when the track starts in relation to the start of the song
//                            for format 1, ignored in all tracks other than the first (since the rest are just sequential)
//                            smpte format (fps) + hour, minute, second, frame, fractional frame
//                            hr is 8 bits: 0, smpte format (2 bits), hour (5 bits)
//                            smpte format: 0=24 fps, 1=25 fps, 2=29.97 fps (30 drop), 3=30 fps
//   0xFF 0x58 0x04 nn dd cc bb: Time Signature
//                            n = numerator, d = denominator (pow of 2)
//                            c = number of MIDI clocks per metronome click
//                            b = number of 1/32 notes per 24 MIDI clocks
//   0xFF 0x59 0x02 sf mi: Key Signature
//                            sf = number of sharps (positive) or flats (negative)
//                            mi = major (0) or minor (1)
//   0xFF 0x7F len data: Sequencer-Specific Meta Event (need to check spec of each sequencer to know what this means)

// Running Status
// if no status byte provided, status byte of last MIDI message is used
// running status cannot occur after a sysex or meta event

bool parse_midi_message(FileContext &ctx, const std::vector<uint8_t> &midi_msg) {
  uint8_t status = midi_msg[0];
  if (status < 0x80) {
    std::cerr << "Invalid MIDI message: status byte must be >= 0x80, got " << std::hex << (int)status << std::dec << std::endl;
    return false;
  }

  if (status < 0xF0) {
    // channel messages
    uint8_t channel = status & 0x0F;
    uint8_t type = (status >> 4);
    switch (type) {
      case 0x8: // note off
        ctx.handler->on_note_off(ctx.track_index, ctx.track_tick, channel, midi_msg[1], midi_msg[2]);
        break;
      case 0x9: // note on
        ctx.handler->on_note_on(ctx.track_index, ctx.track_tick, channel, midi_msg[1], midi_msg[2]);
        break;
      case 0xA: // polyphonic key pressure
        ctx.handler->on_polyphonic_key_pressure(ctx.track_index, ctx.track_tick, channel, midi_msg[1], midi_msg[2]);
        break;
      case 0xB: { // control change
        uint8_t controller = midi_msg[1];
        uint8_t value = midi_msg[2];
        if (!parse_control_change_message(ctx, channel, controller, value)) return false;
      } break;
      case 0xC: // program change
        ctx.handler->on_program_change(ctx.track_index, ctx.track_tick, channel, midi_msg[1]);
        break;
      case 0xD: // channel pressure
        ctx.handler->on_channel_pressure(ctx.track_index, ctx.track_tick, channel, midi_msg[1]);
        break;
      case 0xE: // pitch wheel change
        ctx.handler->on_pitch_wheel_change(ctx.track_index, ctx.track_tick, channel, ((uint16_t)midi_msg[2] << 7) | (uint16_t)midi_msg[1]);
        break;
      default:
        std::cerr << "Unknown MIDI status: " << std::hex << (int)status << std::dec << std::endl;
        return false;
    }
  } else if (status < 0xF8) {
    // system common message
    uint8_t type = status & 0x0F;
    switch (type) {
      case 0x0: // sysex start
        ctx.handler->on_sysex(ctx.track_index, ctx.track_tick, midi_msg);
        break;
      case 0x1: // undefined
        ctx.handler->on_undefined(ctx.track_index, ctx.track_tick, midi_msg);
        break;
      case 0x2: { // song position pointer
        uint16_t midi_beats = ((uint16_t)midi_msg[2] << 7) | (uint16_t)midi_msg[1];
        ctx.handler->on_song_position_pointer(ctx.track_index, ctx.track_tick, midi_beats);
      } break;
      case 0x3: { // song select
        uint8_t song_number = midi_msg[1];
        ctx.handler->on_song_select(ctx.track_index, ctx.track_tick, song_number);
      } break;
      case 0x4: // undefined
        ctx.handler->on_undefined(ctx.track_index, ctx.track_tick, midi_msg);
        break;
      case 0x5: // undefined
        ctx.handler->on_undefined(ctx.track_index, ctx.track_tick, midi_msg);
        break;
      case 0x6: // tune request
        ctx.handler->on_tune_request(ctx.track_index, ctx.track_tick);
        break;
      case 0x7: // sysex end
        ctx.handler->on_sysex(ctx.track_index, ctx.track_tick, midi_msg);
        break;
      default:
        std::cerr << "Unknown MIDI system common message: " << std::hex << (int)status << std::dec << std::endl;
        return false;
    }
  } else {
    // system realtime message
    ctx.handler->on_realtime_message(ctx.track_index, ctx.track_tick, status);
  }

  return true;
}

void parse_meta_event(FileContext &ctx, uint8_t type, const std::vector<uint8_t> &data) {
  switch (type) {
    case 0x00: ctx.handler->set_sequence_number(ctx.track_index, ctx.track_tick, (uint16_t)data[0] << 8 | (uint16_t)data[1]); break;
    case 0x01: ctx.handler->set_generic_text_event(ctx.track_index, ctx.track_tick, std::string(data.begin(), data.end())); break;
    case 0x02: ctx.handler->set_copyright_notice(ctx.track_index, ctx.track_tick, std::string(data.begin(), data.end())); break;
    case 0x03: ctx.handler->set_sequence_or_track_name(ctx.track_index, ctx.track_tick, std::string(data.begin(), data.end())); break;
    case 0x04: ctx.handler->set_instrument_name(ctx.track_index, ctx.track_tick, std::string(data.begin(), data.end())); break;
    case 0x05: ctx.handler->on_lyric(ctx.track_index, ctx.track_tick, std::string(data.begin(), data.end())); break;
    case 0x06: ctx.handler->on_marker(ctx.track_index, ctx.track_tick, std::string(data.begin(), data.end())); break;
    case 0x07: ctx.handler->on_cue_point(ctx.track_index, ctx.track_tick, std::string(data.begin(), data.end())); break;
    case 0x20: ctx.handler->set_midi_channel_prefix(ctx.track_index, ctx.track_tick, data[0]); break;
    case 0x2F: ctx.handler->on_end_of_track(ctx.track_index, ctx.track_tick); break;
    case 0x51: ctx.handler->on_tempo_change(ctx.track_index, ctx.track_tick, (uint32_t)data[0] << 16 | (uint32_t)data[1] << 8 | (uint32_t)data[2]);break;
    case 0x54: if (ctx.track_tick) std::cout << "Warning: SMPTE offset event not at tick 0 (tick=" << ctx.track_tick << ")" << std::endl; ctx.handler->on_smpte_offset(ctx.track_index, ctx.track_tick, data[0] >> 5, data[0] & 0b11111, data[1], data[2], data[3], data[4]); break;
    case 0x58: ctx.handler->on_time_signature(ctx.track_index, ctx.track_tick, data[0], data[1], data[2], data[3]); break;
    case 0x59: ctx.handler->on_key_signature(ctx.track_index, ctx.track_tick, data[0], data[1]); break;
    case 0x7F: ctx.handler->on_sequencer_specific_meta_event(ctx.track_index, ctx.track_tick, data); break;
    default: ctx.handler->on_undefined_meta_event(ctx.track_index, ctx.track_tick, type, data); break;
  }
}

bool parse_track_event(FileContext &ctx) {
  uint32_t delta_time = ctx.read_varlen();
  ctx.track_tick += delta_time;
  
  uint8_t status = ctx.read();
  // meta events
  if (status == 0xFF) {
    ctx.running_status = 0;
    uint8_t type = ctx.read();
    uint32_t len = ctx.read_varlen();
    static std::vector<uint8_t> meta_data;
    meta_data.resize(len);
    ctx.read(meta_data.data(), len);
    parse_meta_event(ctx, type, meta_data);
    return true;
  }

  // midi message events
  static std::vector<uint8_t> midi_msg;
  if (status == 0xF0 || status == 0xF7) {
    // sysex event
    ctx.running_status = 0;
    uint32_t len = ctx.read_varlen();
    uint32_t off = status == 0xF0;
    midi_msg.resize(off + len, 0xF0);
    ctx.read(midi_msg.data() + off, len);
  } else {
    // midi event
    if (status < 0x80) {
      // running status: use last status byte
      status = ctx.running_status;
      ctx.offset--;
    } else {
      ctx.running_status = status;
    }
    
    if (status >= 0x80 && status < 0xF0) {
      // channel messsages: 0b10000000 to 0b11101111
      // 2 data bits aside from control/program change (0b10110000 to 0b11001111)
      uint32_t data_len = 1 + (status < 0xC0 || status >= 0xE0);
      midi_msg.resize(data_len + 1);
      midi_msg[0] = status;
      ctx.read(midi_msg.data() + 1, data_len);
    } else if (status >= 0xF0) {
      // system common messages: 0b11110000 to 0b11111111
      // system realtime messages: 0b11111000 to 0b11111111
      // don't need to handle 0xF0 and 0xF7 since handled by sysex event
      // song pos pointer (0xF2) has 2 data bytes, and song select (0xF3) has 1 data byte
      // rest have 0 data bytes (including realtime)
      uint32_t data_len = status == 0xF2 ? 2 : status == 0xF3 ? 1 : 0;
      midi_msg.resize(data_len + 1);
      midi_msg[0] = status;
      ctx.read(midi_msg.data() + 1, data_len);
    } else {
      std::cerr << "Unknown MIDI event status byte: " << std::hex << (int)status << std::dec << std::endl;
      return false;
    }
  }
  
  // unknown status byte
  if (midi_msg.empty()) {
    std::cerr << "Invalid MIDI event status byte: " << std::hex << (int)status << std::dec << std::endl;
    return false;
  }

  parse_midi_message(ctx, midi_msg);
  return true;
}

// <Track Chunk 8+> = <chunk type 4 = MTrk><length 4><MTrk event>+
bool parse_track_chunk(FileContext &ctx) {
  std::cout << "Parsing track chunk at offset " << ctx.offset << "/0x" << std::hex << ctx.offset << std::dec << std::endl;
  if (!validate_chunk_type(ctx, "MTrk")) {
    return false;
  }

  if (ctx.remaining() < 4) {
    std::cerr << "Not enough bytes remaining to read track chunk length" << std::endl;
    return false;
  }
  uint32_t chunk_len = ctx.read32();
  if (ctx.remaining() < chunk_len) {
    std::cerr << "Not enough bytes remaining to read track chunk data" << std::endl;
    return false;
  }
  uint32_t end = ctx.offset + chunk_len;

  ctx.track_tick = 0;
  ctx.running_status = 0;
  ctx.handler->on_start_of_track(ctx.track_index);
  
  while (ctx.offset < end) {
    if (!parse_track_event(ctx)) {
      std::cerr << "Failed to parse track event at offset " << ctx.offset << "/0x" << std::hex << ctx.offset << std::dec << std::endl;
      return false;
    }
  }
  if (ctx.offset > end) {
    std::cerr << "Track chunk overran length by " << (ctx.offset - end) << " bytes" << std::endl;
    return false;
  }

  return true;
}

bool parse_midi(FileContext &ctx) {
  parse_header_chunk(ctx);
  for (ctx.track_index = 0; ctx.track_index < ctx.ntrks; ctx.track_index++) {
    if (!parse_track_chunk(ctx)) {
      std::cerr << "byte " << (ctx.offset + 1) << "/" << ctx.bytes.size() << std::endl;
      return false;
    }
  }
  if (!ctx.eof()) {
    std::cerr << "Warning: bytes remaining after parsing MIDI file: " << ctx.remaining() << std::endl;
  }
  ctx.handler->on_end_of_file();
  return true;
}

bool parse_midi_file(const std::string& filepath, std::shared_ptr<Handler> handler) {
  // read bytes
  std::ifstream file(filepath, std::ios::binary | std::ios::ate);
  if (file.fail()) {
    std::cerr << "Failed to open file: " << filepath << std::endl;
    return false;
  }

  // since MIDI files typically small, load entire file into memory rather than using mmap or streaming
  std::streamsize size = file.tellg();
  if (size < 0) {
    std::cerr << "Got invalid file size for " << filepath << ": " << size << std::endl;
    return false;
  }
  file.seekg(0, std::ios::beg);
  FileContext ctx(size, handler);
  if (!file.read(reinterpret_cast<char *>(ctx.bytes.data()), size)) {
    std::cerr << "Failed to read file: " << filepath << std::endl;
    return false;
  }

  parse_midi(ctx);

  return true;
}

// trigger handler events in timing order rather than in byte order
struct SequentialHandler : public Handler {
  // template magic to store a function and its arguments to call later
  struct EventCallInterface {
    virtual ~EventCallInterface() = default;
    virtual void call() = 0;
  };
  template<typename Func, typename... Args>
  struct EventCall : public EventCallInterface {
    Func func;
    std::tuple<Args...> args;
    EventCall(Func f, Args... args) : func(f), args(std::make_tuple(std::forward<Args>(args)...)) {}
    void call() override {
      std::apply(func, args);
    }
  };
  struct TimedEventCall {
    uint64_t tick;
    std::unique_ptr<EventCallInterface> event;
    inline bool operator<(const TimedEventCall &other) const {
      return tick < other.tick;
    }
    TimedEventCall(uint64_t t = 0, std::unique_ptr<EventCallInterface> e = nullptr) : tick(t), event(std::move(e)) {}
  };

  std::shared_ptr<Handler> base_handler; // the handler to call events on
  bool seq_track = false; // if track is sequential (0, 1 with 1 track, 2), then no need to sort events by tick, just call immediately
  std::vector<std::queue<TimedEventCall>> track_events; // per-track events
  SequentialHandler(std::shared_ptr<Handler> h) : base_handler(h) {}

  template<typename Func, typename... Args>
  void add_event(uint16_t track_index, uint64_t track_tick, Func func, Args... args) {
    if (seq_track) {
      std::invoke(func, std::forward<Args>(args)...);
      return;
    }

    track_events[track_index].emplace(track_tick, std::make_unique<EventCall<Func, Args...>>(func, std::forward<Args>(args)...));
  }

  void on_header(uint16_t format, uint16_t ntrks, uint16_t division) override {
    seq_track = (format == 0 || (format == 1 && ntrks == 1) || format == 2);
    track_events = std::vector<std::queue<TimedEventCall>>(ntrks);
    base_handler->on_header(format, ntrks, division);
  }

  void on_start_of_track(uint16_t track_index) override {
    add_event(track_index, 0, &Handler::on_start_of_track, base_handler, track_index);
  }

  void on_end_of_track(uint16_t track_index, uint64_t track_tick) override {
    add_event(track_index, track_tick, &Handler::on_end_of_track, base_handler, track_index, track_tick);
  }

  void on_end_of_file() override {
    if (!seq_track) {
      // playback events in order of tick
      // use priority queue to figure out next track
      std::priority_queue<std::pair<uint64_t, size_t>, std::vector<std::pair<uint64_t, size_t>>, std::greater<std::pair<uint64_t, size_t>>> pq; // (tick, track index)
      for (size_t i = 0; i < track_events.size(); i++) {
        // guaranteed to have at least one event in each track (start of track)
        pq.emplace(track_events[i].front().tick, i);
      }
      while (!pq.empty()) {
        auto [tick, track_index] = pq.top();
        pq.pop();
        auto &track_queue = track_events[track_index];
        auto &event_call = track_queue.front();
        event_call.event->call();
        track_queue.pop();
        if (!track_queue.empty()) {
          pq.emplace(track_queue.front().tick, track_index);
        }
      }
    }
    base_handler->on_end_of_file();
  }

  void on_undefined(uint16_t track_index, uint64_t track_tick, const std::vector<uint8_t> &data) override {
    add_event(track_index, track_tick, &Handler::on_undefined, base_handler, track_index, track_tick, data);
  }

  void on_note_off(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t note, uint8_t velocity) override {
    add_event(track_index, track_tick, &Handler::on_note_off, base_handler, track_index, track_tick, channel, note, velocity);
  }

  void on_note_on(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t note, uint8_t velocity) override {
    add_event(track_index, track_tick, &Handler::on_note_on, base_handler, track_index, track_tick, channel, note, velocity);
  }

  void on_polyphonic_key_pressure(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t note, uint8_t pressure) override {
    add_event(track_index, track_tick, &Handler::on_polyphonic_key_pressure, base_handler, track_index, track_tick, channel, note, pressure);
  }

  void on_program_change(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t program) override {
    add_event(track_index, track_tick, &Handler::on_program_change, base_handler, track_index, track_tick, channel, program);
  }

  void on_channel_pressure(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t pressure) override {
    add_event(track_index, track_tick, &Handler::on_channel_pressure, base_handler, track_index, track_tick, channel, pressure);
  }

  void on_pitch_wheel_change(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint16_t value) override  {
    add_event(track_index, track_tick, &Handler::on_pitch_wheel_change, base_handler, track_index, track_tick, channel, value);
  }

  void on_sysex(uint16_t track_index, uint64_t track_tick, const std::vector<uint8_t> &data) override {
    add_event(track_index, track_tick, &Handler::on_sysex, base_handler, track_index, track_tick, data);
  }

  void on_song_position_pointer(uint16_t track_index, uint64_t track_tick, uint16_t midi_beats) override {
    add_event(track_index, track_tick, &Handler::on_song_position_pointer, base_handler, track_index, track_tick, midi_beats);
  }

  void on_song_select(uint16_t track_index, uint64_t track_tick, uint8_t song_number) override {
    add_event(track_index, track_tick, &Handler::on_song_select, base_handler, track_index, track_tick, song_number);
  }

  void on_tune_request(uint16_t track_index, uint64_t track_tick) override {
    add_event(track_index, track_tick, &Handler::on_tune_request, base_handler, track_index, track_tick);
  }

  void on_realtime_message(uint16_t track_index, uint64_t track_tick, uint8_t status) override {
    add_event(track_index, track_tick, &Handler::on_realtime_message, base_handler, track_index, track_tick, status);
  }

  void set_undefined_control(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t controller, uint8_t value, bool msb) override {
    add_event(track_index, track_tick, &Handler::set_undefined_control, base_handler, track_index, track_tick, channel, controller, value, msb);
  }

  void set_local_control(uint16_t track_index, uint64_t track_tick, bool on) override {
    add_event(track_index, track_tick, &Handler::set_local_control, base_handler, track_index, track_tick, on);
  }

  void set_omni_mode(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t controller, bool on) override {
    add_event(track_index, track_tick, &Handler::set_omni_mode, base_handler, track_index, track_tick, channel, controller, on);
  }

  void set_mono_mode(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t controller, bool on, uint8_t channels) override {
    add_event(track_index, track_tick, &Handler::set_mono_mode, base_handler, track_index, track_tick, channel, controller, on, channels);
  }

  void all_notes_off(uint16_t track_index, uint64_t track_tick) override {
    add_event(track_index, track_tick, &Handler::all_notes_off, base_handler, track_index, track_tick);
  }

  void set_general_purpose_controller(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t controller, uint8_t value, bool msb, uint8_t index) override {
    add_event(track_index, track_tick, &Handler::set_general_purpose_controller, base_handler, track_index, track_tick, channel, controller, value, msb, index);
  }

  void set_bank_select(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t value, bool msb) override {
    add_event(track_index, track_tick, &Handler::set_bank_select, base_handler, track_index, track_tick, channel, value, msb);
  }

  void set_modulation_wheel(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t value, bool msb) override {
    add_event(track_index, track_tick, &Handler::set_modulation_wheel, base_handler, track_index, track_tick, channel, value, msb);
  }

  void set_breath_control(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t value, bool msb) override {
    add_event(track_index, track_tick, &Handler::set_breath_control, base_handler, track_index, track_tick, channel, value, msb);
  }

  void set_foot_controller(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t value, bool msb) override {
    add_event(track_index, track_tick, &Handler::set_foot_controller, base_handler, track_index, track_tick, channel, value, msb);
  }

  void set_portamento_time(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t value, bool msb) override {
    add_event(track_index, track_tick, &Handler::set_portamento_time, base_handler, track_index, track_tick, channel, value, msb);
  }

  void set_data_entry(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t value, bool msb) override {
    add_event(track_index, track_tick, &Handler::set_data_entry, base_handler, track_index, track_tick, channel, value, msb);
  }

  void set_channel_volume(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t value, bool msb) override {
    add_event(track_index, track_tick, &Handler::set_channel_volume, base_handler, track_index, track_tick, channel, value, msb);
  }

  void set_balance(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t value, bool msb) override {
    add_event(track_index, track_tick, &Handler::set_balance, base_handler, track_index, track_tick, channel, value, msb);
  }

  void set_pan(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t value, bool msb) override {
    add_event(track_index, track_tick, &Handler::set_pan, base_handler, track_index, track_tick, channel, value, msb);
  }

  void set_expression_controller(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t value, bool msb) override {
    add_event(track_index, track_tick, &Handler::set_expression_controller, base_handler, track_index, track_tick, channel, value, msb);
  }

  void set_effect_control(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t value, bool msb, uint8_t index) override {
    add_event(track_index, track_tick, &Handler::set_effect_control, base_handler, track_index, track_tick, channel, value, msb, index);
  }

  void set_sound_controller(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t controller, uint8_t value, uint8_t index) override {
    add_event(track_index, track_tick, &Handler::set_sound_controller, base_handler, track_index, track_tick, channel, controller, value, index);
  }

  void set_effects_depth(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t controller, uint8_t value, uint8_t index) override {
    add_event(track_index, track_tick, &Handler::set_effects_depth, base_handler, track_index, track_tick, channel, controller, value, index);
  }

  void set_damper_pedal(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t value) override {
    add_event(track_index, track_tick, &Handler::set_damper_pedal, base_handler, track_index, track_tick, channel, value);
  }

  void set_portamento(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t value) override {
    add_event(track_index, track_tick, &Handler::set_portamento, base_handler, track_index, track_tick, channel, value);
  }

  void set_sostenuto(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t value) override {
    add_event(track_index, track_tick, &Handler::set_sostenuto, base_handler, track_index, track_tick, channel, value);
  }

  void set_soft_pedal(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t value) override {
    add_event(track_index, track_tick, &Handler::set_soft_pedal, base_handler, track_index, track_tick, channel, value);
  }

  void set_legato_footswitch(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t value) override {
    add_event(track_index, track_tick, &Handler::set_legato_footswitch, base_handler, track_index, track_tick, channel, value);
  }

  void set_hold_2(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t value) override {
    add_event(track_index, track_tick, &Handler::set_hold_2, base_handler, track_index, track_tick, channel, value);
  }

  void set_portamento_control(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t value) override {
    add_event(track_index, track_tick, &Handler::set_portamento_control, base_handler, track_index, track_tick, channel, value);
  }

  void change_data_entry(uint16_t track_index, uint64_t track_tick, uint8_t channel, int8_t delta) override {
    add_event(track_index, track_tick, &Handler::change_data_entry, base_handler, track_index, track_tick, channel, delta);
  }

  void set_non_registered_parameter_number(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t value, bool msb) override {
    add_event(track_index, track_tick, &Handler::set_non_registered_parameter_number, base_handler, track_index, track_tick, channel, value, msb);
  }

  void set_registered_parameter_number(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t value, bool msb) override {
    add_event(track_index, track_tick, &Handler::set_registered_parameter_number, base_handler, track_index, track_tick, channel, value, msb);
  }

  void set_sequence_number(uint16_t track_index, uint64_t track_tick, uint16_t sequence_number) override {
    add_event(track_index, track_tick, &Handler::set_sequence_number, base_handler, track_index, track_tick, sequence_number);
  }

  void set_generic_text_event(uint16_t track_index, uint64_t track_tick, const std::string &text) override {
    add_event(track_index, track_tick, &Handler::set_generic_text_event, base_handler, track_index, track_tick, text);
  }

  void set_copyright_notice(uint16_t track_index, uint64_t track_tick, const std::string &text) override {
    add_event(track_index, track_tick, &Handler::set_copyright_notice, base_handler, track_index, track_tick, text);
  }

  void set_sequence_or_track_name(uint16_t track_index, uint64_t track_tick, const std::string &text) override {
    add_event(track_index, track_tick, &Handler::set_sequence_or_track_name, base_handler, track_index, track_tick, text);
  }

  void set_instrument_name(uint16_t track_index, uint64_t track_tick, const std::string &text) override {
    add_event(track_index, track_tick, &Handler::set_instrument_name, base_handler, track_index, track_tick, text);
  }

  void on_lyric(uint16_t track_index, uint64_t track_tick, const std::string &text) override {
    add_event(track_index, track_tick, &Handler::on_lyric, base_handler, track_index, track_tick, text);
  }

  void on_marker(uint16_t track_index, uint64_t track_tick, const std::string &text) override {
    add_event(track_index, track_tick, &Handler::on_marker, base_handler, track_index, track_tick, text);
  }

  void on_cue_point(uint16_t track_index, uint64_t track_tick, const std::string &text) override {
    add_event(track_index, track_tick, &Handler::on_cue_point, base_handler, track_index, track_tick, text);
  }

  void set_midi_channel_prefix(uint16_t track_index, uint64_t track_tick, uint8_t channel) override {
    add_event(track_index, track_tick, &Handler::set_midi_channel_prefix, base_handler, track_index, track_tick, channel);
  }

  void on_tempo_change(uint16_t track_index, uint64_t track_tick, uint32_t microseconds_per_quarter_note) override {
    add_event(track_index, track_tick, &Handler::on_tempo_change, base_handler, track_index, track_tick, microseconds_per_quarter_note);
  }

  void on_smpte_offset(uint16_t track_index, uint64_t track_tick, uint8_t smpte_format, uint8_t hour, uint8_t minute, uint8_t second, uint8_t frame, uint8_t fractional_frame) override {
    add_event(track_index, track_tick, &Handler::on_smpte_offset, base_handler, track_index, track_tick, smpte_format, hour, minute, second, frame, fractional_frame);
  }

  void on_time_signature(uint16_t track_index, uint64_t track_tick, uint8_t numerator, uint8_t denominator, uint8_t midi_clocks_per_click, uint8_t thirty_second_notes_per_24_midi_clocks) override {
    add_event(track_index, track_tick, &Handler::on_time_signature, base_handler, track_index, track_tick, numerator, denominator, midi_clocks_per_click, thirty_second_notes_per_24_midi_clocks);
  }

  void on_key_signature(uint16_t track_index, uint64_t track_tick, int8_t sharps_flats, bool minor) override {
    add_event(track_index, track_tick, &Handler::on_key_signature, base_handler, track_index, track_tick, sharps_flats, minor);
  }

  void on_sequencer_specific_meta_event(uint16_t track_index, uint64_t track_tick, const std::vector<uint8_t> &data) override {
    add_event(track_index, track_tick, &Handler::on_sequencer_specific_meta_event, base_handler, track_index, track_tick, data);
  }

  void on_undefined_meta_event(uint16_t track_index, uint64_t track_tick, uint8_t type, const std::vector<uint8_t> &data) override {
    add_event(track_index, track_tick, &Handler::on_undefined_meta_event, base_handler, track_index, track_tick, type, data);
  }
};

// ring buffer of size N
// N must be power of 2
template<auto V>
constexpr bool is_power_of_two = V && ((V & (V - 1)) == 0);
template <typename T, size_t N>
requires is_power_of_two<N>
class RingBuffer {
  std::array<T, N> buffer;
  alignas(64) std::atomic<size_t> head{0};
  alignas(64) std::atomic<size_t> tail{0};
public:
  bool push(const T& v) {
    size_t curr_head = head.load(std::memory_order_relaxed);
    size_t next_head = (curr_head + 1) & (N - 1);
    if (next_head == tail.load(std::memory_order_acquire)) {
      return false;
    }

    buffer[curr_head] = v;
    head.store(next_head, std::memory_order_release);
    return true;
  }

  T *peek() {
    size_t curr_tail = tail.load(std::memory_order_relaxed);
    if (curr_tail == head.load(std::memory_order_acquire)) {
      return nullptr;
    }

    return &buffer[curr_tail];
  }

  // assumes peek() was called and returned non-nullptr
  void pop() {
    size_t curr_tail = tail.load(std::memory_order_relaxed);
    tail.store((curr_tail + 1) & (N - 1), std::memory_order_release);
  }
};

// playback notes (assumes SequentialHandler is used to ensure events are in order)
struct PlaybackHandler : public Handler {
  const static uint32_t SAMPLE_RATE = 44100;
  constexpr static float TONE_ATTACK = 200.f / (float)SAMPLE_RATE; // volume attack per audio tick
  constexpr static float TONE_DECAY = 5.f / (float)SAMPLE_RATE; // volume decay per audio tick
  const static size_t N_TONES = 64;
  const static size_t UPDATE_BUFF_SIZE = 1024; // must be power of 2 for ring buffer

  struct ToneState {
    uint32_t note_id = 0; // 0 if inactive, 1 << 31 | (track_index << 12) | (channel << 8) | note if active
    float phase_incr = 0.f;
    float volume = 0.f;
    float lpf_mult = 0.1f; // higher = less filter, 1.0 = no filter

    // live only values
    float phase = 0.f;
    float smooth_volume = 0.f;
    float lpf_state = 0.f;
  };
  struct ChannelState {
    uint8_t pan_msb = 64;
    uint8_t pan_lsb = 0;
    uint8_t volume_msb = 100;
    uint8_t volume_lsb = 0;
    float volume_left = 0.f;
    float volume_right = 0.f;
    void update_volume() {
      float volume = (volume_msb << 7 | volume_lsb) / 16383.f;
      float pan = ((pan_msb << 7 | pan_lsb) / 16383.f); // 0 to 1
      volume_left = volume * cosf(pan * M_PI_2);
      volume_right = volume * sinf(pan * M_PI_2);
    }
  };
  struct ToneUpdate {
    size_t tone_index;
    ToneState state;
  };
  struct ChannelUpdate {
    size_t channel_index;
    ChannelState state;
  };
  union SynthUpdateData {
    ToneUpdate tone_update;
    ChannelUpdate channel_update;
    SynthUpdateData() : tone_update{} {}
  };
  struct SynthUpdate {
    bool is_tone_update;
    uint64_t tick; // abs audio tick to apply update at (at 48kHz, 1 tick = 1/48000 sec, so won't overflow for 2^64 ticks = ~12M years)
    SynthUpdateData data;
  };
  ToneState buff_tones[N_TONES] = {}; // active tone state at buffered time
  uint64_t tone_last_update_tick[N_TONES] = {}; // last audio tick that tone was updated at
  std::vector<ChannelState> buff_channel_states; // channel states at buffered time
  std::unordered_map<uint32_t, size_t> note_tone_map; // map from note id to tone index
  TimeTracker time_tracker;

  struct SynthState {
    uint64_t inactive_time = 0;
    uint64_t audio_tick = 0; // current absolute audio tick
    ToneState tones[N_TONES] = {}; // active tones
    std::vector<ChannelState> channel_states; // channel states
    RingBuffer<SynthUpdate, UPDATE_BUFF_SIZE> buff; // updates to apply at specific audio ticks
  };
  bool init = false;
  SynthState synth;
  ma_device_config device_config;
  ma_device device;

  static void data_callback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount) {
    SynthState *synth = static_cast<SynthState *>(pDevice->pUserData);
    float *out = static_cast<float *>(pOutput);
    SynthUpdate *next_update = synth->buff.peek();
    for (ma_uint32 i = 0; i < frameCount; ++i, ++synth->audio_tick) {
      while (next_update && synth->audio_tick >= next_update->tick) {
        if (next_update->is_tone_update) {
          ToneState &tone = synth->tones[next_update->data.tone_update.tone_index];
          ToneState &next_tone = next_update->data.tone_update.state;
          if (next_tone.note_id == 0 || next_tone.volume <= 0.f) {
            // off
            tone.note_id = 0;
            tone.volume = 0.f;
          } else {
            // on
            tone.note_id = next_tone.note_id;
            tone.volume = next_tone.volume;
            tone.phase_incr = next_tone.phase_incr;
            tone.lpf_mult = next_tone.lpf_mult;
          }
        } else {
          synth->channel_states[next_update->data.channel_update.channel_index] = next_update->data.channel_update.state;
        }
        synth->buff.pop();
        next_update = synth->buff.peek();
      }
      float mix_left = 0.0f;
      float mix_right = 0.0f;
      for (size_t j = 0; j < N_TONES; j++) {
        ToneState &tone = synth->tones[j];
        ChannelState &channel = synth->channel_states[(tone.note_id >> 8) & 0xfffff];

        // attack/decay smoothing (linear)
        tone.smooth_volume = tone.smooth_volume >= tone.volume ? std::max(tone.volume, tone.smooth_volume - TONE_DECAY) : std::min(tone.volume, tone.smooth_volume + TONE_ATTACK);
        
        // sawtooth wave
        float wave = (tone.phase / M_PI) - 1.0f;
        float sample = wave * tone.smooth_volume * 0.2f;

        // low-pass filter
        tone.lpf_state = tone.lpf_state + (sample - tone.lpf_state) * tone.lpf_mult;

        // mix left/right
        mix_left += tone.lpf_state * channel.volume_left;
        mix_right += tone.lpf_state * channel.volume_right;

        // advance phase
        tone.phase += tone.phase_incr;
        if (tone.phase > 2.f * M_PI) tone.phase -= 2.f * M_PI;

        // fade
        tone.lpf_mult *= 0.99995f; // high frequencies fade faster
        tone.volume *= 0.99999f; // volume fades out over time
      }
      float scale = 1.f / std::max({1.f, std::abs(mix_left), std::abs(mix_right)});
      out[i * 2] = mix_left * scale;
      out[i * 2 + 1] = mix_right * scale;
    }
    synth->inactive_time = next_update || std::any_of(std::begin(synth->tones), std::end(synth->tones), [](const ToneState &tone) { return tone.smooth_volume > 0.f; }) ? 0 : synth->inactive_time + frameCount;
    // std::cout << synth->audio_tick << " " << (next_update ? next_update->tick : 0) << std::endl;
  }

  void init_audio() {
    if (init) return;

    init = true;
    device_config = ma_device_config_init(ma_device_type_playback);
    device_config.playback.format = ma_format_f32;
    device_config.playback.channels = 2;
    device_config.sampleRate = SAMPLE_RATE;
    device_config.dataCallback = data_callback;
    device_config.pUserData = &synth;
    device_config.periods = 4;
    if (ma_device_init(NULL, &device_config, &device) != MA_SUCCESS) {
      std::cerr << "Failed to initialize playback device.\n";
      exit(1);
    }
    if (ma_device_start(&device) != MA_SUCCESS) {
      std::cerr << "Failed to start playback device.\n";
      ma_device_uninit(&device);
      exit(1);
    }
  }

  ~PlaybackHandler() {
    ma_device_uninit(&device);
  }

  void on_header(uint16_t format, uint16_t ntrks, uint16_t division) override {
    time_tracker.set_division(division);
    synth.channel_states.resize(16 * ntrks); // 16 channels per track
    buff_channel_states.resize(16 * ntrks);
    for (size_t i = 0; i < buff_channel_states.size(); i++) {
      buff_channel_states[i].update_volume();
      synth.channel_states[i] = buff_channel_states[i];
    }
  }

  void on_tempo_change(uint16_t track_index, uint64_t track_tick, uint32_t microseconds_per_quarter_note) override {
    time_tracker.add_tempo_change(track_tick, microseconds_per_quarter_note);
  }

  void on_start_of_track(uint16_t track_index) override {
    init_audio(); // do here so that audio thread starts after parsing done
  }

  inline uint32_t get_channel_index(uint16_t track_index, uint8_t channel) {
    return (track_index << 4) | channel;
  }

  inline uint32_t get_note_id(uint16_t track_index, uint8_t channel, uint8_t note) {
    return 1U << 31 | ((uint32_t)track_index << 12) | ((uint32_t)channel << 8) | (uint32_t)note;
  }

  inline float get_note_phase_incr(uint8_t note) {
    return 2.f * M_PI * 440.f * powf(2.f, (note - 69) / 12.f) / (float)SAMPLE_RATE;
  }

  inline float get_note_volume(uint8_t velocity) {
    return std::pow((float)velocity / 127.f, 2.f);
  }

  inline float get_note_lpf_mult(uint8_t velocity) {
    return 0.05f + 0.15f * std::pow((float)velocity / 127.f, 2.f);
  }

  void push_note_update(uint64_t track_tick, ToneState state) {
    size_t tone_index;
    auto it = note_tone_map.find(state.note_id);
    if (it != note_tone_map.end()) {
      tone_index = it->second;
    } else if (state.volume > 0.f) {
      // find an inactive tone to use, prioritize older ones to allow for volume to fade out
      tone_index = N_TONES;
      uint64_t best_tick = 0;
      for (size_t i = 0; i < N_TONES; i++) {
        if (buff_tones[i].volume <= 0.f && (tone_index == N_TONES || tone_last_update_tick[i] < best_tick)) {
          tone_index = i;
          best_tick = tone_last_update_tick[i];
        }
      }
      if (tone_index == N_TONES) {
        std::cout << "Warning: Too many active tones, note dropped" << std::endl;
        return;
      }
      note_tone_map[state.note_id] = tone_index;
    } else {
      return; // note already off, nothing to do
    }
    
    if (state.volume <= 0.f) {
      note_tone_map.erase(state.note_id);
      state.note_id = 0; // mark as inactive
    }

    buff_tones[tone_index] = state;
    tone_last_update_tick[tone_index] = track_tick;
    uint64_t audio_tick = time_tracker.get_absolute_time(track_tick) * SAMPLE_RATE;
    SynthUpdate synth_update;
    synth_update.tick = audio_tick;
    synth_update.is_tone_update = true;
    synth_update.data.tone_update = {tone_index, state};
    while (!synth.buff.push(synth_update)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }

  void push_channel_update(uint64_t track_tick, uint16_t channel_index, ChannelState state) {
    uint64_t audio_tick = time_tracker.get_absolute_time(track_tick) * SAMPLE_RATE;
    SynthUpdate synth_update;
    synth_update.tick = audio_tick;
    synth_update.is_tone_update = false;
    synth_update.data.channel_update = {channel_index, state};
    while (!synth.buff.push(synth_update)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }

  void on_note_on(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t note, uint8_t velocity) override {
    push_note_update(track_tick, ToneState{.note_id = get_note_id(track_index, channel, note), .phase_incr = get_note_phase_incr(note), .volume = get_note_volume(velocity), .lpf_mult = get_note_lpf_mult(velocity)});
  }

  void on_note_off(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t note, uint8_t velocity) override {
    push_note_update(track_tick, ToneState{.note_id = get_note_id(track_index, channel, note), .phase_incr = 0.f, .volume = 0.f, .lpf_mult = 0.f});
  }

  void all_notes_off(uint16_t track_index, uint64_t track_tick) override {
    for (auto [note_id, tone_index] : note_tone_map) {
      push_note_update(track_tick, ToneState{.note_id = note_id, .phase_incr = 0.f, .volume = 0.f, .lpf_mult = 0.f});
    }
  }

  void set_pan(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t value, bool msb) override {
    ChannelState &state = buff_channel_states[(track_index << 4) | channel];
    (msb ? state.pan_msb : state.pan_lsb) = value;
    state.update_volume();
    push_channel_update(track_tick, get_channel_index(track_index, channel), state);
  }

  void set_channel_volume(uint16_t track_index, uint64_t track_tick, uint8_t channel, uint8_t value, bool msb) override {
    ChannelState &state = buff_channel_states[(track_index << 4) | channel];
    (msb ? state.volume_msb : state.volume_lsb) = value;
    state.update_volume();
    push_channel_update(track_tick, get_channel_index(track_index, channel), state);
  }

  void on_end_of_file() override {
    while (synth.inactive_time < SAMPLE_RATE * 2) { // wait for 2 seconds of inactivity before exiting
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }
};

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <midi_file>" << std::endl;
    return 1;
  }

  std::shared_ptr<Handler> handler = std::make_shared<SequentialHandler>(std::make_shared<PlaybackHandler>());
  const std::string filepath = argv[1];
  if (!parse_midi_file(filepath, handler)) {
    std::cerr << "Failed to parse MIDI file: " << filepath << std::endl;
    return 1;
  }

  return 0;
}
