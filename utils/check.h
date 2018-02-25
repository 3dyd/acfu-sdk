#pragma once

#include "common.h"

namespace acfu {

class check: public threaded_process_callback {
 public:
  check(const source::ptr& source);

  static void g_check(HWND parent, const GUID& guid,
    unsigned flags = threaded_process::flag_show_abort | threaded_process::flag_show_delayed,
    const char* title = "Checking for Updates...");

  virtual void run(threaded_process_status& p_status, abort_callback& p_abort);
  file_info_impl run(abort_callback& p_abort);

 private:
  request::ptr request_;
  GUID guid_;
};

inline check::check(const source::ptr& source) {
  request_ = source->create_request();
  if (request_.is_empty()) {
    throw pfc::exception_not_implemented("source does not provide the way to check for update");
  }
  guid_ = source->get_guid();
}

inline void check::g_check(HWND parent, const GUID& guid, unsigned flags, const char* title) {
  try {
    auto source = source::g_get(guid);
    service_ptr_t<threaded_process_callback> tpc = new service_impl_t<check>(source);
    threaded_process::g_run_modal(tpc, flags, parent, title);
  }
  catch (std::exception& e) {
    popup_message::g_complain("Check for updates", e);
  }
}

inline void check::run(threaded_process_status& p_status, abort_callback& p_abort) {
  try {
    run(p_abort);
  }
  catch (exception_aborted&) {
    throw;
  }
  catch (std::exception& e) {
    popup_message::g_complain("Check for updates", e);
  }
}

inline file_info_impl check::run(abort_callback& p_abort) {
  file_info_impl info;
  request_->run(info, p_abort);
  try {
    static_api_ptr_t<cache> cache;
    cache->set_info(guid_, info);
  }
  catch (exception_service_not_found&) {
  }
  return info;
}

} // namespace acfu
