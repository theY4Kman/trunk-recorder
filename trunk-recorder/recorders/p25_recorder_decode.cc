
#include "p25_recorder_decode.h"
#include "../gr_blocks/plugin_wrapper_impl.h"
#include "../plugin_manager/plugin_manager.h"

p25_recorder_decode_sptr make_p25_recorder_decode(Recorder *recorder, const int silence_frames, const bool d_soft_vocoder) {
  auto *decoder = new p25_recorder_decode(recorder);
  decoder->initialize(silence_frames, d_soft_vocoder);
  return gnuradio::get_initial_sptr(decoder);
}

p25_recorder_decode::p25_recorder_decode(Recorder *recorder)
    : gr::hier_block2("p25_recorder_decode",
                      gr::io_signature::make(1, 1, sizeof(float)),
                      gr::io_signature::make(0, 0, sizeof(float))) {
  d_recorder = recorder;
}

p25_recorder_decode::~p25_recorder_decode() = default;

void p25_recorder_decode::stop() {
  wav_sink->stop_recording();
  d_call = nullptr;
}

void p25_recorder_decode::start(Call *call) {
  levels->set_k(call->get_system()->get_digital_levels());

  if(call->get_phase2_tdma()){
    wav_sink->start_recording(call, call->get_tdma_slot());
  } else {
    wav_sink->start_recording(call);
  }
  
  d_call = call;
}

void p25_recorder_decode::set_xor_mask(const char *mask) const {
  op25_frame_assembler->set_xormask(mask);
}

void p25_recorder_decode::set_source(const long src) const {
  wav_sink->set_source(src);
}

std::vector<Transmission> p25_recorder_decode::get_transmission_list() const {
  return wav_sink->get_transmission_list();
}

void p25_recorder_decode::set_tdma_slot(const int slot) {

  tdma_slot = slot;
  op25_frame_assembler->set_slotid(tdma_slot);
}
double p25_recorder_decode::get_current_length() const {
  return wav_sink->total_length_in_seconds();
}

State p25_recorder_decode::get_state() const {
  return wav_sink->get_state();
}

double p25_recorder_decode::since_last_write() const {
  const auto end = std::chrono::steady_clock::now();
  const std::chrono::duration<double> diff = end - wav_sink->get_last_write_time();
  return diff.count();
}

void p25_recorder_decode::switch_tdma(const bool phase2_tdma) const {
  op25_frame_assembler->set_phase2_tdma(phase2_tdma);
}

void p25_recorder_decode::initialize(const int silence_frames, const bool d_soft_vocoder) {
  // OP25 Slicer
  constexpr float l[] = {-2.0, 0.0, 2.0, 4.0};
  std::vector slices(l, l + std::size(l));
  constexpr int msgq_id = 0;
  constexpr int debug = 0;
  slicer = gr::op25_repeater::fsk4_slicer_fb::make(msgq_id, debug, slices);
  wav_sink = gr::blocks::transmission_sink::make(1, 8000, 16);
  // recorder->initialize(src);

  const bool use_streaming = d_recorder->get_enable_audio_streaming();

  // OP25 Frame Assembler
  traffic_queue = gr::msg_queue::make(2);
  rx_queue = gr::msg_queue::make(100);

  constexpr int udp_port = 0;
  constexpr int verbosity = 0; // 10 = lots of debug messages
  const char *udp_host = "127.0.0.1";
  constexpr bool do_imbe = 1;
  constexpr bool do_output = 1;
  constexpr bool do_msgq = 0;
  constexpr bool do_audio_output = 1;
  constexpr bool do_tdma = 0;
  constexpr bool do_nocrypt = 1;

  op25_frame_assembler = gr::op25_repeater::p25_frame_assembler::make(silence_frames, d_soft_vocoder, udp_host, udp_port, verbosity, do_imbe, do_output, do_msgq, rx_queue, do_audio_output, do_tdma, do_nocrypt);
  levels = gr::blocks::multiply_const_ss::make(1);

  if (use_streaming) {
    plugin_sink = gr::blocks::plugin_wrapper_impl::make([this](auto && PH1, auto && PH2) { plugin_callback_handler(std::forward<decltype(PH1)>(PH1), std::forward<decltype(PH2)>(PH2)); });
  }

  connect(self(), 0, slicer, 0);
  connect(slicer, 0, op25_frame_assembler, 0);
  connect(op25_frame_assembler, 0, levels, 0);

  if (use_streaming) {
    connect(levels, 0, plugin_sink, 0);
  }
  connect(levels, 0, wav_sink, 0);
}

void p25_recorder_decode::plugin_callback_handler(int16_t *samples, const int sampleCount) const {
  if (d_call) {
    plugman_audio_callback(d_call, d_recorder, samples, sampleCount);
  }
}

double p25_recorder_decode::get_output_sample_rate() {
  return 8000;
}

// This lead to weird Segfaults. The concept is trying to clear out the buffers for a new call
void p25_recorder_decode::reset_block(const gr::basic_block_sptr &block) {
  const gr::block_sptr grblock = cast_to_block_sptr(block);
  const gr::block_detail_sptr detail = grblock->detail();
  //detail->reset_nitem_counters();
  detail->clear_tags();
}

void p25_recorder_decode::reset() const {
  /*reset_block(op25_frame_assembler);
  reset_block(slicer);
  reset_block(levels);
  reset_block(wav_sink);*/
  op25_frame_assembler->clear();
}

gr::op25_repeater::p25_frame_assembler::sptr p25_recorder_decode::get_transmission_sink() {
  return op25_frame_assembler;
}
