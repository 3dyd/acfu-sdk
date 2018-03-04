#pragma once

namespace acfu {

class request: public service_base {
  FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(request);

 public:
  virtual void run(file_info& info, abort_callback& abort) = 0;
};

class source: public service_base {
  FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(source);

 public:
  virtual GUID get_guid() = 0;
  virtual void get_info(file_info& info) = 0;
  virtual bool is_newer(const file_info& info) = 0;
  virtual request::ptr create_request() = 0;

  static ptr g_get(const GUID& guid);
};

class updates: public service_base {
  FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(updates);

 public:
  class callback {
   public:
    virtual void on_info_changed(const GUID& guid, const file_info& info) {}
    virtual void on_updates_available(const pfc::list_base_const_t<GUID>& ids) {}
  };

  // Should be called from main thread.
  // Being invoked, may call callback::on_updates_available()
  virtual void register_callback(callback* callback) = 0;
  // Should be called from main thread
  virtual void unregister_callback(callback* callback) = 0;

  virtual bool get_info(const GUID& guid, file_info& info) = 0;
  virtual void set_info(const GUID& guid, const file_info& info) = 0;
};

// {4E88EA57-ABDD-49AD-B72B-7C198DA27DBE}
FOOGUIDDECL const GUID request::class_guid =
{ 0x4e88ea57, 0xabdd, 0x49ad, { 0xb7, 0x2b, 0x7c, 0x19, 0x8d, 0xa2, 0x7d, 0xbe } };

// {91AA2ED9-2562-4A5D-8D08-BF430E1F7759}
FOOGUIDDECL const GUID source::class_guid =
{ 0x91aa2ed9, 0x2562, 0x4a5d, { 0x8d, 0x8, 0xbf, 0x43, 0xe, 0x1f, 0x77, 0x59 } };

// {0CB0195A-3FFC-4BF6-84F4-BF7EDA25BE6E}
FOOGUIDDECL const GUID updates::class_guid =
{ 0xcb0195a, 0x3ffc, 0x4bf6, { 0x84, 0xf4, 0xbf, 0x7e, 0xda, 0x25, 0xbe, 0x6e } };

inline source::ptr source::g_get(const GUID& guid) {
  service_enum_t<source> e;
  for (ptr p; e.next(p);) {
    if (p->get_guid() == guid) {
      return p;
    }
  }
  throw pfc::exception_invalid_params(
    PFC_string_formatter() << "unregistered source: " << pfc::print_guid(guid));
}

} // namespace acfu
