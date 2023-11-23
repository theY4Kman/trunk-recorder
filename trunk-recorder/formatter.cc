#include "formatter.h"
#include <boost/lexical_cast.hpp>

int frequency_format = 0;
bool statusAsString = true;

boost::format format_freq(double f) {
  if (frequency_format == 1)
    return boost::format("%10.6f MHz") % (f / 1000000.0);
  else if (frequency_format == 2)
    return boost::format("%.0f Hz") % f;
  else
    return boost::format("%e") % f;
}

std::string get_frequency_format() {
  if (frequency_format == 1)
    return "mhz";
  else if (frequency_format == 2)
    return "hz";
  else
    return "exp";
}

boost::format FormatSamplingRate(float f) {
  return boost::format("%.0f") % f;
}

boost::format format_time(float f) {
  return boost::format("%5.2f") % f;
}

std::string format_state(State state) {
  if (statusAsString) {
    if (state == MONITORING)
      return "monitoring";
    else if (state == RECORDING)
      return "recording";
    else if (state == INACTIVE)
      return "inactive";
    else if (state == ACTIVE)
      return "active";
    else if (state == IDLE)
      return "idle";
    else if (state == STOPPED)
      return "stopped";
    else if (state == AVAILABLE)
      return "available";
    else if (state == IGNORE)
      return "ignore";
    return "Unknown";
  }
  return boost::lexical_cast<std::string>(state);
}

std::string format_state(State state, MonitoringState monitoringState) {
  if (statusAsString) {
    if (state == MONITORING){
      if(monitoringState == UNKNOWN_TG)
        return "monitoring : UNKNOWN TG";
      else if(monitoringState == IGNORED_TG)
        return "monitoring : IGNORED TG";
      else if(monitoringState == NO_SOURCE)
        return "monitoring : NO SOURCE COVERING FREQ";
      else if(monitoringState == NO_RECORDER)
        return "monitoring : NO RECORDER AVAILABLE";
      else if(monitoringState == ENCRYPTED)
        return "monitoring : ENCRYPTED";
      else if(monitoringState == DUPLICATE)
        return "monitoring : DUPLICATE";
      else if(monitoringState == SUPERSEDED)
        return "monitoring : SUPERSEDED";
      else
        return "monitoring";
    }
    else if (state == RECORDING)
      return "recording";
    else if (state == INACTIVE)
      return "inactive";
    else if (state == ACTIVE)
      return "active";
    else if (state == IDLE)
      return "idle";
    else if (state == STOPPED)
      return "stopped";
    else if (state == AVAILABLE)
      return "available";
    return "Unknown";
  }
  return boost::lexical_cast<std::string>(state);
}

