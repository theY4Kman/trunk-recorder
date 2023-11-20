#ifndef TALKGROUPS_H
#define TALKGROUPS_H

#include "talkgroup.h"
#include <boost/algorithm/string.hpp>
#include <string>
#include <vector>

class Talkgroups {
  std::vector<Talkgroup *> talkgroups;

public:
  Talkgroups();
  void load_talkgroups(int sys_num, std::string filename);
  void load_channels(int sys_num, std::string filename);
  [[nodiscard]] Talkgroup *find_talkgroup(int sys_num, long tg) const;
  [[nodiscard]] Talkgroup *find_talkgroup_by_freq(int sys_num, double freq) const;
  std::vector<Talkgroup *> get_talkgroups();
};
#endif // TALKGROUPS_H
