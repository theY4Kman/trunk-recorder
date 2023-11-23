#ifndef SOURCE_H
#define SOURCE_H
#include "./global_structs.h"
#include <gnuradio/basic_block.h>
#include <gnuradio/top_block.h>
#include <gnuradio/uhd/usrp_source.h>
#include <iostream>
#include <numeric>
#include <osmosdr/source.h>
//#include "recorders/recorder.h"
#include "recorders/analog_recorder.h"
#include "recorders/debug_recorder.h"
#include "recorders/dmr_recorder.h"
#include "recorders/p25_recorder.h"
#include "recorders/sigmf_recorder.h"

struct Gain_Stage_t {
  std::string stage_name;
  int value;
};

class Source {

  int src_num;
  double min_hz;
  double max_hz;
  double center;
  double rate;
  double actual_rate;
  double error;
  double ppm;

  bool gain_mode;
  int gain;
  int bb_gain;
  int if_gain;
  int lna_gain;
  int tia_gain;
  int pga_gain;
  int mix_gain;
  int vga1_gain;
  int vga2_gain;
  int max_digital_recorders;
  int max_debug_recorders;
  int max_sigmf_recorders;
  int max_analog_recorders;
  int debug_recorder_port;

  int silence_frames;
  Config *config;

  std::vector<p25_recorder_sptr> digital_recorders;
  std::vector<p25_recorder_sptr> digital_conv_recorders;
  std::vector<debug_recorder_sptr> debug_recorders;
  std::vector<sigmf_recorder_sptr> sigmf_recorders;
  std::vector<analog_recorder_sptr> analog_recorders;
  std::vector<analog_recorder_sptr> analog_conv_recorders;
  std::vector<dmr_recorder_sptr> dmr_conv_recorders;
  std::vector<Gain_Stage_t> gain_stages;
  std::string driver;
  std::string device;
  std::string antenna;
  gr::basic_block_sptr source_block;
  void add_gain_stage(const std::string &stage_name, int value);

public:
  [[nodiscard]] int get_num_available_digital_recorders() const;
  [[nodiscard]] int get_num_available_analog_recorders() const;
  [[nodiscard]] int get_num() const;
  Source(double c, double r, double e, std::string driver, std::string device, Config *cfg);
  gr::basic_block_sptr get_src_block();
  [[nodiscard]] double get_min_hz() const;
  [[nodiscard]] double get_max_hz() const;
  void set_min_max();
  [[nodiscard]] double get_center() const;
  [[nodiscard]] double get_rate() const;
  std::string get_driver();
  std::string get_device();
  void set_antenna(const std::string &ant);
  std::string get_antenna();
  void set_error(double e);
  [[nodiscard]] double get_error() const;
  void set_if_gain(int i);
  [[nodiscard]] int get_if_gain() const;

  void set_gain_mode(bool m);
  bool get_gain_mode();
  void set_gain(int r);
  std::vector<Gain_Stage_t> get_gain_stages();
  int get_gain_by_name(std::string name);
  void set_gain_by_name(std::string name, int r);
  [[nodiscard]] int get_gain() const;

  void set_silence_frames(int m);
  [[nodiscard]] int get_silence_frames() const;
  int get_bb_gain();
  int get_mix_gain();
  int get_lna_gain();
  int get_tia_gain();
  int get_pga_gain();
  int get_vga1_gain();
  int get_vga2_gain();
  void set_freq_corr(double p);
  void print_recorders();
  void tune_digital_recorders() const;
  [[nodiscard]] int debug_recorder_count() const;
  [[nodiscard]] int get_debug_recorder_port() const;
  [[nodiscard]] int sigmf_recorder_count() const;
  [[nodiscard]] int digital_recorder_count() const;
  [[nodiscard]] int analog_recorder_count() const;
  [[nodiscard]] Config *get_config() const;

  void create_debug_recorder(const gr::top_block_sptr &tb, int source_num);
  void create_sigmf_recorders(const gr::top_block_sptr &tb, int r);
  void create_analog_recorders(const gr::top_block_sptr &tb, int r);
  void create_digital_recorders(const gr::top_block_sptr &tb, int r);
  analog_recorder_sptr create_conventional_recorder(const gr::top_block_sptr &tb);
  p25_recorder_sptr create_digital_conventional_recorder(const gr::top_block_sptr &tb);
  dmr_recorder_sptr create_dmr_conventional_recorder(const gr::top_block_sptr &tb);

  Recorder *get_digital_recorder(Call *call);
  Recorder *get_digital_recorder(Talkgroup *talkgroup, int priority, Call *call);
  Recorder *get_analog_recorder(Call *call);
  Recorder *get_analog_recorder(const Talkgroup *talkgroup, int priority, Call *call);
  [[nodiscard]] Recorder *get_debug_recorder() const;
  Recorder *get_sigmf_recorder();

#if GNURADIO_VERSION < 0x030900
  inline osmosdr::source::sptr cast_to_osmo_sptr(gr::basic_block_sptr p) {
    return boost::dynamic_pointer_cast<osmosdr::source, gr::basic_block>(p);
  }
  inline gr::uhd::usrp_source::sptr cast_to_usrp_sptr(gr::basic_block_sptr p) {
    return boost::dynamic_pointer_cast<gr::uhd::usrp_source, gr::basic_block>(p);
  }
#else
  inline osmosdr::source::sptr cast_to_osmo_sptr(gr::basic_block_sptr p) {
    return std::dynamic_pointer_cast<osmosdr::source, gr::basic_block>(p);
  }
  inline gr::uhd::usrp_source::sptr cast_to_usrp_sptr(gr::basic_block_sptr p) {
    return std::dynamic_pointer_cast<gr::uhd::usrp_source, gr::basic_block>(p);
  }
#endif
  [[nodiscard]] std::vector<Recorder *> get_recorders() const;
};
#endif
