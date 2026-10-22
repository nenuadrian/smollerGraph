#include "server.h"

#include <httplib.h>

#include <iostream>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "api.h"

namespace smollergraph {

absl::Status Serve(const Graph& graph, const std::string& address, int port) {
  httplib::Server server;
  // cpp-httplib lets several servers share a port by default, so a second sg
  // serve on a busy port would start without a word. SO_REUSEADDR alone lets
  // a restarted server have its port straight back, and refuses a busy one.
  server.set_socket_options([](socket_t fd) {
    httplib::set_socket_opt(fd, SOL_SOCKET, SO_REUSEADDR, 1);
  });
  // The server answers requests on several threads at once. Nothing writes to
  // the graph while it's served, and pread doesn't move a shared position in
  // the file, so their reads don't get in each other's way.
  server.Get("/api/graph",
             [&graph](const httplib::Request&, httplib::Response& response) {
               response.set_content(GraphJson(graph), "application/json");
             });
  if (!server.bind_to_port(address, port)) {
    return absl::UnavailableError(
        absl::StrCat("can't listen on ", address, " port ", port));
  }
  // std::endl flushes, so the line shows straight away in Docker's logs too.
  std::cout << "Serving the graph on http://localhost:" << port << std::endl;
  if (!server.listen_after_bind()) {
    return absl::InternalError("stopped listening");
  }
  return absl::OkStatus();
}

}  // namespace smollergraph
