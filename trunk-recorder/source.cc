#include "source.h"

#include <utility>
#include "formatter.h"

static int src_counter = 0;

void Source::set_antenna(const std::string &ant) {
  antenna = ant;

  if (driver == "osmosdr") {
    cast_to_osmo_sptr(source_block)->set_antenna(antenna, 0);
    BOOST_LOG_TRIVIAL(info) << "Setting antenna to [" << cast_to_osmo_sptr(source_block)->get_antenna() << "]";
  }

  if (driver == "usrp") {
    BOOST_LOG_TRIVIAL(info) << "Setting antenna to [" << antenna << "]";
    cast_to_usrp_sptr(source_block)->set_antenna(antenna, 0);
  }
}

std::string Source::get_antenna() {
  return antenna;
}

void Source::set_silence_frames(const int m) {
  silence_frames = m;
}

int Source::get_silence_frames() const {
  return silence_frames;
}


double Source::get_min_hz() const {
  return min_hz;
}

double Source::get_max_hz() const {
  return max_hz;
}

double Source::get_center() const {
  return center;
}

double Source::get_rate() const {
  return rate;
}

std::string Source::get_driver() {
  return driver;
}

std::string Source::get_device() {
  return device;
}

void Source::set_error(const double e) {
  error = e;
}

double Source::get_error() const {
  return error;
}

void Source::set_gain(const int r) {
  if (driver == "osmosdr") {
    gain = r;
    cast_to_osmo_sptr(source_block)->set_gain(gain);
    BOOST_LOG_TRIVIAL(info) << "Gain set to: " << cast_to_osmo_sptr(source_block)->get_gain();
  }

  if (driver == "usrp") {
    gain = r;
    cast_to_usrp_sptr(source_block)->set_gain(gain);
  }
}

void Source::add_gain_stage(const std::string &stage_name, const int value) {
  const Gain_Stage_t stage = {stage_name, value};
  gain_stages.push_back(stage);
}

std::vector<Gain_Stage_t> Source::get_gain_stages() {
  return gain_stages;
}

void Source::set_gain_by_name(std::string name, const int r) {
  if (driver == "osmosdr") {
    cast_to_osmo_sptr(source_block)->set_gain(r, name);
    BOOST_LOG_TRIVIAL(info) << name << " Gain set to: " << cast_to_osmo_sptr(source_block)->get_gain(name);
    add_gain_stage(name, r);
  } else {
    BOOST_LOG_TRIVIAL(error) << "Unable to set Gain by Name for SDR drive: " << driver;
  }
}

int Source::get_gain_by_name(std::string name) {
  if (driver == "osmosdr") {
    try {
      return static_cast<int>(cast_to_osmo_sptr(source_block)->get_gain(name, 0));
    } catch (std::exception &e) {
      BOOST_LOG_TRIVIAL(error) << name << " Gain unsupported or other error: " << e.what();
    }
  } else {
    BOOST_LOG_TRIVIAL(error) << "Unable to get Gain by Name for SDR drive: " << driver;
  }
  return -1;
}

int Source::get_gain() const {
  return gain;
}

void Source::set_gain_mode(const bool m) {
  if (driver == "osmosdr") {
    gain_mode = m;
    cast_to_osmo_sptr(source_block)->set_gain_mode(gain_mode);
    if (cast_to_osmo_sptr(source_block)->get_gain_mode()) {
      BOOST_LOG_TRIVIAL(info) << "Auto gain control is ON";
    } else {
      BOOST_LOG_TRIVIAL(info) << "Auto gain control is OFF";
    }
  }
}

void Source::set_freq_corr(const double p) {
  ppm = p;

  if (driver == "osmosdr") {
    cast_to_osmo_sptr(source_block)->set_freq_corr(ppm);
    BOOST_LOG_TRIVIAL(info) << "PPM set to: " << cast_to_osmo_sptr(source_block)->get_freq_corr();
  }
}

int Source::get_if_gain() const {
  return if_gain;
}

analog_recorder_sptr Source::create_conventional_recorder(const gr::top_block_sptr &tb) {
  // Not adding it to the vector of analog_recorders. We don't want it to be available for trunk recording.
  // Conventional recorders are tracked seperately in analog_conv_recorders
  analog_recorder_sptr log = make_analog_recorder(this, ANALOGC);
  analog_conv_recorders.push_back(log);
  tb->connect(source_block, 0, log, 0);
  return log;
}

void Source::create_analog_recorders(const gr::top_block_sptr &tb, const int r) {
  max_analog_recorders = r;

  for (int i = 0; i < max_analog_recorders; i++) {
    analog_recorder_sptr log = make_analog_recorder(this, ANALOG);
    analog_recorders.push_back(log);
    tb->connect(source_block, 0, log, 0);
  }
}

Recorder *Source::get_analog_recorder(const Talkgroup *talkgroup, const int priority, Call *call) {
  const int num_available_recorders = get_num_available_analog_recorders();

  if(talkgroup && (priority == -1)){
    call->set_state(MONITORING);
    call->set_monitoring_state(IGNORED_TG);
    BOOST_LOG_TRIVIAL(info) << "[" << call->get_system()->get_short_name() << "]\t\033[0;34m" << call->get_call_num() << "C\033[0m\tTG: " << call->get_talkgroup_display() << "\tFreq: " << format_freq(call->get_freq()) << "\tNot recording talkgroup. Priority is -1.";
    return nullptr;
  }

  if (talkgroup && priority > num_available_recorders) { // a high priority is bad. You need at least the number of availalbe recorders to your priority
    call->set_state(MONITORING);
    call->set_monitoring_state(NO_RECORDER);
    BOOST_LOG_TRIVIAL(error) << "[" << call->get_system()->get_short_name() << "]\t\033[0;34m" << call->get_call_num() << "C\033[0m\tTG: " << call->get_talkgroup_display() << "\tFreq: " << format_freq(call->get_freq()) << "\tNot recording talkgroup. Priority is " <<  priority << " but only " << num_available_recorders << " recorders are available.";
    return nullptr;
  }

  return get_analog_recorder(call);
}

Recorder *Source::get_analog_recorder(Call *call) {
  for (const auto& rx : analog_recorders) {
    if (rx->get_state() == AVAILABLE) {
      return (Recorder *)rx.get();

      break;
    }
  }
  BOOST_LOG_TRIVIAL(error) << "[" << call->get_system()->get_short_name() << "]\t\033[0;34m" << call->get_call_num() << "C\033[0m\tTG: " << call->get_talkgroup_display() << "\tFreq: " << format_freq(call->get_freq()) << "\t[ " << device << " ] No Analog Recorders Available.";
  return nullptr;
}

void Source::create_digital_recorders(const gr::top_block_sptr &tb, const int r) {
  max_digital_recorders = r;

  for (int i = 0; i < max_digital_recorders; i++) {
    p25_recorder_sptr log = make_p25_recorder(this, P25);
    digital_recorders.push_back(log);
    tb->connect(source_block, 0, log, 0);
  }
}

Recorder *Source::get_digital_recorder(Talkgroup *talkgroup, const int priority, Call *call) {
  const int num_available_recorders = get_num_available_digital_recorders();

  if(talkgroup && (priority == -1)){
    call->set_state(MONITORING);
    call->set_monitoring_state(IGNORED_TG);
    BOOST_LOG_TRIVIAL(info) << "[" << call->get_system()->get_short_name() << "]\t\033[0;34m" << call->get_call_num() << "C\033[0m\tTG: " << call->get_talkgroup_display() << "\tFreq: " << format_freq(call->get_freq()) << "\tNot recording talkgroup. Priority is -1.";
    return nullptr;
  }

  if (talkgroup && priority > num_available_recorders) { // a high priority is bad. You need at least the number of availalbe recorders to your priority
    call->set_state(MONITORING);
    call->set_monitoring_state(NO_RECORDER);
    BOOST_LOG_TRIVIAL(error) << "[" << call->get_system()->get_short_name() << "]\t\033[0;34m" << call->get_call_num() << "C\033[0m\tTG: " << call->get_talkgroup_display() << "\tFreq: " << format_freq(call->get_freq()) << "\tNot recording talkgroup. Priority is " <<  priority << " but only " << num_available_recorders << " recorders are available.";
    return nullptr;
  }

  return get_digital_recorder(call);
}

Recorder *Source::get_digital_recorder(Call *call) {
  for (const auto& rx : digital_recorders) {
    if (rx->get_state() == AVAILABLE) {
      return rx.get();
    }
  }

  BOOST_LOG_TRIVIAL(error) << "[" << call->get_system()->get_short_name() << "]\t\033[0;34m" << call->get_call_num() << "C\033[0m\tTG: " << call->get_talkgroup_display() << "\tFreq: " << format_freq(call->get_freq()) << "\t[ " << device << " ] No Digital Recorders Available.";

  for (const auto& rx : digital_recorders) {
    BOOST_LOG_TRIVIAL(info) << "[ " << rx->get_num() << " ] State: " << format_state(rx->get_state()) << " Freq: " << rx->get_freq();
  }

  return nullptr;
}

p25_recorder_sptr Source::create_digital_conventional_recorder(const gr::top_block_sptr &tb) {
  // Not adding it to the vector of digital_recorders. We don't want it to be available for trunk recording.
  // Conventional recorders are tracked seperately in digital_conv_recorders
  p25_recorder_sptr log = make_p25_recorder(this, P25C);
  digital_conv_recorders.push_back(log);
  tb->connect(source_block, 0, log, 0);
  return log;
}

dmr_recorder_sptr Source::create_dmr_conventional_recorder(const gr::top_block_sptr &tb) {
  // Not adding it to the vector of digital_recorders. We don't want it to be available for trunk recording.
  // Conventional recorders are tracked seperately in digital_conv_recorders
  dmr_recorder_sptr log = make_dmr_recorder(this, DMR);
  dmr_conv_recorders.push_back(log);
  tb->connect(source_block, 0, log, 0);
  return log;
}

void Source::create_debug_recorder(const gr::top_block_sptr &tb, const int source_num) {
  max_debug_recorders = 1;
  debug_recorder_port = config->debug_recorder_port + source_num;
  const debug_recorder_sptr log = make_debug_recorder(this, config->debug_recorder_address, debug_recorder_port);
  debug_recorders.push_back(log);
  tb->connect(source_block, 0, log, 0);
}

Recorder *Source::get_debug_recorder() const {
  for (const auto& rx : debug_recorders) {
    if (rx->get_state() == INACTIVE) {
      return rx.get();
    }
  }
  return nullptr;
}

int Source::get_debug_recorder_port() const {
  return debug_recorder_port;
}

void Source::create_sigmf_recorders(const gr::top_block_sptr &tb, const int r) {
  max_sigmf_recorders = r;

  for (int i = 0; i < max_sigmf_recorders; i++) {
    sigmf_recorder_sptr log = make_sigmf_recorder(this);

    sigmf_recorders.push_back(log);
    tb->connect(source_block, 0, log, 0);
  }
}

Recorder *Source::get_sigmf_recorder() {
  for (std::vector<sigmf_recorder_sptr>::iterator it = sigmf_recorders.begin();
       it != sigmf_recorders.end(); it++) {
    const sigmf_recorder_sptr rx = *it;

    if (rx->get_state() == INACTIVE) {
      return (Recorder *)rx.get();

      break;
    }
  }
  return nullptr;
}

void Source::print_recorders() {
  BOOST_LOG_TRIVIAL(info) << "[ Source " << src_num << ": " << format_freq(center) << " ] " << device ;

  for (const auto& rx : digital_recorders) {
    BOOST_LOG_TRIVIAL(info) << "\t[ " << rx->get_num() << " ] " << rx->get_type_string() << "\tState: " << format_state(rx->get_state());
  }

  for (const auto& rx : digital_conv_recorders) {
    BOOST_LOG_TRIVIAL(info) << "\t[ " << rx->get_num() << " ] " << rx->get_type_string() << "\tState: " << format_state(rx->get_state());
  }

  for (const auto& rx : dmr_conv_recorders) {
    BOOST_LOG_TRIVIAL(info) << "\t[ " << rx->get_num() << " ] " << rx->get_type_string() << "\tState: " << format_state(rx->get_state());
  }

  for (const auto& rx : analog_recorders) {
    BOOST_LOG_TRIVIAL(info) << "\t[ " << rx->get_num() << " ] " << rx->get_type_string() << "\tState: " << format_state(rx->get_state());
  }

  for (const auto& rx : analog_conv_recorders) {
    BOOST_LOG_TRIVIAL(info) << "\t[ " << rx->get_num() << " ] " << rx->get_type_string() << "\tState: " << format_state(rx->get_state());
  }
}

void Source::tune_digital_recorders() const {
  for (const auto& rx : digital_recorders) {
    if (rx->get_state() == ACTIVE) {
      rx->autotune();
    }
  }
}

int Source::digital_recorder_count() const {
  return digital_recorders.size() + digital_conv_recorders.size() + dmr_conv_recorders.size();
}

int Source::analog_recorder_count() const {
  return analog_recorders.size() + analog_conv_recorders.size();
}

int Source::debug_recorder_count() const {
  return debug_recorders.size();
}

int Source::sigmf_recorder_count() const {
  return sigmf_recorders.size();
}

int Source::get_num() const {
  return src_num;
};

int Source::get_num_available_digital_recorders() const {
  int num_available_recorders = 0;

  for (const auto& rx : digital_recorders) {
    if (rx->get_state() == AVAILABLE) {
      num_available_recorders++;
    }
  }
  return num_available_recorders;
}

int Source::get_num_available_analog_recorders() const {
  int num_available_recorders = 0;

  for (const auto& rx : analog_recorders) {
    if (rx->get_state() == AVAILABLE) {
      num_available_recorders++;
    }
  }
  return num_available_recorders;
}

gr::basic_block_sptr Source::get_src_block() {
  return source_block;
}

Config *Source::get_config() const {
  return config;
}

void Source::set_min_max() {
  const long s = rate;
  constexpr long if_freqs[] = {24000, 25000, 32000};
  long decim = 24000;
  for (const long if_freq : if_freqs) {
    if (s % if_freq != 0) {
      continue;
    }
    const long q = s / if_freq;
    if (q & 1) {
      continue;
    }

    if ((q >= 40) && ((q & 3) == 0)) {
      decim = q / 4;
    } else {
      decim = q / 2;
    }
  }
  const long if1 = rate / decim;
  min_hz = center - ((rate / 2) - (if1 / 2));
  max_hz = center + ((rate / 2) - (if1 / 2));
}

Source::Source(double c, double r, double e, std::string drv, std::string dev, Config *cfg) {
  rate = r;
  center = c;
  error = e;
  set_min_max();
  driver = std::move(drv);
  device = dev;
  config = cfg;
  gain = 0;
  lna_gain = 0;
  tia_gain = 0;
  pga_gain = 0;
  mix_gain = 0;
  if_gain = 0;
  src_num = src_counter++;
  max_digital_recorders = 0;
  max_debug_recorders = 0;
  max_sigmf_recorders = 0;
  max_analog_recorders = 0;
  debug_recorder_port = 0;

  if (driver == "osmosdr") {
    osmosdr::source::sptr osmo_src;
    std::vector<std::string> gain_names;
    if (dev == "") {
      BOOST_LOG_TRIVIAL(info) << "Source Device not specified";
      osmo_src = osmosdr::source::make();
    } else {
      std::ostringstream msg;

      if (isdigit(dev[0])) {  // Assume this is a serial number and fail back
                              // to using rtl as default
        msg << "rtl=" << dev; // <<  ",buflen=32764,buffers=8";
        BOOST_LOG_TRIVIAL(info) << "Source device name missing, defaulting to rtl device";
      } else {
        msg << dev; // << ",buflen=32764,buffers=8";
      }
      BOOST_LOG_TRIVIAL(info) << "Source Device: " << msg.str();
      osmo_src = osmosdr::source::make(msg.str());
    }
    BOOST_LOG_TRIVIAL(info) << "SOURCE TYPE OSMOSDR (osmosdr)";
    BOOST_LOG_TRIVIAL(info) << "Setting sample rate to: " << FormatSamplingRate(rate);
    osmo_src->set_sample_rate(rate);
    actual_rate = osmo_src->get_sample_rate();
    rate = round(actual_rate);
    BOOST_LOG_TRIVIAL(info) << "Actual sample rate: " << FormatSamplingRate(actual_rate);
    BOOST_LOG_TRIVIAL(info) << "Tuning to " << format_freq(center + error);
    osmo_src->set_center_freq(center + error, 0);
    gain_names = osmo_src->get_gain_names();
    std::string gain_list;
    for (auto gain_name : gain_names) {
      osmosdr::gain_range_t range = osmo_src->get_gain_range(gain_name);
      std::vector<double> gains = range.values();
      std::string gain_opt_str;
      for (double gain_opt : gains) {
        std::ostringstream ss;
        // gain_opt = floor(gain_opt * 10) / 10;
        ss << gain_opt << " ";

        gain_opt_str += ss.str();
      }
      BOOST_LOG_TRIVIAL(info) << "Gain Stage: " << gain_name << " supported values: " << gain_opt_str;
    }

    source_block = osmo_src;
  }

  if (driver == "usrp") {
    gr::uhd::usrp_source::sptr usrp_src;
    usrp_src = gr::uhd::usrp_source::make(device, uhd::stream_args_t("fc32"));

    BOOST_LOG_TRIVIAL(info) << "SOURCE TYPE USRP (UHD)";

    BOOST_LOG_TRIVIAL(info) << "Setting sample rate to: " << FormatSamplingRate(rate);
    usrp_src->set_samp_rate(rate);
    actual_rate = usrp_src->get_samp_rate();
    BOOST_LOG_TRIVIAL(info) << "Actual sample rate: " << FormatSamplingRate(actual_rate);
    BOOST_LOG_TRIVIAL(info) << "Tuning to " << format_freq(center + error);
    usrp_src->set_center_freq(center + error, 0);

    source_block = usrp_src;
  }
}

std::vector<Recorder *> Source::get_recorders() const {

  std::vector<Recorder *> recorders;

  recorders.reserve(digital_recorders.size() + digital_conv_recorders.size() + dmr_conv_recorders.size() + analog_recorders.size() + analog_conv_recorders.size() + debug_recorders.size() + sigmf_recorders.size());

  for (const auto& rx : digital_recorders) {
    recorders.push_back(rx.get());
  }

  for (const auto& rx : digital_conv_recorders) {
    recorders.push_back(rx.get());
  }

  for (const auto& rx : dmr_conv_recorders) {
    recorders.push_back(rx.get());
  }

  for (const auto& rx : analog_recorders) {
    recorders.push_back(rx.get());
  }

  for (const auto& rx : analog_conv_recorders) {
    recorders.push_back(rx.get());
  }

  for (const auto& rx : debug_recorders) {
    recorders.push_back(rx.get());
  }

  for (const auto& rx : sigmf_recorders) {
    recorders.push_back(rx.get());
  }

  return recorders;
}
