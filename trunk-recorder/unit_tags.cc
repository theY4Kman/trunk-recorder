#include "unit_tags.h"

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/intrusive_ptr.hpp>
#include <boost/log/trivial.hpp>
#include <boost/tokenizer.hpp>
#include <boost/regex.hpp>

#include "csv_helper.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>

UnitTags::UnitTags() = default;

void UnitTags::load_unit_tags(std::string filename) {
  if (filename.empty()) {
    return;
  }

  std::ifstream in(filename.c_str());

  if (!in.is_open()) {
    BOOST_LOG_TRIVIAL(error) << "Error Opening Unit Tag File: " << filename << std::endl;
    return;
  }

  boost::escaped_list_separator<char> sep("\\", ",\t", "\"");
  typedef boost::tokenizer<boost::escaped_list_separator<char>> t_tokenizer;

  std::vector<std::string> vec;
  std::string line;

  int lines_read = 0;
  int lines_pushed = 0;

  while (!safeGetline(in, line).eof()) // this works with \r, \n, or \r\n
  {
    if (!line.empty() && (line[line.size() - 1] == '\r')) {
      line = line.substr(0, line.size() - 1);
    }

    lines_read++;

    if (line.empty())
      continue;

    t_tokenizer tok(line, sep);

    // Unit Tag configuration columns:
    //
    // [0] - talkgroup number
    // [1] - tag

    vec.assign(tok.begin(), tok.end());

    if (vec.size() < 2) {
      BOOST_LOG_TRIVIAL(error) << "Malformed talkgroup entry at line " << lines_read << ".";
      continue;
    }

    add(vec[0], vec[1]);

    lines_pushed++;
  }

  if (lines_pushed != lines_read) {
    // The parser above is pretty brittle. This will help with debugging it, for
    // now.
    BOOST_LOG_TRIVIAL(error) << "Warning: skipped " << lines_read - lines_pushed << " of " << lines_read << " unit tag entries! Invalid format?";
    BOOST_LOG_TRIVIAL(error) << "The format is very particular. See  https://github.com/robotastic/trunk-recorder for example input.";
  } else {
    BOOST_LOG_TRIVIAL(info) << "Read " << lines_pushed << " unit tags.";
  }
}

std::string UnitTags::find_unit_tag(const long unitID) const {
  const std::string tg_num_str = std::to_string(unitID);

  for (const auto& unit_tag : unit_tags) {
    if (regex_match(tg_num_str, unit_tag->pattern)) {
      return regex_replace(tg_num_str, unit_tag->pattern, unit_tag->tag, boost::regex_constants::format_no_copy | boost::regex_constants::format_all);
    }
  }

  return "";
}

void UnitTags::add(std::string pattern, const std::string &tag) {
  // If the pattern is like /someregex/
  if (pattern.substr(0, 1) == "/" && pattern.substr(pattern.length()-1, 1) == "/") {
    // then remove the / at the beginning and end
    pattern = pattern.substr(1, pattern.length()-2);
  } else {
    // otherwise add ^ and $ to the pattern e.g. ^123$ to make a regex for simple IDs
    pattern = "^" + pattern + "$";
  }

  auto *unit_tag = new UnitTag(pattern, tag);
  unit_tags.push_back(unit_tag);
}
