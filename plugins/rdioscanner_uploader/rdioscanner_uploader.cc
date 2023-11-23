#include <curl/curl.h>
#include <iomanip>
#include <time.h>
#include <vector>

#include "../../trunk-recorder/call_concluder/call_concluder.h"
#include "../../trunk-recorder/plugin_manager/plugin_api.h"
#include "../trunk-recorder/gr_blocks/decoder_wrapper.h"
#include <boost/algorithm/string.hpp>
#include <boost/dll/alias.hpp> // for BOOST_DLL_ALIAS
#include <boost/foreach.hpp>
#include <sys/stat.h>

struct Rdio_Scanner_System {
  std::string api_key;
  std::string short_name;
  uint32_t    system_id;
  std::string talkgroupsFile;
  Talkgroups *talkgroups;
  bool compress_wav;
};

struct Rdio_Scanner_Uploader_Data {
  std::vector<Rdio_Scanner_System> systems;
  std::string server;
};

class Rdio_Scanner_Uploader : public Plugin_Api {
  Rdio_Scanner_Uploader_Data data;

public:
  Rdio_Scanner_System *get_system(const std::string &short_name) {
    for (auto &system : data.systems) {
      if (system.short_name == short_name) {
        return &system;
      }
    }
    return nullptr;
  }

  static size_t write_callback(void *contents, const size_t size, const size_t nmemb, void *userp) {
    static_cast<std::string *>(userp)->append(static_cast<char *>(contents), size * nmemb);
    return size * nmemb;
  }

  int upload(Call_Data_t call_info) {
    std::string api_key;
    uint32_t system_id;
    std::string talkgroup_group = call_info.talkgroup_group;
    std::string talkgroup_tag = call_info.talkgroup_tag;
    std::string talkgroup_alpha_tag = call_info.talkgroup_alpha_tag;
    std::string talkgroup_description = call_info.talkgroup_description;
    bool compress_wav = false;
    Rdio_Scanner_System *sys = get_system(call_info.short_name);

    if (call_info.encrypted) {
      return 0;
    }

    if (sys) {
      api_key = sys->api_key;
      compress_wav = call_info.compress_wav;
      system_id = sys->system_id;
    }

    if (api_key.empty()) {
      return 0;
    }

    std::ostringstream freq;
    std::string freq_string;
    freq << std::fixed << std::setprecision(0);
    freq << call_info.freq;

    std::ostringstream call_length;
    std::string call_length_string;
    call_length << std::fixed << std::setprecision(0);
    call_length << call_info.length;

    std::ostringstream source_list;
    std::string source_list_string;
    source_list << std::fixed << std::setprecision(2);
    source_list << "[";

    std::ostringstream patch_list;
    std::string patch_list_string;
    patch_list << std::fixed << std::setprecision(2);
    patch_list << "[";

    boost::filesystem::path audioPath(compress_wav ? call_info.converted : call_info.filename);
    boost::filesystem::path audioName = audioPath.filename();

    if (!call_info.transmission_source_list.empty()) {
      for (unsigned long i = 0; i < call_info.transmission_source_list.size(); i++) {
        source_list << "{ \"pos\": " << std::setprecision(2) << call_info.transmission_source_list[i].position << ", \"src\": " << std::setprecision(0) << call_info.transmission_source_list[i].source << " }";

        if (i < (call_info.transmission_source_list.size() - 1)) {
          source_list << ", ";
        } else {
          source_list << "]";
        }
      }
    } else {
      source_list << "]";
    }

    if (call_info.patched_talkgroups.size() > 1) {
      for (unsigned long i = 0; i < call_info.patched_talkgroups.size(); i++) {
        if (i != 0) {
          patch_list << ",";
        }
        patch_list << static_cast<int>(call_info.patched_talkgroups[i]);
      }
      patch_list << "]";
    } else {
      patch_list << "]";
    }

    std::ostringstream freq_list;
    std::string freq_list_string;
    freq_list << std::fixed << std::setprecision(2);
    freq_list << "[";

    if (!call_info.transmission_error_list.empty()) {
      for (std::size_t i = 0; i < call_info.transmission_error_list.size(); i++) {
        freq_list << "{\"freq\": " << std::fixed << std::setprecision(0) << call_info.freq << ", \"time\": " << call_info.transmission_error_list[i].time << ", \"pos\": " << std::fixed << std::setprecision(2) << call_info.transmission_error_list[i].position << ", \"len\": " << call_info.transmission_error_list[i].total_len << ", \"errorCount\": " << std::setprecision(0) << call_info.transmission_error_list[i].error_count << ", \"spikeCount\": " << call_info.transmission_error_list[i].spike_count << "}";
        if (i < (call_info.transmission_error_list.size() - 1)) {
          freq_list << ", ";
        } else {
          freq_list << "]";
        }
      }
    } else {
      freq_list << "]";
    }

    // BOOST_LOG_TRIVIAL(error) << "Got source list: " << source_list.str();
    CURL *curl;
    CURLMcode res;
    CURLM *multi_handle;
    int still_running = 0;
    std::string response_buffer;
    freq_string = freq.str();

    source_list_string = source_list.str();
    freq_list_string = freq_list.str();
    call_length_string = call_length.str();
    patch_list_string = patch_list.str();

    struct curl_httppost *formpost = nullptr;
    struct curl_httppost *lastptr = nullptr;
    struct curl_slist *headerlist = nullptr;

    /* Fill in the file upload field. This makes libcurl load data from
     the given file name when curl_easy_perform() is called. */
    curl_formadd(&formpost,
                 &lastptr,
                 CURLFORM_COPYNAME, "audio",
                 CURLFORM_FILE, compress_wav ? call_info.converted : call_info.filename,
                 CURLFORM_CONTENTTYPE, "application/octet-stream",
                 CURLFORM_END);

    curl_formadd(&formpost,
                 &lastptr,
                 CURLFORM_COPYNAME, "audioName",
                 CURLFORM_COPYCONTENTS, audioName.c_str(),
                 CURLFORM_END);

    curl_formadd(&formpost,
                 &lastptr,
                 CURLFORM_COPYNAME, "audioType",
                 CURLFORM_COPYCONTENTS, compress_wav ? "audio/mp4" : "audio/wav",
                 CURLFORM_END);

    curl_formadd(&formpost,
                 &lastptr,
                 CURLFORM_COPYNAME, "dateTime",
                 CURLFORM_COPYCONTENTS, boost::lexical_cast<std::string>(call_info.start_time).c_str(),
                 CURLFORM_END);

    curl_formadd(&formpost,
                 &lastptr,
                 CURLFORM_COPYNAME, "frequencies",
                 CURLFORM_COPYCONTENTS, freq_list_string.c_str(),
                 CURLFORM_END);

    curl_formadd(&formpost,
                 &lastptr,
                 CURLFORM_COPYNAME, "frequency",
                 CURLFORM_COPYCONTENTS, freq_string.c_str(),
                 CURLFORM_END);

    curl_formadd(&formpost,
                 &lastptr,
                 CURLFORM_COPYNAME, "key",
                 CURLFORM_COPYCONTENTS, api_key.c_str(),
                 CURLFORM_END);

    curl_formadd(&formpost,
                 &lastptr,
                 CURLFORM_COPYNAME, "patches",
                 CURLFORM_COPYCONTENTS, patch_list_string.c_str(),
                 CURLFORM_END);

    curl_formadd(&formpost,
                 &lastptr,
                 CURLFORM_COPYNAME, "talkgroup",
                 CURLFORM_COPYCONTENTS, boost::lexical_cast<std::string>(call_info.talkgroup).c_str(),
                 CURLFORM_END);

    curl_formadd(&formpost,
                 &lastptr,
                 CURLFORM_COPYNAME, "talkgroupGroup",
                 CURLFORM_COPYCONTENTS, boost::lexical_cast<std::string>(call_info.talkgroup_group).c_str(),
                 CURLFORM_END);

    curl_formadd(&formpost,
                 &lastptr,
                 CURLFORM_COPYNAME, "talkgroupLabel",
                 CURLFORM_COPYCONTENTS, boost::lexical_cast<std::string>(call_info.talkgroup_alpha_tag).c_str(),
                 CURLFORM_END);

    curl_formadd(&formpost,
                 &lastptr,
                 CURLFORM_COPYNAME, "talkgroupTag",
                 CURLFORM_COPYCONTENTS, boost::lexical_cast<std::string>(call_info.talkgroup_tag).c_str(),
                 CURLFORM_END);

    curl_formadd(&formpost,
                 &lastptr,
                 CURLFORM_COPYNAME, "talkgroupName",
                 CURLFORM_COPYCONTENTS, boost::lexical_cast<std::string>(call_info.talkgroup_description).c_str(),
                 CURLFORM_END);

    curl_formadd(&formpost,
                 &lastptr,
                 CURLFORM_COPYNAME, "sources",
                 CURLFORM_COPYCONTENTS, source_list_string.c_str(),
                 CURLFORM_END);

    curl_formadd(&formpost,
                 &lastptr,
                 CURLFORM_COPYNAME, "system",
                 CURLFORM_COPYCONTENTS, std::to_string(system_id).c_str(),
                 CURLFORM_END);

    curl_formadd(&formpost,
                 &lastptr,
                 CURLFORM_COPYNAME, "systemLabel",
                 CURLFORM_COPYCONTENTS, call_info.short_name.c_str(),
                 CURLFORM_END);

    curl = curl_easy_init();
    multi_handle = curl_multi_init();

    /* initialize custom header list (stating that Expect: 100-continue is not wanted */
    headerlist = curl_slist_append(headerlist, "Expect:");
    if (curl && multi_handle) {
      std::string url = data.server + "/api/call-upload";

      /* what URL that receives this POST */
      curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

      curl_easy_setopt(curl, CURLOPT_USERAGENT, "TrunkRecorder1.0");
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);
      curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);

      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_buffer);

      curl_multi_add_handle(multi_handle, curl);

      curl_multi_perform(multi_handle, &still_running);

      while (still_running) {
        timeval timeout{};
        int rc;       /* select() return code */
        CURLMcode mc; /* curl_multi_fdset() return code */

        fd_set fdread;
        fd_set fdwrite;
        fd_set fdexcep;
        int maxfd = -1;

        long curl_timeo = -1;

        FD_ZERO(&fdread);
        FD_ZERO(&fdwrite);
        FD_ZERO(&fdexcep);

        /* set a suitable timeout to play around with */
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        curl_multi_timeout(multi_handle, &curl_timeo);
        if (curl_timeo >= 0) {
          timeout.tv_sec = curl_timeo / 1000;
          if (timeout.tv_sec > 1)
            timeout.tv_sec = 1;
          else
            timeout.tv_usec = (curl_timeo % 1000) * 1000;
        }

        /* get file descriptors from the transfers */
        mc = curl_multi_fdset(multi_handle, &fdread, &fdwrite, &fdexcep, &maxfd);

        if (mc != CURLM_OK) {
          fprintf(stderr, "curl_multi_fdset() failed, code %d.\n", mc);
          break;
        }

        /* On success the value of maxfd is guaranteed to be >= -1. We call
         select(maxfd + 1, ...); specially in case of (maxfd == -1) there are
         no fds ready yet so we call select(0, ...) --or Sleep() on Windows--
         to sleep 100ms, which is the minimum suggested value in the
         curl_multi_fdset() doc. */

        if (maxfd == -1) {
          /* Portable sleep for platforms other than Windows. */
          struct timeval wait = {0, 100 * 1000}; /* 100ms */
          rc = select(0, nullptr, nullptr, nullptr, &wait);
        } else {
          /* Note that on some platforms 'timeout' may be modified by select().
           If you need access to the original value save a copy beforehand. */
          rc = select(maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout);
        }

        switch (rc) {
        case -1:
          /* select error */
          break;
        case 0:
        default:
          /* timeout or readable/writable sockets */
          curl_multi_perform(multi_handle, &still_running);
          break;
        }
      }

      res = curl_multi_cleanup(multi_handle);

      /* always cleanup */
      curl_easy_cleanup(curl);

      /* then cleanup the formpost chain */
      curl_formfree(formpost);

      /* free slist */
      curl_slist_free_all(headerlist);

      long response_code;
      curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

      if (res == CURLM_OK && response_code == 200) {
        struct stat file_info {};
        stat(compress_wav ? call_info.converted : call_info.filename, &file_info);

        BOOST_LOG_TRIVIAL(info) << "[" << call_info.short_name << "]\t\033[0;34m" << call_info.call_num << "C\033[0m\tTG: " << call_info.talkgroup_display << "\tFreq: " << format_freq(call_info.freq) << "\tRdio Scanner Upload Success - file size: " << file_info.st_size;
        ;
        return 0;
      }
    }
    BOOST_LOG_TRIVIAL(error) << "[" << call_info.short_name << "]\t\033[0;34m" << call_info.call_num << "C\033[0m\tTG: " << call_info.talkgroup_display << "\tFreq: " << format_freq(call_info.freq) << "\tRdio Scanner Upload Error: " << response_buffer;
    return 1;
  }

  int call_end(const Call_Data_t call_info) override {
    return upload(call_info);
  }

 int parse_config(json config_data) override {
    /*
          system->set_rdioscanner_api_key(node.second.get<std::string>("rdioscannerApiKey", ""));
      BOOST_LOG_TRIVIAL(info) << "Rdio Scanner API Key: " << system->get_rdioscanner_api_key();
      system->set_rdioscanner_system_id(node.second.get<int>("rdioscannerSystemId", 0));
      BOOST_LOG_TRIVIAL(info) << "Rdio Scanner System ID: " << system->get_rdioscanner_system_id();

    config.rdioscanner_server = pt.get<std::string>("rdioscannerServer", "");
    BOOST_LOG_TRIVIAL(info) << "Rdio Scanner Server: " << config.rdioscanner_server;*/

    // Tests to see if the rdioscannerServer value exists in the config file
    if (const bool upload_server_exists = config_data.contains("server"); !upload_server_exists) {
      return 1;
    }

    this->data.server = config_data.value("server", "");
    BOOST_LOG_TRIVIAL(info) << "Rdio Scanner Server: " << this->data.server;

    // from: http://www.zedwood.com/article/cpp-boost-url-regex
    const boost::regex ex("(http|https)://([^/ :]+):?([^/ ]*)(/?[^ #?]*)\\x3f?([^ #]*)#?([^ ]*)");

    if (boost::cmatch what; !regex_match(this->data.server.c_str(), what, ex)) {
      BOOST_LOG_TRIVIAL(error) << "Unable to parse Rdio Scanner Server URL\n";
      return 1;
    }

        // Gets the API key for each system, if defined
      for (const json& element : config_data["systems"]) {
        if (!element.contains("apiKey")) {
          continue;
        }

        Rdio_Scanner_System sys;
        sys.api_key = element.value("apiKey", "");
        sys.system_id = element.value("systemId", 0);
        sys.short_name = element.value("shortName", "");
        BOOST_LOG_TRIVIAL(info) << "Uploading calls for: " << sys.short_name;
        this->data.systems.push_back(sys);
    }

     if (this->data.systems.empty()) {
       BOOST_LOG_TRIVIAL(error) << "Rdio Scanner Server set, but no Systems are configured\n";
       return 1;
     }

    return 0;
  }


 /*
   int start() { return 0; }
   int stop() { return 0; }
   int poll_one() { return 0; }
   int signal(long unitId, const char *signaling_type, gr::blocks::SignalType sig_type, Call *call, System *system, Recorder *recorder) { return 0; }
   int audio_stream(Recorder *recorder, float *samples, int sampleCount) { return 0; }
   int call_start(Call *call) { return 0; }
   int calls_active(std::vector<Call *> calls) { return 0; }
   int setup_recorder(Recorder *recorder) { return 0; }
   int setup_system(System *system) { return 0; }
   int setup_systems(std::vector<System *> systems) { return 0; }
   int setup_sources(std::vector<Source *> sources) { return 0; }
   int setup_config(std::vector<Source *> sources, std::vector<System *> systems) { return 0; }
   int system_rates(std::vector<System *> systems, float timeDiff) { return 0; }
*/
  // Factory method
  static boost::shared_ptr<Rdio_Scanner_Uploader> create() {
    return boost::make_shared<Rdio_Scanner_Uploader>();
  }
};

BOOST_DLL_ALIAS(
    Rdio_Scanner_Uploader::create, // <-- this function is exported with...
    create_plugin                  // <-- ...this alias name
)
