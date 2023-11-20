#ifndef P25_RECORDER_DECODE_H
#define P25_RECORDER_DECODE_H

#include <boost/shared_ptr.hpp>
#include <gnuradio/block.h>
#include <gnuradio/block_detail.h>
#include <gnuradio/blocks/short_to_float.h>
#include <gnuradio/hier_block2.h>
#include <gnuradio/io_signature.h>
#include <gnuradio/msg_queue.h>

#include <op25_repeater/fsk4_slicer_fb.h>
#include <op25_repeater/costas_loop_cc.h>
#include <op25_repeater/gardner_cc.h>
#include <op25_repeater/include/op25_repeater/fsk4_demod_ff.h>
#include <op25_repeater/include/op25_repeater/p25_frame_assembler.h>
#include <op25_repeater/include/op25_repeater/rx_status.h>
#include <op25_repeater/vocoder.h>

#if GNURADIO_VERSION < 0x030800
#include <gnuradio/blocks/multiply_const_ss.h>
#else
#include <gnuradio/blocks/multiply_const.h>
#endif

#include "../gr_blocks/plugin_wrapper.h"
#include "../gr_blocks/transmission_sink.h"
#include "recorder.h"

class p25_recorder_decode;

#if GNURADIO_VERSION < 0x030900
typedef boost::shared_ptr<p25_recorder_decode> p25_recorder_decode_sptr;
#else
typedef std::shared_ptr<p25_recorder_decode> p25_recorder_decode_sptr;
#endif

p25_recorder_decode_sptr make_p25_recorder_decode(Recorder *recorder, int silence_frames, bool d_soft_vocoder);

class p25_recorder_decode : public gr::hier_block2 {
  friend p25_recorder_decode_sptr make_p25_recorder_decode(Recorder *recorder, int silence_frames, bool d_soft_vocoder);

protected:
  virtual void initialize(int silence_frames, bool d_soft_vocoder);
  Recorder *d_recorder;
  Call *d_call{};
  gr::op25_repeater::p25_frame_assembler::sptr op25_frame_assembler;
  gr::msg_queue::sptr traffic_queue;
  gr::msg_queue::sptr rx_queue;
  gr::op25_repeater::fsk4_slicer_fb::sptr slicer;
  gr::blocks::short_to_float::sptr converter;
  gr::blocks::multiply_const_ss::sptr levels;
  gr::blocks::transmission_sink::sptr wav_sink;
  gr::blocks::plugin_wrapper::sptr plugin_sink;

public:
  explicit p25_recorder_decode(Recorder *recorder);
  void set_tdma_slot(int slot);
  std::vector<Transmission> get_transmission_list() const;
  void set_source(long src) const;
  void set_xor_mask(const char *mask) const;
  void switch_tdma(bool phase2_tdma) const;
  void start(Call *call);
  double since_last_write() const;
  void stop();
  void reset() const;
  static void reset_block(const gr::basic_block_sptr &block);
  int tdma_slot{};
  bool delay_open{};
  ~p25_recorder_decode() override;
  double get_current_length() const;
  void plugin_callback_handler(int16_t *samples, int sampleCount) const;
  static double get_output_sample_rate();
  State get_state() const;
  gr::op25_repeater::p25_frame_assembler::sptr get_transmission_sink();

};
#endif
