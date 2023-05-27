
#include "analog_recorder.h"
#include "../formatter.h"
#include "../gr_blocks/decoder_wrapper_impl.h"
#include "../gr_blocks/plugin_wrapper_impl.h"
#include "../gr_blocks/transmission_sink.h"
#include "../plugin_manager/plugin_manager.h"
#include "../recorder_globals.h"

using namespace std;

bool analog_recorder::logging = false;
// static int rec_counter = 0;

std::vector<float> design_filter(double interpolation, double deci) {
  float beta = 5.0;
  float trans_width = 0.5 - 0.4;
  float mid_transition_band = 0.5 - trans_width / 2;

#if GNURADIO_VERSION < 0x030900
  std::vector<float> result = gr::filter::firdes::low_pass(
      interpolation,
      1,
      mid_transition_band / interpolation,
      trans_width / interpolation,
      gr::filter::firdes::WIN_KAISER,
      beta);
#else
  std::vector<float> result = gr::filter::firdes::low_pass(
      interpolation,
      1,
      mid_transition_band / interpolation,
      trans_width / interpolation,
      gr::fft::window::WIN_KAISER,
      beta);
#endif
  return result;
}

analog_recorder_sptr make_analog_recorder(Source *src, Recorder_Type type) {
  return gnuradio::get_initial_sptr(new analog_recorder(src, type));
}

/*! \brief Calculate taps for FM de-emph IIR filter. */
void analog_recorder::calculate_iir_taps(double tau) {
  // copied from fm_emph.py in gr-analog
  double w_c;  // Digital corner frequency
  double w_ca; // Prewarped analog corner frequency
  double k, z1, p1, b0;
  double fs = system_channel_rate;

  w_c = 1.0 / tau;
  w_ca = 2.0 * fs * tan(w_c / (2.0 * fs));

  // Resulting digital pole, zero, and gain term from the bilinear
  // transformation of H(s) = w_ca / (s + w_ca) to
  // H(z) = b0 (1 - z1 z^-1)/(1 - p1 z^-1)
  k = -w_ca / (2.0 * fs);
  z1 = -1.0;
  p1 = (1.0 + k) / (1.0 - k);
  b0 = -k / (1.0 - k);

  d_fftaps[0] = b0;
  d_fftaps[1] = -z1 * b0;
  d_fbtaps[0] = 1.0;
  d_fbtaps[1] = -p1;
}

analog_recorder::analog_recorder(Source *src, Recorder_Type type)
    : gr::hier_block2("analog_recorder",
                      gr::io_signature::make(1, 1, sizeof(gr_complex)),
                      gr::io_signature::make(0, 0, sizeof(float))),
      Recorder(type) {
  // int nchars;

  source = src;
  chan_freq = source->get_center();
  center_freq = source->get_center();
  config = source->get_config();
  samp_rate = source->get_rate();
  squelch_db = 0;
  talkgroup = 0;
  recording_count = 0;
  recording_duration = 0;

  rec_num = rec_counter++;
  state = INACTIVE;

  timestamp = time(NULL);
  starttime = time(NULL);

  float offset = 0;
  bool use_streaming = false;

  if (config != NULL) {
    use_streaming = config->enable_audio_streaming;
  }

  // int samp_per_sym        = 10;
  system_channel_rate = 96000; // 4800 * samp_per_sym;
  wav_sample_rate = 16000;     // Must be an integer decimation of system_channel_rate
                               /*  int decim               = floor(samp_rate / 384000);
                             
  double pre_channel_rate = samp_rate / decim;*/

  int initial_decim = floor(samp_rate / 480000);
  initial_rate = double(samp_rate) / double(initial_decim);
  int decim = floor(initial_rate / system_channel_rate);
  double resampled_rate = double(initial_rate) / double(decim);

#if GNURADIO_VERSION < 0x030900
  inital_lpf_taps = gr::filter::firdes::low_pass_2(1.0, samp_rate, 96000, 30000, 100, gr::filter::firdes::WIN_HANN);
#else
  inital_lpf_taps = gr::filter::firdes::low_pass_2(1.0, samp_rate, 96000, 30000, 100, gr::fft::window::WIN_HANN);
#endif
  //  channel_lpf_taps =  gr::filter::firdes::low_pass_2(1.0, pre_channel_rate, 5000, 2000, 60);
  channel_lpf_taps = gr::filter::firdes::low_pass_2(1.0, initial_rate, 4000, 1000, 100);

  std::vector<gr_complex> dest(inital_lpf_taps.begin(), inital_lpf_taps.end());

  prefilter = make_freq_xlating_fft_filter(initial_decim, dest, offset, samp_rate);

  channel_lpf = gr::filter::fft_filter_ccf::make(decim, channel_lpf_taps);

  double arb_rate = (double(system_channel_rate) / resampled_rate);
  double arb_size = 32;
  double arb_atten = 100;

  // Create a filter that covers the full bandwidth of the output signal

  // If rate >= 1, we need to prevent images in the output,
  // so we have to filter it to less than half the channel
  // width of 0.5.  If rate < 1, we need to filter to less
  // than half the output signal's bw to avoid aliasing, so
  // the half-band here is 0.5*rate.
  // double percent = 0.80;
  double percent = 1.00; // Slightly widening this filter helps wideband and makes the audio a little better when using a higher sample rate

  if (arb_rate <= 1) {
    double halfband = 0.5 * arb_rate;
    double bw = percent * halfband;
    double tb = (percent / 2.0) * halfband;

// BOOST_LOG_TRIVIAL(info) << "Arb Rate: " << arb_rate << " Half band: " << halfband << " bw: " << bw << " tb: " <<
// tb;

// As we drop the bw factor, the optfir filter has a harder time converging;
// using the firdes method here for better results.
#if GNURADIO_VERSION < 0x030900
    arb_taps = gr::filter::firdes::low_pass_2(arb_size, arb_size, bw, tb, arb_atten, gr::filter::firdes::WIN_BLACKMAN_HARRIS);
#else
    arb_taps = gr::filter::firdes::low_pass_2(arb_size, arb_size, bw, tb, arb_atten, gr::fft::window::WIN_BLACKMAN_HARRIS);
#endif
    double tap_total = inital_lpf_taps.size() + channel_lpf_taps.size() + arb_taps.size();
    BOOST_LOG_TRIVIAL(info) << "\t Analog Recorder Taps - initial: " << inital_lpf_taps.size() << " channel: " << channel_lpf_taps.size() << " ARB: " << arb_taps.size() << " Total: " << tap_total;
  } else {
    BOOST_LOG_TRIVIAL(error) << "Something is probably wrong! Resampling rate too low";
    exit(1);
  }

  arb_resampler = gr::filter::pfb_arb_resampler_ccf::make(arb_rate, arb_taps);

  // on a trunked network where you know you will have good signal, a carrier
  // power squelch works well. real FM receviers use a noise squelch, where
  // the received audio is high-passed above the cutoff and then fed to a
  // reverse squelch. If the power is then BELOW a threshold, open the squelch.

  // Non-blocking as we are using squelch_two as a gate.
  squelch = gr::analog::pwr_squelch_cc::make(squelch_db, 0.01, 10, false);

  //  based on squelch code form ham2mon
  // set low -200 since its after demod and its just gate for previous squelch so that the audio
  // recording doesn't contain blank spaces between transmissions
  squelch_two = gr::analog::pwr_squelch_ff::make(-200, 0.01, 0, true);

  // k = quad_rate/(2*math.pi*max_dev) = 48k / (6.283185*5000) = 1.527

  int d_max_dev = 5000;
  /* demodulator gain */
  quad_gain = system_channel_rate / (2.0 * M_PI * d_max_dev);
  demod = gr::analog::quadrature_demod_cf::make(quad_gain);
  levels = gr::blocks::multiply_const_ff::make(1); // 33);
  converter = gr::blocks::float_to_short::make(1, 32767);
  valve = gr::blocks::copy::make(sizeof(gr_complex));
  valve->set_enabled(false);

  /* de-emphasis */
  d_tau = 0.000075; // 75us
  d_fftaps.resize(2);
  d_fbtaps.resize(2);
  calculate_iir_taps(d_tau);
  deemph = gr::filter::iir_filter_ffd::make(d_fftaps, d_fbtaps);

  audio_resampler_taps = design_filter(1, (system_channel_rate / wav_sample_rate)); // Calculated to make sample rate changable -- must be an integer

  // downsample from 48k to 8k
  decim_audio = gr::filter::fir_filter_fff::make((system_channel_rate / wav_sample_rate), audio_resampler_taps); // Calculated to make sample rate changable

  // tm *ltm = localtime(&starttime);

  wav_sink = gr::blocks::transmission_sink::make(1, wav_sample_rate, 16); //  Configurable

  if (use_streaming) {
    BOOST_LOG_TRIVIAL(info) << "\t Creating plugin sink..." << std::endl;
    plugin_sink = gr::blocks::plugin_wrapper_impl::make(std::bind(&analog_recorder::plugin_callback_handler, this, std::placeholders::_1, std::placeholders::_2));
    BOOST_LOG_TRIVIAL(info) << "\t Plugin sink created!" << std::endl;
  }

  BOOST_LOG_TRIVIAL(info) << "\t Creating decoder sink..." << std::endl;
  decoder_sink = gr::blocks::decoder_wrapper_impl::make(wav_sample_rate, std::bind(&analog_recorder::decoder_callback_handler, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
  BOOST_LOG_TRIVIAL(info) << "\t Decoder sink created!" << std::endl;

  // Analog audio band pass from 300 to 3000 Hz
  // can't use gnuradio.filter.firdes.band_pass since we have different transition widths
  // 300 Hz high pass (275-325 Hz): removes CTCSS/DCS and Type II 150 bps Low Speed Data (LSD), or "FSK wobble"
#if GNURADIO_VERSION < 0x030900
  high_f_taps = gr::filter::firdes::high_pass(1, wav_sample_rate, 300, 50, gr::filter::firdes::WIN_HANN); // Configurable
  low_f_taps = gr::filter::firdes::low_pass(1, wav_sample_rate, 3250, 500, gr::filter::firdes::WIN_HANN);
#else
  high_f_taps = gr::filter::firdes::high_pass(1, wav_sample_rate, 300, 50, gr::fft::window::WIN_HANN); // Configurable
  low_f_taps = gr::filter::firdes::low_pass(1, wav_sample_rate, 3250, 500, gr::fft::window::WIN_HANN);
#endif

  high_f = gr::filter::fir_filter_fff::make(1, high_f_taps);
  // 3000 Hz low pass (3000-3500 Hz)

  low_f = gr::filter::fir_filter_fff::make(1, low_f_taps);

  // using squelch
  connect(self(), 0, valve, 0);
  connect(valve, 0, prefilter, 0);
  connect(prefilter, 0, channel_lpf, 0);
  if (arb_rate == 1) {
    connect(channel_lpf, 0, squelch, 0);
  } else {
    connect(channel_lpf, 0, arb_resampler, 0);
    connect(arb_resampler, 0, squelch, 0);
  }
  connect(squelch, 0, demod, 0);
  connect(demod, 0, deemph, 0);
  connect(deemph, 0, decim_audio, 0);
  connect(decim_audio, 0, high_f, 0);
  connect(decim_audio, 0, decoder_sink, 0);

  connect(high_f, 0, low_f, 0);
  connect(low_f, 0, squelch_two, 0);
  connect(squelch_two, 0, levels, 0);
  connect(levels, 0, converter, 0);
  connect(converter, 0, wav_sink, 0);

  if (use_streaming) {
    connect(converter, 0, plugin_sink, 0);
  }
}

analog_recorder::~analog_recorder() {}

long analog_recorder::get_wav_hz() { return wav_sample_rate; };

State analog_recorder::get_state() {
  return wav_sink->get_state();
}

double analog_recorder::since_last_write() {
  auto end = std::chrono::steady_clock::now();
  std::chrono::duration<double> diff = end - wav_sink->get_last_write_time();
  return diff.count();
}

int analog_recorder::get_num() {
  return rec_num;
}

std::vector<Transmission> analog_recorder::get_transmission_list() {
  return wav_sink->get_transmission_list();
}

void analog_recorder::stop() {
  if (state == ACTIVE) {
    recording_duration += wav_sink->length_in_seconds();
    state = INACTIVE;
    valve->set_enabled(false);
    wav_sink->stop_recording();
  } else {

    BOOST_LOG_TRIVIAL(error) << "analog_recorder.cc: Stopping an inactive Logger \t[ " << rec_num << " ] - freq[ " << format_freq(chan_freq) << "] \t talkgroup[ " << talkgroup << " ]";
  }

  decoder_sink->set_mdc_enabled(false);
  decoder_sink->set_fsync_enabled(false);
  decoder_sink->set_star_enabled(false);
  decoder_sink->set_tps_enabled(false);
}

void analog_recorder::process_message_queues() {
  decoder_sink->process_message_queues();
}

bool analog_recorder::is_analog() {
  return true;
}

bool analog_recorder::is_active() {
  if (state == ACTIVE) {
    return true;
  } else {
    return false;
  }
}

bool analog_recorder::is_squelched() {
  return is_idle();
}

bool analog_recorder::is_idle() {
  if (state == ACTIVE) {
    return !squelch->unmuted();
  }
  return true;
}

long analog_recorder::get_talkgroup() {
  return talkgroup;
}

double analog_recorder::get_freq() {
  return chan_freq;
}

void analog_recorder::set_source(long src) {
  wav_sink->set_source(src);
}

Source *analog_recorder::get_source() {
  return source;
}

int analog_recorder::lastupdate() {
  return time(NULL) - timestamp;
}

long analog_recorder::elapsed() {
  return time(NULL) - starttime;
}

time_t analog_recorder::get_start_time() {
  return starttime;
}

double analog_recorder::get_current_length() {
  return wav_sink->total_length_in_seconds();
}

void analog_recorder::tune_offset(double f) {
  chan_freq = f;
  int offset_amount = (f - center_freq);
  prefilter->set_center_freq(offset_amount);
}

void analog_recorder::decoder_callback_handler(long unitId, const char *signaling_type, gr::blocks::SignalType signal) {
  if (call != NULL) {
    wav_sink->set_source(unitId);
    plugman_signal(unitId, signaling_type, signal, call, call->get_system(), this);
  } else {
    plugman_signal(unitId, signaling_type, signal, NULL, NULL, this);
  }
}

void analog_recorder::plugin_callback_handler(int16_t *samples, int sampleCount) {
  plugman_audio_callback(call, this, samples, sampleCount);
}

void analog_recorder::setup_decoders_for_system(System *system) {
  decoder_sink->set_mdc_enabled(system->get_mdc_enabled());
  decoder_sink->set_fsync_enabled(system->get_fsync_enabled());
  decoder_sink->set_star_enabled(system->get_star_enabled());
  decoder_sink->set_tps_enabled(system->get_tps_enabled());
}

bool analog_recorder::start(Call *call) {
  starttime = time(NULL);
  System *system = call->get_system();
  this->call = call;

  setup_decoders_for_system(call->get_system());

  talkgroup = call->get_talkgroup();
  chan_freq = call->get_freq();

  squelch_db = system->get_squelch_db();
  squelch->set_threshold(squelch_db);
  BOOST_LOG_TRIVIAL(info) << "[" << call->get_short_name() << "]\t\033[0;34m" << call->get_call_num() << "C\033[0m\tTG: " << this->call->get_talkgroup_display() << "\tFreq: " << format_freq(chan_freq) << "\t\u001b[32mStarting Analog Recorder Num [" << rec_num << "]\u001b[0m \tSquelch: " << squelch_db;

  // BOOST_LOG_TRIVIAL(error) << "Setting squelch to: " << squelch_db << " block says: " << squelch->threshold();
  levels->set_k(system->get_analog_levels());
  int d_max_dev = system->get_max_dev();
  channel_lpf_taps = gr::filter::firdes::low_pass_2(1.0, initial_rate, d_max_dev, 1000, 100);
  channel_lpf->set_taps(channel_lpf_taps);
  quad_gain = system_channel_rate / (2.0 * M_PI * (d_max_dev + 1000));
  demod->set_gain(quad_gain);
  prefilter->set_center_freq(chan_freq - center_freq);

  wav_sink->start_recording(call);

  state = ACTIVE;
  valve->set_enabled(true);
  return true;
}

double analog_recorder::get_output_sample_rate() {
  return wav_sample_rate;
}
