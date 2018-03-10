#pragma once

#include <rapidjson/error/en.h>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include "common.h"

#define ACFU_EXPECT_JSON(x)  if (!(x)) throw std::logic_error("unexpected JSON schema")

namespace acfu {

struct github_conf {
  static const char* get_owner() {
    throw pfc::exception_not_implemented("implement get_owner() in derived class");
  }

  static const char* get_repo() {
    throw pfc::exception_not_implemented("implement get_repo() in derived class");
  }

  static http_request::ptr create_http_request() {
    return static_api_ptr_t<http_client>()->create_request("GET");
  }

  static bool is_acceptable(const rapidjson::Value& release) {
    return true;
  }
};

template <class t_github_conf>
class github_releases: public request {
 public:
  virtual void run(file_info& info, abort_callback& abort) {
    pfc::string8 url = form_releases_url();
    http_request::ptr request = t_github_conf::create_http_request();

    file::ptr response = request->run(url.get_ptr(), abort);
    pfc::array_t<uint8_t> data;
    response->read_till_eof(data, abort);

    rapidjson::Document doc;
    doc.Parse((const char*)data.get_ptr(), data.get_count());
    if (doc.HasParseError()) {
      throw exception_io_data(PFC_string_formatter()
        << "error(" << doc.GetErrorOffset() << "): " << rapidjson::GetParseError_En(doc.GetParseError()));
    }

    process_response(doc, info);
  }

 protected:
  virtual pfc::string8 form_releases_url() {
    pfc::string8 url;
    url << "https://api.github.com/repos/" << t_github_conf::get_owner()
        << "/" << t_github_conf::get_repo() << "/releases";
    return url;
  }

  virtual bool is_acceptable(const rapidjson::Value& release) {
    return t_github_conf::is_acceptable(release);
  }

  virtual void process_release(const rapidjson::Value& release, file_info& info) {
    ACFU_EXPECT_JSON(release.HasMember("tag_name") && release["tag_name"].IsString());
    info.meta_set("version", release["tag_name"].GetString());

    ACFU_EXPECT_JSON(release.HasMember("html_url") && release["html_url"].IsString());
    info.meta_set("download_page", release["html_url"].GetString());

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    release.Accept(writer);
    info.info_set_ex("release", pfc_infinite, buffer.GetString(), buffer.GetLength());
  }

  virtual void process_response(const rapidjson::Value& json, file_info& info) {
    ACFU_EXPECT_JSON(json.IsArray());
    auto releases = json.GetArray();
    for (const auto& release : releases) {
      if (is_acceptable(release)) {
        return process_release(release, info);
      }
    }
  }
};

template <class t_github_conf>
class github_latest_release: public github_releases<t_github_conf> {
 protected:
  virtual pfc::string8 form_releases_url() {
    pfc::string8 url;
    url << "https://api.github.com/repos/" << t_github_conf::get_owner()
        << "/" << t_github_conf::get_repo() << "/releases/latest";
    return url;
  }

  virtual void process_response(const rapidjson::Value& json, file_info& info) {
    ACFU_EXPECT_JSON(json.IsObject());
    if (is_acceptable(json)) {
      process_release(json, info);
    }
  }
};

} // namespace acfu
