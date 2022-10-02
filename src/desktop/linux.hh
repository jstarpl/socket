#include <cstdint>
#include <stdlib.h>
#include <math.h>

#include <JavaScriptCore/JavaScript.h>
#include <webkit2/webkit2.h>
#include <gtk/gtk.h>

#include "common.hh"
#include "core.hh"

static GtkTargetEntry droppableTypes[] = {
  { (char*) "text/uri-list", 0, 0 }
};

namespace SSC {
  std::map<std::string, std::string> bufferQueue;

  class Bridge {
    public:
      struct CallbackContext {
        Callback cb;
        std::string seq;
        Window *window;
        void *data;
      };

      IApp *app;
      Core *core;

      Bridge (IApp *app) {
        this->core = new Core();
        this->app = app;
      }

      bool route (std::string msg, char *buf, size_t bufsize);
      void send (Parse cmd, std::string seq, std::string msg, Post post);
      bool invoke (Parse cmd, char *buf, size_t bufsize, Callback cb);
      bool invoke (Parse cmd, Callback cb);
  };

  class App : public IApp {
    public:
      bool fromSSC = false;
      Bridge bridge;
      App(int);

      static std::atomic<bool> isReady;

      int run();
      void kill();
      void restart();
      void dispatch(std::function<void()> work);
      std::string getCwd(const std::string&);
  };

  std::atomic<bool> App::isReady {false};

  class Window : public IWindow {
    GtkAccelGroup *accel_group;
    GtkWidget* window;
    GtkWidget* menubar = nullptr;
    GtkWidget* vbox;
    GtkWidget* popup;
    GtkSelectionData *selectionData;
    std::vector<std::string> draggablePayload;
    int popupId;
    double dragLastX = 0;
    double dragLastY = 0;
    bool isDragInvokedInsideWindow;

    public:
      App app;
      GtkWidget* webview;
      WindowOptions opts;
      Window(App&, WindowOptions);

      void about();
      void eval(const std::string&);
      void show(const std::string&);
      void hide(const std::string&);
      void setBackgroundColor(int r, int g, int b, float a);
      void kill();
      void close(int code);
      void exit(int code);
      void navigate(const std::string&, const std::string&);
      void setTitle(const std::string&, const std::string&);
      void setSize(const std::string&, int, int, int);
      void setContextMenu(const std::string&, const std::string&);
      void closeContextMenu(GtkWidget *, const std::string&);
      void closeContextMenu(const std::string&);
      void closeContextMenu();
      void showInspector();
      void openDialog(const std::string&, bool, bool, bool, bool, const std::string&, const std::string&, const std::string&);
      ScreenSize getScreenSize();

      void setSystemMenu(const std::string& seq, const std::string& menu);
      void setSystemMenuItemEnabled(bool enabled, int barPos, int menuPos);
      int openExternal(const std::string& s);
  };

  App::App (int instanceId) : bridge(this) {
    auto webkitContext = webkit_web_context_get_default();
    gtk_init_check(0, nullptr);
    // TODO enforce single instance is set
    webkit_web_context_register_uri_scheme(
      webkitContext,
      "ipc",
      [](WebKitURISchemeRequest *request, gpointer arg) {
        auto *app = static_cast<App*>(arg);
        auto uri = std::string(webkit_uri_scheme_request_get_uri(request));
        auto msg = std::string(uri);

        Parse cmd(msg);

        auto invoked = app->bridge.invoke(cmd, [=](auto seq, auto result, auto post) {
          auto size = post.body != nullptr ? post.length : result.size();
          auto body = post.body != nullptr ? post.body : result.c_str();

          // `post.body` is free'd with `freeFunction`
          post.bodyNeedsFree = false;
          auto freeFunction = post.body != nullptr ? free : nullptr;
          auto stream = g_memory_input_stream_new_from_data(body, size, freeFunction);
          auto response = webkit_uri_scheme_response_new(stream, size);

          webkit_uri_scheme_response_set_content_type(
            response,
            "application/octet-stream"
          );

          webkit_uri_scheme_request_finish_with_response(request, response);

          g_object_unref(stream);
        });

        if (!invoked) {
          auto windowFactory = reinterpret_cast<WindowFactory<Window, App> *>(app->getWindowFactory());

          if (windowFactory) {
            auto window = windowFactory->getWindow(cmd.index);

            if (window && window->onMessage) {
              window->onMessage(msg);
            }
          }

          auto msg = SSC::format(R"MSG({
            "err": {
              "message": "Not found",
              "type": "NotFoundError",
              "url": "$S"
            }
          })MSG", uri);

          auto stream = g_memory_input_stream_new_from_data(msg.c_str(), msg.size(), 0);
          auto response = webkit_uri_scheme_response_new(stream, msg.size());

          webkit_uri_scheme_response_set_content_type(
            response,
            "application/octet-stream"
          );

          webkit_uri_scheme_request_finish_with_response(request, response);
          g_object_unref(stream);
        }
      },
      this,
      0
    );
  }

  int App::run () {
    auto cwd = getCwd("");
    uv_chdir(cwd.c_str());
    gtk_main();
    return shouldExit ? 1 : 0;
  }

  void App::kill () {
    shouldExit = true;
    gtk_main_quit();
  }

  void App::restart () {
  }

  std::string App::getCwd(const std::string &s) {
    auto canonical = fs::canonical("/proc/self/exe");
    return std::string(fs::path(canonical).parent_path());
  }

  void App::dispatch(std::function<void()> f) {
    g_idle_add_full(
      G_PRIORITY_HIGH_IDLE,
      (GSourceFunc)([](void* f) -> int {
        (*static_cast<std::function<void()>*>(f))();
        return G_SOURCE_REMOVE;
      }),
      new std::function<void()>(f),
      [](void* f) {
        delete static_cast<std::function<void()>*>(f);
      }
    );
  }

  bool Bridge::invoke (Parse cmd, Callback cb) {
    return this->invoke(cmd, nullptr, 0, cb);
  }

  bool Bridge::invoke (Parse cmd, char *buf, size_t bufsize, Callback cb) {
    auto seq = cmd.get("seq");

    if (cmd.name == "post" || cmd.name == "data") {
      auto id = cmd.get("id");

      if (id.size() == 0) {
        auto err = SSC::format(R"MSG({
          "err": {
            "id", "$S",
            "type": "InternalError",
            "message": "'id' is required"
          }
        })MSG", id);

        cb(seq, err, Post{});
        return true;
      }

      auto pid = std::stoull(id);

      if (!this->core->hasPost(pid)) {
        auto err = SSC::format(R"MSG({
          "err": {
            "id", "$S",
            "type": "InternalError",
            "message": "Invalid 'id' for post"
          }
        })MSG", id);

        cb(seq, err, Post{});
        return true;
      }

      auto post = this->core->getPost(pid);
      cb(seq, "{}", post);
      this->core->removePost(pid);
      return true;
    }

    if (cmd.name == "getPlatformOS" || cmd.name == "os.platform") {
      auto msg = SSC::format(R"JSON({
        "data": "$S"
      })JSON", SSC::platform.os);

      cb(seq, msg, Post{});
      return true;
    }

    if (cmd.name == "getPlatformType" || cmd.name == "os.type") {
      auto msg = SSC::format(R"JSON({
        "data": "$S"
      })JSON", SSC::platform.os);

      cb(seq, msg, Post{});
      return true;
    }

    if (cmd.name == "getPlatformArch" || cmd.name == "os.arch") {
      auto msg = SSC::format(R"JSON({
        "data": "$S"
      })JSON", SSC::platform.arch);

      cb(seq, msg, Post{});
      return true;
    }

    if (cmd.name == "getNetworkInterfaces" || cmd.name == "os.networkInterfaces") {
      this->app->dispatch([=, this] {
        auto msg = this->core->getNetworkInterfaces();
        cb(seq, msg, Post{});
      });
      return true;
    }

    if (cmd.name == "window.eval" && cmd.index >= 0) {
      auto windowFactory = reinterpret_cast<WindowFactory<Window, App> *>(app->getWindowFactory());
      if (windowFactory == nullptr) {
        // @TODO(jwerle): print warning
        return false;
      }

      auto window = windowFactory->getWindow(cmd.index);

      if (window == nullptr) {
        return false;
      }

      auto value = decodeURIComponent(cmd.get("value"));
      auto ctx = new Bridge::CallbackContext { cb, seq, window, (void *) this };

      webkit_web_view_run_javascript(
        WEBKIT_WEB_VIEW(window->webview),
        value.c_str(),
        nullptr,
        [](GObject *object, GAsyncResult *res, gpointer data) {
          GError *error = nullptr;
          auto ctx = reinterpret_cast<Bridge::CallbackContext *>(data);
          auto result = webkit_web_view_run_javascript_finish(
            WEBKIT_WEB_VIEW(ctx->window->webview),
            res,
            &error
          );

          if (!result) {
            auto msg = SSC::format(
              R"MSG({"err": { "code": "$S", "message": "$S" } })MSG",
              std::to_string(error->code),
              std::string(error->message)
            );

            g_error_free(error);
            ctx->cb(ctx->seq, msg, Post{});
            return;
          } else {
            auto value = webkit_javascript_result_get_js_value(result);

            if (
              jsc_value_is_null(value) ||
              jsc_value_is_array(value) ||
              jsc_value_is_object(value) ||
              jsc_value_is_number(value) ||
              jsc_value_is_string(value) ||
              jsc_value_is_function(value) ||
              jsc_value_is_undefined(value) ||
              jsc_value_is_constructor(value)
            ) {
              auto context = jsc_value_get_context(value);
              auto string = jsc_value_to_string(value);
              auto exception = jsc_context_get_exception(context);
              std::string msg = "";

              if (exception) {
                auto message = jsc_exception_get_message(exception);
                msg = SSC::format(
                  R"MSG({"err": { "message": "$S" } })MSG",
                  std::string(message)
                );
              } else if (string) {
                msg = std::string(string);
              }

              ctx->cb(ctx->seq, msg, Post{});
              if (string) {
                g_free(string);
              }
            } else {
              auto msg = SSC::format(
                R"MSG({"err": { "message": "Error: An unknown JavaScript evaluation error has occurred" } })MSG"
               );

              ctx->cb(ctx->seq, msg, Post{});
            }
          }

          webkit_javascript_result_unref(result);
        },
        ctx
      );

      return true;
    }

    if (cmd.name == "dnsLookup" || cmd.name == "dns.lookup") {
      auto hostname = cmd.get("hostname");
      auto strFamily = cmd.get("family");
      int family = 0;

      if (strFamily.size() > 0) {
        try {
          family = std::stoi(strFamily);
        } catch (...) {
        }
      }

      this->app->dispatch([=, this] {
        this->core->dnsLookup(seq, hostname, family, cb);
      });
      return true;
    }

    if (cmd.name == "buffer.queue" && buf != nullptr) {
      if (seq.size() == 0) {
        auto err = SSC::format(R"MSG({
          "err": {
            "type": "InternalError",
            "message": "Missing 'seq' for buffer.queue"
          }
        })MSG");

        cb(seq, err, Post{});
        return true;
      }

      auto bufferKey = std::to_string(cmd.index) + seq;
      auto str = std::string();
      str.assign(buf, bufsize);
      bufferQueue[bufferKey] = str;
      return true;
    }

    if (cmd.name == "event") {
      this->app->dispatch([=, this] {
        auto event = decodeURIComponent(cmd.get("value"));
        auto data = decodeURIComponent(cmd.get("data"));
        auto seq = cmd.get("seq");

        this->core->handleEvent(seq, event, data, cb);
      });
      return true;
    }

    if (cmd.name == "getFSConstants" || cmd.name == "fs.constants") {
      cb(seq, this->core->getFSConstants(), Post{});
      return true;
    }

    if (cmd.name == "fsAccess" || cmd.name == "fs.access") {
      if (cmd.get("path").size() == 0) {
        auto err = SSC::format(R"MSG({
          "err": {
            "type": "InternalError",
            "message": "'path' is required"
          }
        })MSG");

        cb(seq, err, Post{});
        return true;
      }

      if (cmd.get("mode").size() == 0) {
        auto err = SSC::format(R"MSG({
          "err": {
            "type": "InternalError",
            "message": "'mode' is required"
          }
        })MSG");

        cb(seq, err, Post{});
        return true;
      }

      this->app->dispatch([=, this] {
        auto path = decodeURIComponent(cmd.get("path"));
        auto mode = std::stoi(cmd.get("mode"));

        this->core->fsAccess(seq, path, mode, cb);
      });
      return true;
    }

    if (cmd.name == "fsChmod" || cmd.name == "fs.chmod") {
      if (cmd.get("path").size() == 0) {
        auto err = SSC::format(R"MSG({
          "err": {
            "type": "InternalError",
            "message": "'path' is required"
          }
        })MSG");

        cb(seq, err, Post{});
        return true;
      }

      if (cmd.get("mode").size() == 0) {
        auto err = SSC::format(R"MSG({
          "err": {
            "type": "InternalError",
            "message": "'mode' is required"
          }
        })MSG");

        cb(seq, err, Post{});
        return true;
      }

      this->app->dispatch([=, this] {
        auto path = decodeURIComponent(cmd.get("path"));
        auto mode = std::stoi(cmd.get("mode"));

        this->core->fsChmod(seq, path, mode, cb);
      });
      return true;
    }

    if (cmd.name == "fsClose" || cmd.name == "fs.close") {
      if (cmd.get("id").size() == 0) {
        auto err = SSC::format(R"MSG({
          "err": {
            "type": "InternalError",
            "message": "'id' is required"
          }
        })MSG");

        cb(seq, err, Post{});
        return true;
      }

      this->app->dispatch([=, this] {
        auto id = std::stoull(cmd.get("id"));
        this->core->fsClose(seq, id, cb);
      });
      return true;
    }

    if (cmd.name == "fsClosedir" || cmd.name == "fs.closedir") {
      if (cmd.get("id").size() == 0) {
        auto err = SSC::format(R"MSG({
          "err": {
            "type": "InternalError",
            "message": "'id' is required"
          }
        })MSG");

        cb(seq, err, Post{});
        return true;
      }

      this->app->dispatch([=, this] {
        auto id = std::stoull(cmd.get("id"));
        this->core->fsClosedir(seq, id, cb);
      });
      return true;
    }

    if (cmd.name == "fsGetOpenDescriptors" || cmd.name == "fs.getOpenDescriptors") {
      this->app->dispatch([=, this] {
        this->core->fsGetOpenDescriptors(seq, cb);
      });
      return true;
    }

    if (cmd.name == "fsCloseOpenDescriptor" || cmd.name == "fs.closeOpenDescriptor") {
      if (cmd.get("id").size() == 0) {
        auto err = SSC::format(R"MSG({
          "err": {
            "type": "InternalError",
            "message": "'id' is required"
          }
        })MSG");

        cb(seq, err, Post{});
        return true;
      }

      this->app->dispatch([=, this] {
        auto id = std::stoull(cmd.get("id"));
        this->core->fsCloseOpenDescriptor(seq, id, cb);
      });
      return true;
    }

    if (cmd.name == "fsCloseOpenDescriptors" || cmd.name == "fs.closeOpenDescriptors") {
      this->app->dispatch([=, this] {
        auto preserveRetained = cmd.get("retain") != "false";
        this->core->fsCloseOpenDescriptors(seq, preserveRetained, cb);
      });
      return true;
    }

    if (cmd.name == "fsCopyFile" || cmd.name == "fs.copyFile") {
      this->app->dispatch([=, this] {
        auto src = cmd.get("src");
        auto dest = cmd.get("dest");
        auto mode = std::stoi(cmd.get("dest"));

        this->core->fsCopyFile(seq, src, dest, mode, cb);
      });
      return true;
    }

    if (cmd.name == "fsOpen" || cmd.name == "fs.open") {
      this->app->dispatch([=, this] {
        auto seq = cmd.get("seq");
        auto path = decodeURIComponent(cmd.get("path"));
        auto flags = std::stoi(cmd.get("flags"));
        auto mode = std::stoi(cmd.get("mode"));
        auto id = std::stoull(cmd.get("id"));

        this->core->fsOpen(seq, id, path, flags, mode, cb);
      });

      return true;
    }

    if (cmd.name == "fsOpendir" || cmd.name == "fs.opendir") {
      if (cmd.get("id").size() == 0) {
        auto err = SSC::format(R"MSG({
          "err": {
            "type": "InternalError",
            "message": "'id' is required"
          }
        })MSG");

        cb(seq, err, Post{});
        return true;
      }

      if (cmd.get("path").size() == 0) {
        auto err = SSC::format(R"MSG({
          "err": {
            "type": "InternalError",
            "message": "'path' is required"
          }
        })MSG");

        cb(seq, err, Post{});
        return true;
      }

      this->app->dispatch([=, this] {
        auto path = decodeURIComponent(cmd.get("path"));
        auto id = std::stoull(cmd.get("id"));

        this->core->fsOpendir(seq, id, path,  cb);
      });
      return true;
    }

    if (cmd.name == "fsRead" || cmd.name == "fs.read") {
      this->app->dispatch([=, this] {
        auto id = std::stoull(cmd.get("id"));
        auto size = std::stoi(cmd.get("size"));
        auto offset = std::stoi(cmd.get("offset"));

        this->core->fsRead(seq, id, size, offset, cb);
      });
      return true;
    }

    if (cmd.name == "fsRetainOpenDescriptor" || cmd.name == "fs.retainOpenDescriptor") {
      this->app->dispatch([=, this] {
        auto id = std::stoull(cmd.get("id"));

        this->core->fsRetainOpenDescriptor(seq, id, cb);
      });
      return true;
    }

    if (cmd.name == "fsReaddir" || cmd.name == "fs.readdir") {
      if (cmd.get("id").size() == 0) {
        auto err = SSC::format(R"MSG({
          "err": {
            "type": "InternalError",
            "message": "'id' is required"
          }
        })MSG");

        cb(seq, err, Post{});
        return true;
      }

      this->app->dispatch([=, this] {
        auto id = std::stoull(cmd.get("id"));
        auto entries = std::stoi(cmd.get("entries", "256"));

        this->core->fsReaddir(seq, id, entries, cb);
      });
      return true;
    }

    if (cmd.name == "fsWrite" || cmd.name == "fs.write") {
      auto bufferKey = std::to_string(cmd.index) + seq;
      if (bufferQueue.count(bufferKey)) {
          auto id = std::stoull(cmd.get("id"));
          auto offset = std::stoi(cmd.get("offset"));
          auto buffer = bufferQueue[bufferKey];
          bufferQueue.erase(bufferQueue.find(bufferKey));

        this->app->dispatch([=, this] {
          this->core->fsWrite(seq, id, buffer, offset, cb);
        });
      }

      return true;
    }

    if (cmd.name == "udpClose" || cmd.name == "udp.close") {
      uint64_t peerId = 0ll;
      std::string err = "";

      if (cmd.get("id").size() == 0) {
        err = ".id is required";
      } else {
        try {
          peerId = std::stoull(cmd.get("id"));
        } catch (...) {
          err = "property .id is invalid";
        }
      }

      this->app->dispatch([=, this] {
        this->core->close(seq, peerId, cb);
      });
      return true;
    }

    if (cmd.name == "udpBind" || cmd.name == "udp.bind") {
      if (cmd.get("id").size() == 0) {
        auto err = SSC::format(R"MSG({
          "err": {
            "type": "InternalError",
            "message": ".id is required"
          }
        })MSG");

        cb(seq, err, Post{});
        return true;
      }

      if (cmd.get("port").size() == 0) {
        auto err = SSC::format(R"MSG({
          "err": {
            "type": "InternalError",
            "message": "'port' is required"
          }
        })MSG");

        cb(seq, err, Post{});
        return true;
      }

      this->app->dispatch([=, this] {
        auto ip = cmd.get("address");
        auto reuseAddr = cmd.get("reuseAddr") == "true";
        int port;
        uint64_t peerId;

        if (ip.size() == 0) {
          ip = "0.0.0.0";
        }

        port = std::stoi(cmd.get("port"));
        peerId = std::stoull(cmd.get("id"));

        this->core->udpBind(seq, peerId, ip, port, reuseAddr, cb);
      });
      return true;
    }

    if (cmd.name == "udpConnect" || cmd.name == "udp.connect") {
      auto strId = cmd.get("id");
      std::string err = "";
      uint64_t peerId = 0ll;
      int port = 0;
      auto strPort = cmd.get("port");
      auto ip = cmd.get("address");

      if (strId.size() == 0) {
        err = "invalid peerId";
      } else {
        try {
          peerId = std::stoull(cmd.get("id"));
        } catch (...) {
          err = "invalid peerId";
        }
      }

      if (strPort.size() == 0) {
        err = "invalid port";
      } else {
        try {
          port = std::stoi(strPort);
        } catch (...) {
          err = "invalid port";
        }
      }

      if (port == 0) {
        err = "Can not bind to port 0";
      }

      if (err.size() > 0) {
        auto msg = SSC::format(R"MSG({
          "err": {
            "message": "$S"
          }
        })MSG", err);
        cb(seq, msg, Post{});
        return true;
      }

      if (ip.size() == 0) {
        ip = "0.0.0.0";
      }

      this->app->dispatch([=, this] {
        this->core->udpConnect(seq, peerId, ip, port, cb);
      });

      return true;
    }

    if (cmd.name == "udpDisconnect" || cmd.name == "udp.disconnect") {
      auto strId = cmd.get("id");

      if (strId.size() == 0) {
        auto msg = SSC::format(R"MSG({
          "err": {
            "message": "expected .peerId"
          }
        })MSG");
        cb(seq, msg, Post{});
        return true;
      }

      auto peerId = std::stoull(strId);

      this->app->dispatch([=, this] {
        this->core->udpDisconnect(seq, peerId, cb);
      });
      return true;
    }

    if (cmd.name == "udpGetPeerName" || cmd.name == "udp.getPeerName") {
      auto strId = cmd.get("id");

      if (strId.size() == 0) {
        auto msg = SSC::format(R"MSG({
          "err": {
            "message": "expected .peerId"
          }
        })MSG");
        cb(seq, msg, Post{});
        return true;
      }

      auto peerId = std::stoull(strId);

      this->app->dispatch([=, this] {
        this->core->udpGetPeerName(seq, peerId, cb);
      });
      return true;
    }

    if (cmd.name == "udpGetSockName" || cmd.name == "udp.getSockName") {
      auto strId = cmd.get("id");

      if (strId.size() == 0) {
        auto msg = SSC::format(R"MSG({
          "err": {
            "message": "expected .peerId"
          }
        })MSG");
        cb(seq, msg, Post{});
        return true;
      }

      auto peerId = std::stoull(strId);

      this->app->dispatch([=, this] {
        this->core->udpGetSockName(seq, peerId, cb);
      });
      return true;
    }

    if (cmd.name == "udpGetState" || cmd.name == "udp.getState") {
      auto strId = cmd.get("id");

      if (strId.size() == 0) {
        auto msg = SSC::format(R"MSG({
          "err": {
            "message": "expected .peerId"
          }
        })MSG");
        cb(seq, msg, Post{});
        return true;
      }

      auto peerId = std::stoull(strId);

      this->app->dispatch([=, this] {
        this->core->udpGetState(seq, peerId, cb);
      });
      return true;
    }

    if (cmd.name == "udpReadStart" || cmd.name == "udp.readStart") {
      if (cmd.get("id").size() == 0) {
        auto err = SSC::format(R"MSG({
          "err": {
            "type": "InternalError",
            "message": ".id is required"
          }
        })MSG");

        cb(seq, err, Post{});
        return true;
      }

      auto peerId = std::stoull(cmd.get("id"));
      this->app->dispatch([=, this] {
        this->core->udpReadStart(seq, peerId, cb);
      });
      return true;
    }

    if (cmd.name == "udpReadStop" || cmd.name == "udp.readStop") {
      if (cmd.get("id").size() == 0) {
        auto err = SSC::format(R"MSG({
          "err": {
            "type": "InternalError",
            "message": ".id is required"
          }
        })MSG");

        cb(seq, err, Post{});
        return true;
      }

      auto peerId = std::stoull(cmd.get("id"));
      this->app->dispatch([=, this] {
        this->core->udpReadStop(seq, peerId, cb);
      });
      return true;
    }

    if (cmd.name == "udpSend" || cmd.name == "udp.send") {
      int offset = 0;
      int port = 0;
      uint64_t peerId;
      std::string err;

      auto ephemeral = cmd.get("ephemeral") == "true";
      auto strOffset = cmd.get("offset");
      auto strPort = cmd.get("port");
      auto ip = cmd.get("address");

      if (strOffset.size() > 0) {
        try {
          offset = std::stoi(strOffset);
        } catch (...) {
          err = "invalid offset";
        }
      }

      try {
        port = std::stoi(strPort);
      } catch (...) {
        err = "invalid port";
      }

      if (ip.size() == 0) {
        ip = "0.0.0.0";
      }

      try {
        peerId = std::stoull(cmd.get("id"));
      } catch (...) {
        err = "invalid id";
      }

      if (err.size() > 0) {
        auto msg = SSC::format(R"MSG({
          "err": {
            "message": "$S"
          }
        })MSG", err);
        cb(seq, err, Post{});
        return true;
      }

      this->app->dispatch([=, this] {
        auto bufferKey = std::to_string(cmd.index) + seq;
        auto buffer = bufferQueue[bufferKey];
        auto data = buffer.data();
        auto size = buffer.size();

        bufferQueue.erase(bufferQueue.find(bufferKey));
        this->core->udpSend(seq, peerId, data, size, port, ip, ephemeral, cb);
      });
      return true;
    }

    if (cmd.name == "bufferSize") {
      if (cmd.get("id").size() == 0) {
        auto err = SSC::format(R"MSG({
          "source": "bufferSize",
          "err": {
            "type": "InternalError",
            "message": ".id is required"
          }
        })MSG");

        cb(seq, err, Post{});
        return true;
      }

      auto buffer = std::stoi(cmd.get("buffer", "0"));
      auto size = std::stoi(cmd.get("size", "0"));
      auto id  = std::stoull(cmd.get("id"));

      this->app->dispatch([=, this] {
        this->core->bufferSize(seq, id, size, buffer, cb);
      });

      return true;
    }

    return false;
  }

  bool Bridge::route (std::string msg, char *buf, size_t bufsize) {
    Parse cmd(msg);

    return this->invoke(cmd, buf, bufsize, [cmd, this](auto seq, auto msg, auto post) {
      this->send(cmd, seq, msg, post);
    });
  }

  void Bridge::send (Parse cmd, std::string seq, std::string msg, Post post) {
    if (cmd.index == -1) {
      // @TODO(jwerle): print warning
      return;
    }

    if (app == nullptr) {
      // @TODO(jwerle): print warning
      return;
    }

    auto windowFactory = reinterpret_cast<WindowFactory<Window, App> *>(app->getWindowFactory());
    if (windowFactory == nullptr) {
      // @TODO(jwerle): print warning
      return;
    }

    auto window = windowFactory->getWindow(cmd.index);

    if (window == nullptr) {
      // @TODO(jwerle): print warning
      return;
    }

    if (post.body || seq == "-1") {
      auto script = this->core->createPost(seq, msg, post);
      window->eval(script);
      return;
    }

    if (seq != "-1" && seq.size() > 0) {
      msg = SSC::resolveToRenderProcess(seq, "0", encodeURIComponent(msg));
    }

    window->eval(msg);
  }

  Window::Window (App& app, WindowOptions opts) : app(app) , opts(opts) {
    setenv("GTK_OVERLAY_SCROLLING", "1", 1);
    accel_group = gtk_accel_group_new();
    popupId = 0;
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    popup = nullptr;

    if (opts.resizable) {
      gtk_window_set_default_size(GTK_WINDOW(window), opts.width, opts.height);
      gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    } else {
      gtk_widget_set_size_request(window, opts.width, opts.height);
    }

    gtk_window_set_resizable(GTK_WINDOW(window), opts.resizable);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);

    WebKitUserContentManager* cm = webkit_user_content_manager_new();
    webkit_user_content_manager_register_script_message_handler(cm, "external");

    g_signal_connect(
      cm,
      "script-message-received::external",
      G_CALLBACK(+[](WebKitUserContentManager*, WebKitJavascriptResult* r, gpointer arg) {
        auto *window = static_cast<Window*>(arg);
        auto value = webkit_javascript_result_get_js_value(r);
        auto str = std::string(jsc_value_to_string(value));

        char *buf = nullptr;
        size_t bufsize = 0;

        // 'b5' for 'buffer'
        if (str.size() >= 2 && str.at(0) == 'b' && str.at(1) == '5') {
          gsize size = 0;
          auto bytes = jsc_value_to_string_as_bytes(value);
          auto data = (char *) g_bytes_get_data(bytes, &size);

          if (size > 6) {
            size_t offset = 2 + 4 + 20; // buf offset
            auto index = new char[4]{0};
            auto seq = new char[20]{0};

            decodeUTF8(index, data + 2, 4);
            decodeUTF8(seq, data + 2 + 4, 20);

            buf = new char[size - offset]{0};
            bufsize = decodeUTF8(buf, data + offset, size - offset);

            str = std::string("ipc://buffer.queue?")
              + std::string("index=") + std::string(index)
              + std::string("&seq=") + std::string(seq);

            delete [] index;
            delete [] seq;
          }
        }

        if (!window->app.bridge.route(str, buf, bufsize)) {
          if (window->onMessage != nullptr) {
            window->onMessage(str);
          }
        }

        if (buf != nullptr) {
          delete [] buf;
        }
      }),
      this
    );

    webview = webkit_web_view_new_with_user_content_manager(cm);

    g_signal_connect(
      G_OBJECT(webview),
      "decide-policy",
      G_CALLBACK(+[](
        WebKitWebView           *web_view,
        WebKitPolicyDecision    *decision,
        WebKitPolicyDecisionType decision_type,
        gpointer                 user_data
      ) {
        if (decision_type != WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION) {
          return true;
        }

        auto nav = WEBKIT_NAVIGATION_POLICY_DECISION (decision);
        auto action = webkit_navigation_policy_decision_get_navigation_action(nav);
        auto req = webkit_navigation_action_get_request(action);
        auto uri = webkit_uri_request_get_uri(req);

        std::string s(uri);

        if (s.find("file://") != 0 && s.find("http://localhost") != 0) {
          webkit_policy_decision_ignore(decision);
          return false;
        }

        return true;
      }),
      this
    );

    g_signal_connect(
      G_OBJECT(webview),
      "load-changed",
      G_CALLBACK(+[](WebKitWebView* wv, WebKitLoadEvent event, gpointer arg) {
        auto *window = static_cast<Window*>(arg);
        auto core = window->app.bridge.core;

        if (event == WEBKIT_LOAD_STARTED) {
          window->app.isReady = false;
        }

        if (event == WEBKIT_LOAD_FINISHED) {
          window->app.isReady = true;
        }
      }),
      this
    );

    // Calling gtk_drag_source_set interferes with text selection
    /* gtk_drag_source_set(
      webview,
      (GdkModifierType)(GDK_BUTTON1_MASK | GDK_BUTTON2_MASK),
      droppableTypes,
      G_N_ELEMENTS(droppableTypes),
      GDK_ACTION_COPY
    ); */

    /* gtk_drag_dest_set(
      webview,
      GTK_DEST_DEFAULT_ALL,
      droppableTypes,
      1,
      GDK_ACTION_MOVE
    ); */

    g_signal_connect(
      G_OBJECT(webview),
      "drag-begin",
      G_CALLBACK(+[](GtkWidget *wv, GdkDragContext *context, gpointer arg) {
        auto *w = static_cast<Window*>(arg);
        w->isDragInvokedInsideWindow = true;

        GdkDevice* device;
        gint wx;
        gint wy;
        gint x;
        gint y;

        device = gdk_drag_context_get_device(context);
        gdk_device_get_window_at_position(device, &x, &y);
        gdk_device_get_position(device, 0, &wx, &wy);

        std::string js(
          "(() => {"
          "  let el = null;"
          "  try { el = document.elementFromPoint(" + std::to_string(x) + "," + std::to_string(y) + "); }"
          "  catch (err) { console.error(err.stack || err.message || err); }"
          "  if (!el) return;"
          "  const found = el.matches('[data-src]') ? el : el.closest('[data-src]');"
          "  return found && found.dataset.src"
          "})()"
        );

        webkit_web_view_run_javascript(
          WEBKIT_WEB_VIEW(wv),
          js.c_str(),
          nullptr,
          [](GObject* src, GAsyncResult* result, gpointer arg) {
            auto *w = static_cast<Window*>(arg);
            if (!w) return;

            GError* error = NULL;
            WebKitJavascriptResult* wkr = webkit_web_view_run_javascript_finish(WEBKIT_WEB_VIEW(src), result, &error);
            if (!wkr || error) return;

            auto* value = webkit_javascript_result_get_js_value(wkr);
            if (!jsc_value_is_string(value)) return;

            JSCException *exception;
            gchar *str_value = jsc_value_to_string(value);


            w->draggablePayload = SSC::split(str_value, ';');
            exception = jsc_context_get_exception(jsc_value_get_context(value));
          },
          w
        );
      }),
      this
    );

    g_signal_connect(
      G_OBJECT(webview),
      "focus",
      G_CALLBACK(+[](
        GtkWidget *wv,
        GtkDirectionType direction,
        gpointer arg)
      {
        auto *w = static_cast<Window*>(arg);
        if (!w) return;
      }),
      this
    );

    g_signal_connect(
      G_OBJECT(webview),
      "drag-data-get",
      G_CALLBACK(+[](
        GtkWidget *wv,
        GdkDragContext *context,
        GtkSelectionData *data,
        guint info,
        guint time,
        gpointer arg)
      {
        auto *w = static_cast<Window*>(arg);
        if (!w) return;

        if (w->isDragInvokedInsideWindow) {
          // FIXME: Once, write a single tmp file `/tmp/{i64}.download` and
          // add it to the draggablePayload, then start the fanotify watcher
          // for that particular file.
          return;
        }

        if (w->draggablePayload.size() == 0) return;

        gchar* uris[w->draggablePayload.size() + 1];
        int i = 0;

        for (auto& file : w->draggablePayload) {
          if (file[0] == '/') {
            // file system paths must be proper URIs
            file = std::string("file://" + file);
          }
          uris[i++] = strdup(file.c_str());
        }

        uris[i] = NULL;

        gtk_selection_data_set_uris(data, uris);
      }),
      this
    );

    g_signal_connect(
      G_OBJECT(webview),
      "drag-motion",
      G_CALLBACK(+[](
        GtkWidget *wv,
        GdkDragContext *context,
        gint x,
        gint y,
        guint32 time,
        gpointer arg)
      {
        auto *w = static_cast<Window*>(arg);
        if (!w) return;

        w->dragLastX = x;
        w->dragLastY = y;

        // char* target_uri = g_file_get_uri(drag_info->target_location);

        int count = w->draggablePayload.size();
        bool inbound = !w->isDragInvokedInsideWindow;

        // w->eval(SSC::emitToRenderProcess("dragend", "{}"));

        // TODO wtf we get a toaster instead of actual focus
        gtk_window_present(GTK_WINDOW(w->window));
        // gdk_window_focus(GDK_WINDOW(w->window), nullptr);

        std::string json = (
          "{\"count\":" + std::to_string(count) + ","
          "\"inbound\":" + (inbound ? "true" : "false") + ","
          "\"x\":" + std::to_string(x) + ","
          "\"y\":" + std::to_string(y) + "}"
        );

        w->eval(SSC::emitToRenderProcess("drag", json));
      }),
      this
    );

    // https://wiki.gnome.org/Newcomers/XdsTutorial
    // https://wiki.gnome.org/action/show/Newcomers/OldDragNDropTutorial?action=show&redirect=Newcomers%2FDragNDropTutorial

    g_signal_connect(
      G_OBJECT(webview),
      "drag-end",
      G_CALLBACK(+[](GtkWidget *wv, GdkDragContext *context, gpointer arg) {
        auto *w = static_cast<Window*>(arg);
        if (!w) return;

        w->isDragInvokedInsideWindow = false;
        w->draggablePayload.clear();
        w->eval(SSC::emitToRenderProcess("dragend", "{}"));
      }),
      this
    );

    g_signal_connect(
      G_OBJECT(window),
      "button-release-event",
      G_CALLBACK(+[](GtkWidget* window, GdkEventButton event, gpointer arg) {
        auto *w = static_cast<Window*>(arg);
        if (!w) return;

        /**
         * Calling w->eval() inside button-release-event causes
         * a Segmentation Fault on Ubuntu 22, but works fine on
         * other linuxes like Ubuntu 20.
         *
         * The dragend feature causes the application to crash in
         * all operations including non drag-drop and causes applications
         * that do not use drag and drop at all to crash.
         *
         * So disabling this experimental linux dragdrop code for now.
         */

        // w->isDragInvokedInsideWindow = false;
        // w->eval(SSC::emitToRenderProcess("dragend", "{}"));
      }),
      this
    );

    g_signal_connect(
      G_OBJECT(webview),
      "drag-data-received",
      G_CALLBACK(+[](
        GtkWidget* wv,
        GdkDragContext* context,
        gint x,
        gint y,
        GtkSelectionData *data,
        guint info,
        guint time,
        gpointer arg)
      {
        auto* w = static_cast<Window*>(arg);
        if (!w) return;

        gtk_drag_dest_add_uri_targets(wv);
        gchar** uris = gtk_selection_data_get_uris(data);
        int len = gtk_selection_data_get_length(data) - 1;
        if (!uris) return;

        auto v = &w->draggablePayload;

        for(size_t n = 0; uris[n] != nullptr; n++) {
          gchar* src = g_filename_from_uri(uris[n], nullptr, nullptr);
          if (src) {
            auto s = std::string(src);
            if (std::find(v->begin(), v->end(), s) == v->end()) {
              v->push_back(s);
            }
            g_free(src);
          }
        }
      }),
      this
    );

    g_signal_connect(
      G_OBJECT(webview),
      "drag-drop",
      G_CALLBACK(+[](
        GtkWidget* widget,
        GdkDragContext* context,
        gint x,
        gint y,
        guint time,
        gpointer arg)
      {
        auto* w = static_cast<Window*>(arg);
        auto count = w->draggablePayload.size();
        std::stringstream files;

        for (int i = 0 ; i < count; ++i) {
          auto src = w->draggablePayload[i];
          files << '"' << src << '"';
          if (i < count - 1) {
            files << ",";
          }
        }

        std::string json = ("{"
          "\"files\": [" + files.str() + "],"
          "\"x\":" + std::to_string(x) + ","
          "\"y\":" + std::to_string(y) +
        "}");

        w->eval(std::string(
          "(() => {"
          "  try {"
          "    const target = document.elementFromPoint(" + std::to_string(x) + "," + std::to_string(y) + ");"
          "    window._ipc.emit('dropin', '" + json + "', target, { bubbles: true });"
          "  } catch (err) { "
          "    console.error(err.stack || err.message || err);"
          "  }"
          "})()"
        ));

        w->draggablePayload.clear();
        w->eval(SSC::emitToRenderProcess("dragend", "{}"));
        gtk_drag_finish(context, TRUE, TRUE, time);
        return TRUE;
      }),
      this
    );

    g_signal_connect(
      G_OBJECT(window),
      "destroy",
      G_CALLBACK(+[](GtkWidget*, gpointer arg) {
        auto* w = static_cast<Window*>(arg);
        w->close(0);
      }),
      this
    );

    g_signal_connect(
      G_OBJECT(window),
      "delete-event",
      G_CALLBACK(+[](GtkWidget* widget, GdkEvent*, gpointer arg) {
        auto* w = static_cast<Window*>(arg);

        if (w->opts.canExit == false) {
          w->eval(emitToRenderProcess("windowHide", "{}"));
          return gtk_widget_hide_on_delete(widget);
        }

        w->close(0);
        return FALSE;
      }),
      this
    );

    std::string preload = Str(
      "window.external = {\n"
      "  invoke: arg => window.webkit.messageHandlers.external.postMessage(arg)\n"
      "};\n"
      "" + createPreload(opts) + "\n"
    );

    WebKitUserContentManager *manager =
      webkit_web_view_get_user_content_manager(WEBKIT_WEB_VIEW(webview));

    webkit_user_content_manager_add_script(
      manager,
      webkit_user_script_new(
        preload.c_str(),
        WEBKIT_USER_CONTENT_INJECT_TOP_FRAME,
        WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START,
        nullptr,
        nullptr
      )
    );

    WebKitSettings *settings = webkit_web_view_get_settings(WEBKIT_WEB_VIEW(webview));
    webkit_settings_set_javascript_can_access_clipboard(settings, true);
    webkit_settings_set_zoom_text_only(settings, false);

    GdkRGBA rgba = {0};
    webkit_web_view_set_background_color(WEBKIT_WEB_VIEW(webview), &rgba);

    if (this->opts.debug) {
      webkit_settings_set_enable_write_console_messages_to_stdout(settings, true);
      webkit_settings_set_enable_developer_extras(settings, true);
    }

    webkit_settings_set_allow_universal_access_from_file_urls(settings, true);

    if (this->opts.isTest) {
      // webkit_settings_set_allow_universal_access_from_file_urls(settings, true);
      webkit_settings_set_allow_file_access_from_file_urls(settings, true);
    }

    // webkit_settings_set_allow_top_navigation_to_data_urls(settings, true);

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    gtk_box_pack_end(GTK_BOX(vbox), webview, true, true, 0);

    gtk_container_add(GTK_CONTAINER(window), vbox);
    gtk_widget_add_events(window, GDK_ALL_EVENTS_MASK);

    gtk_widget_grab_focus(GTK_WIDGET(webview));
  }

  ScreenSize Window::getScreenSize () {
    gtk_widget_realize(window);

    auto* display = gdk_display_get_default();
    auto* win = gtk_widget_get_window(window);
    auto* mon = gdk_display_get_monitor_at_window(display, win);

    GdkRectangle workarea = {0};
    gdk_monitor_get_geometry(mon, &workarea);

    return ScreenSize {
      .height = (int) workarea.height,
      .width = (int) workarea.width
    };
  }

  void Window::eval(const std::string& source) {
    auto webview = this->webview;
    this->app.dispatch([=, this] {
      webkit_web_view_run_javascript(
        WEBKIT_WEB_VIEW(this->webview),
        std::string(source).c_str(),
        nullptr,
        nullptr,
        nullptr
      );
    });
  }

  void Window::show(const std::string &seq) {
    gtk_widget_realize(this->window);

    if (this->opts.headless == false) {
      gtk_widget_show_all(this->window);
      gtk_window_present(GTK_WINDOW(this->window));
    }

    if (seq.size() > 0) {
      auto index = std::to_string(this->opts.index);
      this->resolvePromise(seq, "0", index);
    }
  }

  void Window::hide(const std::string &seq) {
    gtk_widget_realize(this->window);
    gtk_widget_hide(this->window);
    this->eval(emitToRenderProcess("windowHide", "{}"));

    if (seq.size() > 0) {
      auto index = std::to_string(this->opts.index);
      this->resolvePromise(seq, "0", index);
    }
  }

  void Window::setBackgroundColor(int r, int g, int b, float a) {
    GdkRGBA color;
    color.red = r / 255.0;
    color.green = g / 255.0;
    color.blue = b / 255.0;
    color.alpha = a;

    gtk_widget_realize(this->window);
    gtk_widget_override_background_color(
      this->window, GTK_STATE_FLAG_NORMAL, &color
    );
  }

  void Window::showInspector () {
    // this->webview->inspector.show();
  }

  void Window::exit(int code) {
    if (onExit != nullptr) onExit(code);
  }

  void Window::kill() {
    // gtk releases objects automatically.
  }

  void Window::close (int code) {
    if (opts.canExit) {
      this->exit(code);
    }
  }

  void Window::navigate(const std::string &seq, const std::string &s) {
    webkit_web_view_load_uri(WEBKIT_WEB_VIEW(webview), s.c_str());

    if (seq.size() > 0) {
      auto index = std::to_string(this->opts.index);
      this->resolvePromise(seq, "0", index);
    }
  }

  void Window::setTitle(const std::string &seq, const std::string &s) {
    gtk_widget_realize(window);
    gtk_window_set_title(GTK_WINDOW(window), s.c_str());

    if (seq.size() > 0) {
      auto index = std::to_string(this->opts.index);
      this->resolvePromise(seq, "0", index);
    }
  }

  int Window::openExternal(const std::string& url) {
    gtk_widget_realize(window);
    return gtk_show_uri_on_window(GTK_WINDOW(window), url.c_str(), GDK_CURRENT_TIME, nullptr);
  }

  void Window::about () {
    GtkWidget *dialog = gtk_dialog_new();
    gtk_window_set_default_size(GTK_WINDOW(dialog), 300, 200);

    GtkWidget *body = gtk_dialog_get_content_area(GTK_DIALOG(GTK_WINDOW(dialog)));
    GtkContainer *content = GTK_CONTAINER(body);

    std::string imgPath = "/usr/share/icons/hicolor/256x256/apps/" +
      appData["executable"] +
      ".png";

    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_scale(
      imgPath.c_str(),
      60,
      60,
      true,
      nullptr
    );

    GtkWidget *img = gtk_image_new_from_pixbuf(pixbuf);
    gtk_widget_set_margin_top(img, 20);
    gtk_widget_set_margin_bottom(img, 20);

    gtk_box_pack_start(GTK_BOX(content), img, false, false, 0);

    std::string title_value(appData["title"] + " v" + appData["version"]);
    std::string version_value("Built with ssc v" + std::string(SSC::full_version));

    GtkWidget *label_title = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(label_title), title_value.c_str());
    gtk_container_add(content, label_title);

    GtkWidget *label_op_version = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(label_op_version), version_value.c_str());
    gtk_container_add(content, label_op_version);

    GtkWidget *label_copyright = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(label_copyright), appData["copyright"].c_str());
    gtk_container_add(content, label_copyright);

    g_signal_connect(
      dialog,
      "response",
      G_CALLBACK(gtk_widget_destroy),
      nullptr
    );

    gtk_widget_show_all(body);
    gtk_widget_show_all(dialog);
    gtk_window_set_title(GTK_WINDOW(dialog), "About");

    gtk_dialog_run(GTK_DIALOG(dialog));
  }

  void Window::setSize(const std::string& seq, int width, int height, int hints) {
    gtk_widget_realize(window);
    gtk_window_set_resizable(GTK_WINDOW(window), hints != WINDOW_HINT_FIXED);

    if (hints == WINDOW_HINT_NONE) {
      gtk_window_resize(GTK_WINDOW(window), width, height);
    } else if (hints == WINDOW_HINT_FIXED) {
      gtk_widget_set_size_request(window, width, height);
    } else {
      GdkGeometry g;
      g.min_width = g.max_width = width;
      g.min_height = g.max_height = height;

      GdkWindowHints h = (hints == WINDOW_HINT_MIN
        ? GDK_HINT_MIN_SIZE
        : GDK_HINT_MAX_SIZE
      );

      gtk_window_set_geometry_hints(GTK_WINDOW(window), nullptr, &g, h);
    }

    if (seq.size() > 0) {
      auto index = std::to_string(this->opts.index);
      this->resolvePromise(seq, "0", index);
    }
  }

  void Window::setSystemMenu(const std::string &seq, const std::string &value) {
    if (value.empty()) return void(0);

    auto menu = std::string(value);

    if (menubar == nullptr) {
      menubar = gtk_menu_bar_new();
    } else {
      GList *iter;
      GList *children = gtk_container_get_children(GTK_CONTAINER(menubar));

      for (iter = children; iter != nullptr; iter = g_list_next(iter)) {
        gtk_widget_destroy(GTK_WIDGET(iter->data));
      }
      g_list_free(children);
    }

    // deserialize the menu
    menu = replace(menu, "%%", "\n");

    // split on ;
    auto menus = split(menu, ';');

    for (auto m : menus) {
      auto menu = split(m, '\n');
      auto line = trim(menu[0]);
      if (line.empty()) continue;
      auto menuTitle = split(line, ':')[0];
      GtkWidget *subMenu = gtk_menu_new();
      GtkWidget *menuItem = gtk_menu_item_new_with_label(menuTitle.c_str());

      for (int i = 1; i < menu.size(); i++) {
        auto line = trim(menu[i]);
        if (line.empty()) continue;
        auto parts = split(line, ':');
        auto title = parts[0];
        std::string key = "";

        GtkWidget *item;

        if (parts[0].find("---") != -1) {
          item = gtk_separator_menu_item_new();
        } else {
          item = gtk_menu_item_new_with_label(title.c_str());

          if (parts.size() > 1) {
            auto value = trim(parts[1]);
            key = value == "_" ? "" : value;

            if (key.size() > 0) {
              auto accelerator = split(parts[1], '+');
              key = trim(parts[1]) == "_" ? "" : trim(accelerator[0]);

              GdkModifierType mask = (GdkModifierType)(0);
              bool isShift = std::string("ABCDEFGHIJKLMNOPQRSTUVWXYZ").find(key) != -1;

              if (accelerator.size() > 1) {
                if (accelerator[1].find("Meta") != -1) {
                  mask = (GdkModifierType)(mask | GDK_META_MASK);
                }

                if (accelerator[1].find("CommandOrControl") != -1) {
                  mask = (GdkModifierType)(mask | GDK_CONTROL_MASK);
                } else if (accelerator[1].find("Control") != -1) {
                  mask = (GdkModifierType)(mask | GDK_CONTROL_MASK);
                }

                if (accelerator[1].find("Alt") != -1) {
                  mask = (GdkModifierType)(mask | GDK_MOD1_MASK);
                }
              }

              if (isShift) {
                mask = (GdkModifierType)(mask | GDK_SHIFT_MASK);
              }

              gtk_widget_add_accelerator(
                item,
                "activate",
                accel_group,
                (guint) key[0],
                mask,
                GTK_ACCEL_VISIBLE
              );

              gtk_widget_show(item);
            }
          }

          g_signal_connect(
            G_OBJECT(item),
            "activate",
            G_CALLBACK(+[](GtkWidget *t, gpointer arg) {
              auto w = static_cast<Window*>(arg);
              auto title = gtk_menu_item_get_label(GTK_MENU_ITEM(t));
              auto parent = gtk_widget_get_name(t);

              if (std::string(title).find("About") == 0) {
                return w->about();
              }

              if (std::string(title).find("Quit") == 0) {
                return w->exit(0);
              }

              w->eval(resolveMenuSelection("0", title, parent));
            }),
            this
          );

        }

        gtk_widget_set_name(item, menuTitle.c_str());
        gtk_menu_shell_append(GTK_MENU_SHELL(subMenu), item);
      }

      gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuItem), subMenu);
      gtk_menu_shell_append(GTK_MENU_SHELL(menubar), menuItem);
    }

    gtk_box_pack_start(GTK_BOX(vbox), menubar, false, false, 0);
    gtk_widget_show_all(menubar);

    if (seq.size() > 0) {
      auto index = std::to_string(this->opts.index);
      this->resolvePromise(seq, "0", index);
    }
  }

  void Window::setSystemMenuItemEnabled (bool enabled, int barPos, int menuPos) {
    // @TODO(): provide impl
  }

  void Window::closeContextMenu() {
    if (popupId > 0) {
      popupId = 0;
      auto seq = std::to_string(popupId);
      closeContextMenu(seq);
    }
  }

  void Window::closeContextMenu(const std::string &seq) {
    if (popup != nullptr) {
      auto ptr = popup;
      popup = nullptr;
      closeContextMenu(ptr, seq);
    }
  }

  void Window::closeContextMenu(GtkWidget *popupMenu, const std::string &seq) {
    if (popupMenu != nullptr) {
      gtk_menu_popdown((GtkMenu *) popupMenu);
      gtk_widget_destroy(popupMenu);
      this->eval(resolveMenuSelection(seq, "", "contextMenu"));
    }
  }

  void Window::setContextMenu(const std::string &seq, const std::string &value) {
    closeContextMenu();

    // members
    popup = gtk_menu_new();

    try {
      popupId = std::stoi(seq);
    } catch (...) {
      popupId = 0;
    }

    auto menuData = replace(value, "_", "\n");
    auto menuItems = split(menuData, '\n');

    for (auto itemData : menuItems) {
      if (trim(itemData).size() == 0) {
        continue;
      }

      if (itemData.find("---") != -1) {
        auto *item = gtk_separator_menu_item_new();
        gtk_widget_show(item);
        gtk_menu_shell_append(GTK_MENU_SHELL(popup), item);
        continue;
      }

      auto pair = split(itemData, ':');
      auto meta = std::string(seq + ";" + pair[0].c_str());
      auto *item = gtk_menu_item_new_with_label(pair[0].c_str());

      g_signal_connect(
        G_OBJECT(item),
        "activate",
        G_CALLBACK(+[](GtkWidget *t, gpointer arg) {
          auto window = static_cast<Window*>(arg);
          auto label = gtk_menu_item_get_label(GTK_MENU_ITEM(t));
          auto title = std::string(label);
          auto meta = gtk_widget_get_name(t);
          auto pair = split(meta, ';');
          auto seq = pair[0];

          window->eval(resolveMenuSelection(seq, title, "contextMenu"));
        }),
        this
      );

      gtk_widget_set_name(item, meta.c_str());
      gtk_widget_show(item);
      gtk_menu_shell_append(GTK_MENU_SHELL(popup), item);
    }

    GdkRectangle rect;
    gint x, y;

    auto win = GDK_WINDOW(gtk_widget_get_window(window));
    auto seat = gdk_display_get_default_seat(gdk_display_get_default());
    auto event = gdk_event_new(GDK_BUTTON_PRESS);
    auto mouse_device = gdk_seat_get_pointer(seat);

    gdk_window_get_device_position(win, mouse_device, &x, &y, nullptr);
    gdk_event_set_device(event, mouse_device);

    event->button.send_event = 1;
    event->button.button = GDK_BUTTON_SECONDARY;
    event->button.window = GDK_WINDOW(g_object_ref(win));
    event->button.time = GDK_CURRENT_TIME;

    rect.height = 0;
    rect.width = 0;
    rect.x = x - 1;
    rect.y = y - 1;

    gtk_widget_add_events(popup, GDK_ALL_EVENTS_MASK);
    gtk_widget_set_can_focus(popup, true);
    gtk_widget_show_all(popup);
    gtk_widget_grab_focus(popup);

    gtk_menu_popup_at_rect(
      GTK_MENU(popup),
      win,
      &rect,
      GDK_GRAVITY_SOUTH_WEST,
      GDK_GRAVITY_NORTH_WEST,
      event
    );
  }

  void Window::openDialog (
    const std::string& seq,
    bool isSave,
    bool allowDirs,
    bool allowFiles,
    bool allowMultiple,
    const std::string& defaultPath,
    const std::string& title,
    const std::string& defaultName
  ) {
    const guint SELECT_RESPONSE = 0;
    GtkFileChooserAction action;
    GtkFileChooser *chooser;
    GtkFileFilter *filter;
    GtkWidget *dialog;

    if (isSave) {
      action = GTK_FILE_CHOOSER_ACTION_SAVE;
    } else {
      action = GTK_FILE_CHOOSER_ACTION_OPEN;
    }

    if (!allowFiles && allowDirs) {
      action = (GtkFileChooserAction) (action | GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
    }

    gtk_init_check(nullptr, nullptr);

    std::string dialogTitle = isSave ? "Save File" : "Open File";
    if (title.size() > 0) {
      dialogTitle = title;
    }

    dialog = gtk_file_chooser_dialog_new(
      dialogTitle.c_str(),
      nullptr,
      action,
      "_Cancel",
      GTK_RESPONSE_CANCEL,
      nullptr
    );

    chooser = GTK_FILE_CHOOSER(dialog);

    if (!allowDirs) {
      if (isSave) {
        gtk_dialog_add_button(GTK_DIALOG(dialog), "_Save", GTK_RESPONSE_ACCEPT);
      } else {
        gtk_dialog_add_button(GTK_DIALOG(dialog), "_Open", GTK_RESPONSE_ACCEPT);
      }
    }

    if (allowMultiple || allowDirs) {
      gtk_dialog_add_button(GTK_DIALOG(dialog), "Select", SELECT_RESPONSE);
    }

    // if (FILE_DIALOG_OVERWRITE_CONFIRMATION) {
    gtk_file_chooser_set_do_overwrite_confirmation(chooser, true);
    // }

    if ((!isSave || allowDirs) && allowMultiple) {
      gtk_file_chooser_set_select_multiple(chooser, true);
    }

    if (defaultPath.size() > 0) {
      auto status = fs::status(defaultPath);

      if (fs::exists(status)) {
        if (fs::is_directory(status)) {
          gtk_file_chooser_set_current_folder(chooser, defaultPath.c_str());
        } else {
          gtk_file_chooser_set_filename(chooser, defaultPath.c_str());
        }
      }
    }

    if (defaultName.size() > 0) {
      if ((!allowFiles && allowDirs) || isSave) {
        gtk_file_chooser_set_current_name(chooser, defaultName.c_str());
      } else {
        gtk_file_chooser_set_current_folder(chooser, defaultName.c_str());
      }
    }

    guint response = gtk_dialog_run(GTK_DIALOG(dialog));
    if (response != GTK_RESPONSE_ACCEPT && response != SELECT_RESPONSE) {
      this->resolvePromise(seq, "0", "null");
      gtk_widget_destroy(dialog);
      return;
    }

    // TODO (@heapwolf): validate multi-select

    while (gtk_events_pending()) {
      gtk_main_iteration();
    }

    std::string result("");
    GSList* filenames = gtk_file_chooser_get_filenames(chooser);
    int i = 0;

    for (int i = 0; filenames != nullptr; ++i) {
      gchar* file = (gchar*) filenames->data;
      result += (i ? "\\n" : "");
      result += std::string(file);
      filenames = filenames->next;
    }

    g_slist_free(filenames);

    auto wrapped = std::string("\"" + std::string(result) + "\"");
    this->resolvePromise(seq, "0", encodeURIComponent(wrapped));
    gtk_widget_destroy(dialog);
  }
}