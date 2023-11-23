/**
 * Method name: load_config()
 * Description: <#description#>
 * Parameters: <#parameters#>
 */
#include "./config.h"

using json = nlohmann::json;

namespace logging = boost::log;
namespace keywords = boost::log::keywords;
namespace src = boost::log::sources;
namespace sinks = boost::log::sinks;

using namespace std;

bool parse_logging_level(const std::string &log_level, boost::log::trivial::severity_level *severity) {
  if (log_level == "trace")
    *severity = boost::log::trivial::trace;
  else if (log_level == "debug")
    *severity = boost::log::trivial::debug;
  else if (log_level == "info")
    *severity = boost::log::trivial::info;
  else if (log_level == "warning")
    *severity = boost::log::trivial::warning;
  else if (log_level == "error")
    *severity = boost::log::trivial::error;
  else if (log_level == "fatal")
    *severity = boost::log::trivial::fatal;
  else {
    return false;
  }

  return true;
}

template <class F>
boost::shared_ptr<sinks::synchronous_sink<sinks::basic_text_ostream_backend<char>>> add_logs(const F &fmt) {
  boost::shared_ptr<sinks::synchronous_sink<sinks::basic_text_ostream_backend<char>>> sink =
      boost::log::add_console_log(std::clog, keywords::format = fmt);

  const std::locale loc = std::locale("C");
  sink->imbue(loc);

  return sink;
}

bool load_config(string config_file, Config &config, gr::top_block_sptr &tb, std::vector<Source *> &sources, std::vector<System *> &systems) {

  string system_modulation;

  json data;
  int sys_count = 0;
  int source_count = 0;

  try {
    std::ifstream f(config_file);
    data = json::parse(f);
  } catch (const json::parse_error &e) {
    // output exception information
    std::cout << "message: " << e.what() << '\n'
              << "exception id: " << e.id << '\n'
              << "byte position of error: " << e.byte << std::endl;
  }

  try {
    // const std::string json_filename = "config.json";

    boost::property_tree::ptree pt;
    read_json(config_file, pt);

    boost::shared_ptr<sinks::synchronous_sink<sinks::basic_text_ostream_backend<char>>> console_sink = nullptr;
    boost::shared_ptr<sinks::synchronous_sink<sinks::text_file_backend>> log_file_sink = nullptr;

    config.console_log = pt.get<bool>("consoleLog", true);
    if (config.console_log) {
      console_sink = add_logs(boost::log::expressions::format("[%1%] (%2%)   %3%") % boost::log::expressions::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S.%f") % boost::log::expressions::attr<boost::log::trivial::severity_level>("Severity") % boost::log::expressions::smessage);
    }

    BOOST_LOG_TRIVIAL(info) << "\n-------------------------------------\n     Trunk Recorder\n-------------------------------------\n";

    BOOST_LOG_TRIVIAL(info) << "\n\n-------------------------------------\nINSTANCE\n-------------------------------------\n";

    config.log_file = data.value("logFile", false);
    config.log_dir = data.value("logDir", "logs");
    if (config.log_file) {
      logging::add_file_log(
          keywords::file_name = config.log_dir + "/%m-%d-%Y_%H%M_%2N.log",
          keywords::format = "[%TimeStamp%] (%Severity%)   %Message%",
          keywords::rotation_size = 100 * 1024 * 1024,
          keywords::time_based_rotation = sinks::file::rotation_at_time_point(0, 0, 0),
          keywords::auto_flush = true);
    }

    if (double config_ver = data.value("ver", 0.0); config_ver < 2) {
      BOOST_LOG_TRIVIAL(info) << "The formatting for config files has changed.";
      BOOST_LOG_TRIVIAL(info) << "Modulation type, Squelch and audio levels are now set in each System instead of under a Source.";
      BOOST_LOG_TRIVIAL(info) << "See sample config files in the /example folder and look at readme.md for more details.";
      BOOST_LOG_TRIVIAL(info) << "After you have made these updates, make sure you add \"ver\": 2, to the top.\n\n";
      return false;
    }

    BOOST_LOG_TRIVIAL(info) << "Using Config file: " << config_file << "\n";
    BOOST_LOG_TRIVIAL(info) << PROJECT_NAME << ": "
                            << "Version: " << PROJECT_VER << "\n";

    BOOST_LOG_TRIVIAL(info) << "Log to File: " << config.log_file;
    BOOST_LOG_TRIVIAL(info) << "Log Directory: " << config.log_dir;

    std::string defaultTempDir = boost::filesystem::current_path().string();

    if (boost::filesystem::exists("/dev/shm")) {
      defaultTempDir = "/dev/shm";
    }
    config.temp_dir = data.value("tempDir", defaultTempDir);
    size_t pos = config.temp_dir.find_last_of('/');

    if (pos == config.temp_dir.length() - 1) {
      config.temp_dir.erase(config.temp_dir.length() - 1);
    }

    BOOST_LOG_TRIVIAL(info) << "Temporary Transmission Directory: " << config.temp_dir;

    config.capture_dir = data.value("captureDir", boost::filesystem::current_path().string());
    pos = config.capture_dir.find_last_of('/');

    if (pos == config.capture_dir.length() - 1) {
      config.capture_dir.erase(config.capture_dir.length() - 1);
    }

    BOOST_LOG_TRIVIAL(info) << "Capture Directory: " << config.capture_dir;
    config.upload_server = data.value("uploadServer", "");
    BOOST_LOG_TRIVIAL(info) << "Upload Server: " << config.upload_server;
    config.bcfy_calls_server = data.value("broadcastifyCallsServer", "");
    BOOST_LOG_TRIVIAL(info) << "Broadcastify Calls Server: " << config.bcfy_calls_server;
    config.status_server = data.value("statusServer", "");
    BOOST_LOG_TRIVIAL(info) << "Status Server: " << config.status_server;
    config.instance_key = data.value("instanceKey", "");
    BOOST_LOG_TRIVIAL(info) << "Instance Key: " << config.instance_key;
    config.instance_id = data.value("instanceId", "");
    BOOST_LOG_TRIVIAL(info) << "Instance Id: " << config.instance_id;
    config.broadcast_signals = data.value("broadcastSignals", false);
    BOOST_LOG_TRIVIAL(info) << "Broadcast Signals: " << config.broadcast_signals;
    config.default_mode = data.value("defaultMode", "digital");
    BOOST_LOG_TRIVIAL(info) << "Default Mode: " << config.default_mode;
    config.call_timeout = data.value("callTimeout", 3);
    BOOST_LOG_TRIVIAL(info) << "Call Timeout (seconds): " << config.call_timeout;
    config.control_message_warn_rate = data.value("controlWarnRate", 10);
    BOOST_LOG_TRIVIAL(info) << "Control channel warning rate: " << config.control_message_warn_rate;
    config.control_retune_limit = data.value("controlRetuneLimit", 0);
    BOOST_LOG_TRIVIAL(info) << "Control channel retune limit: " << config.control_retune_limit;
    config.soft_vocoder = data.value("softVocoder", false);
    BOOST_LOG_TRIVIAL(info) << "Phase 1 Software Vocoder: " << config.soft_vocoder;
    config.enable_audio_streaming = data.value("audioStreaming", false);
    BOOST_LOG_TRIVIAL(info) << "Enable Audio Streaming: " << config.enable_audio_streaming;
    config.record_uu_v_calls = data.value("recordUUVCalls", true);
    BOOST_LOG_TRIVIAL(info) << "Record Unit to Unit Voice Calls: " << config.record_uu_v_calls;
    config.new_call_from_update = data.value("newCallFromUpdate", true);
    BOOST_LOG_TRIVIAL(info) << "New Call from UPDATE Messages" << config.new_call_from_update;
    std::string frequency_format_string = data.value("frequencyFormat", "mhz");

    if (boost::iequals(frequency_format_string, "mhz")) {
      frequency_format = 1;
    } else if (boost::iequals(frequency_format_string, "hz")) {
      frequency_format = 2;
    } else {
      frequency_format = 0;
    }
    config.frequency_format = frequency_format;
    BOOST_LOG_TRIVIAL(info) << "Frequency format: " << get_frequency_format();

    statusAsString = data.value("statusAsString", statusAsString);
    BOOST_LOG_TRIVIAL(info) << "Status as String: " << statusAsString;
    std::string log_level = data.value("logLevel", "info");
    BOOST_LOG_TRIVIAL(info) << "Log Level: " << log_level;
    std::string file_log_level = pt.get<std::string>("logFileLevel", log_level);
    BOOST_LOG_TRIVIAL(info) << "File log Level: " << file_log_level;

    boost::log::trivial::severity_level console_log_sev_level = boost::log::trivial::info;
    parse_logging_level(log_level, &console_log_sev_level);

    boost::log::trivial::severity_level file_log_sev_level = console_log_sev_level;
    parse_logging_level(file_log_level, &file_log_sev_level);

    if (console_sink) {
      console_sink->set_filter(boost::log::trivial::severity >= console_log_sev_level);
    }
    if (log_file_sink) {
      log_file_sink->set_filter(boost::log::trivial::severity >= file_log_sev_level);
    }

    boost::log::core::get()->set_filter(
        boost::log::trivial::severity >= min(console_log_sev_level, file_log_sev_level));

    config.debug_recorder = data.value("debugRecorder", 0);
    config.debug_recorder_address = data.value("debugRecorderAddress", "127.0.0.1");
    config.debug_recorder_port = data.value("debugRecorderPort", 1234);

    BOOST_LOG_TRIVIAL(info) << "\n-------------------------------------\nSYSTEMS\n-------------------------------------\n";


    for (json element : data["systems"]) {
      if (!element.value("enabled", true))
        continue;

      // each system should have a unique index value;
      System *system = System::make(sys_count++);

      std::stringstream default_script;
      unsigned long sys_id;
      unsigned long wacn;
      unsigned long nac;
      default_script << "sys_" << sys_count;

      BOOST_LOG_TRIVIAL(info) << "\n\nSystem Number: " << sys_count << "\n-------------------------------------\n";
      system->set_short_name(element.value("shortName", default_script.str()));
      BOOST_LOG_TRIVIAL(info) << "Short Name: " << system->get_short_name();

      system->set_system_type(element["type"]);
      BOOST_LOG_TRIVIAL(info) << "System Type: " << system->get_system_type();

      // If it is a conventional System
      if ((system->get_system_type() == "conventional") || (system->get_system_type() == "conventionalP25") || (system->get_system_type() == "conventionalDMR")) {

        bool channel_file_exist = element.contains("channelFile");
        bool channels_exist = element.contains("channels");

        if (channel_file_exist && channels_exist) {
          BOOST_LOG_TRIVIAL(error) << "Both \"channels\" and \"channelFile\" cannot be defined for a system!";
          return false;
        }

        if (channels_exist) {
          BOOST_LOG_TRIVIAL(info) << "Conventional Channels: ";
          std::vector<double> channels = element["channels"];
          for (auto &channel : channels) {
            BOOST_LOG_TRIVIAL(info) << "  " << format_freq(channel);
            system->add_channel(channel);
          }
        } else if (channel_file_exist) {
          std::string channel_file = element["channelFile"];
          BOOST_LOG_TRIVIAL(info) << "Channel File: " << channel_file;
          system->set_channel_file(channel_file);
        } else {
          BOOST_LOG_TRIVIAL(error) << "Either \"channels\" or \"channelFile\" need to be defined for a conventional system!";
          return false;
        }
        // If it is a Trunked System
      } else if ((system->get_system_type() == "smartnet") || (system->get_system_type() == "p25")) {
        BOOST_LOG_TRIVIAL(info) << "Control Channels: ";
        std::vector<double> control_channels = element["control_channels"];
        for (auto &control_channel : control_channels) {
          system->add_control_channel(control_channel);
        }
        for (double control_channel : control_channels) {
          BOOST_LOG_TRIVIAL(info) << "  " << format_freq(control_channel);
        }
        system->set_talkgroups_file(element.value("talkgroupsFile", ""));
        BOOST_LOG_TRIVIAL(info) << "Talkgroups File: " << system->get_talkgroups_file();
      } else {
        BOOST_LOG_TRIVIAL(error) << "System Type in config.json not recognized";
        return false;
      }

      bool qpsk_mod = true;
      double digital_levels = element.value("digitalLevels", 1.0);
      double analog_levels = element.value("analogLevels", 8.0);
      double squelch_db = element.value("squelch", -160);
      int max_dev = element.value("maxDev", 4000);
      double filter_width = element.value("filterWidth", 1.0);
      bool conversation_mode = element.value("conversationMode", true);

      if (element.contains("modulation")) {
        system_modulation = element["modulation"];

        if (boost::iequals(system_modulation, "qpsk")) {
          qpsk_mod = true;
          BOOST_LOG_TRIVIAL(info) << "Modulation: qpsk";
        } else if (boost::iequals(system_modulation, "fsk4")) {
          qpsk_mod = false;
          BOOST_LOG_TRIVIAL(info) << "Modulation: fsk4";
        } else {
          qpsk_mod = true;
          BOOST_LOG_TRIVIAL(error) << "! System Modulation Not Specified, could be fsk4 or qpsk, assuming qpsk";
        }
      } else {
        qpsk_mod = true;
      }

      system->set_squelch_db(squelch_db);
      system->set_analog_levels(analog_levels);
      system->set_digital_levels(digital_levels);
      system->set_qpsk_mod(qpsk_mod);
      system->set_max_dev(max_dev);
      system->set_filter_width(filter_width);
      system->set_conversation_mode(conversation_mode);
      BOOST_LOG_TRIVIAL(info) << "Conversation Mode: " << conversation_mode;
      BOOST_LOG_TRIVIAL(info) << "Analog Recorder Maximum Deviation: " << element.value("maxDev", 4000);
      BOOST_LOG_TRIVIAL(info) << "Filter Width: " << filter_width;
      BOOST_LOG_TRIVIAL(info) << "Squelch: " << element.value("squelch", -160);
      system->set_api_key(element.value("apiKey", ""));
      BOOST_LOG_TRIVIAL(info) << "API Key: " << system->get_api_key();
      system->set_bcfy_api_key(element.value("broadcastifyApiKey", ""));
      BOOST_LOG_TRIVIAL(info) << "Broadcastify API Key: " << system->get_bcfy_api_key();
      system->set_bcfy_system_id(element.value("broadcastifySystemId", 0));
      BOOST_LOG_TRIVIAL(info) << "Broadcastify Calls System ID: " << system->get_bcfy_system_id();
      system->set_upload_script(element.value("uploadScript", ""));
      BOOST_LOG_TRIVIAL(info) << "Upload Script: " << system->get_upload_script();
      system->set_compress_wav(element.value("compressWav", true));
      BOOST_LOG_TRIVIAL(info) << "Compress .wav Files: " << system->get_compress_wav();
      system->set_call_log(element.value("callLog", true));
      BOOST_LOG_TRIVIAL(info) << "Call Log: " << system->get_call_log();
      system->set_audio_archive(element.value("audioArchive", true));
      BOOST_LOG_TRIVIAL(info) << "Audio Archive: " << system->get_audio_archive();
      system->set_transmission_archive(element.value("transmissionArchive", false));
      BOOST_LOG_TRIVIAL(info) << "Transmission Archive: " << system->get_transmission_archive();
      system->set_unit_tags_file(element.value("unitTagsFile", ""));
      BOOST_LOG_TRIVIAL(info) << "Unit Tags File: " << system->get_unit_tags_file();
      system->set_record_unknown(element.value("recordUnknown", true));
      BOOST_LOG_TRIVIAL(info) << "Record Unknown Talkgroups: " << system->get_record_unknown();
      system->set_mdc_enabled(element.value("decodeMDC", false));
      BOOST_LOG_TRIVIAL(info) << "Decode MDC: " << system->get_mdc_enabled();
      system->set_fsync_enabled(element.value("decodeFSync", false));
      BOOST_LOG_TRIVIAL(info) << "Decode FSync: " << system->get_fsync_enabled();
      system->set_star_enabled(element.value("decodeStar", false));
      BOOST_LOG_TRIVIAL(info) << "Decode Star: " << system->get_star_enabled();
      system->set_tps_enabled(element.value("decodeTPS", false));
      BOOST_LOG_TRIVIAL(info) << "Decode TPS: " << system->get_tps_enabled();
      std::string talkgroup_display_format_string = element.value("talkgroupDisplayFormat", "Id");
      if (boost::iequals(talkgroup_display_format_string, "id_tag")) {
        system->set_talkgroup_display_format(talkGroupDisplayFormat_id_tag);
      } else if (boost::iequals(talkgroup_display_format_string, "tag_id")) {
        system->set_talkgroup_display_format(talkGroupDisplayFormat_tag_id);
      } else {
        system->set_talkgroup_display_format(talkGroupDisplayFormat_id);
      }
      BOOST_LOG_TRIVIAL(info) << "Talkgroup Display Format: " << talkgroup_display_format_string;

      sys_id = element.value("sysId", 0);
      nac = element.value("nac", 0);
      wacn = element.value("wacn", 0);
      system->set_xor_mask(sys_id, wacn, nac);
      system->set_bandplan(element.value("bandplan", "800_standard"));
      system->set_bandfreq(800); // Default to 800

      if (boost::starts_with(system->get_bandplan(), "400")) {
        system->set_bandfreq(400);
      }
      system->set_bandplan_base(element.value("bandplanBase", 0.0));
      system->set_bandplan_high(element.value("bandplanHigh", 0.0));
      system->set_bandplan_spacing(element.value("bandplanSpacing", 0.0));
      system->set_bandplan_offset(element.value("bandplanOffset", 0));

      if (system->get_system_type() == "smartnet") {
        BOOST_LOG_TRIVIAL(info) << "Smartnet bandplan: " << system->get_bandplan();
        BOOST_LOG_TRIVIAL(info) << "Smartnet band: " << system->get_bandfreq();

        if (system->get_bandplan_base() || system->get_bandplan_spacing() || system->get_bandplan_offset()) {
          BOOST_LOG_TRIVIAL(info) << "Smartnet bandplan base freq: " << system->get_bandplan_base();
          BOOST_LOG_TRIVIAL(info) << "Smartnet bandplan high freq: " << system->get_bandplan_high();
          BOOST_LOG_TRIVIAL(info) << "Smartnet bandplan spacing: " << system->get_bandplan_spacing();
          BOOST_LOG_TRIVIAL(info) << "Smartnet bandplan offset: " << system->get_bandplan_offset();
        }
      }

      system->set_hideEncrypted(element.value("hideEncrypted", system->get_hideEncrypted()));
      BOOST_LOG_TRIVIAL(info) << "Hide Encrypted Talkgroups: " << system->get_hideEncrypted();
      system->set_hideUnknown(element.value("hideUnknownTalkgroups", system->get_hideUnknown()));
      BOOST_LOG_TRIVIAL(info) << "Hide Unknown Talkgroups: " << system->get_hideUnknown();
      system->set_min_duration(element.value("minDuration", 0.0));
      BOOST_LOG_TRIVIAL(info) << "Minimum Call Duration (in seconds): " << system->get_min_duration();
      system->set_max_duration(element.value("maxDuration", 0.0));
      BOOST_LOG_TRIVIAL(info) << "Maximum Call Duration (in seconds): " << system->get_max_duration();
      system->set_min_tx_duration(element.value("minTransmissionDuration", 0.0));
      BOOST_LOG_TRIVIAL(info) << "Minimum Transmission Duration (in seconds): " << system->get_min_tx_duration();
      system->set_multiSite(element.value("multiSite", false));
      BOOST_LOG_TRIVIAL(info) << "Multiple Site System: " << system->get_multiSite();
      system->set_multiSiteSystemName(element.value("multiSiteSystemName", ""));
      BOOST_LOG_TRIVIAL(info) << "Multiple Site System Name: " << system->get_multiSiteSystemName();
      system->set_multiSiteSystemNumber(element.value("multiSiteSystemNumber", 0));
      BOOST_LOG_TRIVIAL(info) << "Multiple Site System Number: " << system->get_multiSiteSystemNumber();

      if (!system->get_compress_wav()) {
        if ((system->get_api_key().length() > 0) || (system->get_bcfy_api_key().length() > 0)) {
          BOOST_LOG_TRIVIAL(error) << "Compress WAV must be set to true if you are using OpenMHz or Broadcastify";
          return false;
        }
      }

      systems.push_back(system);
      BOOST_LOG_TRIVIAL(info);
    }

    BOOST_LOG_TRIVIAL(info) << "\n\n-------------------------------------\nSOURCES\n-------------------------------------\n";
    for (json element : data["sources"]) {
      if (!element.value("enabled", true))
        continue;

      bool gain_set = false;
      int silence_frames = element.value("silenceFrames", 0);
      double center = element.value("center", 0.0);
      double rate = element.value("rate", 0.0);
      double error = element.value("error", 0.0);
      double ppm = element.value("ppm", 0.0);
      bool agc = element.value("agc", false);
      int gain = element.value("gain", 0);
      int if_gain = element.value("ifGain", 0);
      int bb_gain = element.value("bbGain", 0);
      int mix_gain = element.value("mixGain", 0);
      int lna_gain = element.value("lnaGain", 0);
      int pga_gain = element.value("pgaGain", 0);
      int tia_gain = element.value("tiaGain", 0);
      int amp_gain = element.value("ampGain", 0);
      int vga_gain = element.value("vgaGain", 0);
      int vga1_gain = element.value("vga1Gain", 0);
      int vga2_gain = element.value("vga2Gain", 0);

      std::string antenna = element.value("antenna", "");
      int digital_recorders = element.value("digitalRecorders", 0);
      int sigmf_recorders = element.value("sigmfRecorders", 0);
      int analog_recorders = element.value("analogRecorders", 0);

      std::string driver = element.value("driver", "");

      if ((driver != "osmosdr") && (driver != "usrp")) {
        BOOST_LOG_TRIVIAL(error) << "Driver specified in config.json not recognized, needs to be osmosdr or usrp";
      }

      std::string device = element.value("device", "");
      BOOST_LOG_TRIVIAL(info) << "Driver: " << element.value("driver", "");
      BOOST_LOG_TRIVIAL(info) << "Center: " << format_freq(element.value("center", 0.0));
      BOOST_LOG_TRIVIAL(info) << "Rate: " << FormatSamplingRate(element.value("rate", 0.0));
      BOOST_LOG_TRIVIAL(info) << "Error: " << element.value("error", 0.0);
      BOOST_LOG_TRIVIAL(info) << "PPM Error: " << element.value("ppm", 0.0);
      BOOST_LOG_TRIVIAL(info) << "Auto gain control: " << element.value("agc", false);
      BOOST_LOG_TRIVIAL(info) << "Gain: " << element.value("gain", 0);
      BOOST_LOG_TRIVIAL(info) << "IF Gain: " << element.value("ifGain", 0);
      BOOST_LOG_TRIVIAL(info) << "BB Gain: " << element.value("bbGain", 0);
      BOOST_LOG_TRIVIAL(info) << "LNA Gain: " << element.value("lnaGain", 0);
      BOOST_LOG_TRIVIAL(info) << "PGA Gain: " << element.value("pgaGain", 0);
      BOOST_LOG_TRIVIAL(info) << "TIA Gain: " << element.value("tiaGain", 0);
      BOOST_LOG_TRIVIAL(info) << "MIX Gain: " << element.value("mixGain", 0);
      BOOST_LOG_TRIVIAL(info) << "AMP Gain: " << element.value("ampGain", 0);
      BOOST_LOG_TRIVIAL(info) << "VGA Gain: " << element.value("vgaGain", 0);
      BOOST_LOG_TRIVIAL(info) << "VGA1 Gain: " << element.value("vga1Gain", 0);
      BOOST_LOG_TRIVIAL(info) << "VGA2 Gain: " << element.value("vga2Gain", 0);
      BOOST_LOG_TRIVIAL(info) << "Idle Silence: " << element.value("silenceFrame", 0);
      BOOST_LOG_TRIVIAL(info) << "Digital Recorders: " << element.value("digitalRecorders", 0);
      BOOST_LOG_TRIVIAL(info) << "SigMF Recorders: " << element.value("sigmfRecorders", 0);
      BOOST_LOG_TRIVIAL(info) << "Analog Recorders: " << element.value("analogRecorders", 0);

      if ((ppm != 0) && (error != 0)) {
        BOOST_LOG_TRIVIAL(info) << "Both PPM and Error should not be set at the same time. Setting Error to 0.";
        error = 0;
      }

      auto *source = new Source(center, rate, error, driver, device, &config);
      BOOST_LOG_TRIVIAL(info) << "Max Frequency: " << format_freq(source->get_max_hz());
      BOOST_LOG_TRIVIAL(info) << "Min Frequency: " << format_freq(source->get_min_hz());

      // SoapySDRPlay3 quirk: autogain must be disabled before any of the gains can be set
      if (source->get_device().find("sdrplay") != std::string::npos) {
        source->set_gain_mode(agc);
      }

      if (element.contains("gainSettings")) {
        for (auto it = element["gainSettings"].begin(); it != element["gainSettings"].end(); ++it) {

          source->set_gain_by_name(it.key(), it.value());
          gain_set = true;
        }
      }

      if (if_gain != 0) {
        gain_set = true;
        source->set_gain_by_name("IF", if_gain);
      }

      if (bb_gain != 0) {
        gain_set = true;
        source->set_gain_by_name("BB", bb_gain);
      }

      if (mix_gain != 0) {
        gain_set = true;
        source->set_gain_by_name("MIX", mix_gain);
      }

      if (lna_gain != 0) {
        gain_set = true;
        source->set_gain_by_name("LNA", lna_gain);
      }

      if (tia_gain != 0) {
        gain_set = true;
        source->set_gain_by_name("TIA", tia_gain);
      }

      if (pga_gain != 0) {
        gain_set = true;
        source->set_gain_by_name("PGA", pga_gain);
      }

      if (amp_gain != 0) {
        gain_set = true;
        source->set_gain_by_name("AMP", amp_gain);
      }

      if (vga_gain != 0) {
        gain_set = true;
        source->set_gain_by_name("VGA", vga_gain);
      }

      if (vga1_gain != 0) {
        gain_set = true;
        source->set_gain_by_name("VGA1", vga1_gain);
      }

      if (vga2_gain != 0) {
        gain_set = true;
        source->set_gain_by_name("VGA2", vga2_gain);
      }

      if (gain != 0) {
        gain_set = true;
        source->set_gain(gain);
      }

      if (!gain_set) {
        BOOST_LOG_TRIVIAL(error) << "! No Gain was specified! Things will probably not work";
      }

      source->set_gain_mode(agc);
      source->set_antenna(antenna);
      source->set_silence_frames(silence_frames);

      if (ppm != 0) {
        source->set_freq_corr(ppm);
      }
      source->create_digital_recorders(tb, digital_recorders);
      source->create_analog_recorders(tb, analog_recorders);
      source->create_sigmf_recorders(tb, sigmf_recorders);
      if (config.debug_recorder) {
        source->create_debug_recorder(tb, source_count);
      }

      sources.push_back(source);
      source_count++;
      BOOST_LOG_TRIVIAL(info) << "\n-------------------------------------\n\n";
    }

    BOOST_LOG_TRIVIAL(info) << "\n\n-------------------------------------\nPLUGINS\n-------------------------------------\n";
    add_internal_plugin("openmhz_uploader", "libopenmhz_uploader.so", data);
    add_internal_plugin("broadcastify_uploader", "libbroadcastify_uploader.so", data);
    add_internal_plugin("unit_script", "libunit_script.so", data);
    add_internal_plugin("stat_socket", "libstat_socket.so", data);
    initialize_plugins(data, &config, sources, systems);
  } catch (std::exception const &e) {
    BOOST_LOG_TRIVIAL(error) << "Failed parsing Config: " << e.what();
    return false;
  }
  if (config.debug_recorder) {
    BOOST_LOG_TRIVIAL(info) << "\n\n-------------------------------------\nDEBUG RECORDER\n-------------------------------------\n";
    BOOST_LOG_TRIVIAL(info) << "  Address: " << config.debug_recorder_address;

    for (auto source : sources) {
      BOOST_LOG_TRIVIAL(info) << "  " << source->get_driver() << " - " << source->get_device() << " [ " << format_freq(source->get_center()) << " ]  Port: " << source->get_debug_recorder_port();
    }
    BOOST_LOG_TRIVIAL(info) << "\n\n-------------------------------------\n";
  }
  BOOST_LOG_TRIVIAL(info) << "\n\n";
  return true;
}
