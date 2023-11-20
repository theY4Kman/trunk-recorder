#ifndef CALL_IMPL_H
#define CALL_IMPL_H

#include "./global_structs.h"
#include "gr_blocks/decoder_wrapper.h"
#include <boost/log/trivial.hpp>
#include <string>
#include <sys/time.h>
#include <vector>

class Recorder;
class System;

#include "call.h"
#include "state.h"
#include "systems/parser.h"
#include "systems/system.h"
#include "systems/system_impl.h"
#include <op25_repeater/include/op25_repeater/rx_status.h>
// enum  CallState { MONITORING=0, recording=1, stopping=2};

class Call_impl : public Call {
public:
  Call_impl(long t, double f, System *s, const Config &c);
  Call_impl(const TrunkMessage &message, System *s, const Config &c);

  long get_call_num() override;
  void restart_call() override;
  void stop_call() override;
  void conclude_call() override;
  void set_sigmf_recorder(Recorder *r) override;
  Recorder *get_sigmf_recorder() override;
  void set_debug_recorder(Recorder *r) override;
  Recorder *get_debug_recorder() override;
  void set_recorder(Recorder *r) override;
  Recorder *get_recorder() override;
  double get_freq() override;
  int get_sys_num() override;
  std::string get_short_name() override;
  std::string get_capture_dir() override;
  std::string get_temp_dir() override;
  void set_freq(double f) override;
  long get_talkgroup() override;

  bool update(TrunkMessage message) override;
  int get_idle_count() override;
  void increase_idle_count() override;
  void reset_idle_count() override;
  double since_last_voice_update() override;
  int since_last_update() override;
  long elapsed() override;

  double get_current_length() override;
  long get_stop_time() override;
  void set_debug_recording(bool m) override;
  bool get_debug_recording() override;
  void set_sigmf_recording(bool m) override;
  bool get_sigmf_recording() override;
  void set_state(State s) override;
  State get_state() override;
  void set_monitoring_state(MonitoringState s) override;
  MonitoringState get_monitoring_state() override;
  void set_phase2_tdma(bool m) override;
  bool get_phase2_tdma() override;
  void set_tdma_slot(int s) override;
  int get_tdma_slot() override;
  bool get_is_analog() override;
  void set_is_analog(bool a) override;
  const char *get_xor_mask() override;
  time_t get_start_time() override { return start_time; }
  bool is_conventional() override { return false; }
  void set_encrypted(bool m) override;
  bool get_encrypted() override;
  void set_emergency(bool m) override;
  bool get_emergency() override;
  int get_priority() override;
  bool get_mode() override;
  bool get_duplex() override;
  std::string get_talkgroup_display() override;
  void set_talkgroup_tag(std::string tag) override;
  void clear_transmission_list() override;
  boost::property_tree::ptree get_stats() override;

  std::string get_talkgroup_tag() override;
  std::string get_system_type() override;
  double get_final_length() override;
  long get_current_source_id() override;
  bool get_conversation_mode() override;
  System *get_system() override;
  std::vector<Transmission> get_transmissions() override;

protected:
  State state;
  MonitoringState monitoringState;
  static long call_counter;
  long call_num;
  long talkgroup;
  double curr_freq;
  std::vector<Transmission> transmission_list;
  System *sys;
  std::string short_name;
  long curr_src_id;
  long error_list_count;
  long freq_count;
  time_t last_update;
  int idle_count;
  time_t stop_time;
  time_t start_time;
  bool debug_recording;
  bool sigmf_recording;
  bool was_update;
  bool encrypted;
  bool emergency;
  bool mode;
  bool duplex;
  bool is_analog;
  int priority;
  char filename[255];
  char transmission_filename[255];
  char converted_filename[255];
  char status_filename[255];
  char debug_filename[255];
  char sigmf_filename[255];
  char path[255];
  bool phase2_tdma;
  int tdma_slot;
  double final_length;

  Config config;
  Recorder *recorder;
  Recorder *debug_recorder;
  Recorder *sigmf_recorder;
  bool add_source(long src);
  std::string talkgroup_display;
  std::string talkgroup_tag;
  void update_talkgroup_display();
};

int plugman_signal(long unitId, const char *signaling_type, gr::blocks::SignalType sig_type, Call *call, System *system, Recorder *recorder);

#endif
