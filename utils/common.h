#pragma once

#include "../acfu.h"

namespace acfu {

class version_error: public std::logic_error {
 public:
  version_error(): std::logic_error("invalid version string format") {}
};

template <class t_service, class t_func>
void for_each_service(t_func func) {
  service_enum_t<t_service> e;
  for (service_ptr_t<t_service> ptr; e.next(ptr);) {
    func(ptr);
  }
}

int compare_versions(const char* version1, const char* version2, const char* prefix = NULL);
pfc::list_t<int> parse_version_string(const char* str, const char* prefix = NULL);

inline int compare_versions(const char* version1, const char* version2, const char* prefix) {
  auto parts1 = parse_version_string(version1, prefix);
  auto parts2 = parse_version_string(version2, prefix);
  for (size_t i = 0, count = pfc::max_t(parts1.get_count(), parts2.get_count()); i < count; i ++) {
    int x1 = i < parts1.get_count() ? parts1[i] : 0;
    int x2 = i < parts2.get_count() ? parts2[i] : 0;
    if (int diff = x1 - x2) {
      return diff;
    }
  }
  return 0;
}

inline pfc::list_t<int> parse_version_string(const char* str, const char* prefix) {
  if (!str) {
    return {};
  }
  while (isspace(*str)) { // trim whitespace
    str ++;
  }
  if (prefix && 0 == strncmp(str, prefix, strlen(prefix))) {
    str += strlen(prefix);
  }
  pfc::list_t<int> parts;
  for (auto start = str; ; str ++) {
    if ('.' == *str || '\0' == *str) {
      if (start == str) {
        throw version_error();
      }
      parts.add_item(pfc::atodec<int>(start, str - start));
      if ('\0' == *str) {
        break;
      }
      start = str + 1;
    }
    else if (*str < '0' || *str > '9') {
      while (isspace(*str)) { // trim trailing whitespace
        str ++;
      }
      if ('\0' != *str) {
        throw version_error();
      }
    }
  }
  if (0 == parts.get_count()) {
    throw version_error();
  }
  return parts;
}

} // namespace acfu
