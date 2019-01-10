#pragma once

#include <rapidjson/error/en.h>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include "common.h"

/******************************************************************************

These helpers can be used if Source is hosted on GitHub and uses standard
release process there.

Example of how to use latest release on GitHub:

class MySource: public acfu::source, public acfu::github_conf {
  const char* my_version() const {
    return "1.0";
  }
  virtual GUID get_guid() {
    static const GUID guid = {...};
    return guid;
  }
  virtual void get_info(file_info& info) {
    info.meta_set("version", my_version());
    info.meta_set("name", "My Component");
    info.meta_set("module", "foo_mycomponent");
  }
  virtual bool is_newer(const file_info& info) {
    const char* version = info.meta_get("version", 0);
    return acfu::is_newer(version, my_version());
  }
  virtual acfu::request::ptr create_request() {
    return new service_impl_t<acfu::github_latest_release<MySource>>();
  }
  // If project on GitHub is located at https://github.com/myname/myproject
  // then need to return this config:
  static pfc::string get_owner() { return "myname"; }
  static pfc::string get_repo() { return "myproject"; }
};
static service_factory_t<MySource> g_my_source;

If one needs to use more specific criteria, not just the latest release, then
github_releases helper can be used:

  virtual acfu::request::ptr create_request() {
    return new service_impl_t<acfu::github_releases<MySource>>();
  }

Then in is_acceptable_release() one can make desision if particular release is
acceptable.

If one wants to provide "download_page" meta (see comment in ./acfu.h what is
it for), need to implement is_acceptable_asset() method. Example:

  static bool is_acceptable_asset(const rapidjson::Value& asset) {
    if (asset.HasMember("name") && asset["name"].IsString()) {
      pfc::string_extension ext(asset["name"].GetString());
      return 0 == pfc::stricmp_ascii(ext, "fb2k-component");
    }
    return false;
  }

******************************************************************************/

#define ACFU_EXPECT_JSON(x)  if (!(x)) throw std::logic_error("unexpected JSON schema")

namespace acfu {

struct github_conf {
  static pfc::string get_owner() {
    throw pfc::exception_not_implemented("implement get_owner() in derived class");
  }

  static pfc::string get_repo() {
    throw pfc::exception_not_implemented("implement get_repo() in derived class");
  }

  static http_request::ptr create_http_request() {
    return static_api_ptr_t<http_client>()->create_request("GET");
  }

  static bool is_acceptable_release(const rapidjson::Value& release) {
    return true;
  }

  static bool is_acceptable_asset(const rapidjson::Value& asset) {
    return false;
  }
};

template <class t_github_conf>
class github_releases: public request {
 public:
  virtual void run(file_info& info, abort_callback& abort) override {
    pfc::string8 url = form_releases_url();
    http_request::ptr request = t_github_conf::create_http_request();

    service_enum_t<authorization> e;
    for (service_ptr_t<authorization> auth; e.next(auth);) {
      auth->authorize(url.get_ptr(), request, abort);
    }

    file::ptr response = request->run_ex(url.get_ptr(), abort);
    pfc::array_t<uint8_t> data;
    response->read_till_eof(data, abort);

    http_reply::ptr reply;
    if (!response->cast(reply)) {
      throw exception_service_extension_not_found();
    }
    
    rapidjson::Document doc;
    doc.Parse((const char*)data.get_ptr(), data.get_count());
    if (doc.HasParseError()) {
      throw exception_io_data(PFC_string_formatter()
        << "error(" << doc.GetErrorOffset() << "): " << rapidjson::GetParseError_En(doc.GetParseError()));
    }

    pfc::string8 status;
    reply->get_status(status);
    // RFC: Status-Line = HTTP-Version SP Status-Code SP Reason-Phrase CRLF
    auto pos = status.find_first(' ');
    if (~0 == pos || 0 != pfc::strcmp_partial(status.get_ptr() + pos + 1, "200 ")) {
      process_error(doc, status.get_ptr());
    }
    else {
      process_response(doc, info);
    }
  }

 protected:
  virtual pfc::string8 form_releases_url() {
    pfc::string8 url;
    url << "https://api.github.com/repos/" << t_github_conf::get_owner()
        << "/" << t_github_conf::get_repo() << "/releases";
    return url;
  }

  virtual void process_asset(const rapidjson::Value& asset, file_info& info) {
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    asset.Accept(writer);
    info.info_set_ex("asset", pfc_infinite, buffer.GetString(), buffer.GetLength());

    if (asset.HasMember("browser_download_url") && asset["browser_download_url"].IsString()) {
      info.meta_set("download_url", asset["browser_download_url"].GetString());
    }
  }

  virtual void process_release(const rapidjson::Value& release, file_info& info) {
    ACFU_EXPECT_JSON(release.HasMember("tag_name") && release["tag_name"].IsString());
    info.meta_set("version", release["tag_name"].GetString());

    ACFU_EXPECT_JSON(release.HasMember("html_url") && release["html_url"].IsString());
    info.meta_set("download_page", release["html_url"].GetString());

    if (release.HasMember("assets") && release["assets"].IsArray()) {
      auto assets = release["assets"].GetArray();
      for (const auto& asset : assets) {
        if (t_github_conf::is_acceptable_asset(asset)) {
          process_asset(asset, info);
          break;
        }
      }
    }

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    release.Accept(writer);
    info.info_set_ex("release", pfc_infinite, buffer.GetString(), buffer.GetLength());
  }

  virtual void process_response(const rapidjson::Value& json, file_info& info) {
    ACFU_EXPECT_JSON(json.IsArray());
    auto releases = json.GetArray();
    for (const auto& release : releases) {
      if (t_github_conf::is_acceptable_release(release)) {
        return process_release(release, info);
      }
    }
  }

 private:
  void process_error(const rapidjson::Value& json, const char* http_status) {
    if (json.IsObject() && json.HasMember("message") && json["message"].IsString()) {
      throw exception_io_data(json["message"].GetString());
    }
    else {
      throw exception_io_data(PFC_string_formatter()
        << "unexpected response; HTTP status: " << http_status);
    }
  }
};

template <class t_github_conf>
class github_latest_release: public github_releases<t_github_conf> {
 protected:
  virtual pfc::string8 form_releases_url() override {
    pfc::string8 url;
    url << "https://api.github.com/repos/" << t_github_conf::get_owner()
        << "/" << t_github_conf::get_repo() << "/releases/latest";
    return url;
  }

  virtual void process_response(const rapidjson::Value& json, file_info& info) override {
    ACFU_EXPECT_JSON(json.IsObject());
    if (t_github_conf::is_acceptable_release(json)) {
      process_release(json, info);
    }
  }
};

} // namespace acfu
