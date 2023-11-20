#ifndef TALKGROUP_H
#define TALKGROUP_H

#include <iostream>
#include <cstdio>
#include <string>
//#include <sstream>

class Talkgroup {
public:
  long number;
  std::string mode;
  std::string alpha_tag;
  std::string description;
  std::string tag;
  std::string group;
  int priority;
  int sys_num;


  // For Conventional
  double freq;
  double tone;

  Talkgroup(int sys_num, long num, std::string mode, std::string alpha_tag, std::string description, std::string tag, std::string group, int priority, unsigned long preferredNAC);
  Talkgroup(int sys_num, long num, double freq, double tone, std::string alpha_tag, std::string description, std::string tag, std::string group);

  [[nodiscard]] bool is_active() const;
  [[nodiscard]] int get_priority() const;
  [[nodiscard]] unsigned long get_preferredNAC() const;
  void set_priority(int new_priority);
  void set_active(bool a);
  std::string menu_string();

private:
  unsigned long preferredNAC;
  bool active;
};

#endif
