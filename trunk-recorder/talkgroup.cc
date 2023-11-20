#include "talkgroup.h"

#include <utility>

Talkgroup::Talkgroup(const int sys_num, const long num, std::string mode, std::string alpha_tag, std::string description, std::string tag, std::string group, const int priority, const unsigned long preferredNAC) {
  this->sys_num = sys_num;
  this->number = num;
  this->mode = std::move(mode);
  this->alpha_tag = std::move(alpha_tag);
  this->description = std::move(description);
  this->tag = std::move(tag);
  this->group = std::move(group);
  this->priority = priority;
  this->active = false;
  this->freq = 0;
  this->tone = 0;
  this->preferredNAC = preferredNAC;
}

Talkgroup::Talkgroup(const int sys_num, const long num, const double freq, const double tone, std::string alpha_tag, std::string description, std::string tag, std::string group) {
  this->sys_num = sys_num;
  this->number = num;
  this->mode = "Z";
  this->alpha_tag = std::move(alpha_tag);
  this->description = std::move(description);
  this->tag = std::move(tag);
  this->group = std::move(group);
  this->active = false;
  this->freq = freq;
  this->tone = tone;
  this->priority = 0;
  this->preferredNAC = 0;
}

std::string Talkgroup::menu_string() {
  char buff[150];

  // std::ostringstream oss;

  snprintf(buff, 150, "%5lu - %-15s %-20s %-15s %-40s", number, alpha_tag.c_str(), tag.c_str(), group.c_str(), description.c_str());

  // sprintf(buff, "%5lu - %s", number, alpha_tag.c_str());

  std::string buffAsStdStr = buff;

  return buffAsStdStr;
}

int Talkgroup::get_priority() const {
  return priority;
}

unsigned long Talkgroup::get_preferredNAC() const {
  return preferredNAC;
}

void Talkgroup::set_priority(const int new_priority) {
  priority = new_priority;
}

bool Talkgroup::is_active() const {
  return active;
}

void Talkgroup::set_active(bool a) {
  active = a;
}
