#include "unit_tag.h"
#include <boost/regex.hpp>

UnitTag::UnitTag(const std::string &p, const std::string &t) {
  pattern = p;
  tag = t;
}
