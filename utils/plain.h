#pragma once

#include "common.h"

/******************************************************************************

This helper can be used in case when plain version string is stored in a file.
Usage example:

#define MY_VERSION "1.0"

class MySource: public acfu::source, public acfu::plain_conf {
 public:
  static const char* get_url() {
    return "https://example.com/version.txt";
  }
  virtual GUID get_guid() {
    static const GUID guid = {...};
    return guid;
  }
  virtual void get_info(file_info& info) {
    info.meta_set("name", "My Component");
    info.meta_set("module", "foo_mycomponent");
    info.meta_set("version", MY_VERSION);
  }
  virtual bool is_newer(const file_info& info) {
    return acfu::is_newer(info.meta_get("version", 0), MY_VERSION);
  }
  virtual acfu::request::ptr create_request() {
    return new service_impl_t<acfu::plain_request<MySource>>();
  }
};

static service_factory_t<MySource> g_my_source;

******************************************************************************/

namespace acfu {

struct plain_conf {
  static const char* get_url() {
    throw pfc::exception_not_implemented("implement get_url() in derived class");
  }

  static http_request::ptr create_http_request() {
    return static_api_ptr_t<http_client>()->create_request("GET");
  }
};

template <class t_plain_conf>
class plain_request: public request {
 public:
  virtual void run(file_info& info, abort_callback& abort) {
    pfc::string8 url = t_plain_conf::get_url();
    http_request::ptr request = t_plain_conf::create_http_request();

    file::ptr response = request->run(url.get_ptr(), abort);
    pfc::string8 version;

    bool is_utf8;
    text_file_loader::read(response, abort, version, is_utf8);
    
    info.meta_set("version", version.get_ptr());
  }
};

} // namespace acfu
