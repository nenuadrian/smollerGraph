#include "server.h"

#include <httplib.h>

#include <iostream>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "api.h"

namespace smollergraph {
namespace {

// Sends JSON, or an error as JSON, with the HTTP status that goes with the
// error's code: 404 for something that isn't there, and 400 for a request
// that's wrong.
void Send(const absl::StatusOr<std::string>& json,
          httplib::Response& response) {
  if (!json.ok()) {
    response.status = absl::IsNotFound(json.status()) ? 404 : 400;
    response.set_content(ErrorJson(json.status().message()),
                         "application/json");
    return;
  }
  response.set_content(*json, "application/json");
}

}  // namespace

absl::Status Serve(const Graph& graph, const std::string& address, int port) {
  httplib::Server server;
  // cpp-httplib lets several servers share a port by default, so a second sg
  // serve on a busy port would start without a word. SO_REUSEADDR alone lets
  // a restarted server have its port straight back, and refuses a busy one.
  server.set_socket_options([](socket_t fd) {
    httplib::set_socket_opt(fd, SOL_SOCKET, SO_REUSEADDR, 1);
  });
  // The UI's files are served from the source tree, so changing one needs a
  // reload in the browser, not a new build.
  if (!server.set_mount_point("/", SMOLLERGRAPH_UI)) {
    return absl::NotFoundError(
        absl::StrCat("there's no folder ", SMOLLERGRAPH_UI));
  }
  // The server answers requests on several threads at once. Nothing writes to
  // the graph while it's served, and the buffer pools' latches keep their
  // reads from getting in each other's way.
  server.Get("/api/graph",
             [&graph](const httplib::Request&, httplib::Response& response) {
               response.set_content(GraphJson(graph), "application/json");
             });
  server.Get("/api/names", [&graph](const httplib::Request& request,
                                    httplib::Response& response) {
    response.set_content(NamesJson(graph, request.get_param_value("prefix")),
                         "application/json");
  });
  server.Get("/api/walk", [&graph](const httplib::Request& request,
                                   httplib::Response& response) {
    Send(WalkJson(graph, request.get_param_value("from"),
                  request.get_param_value("type"),
                  request.get_param_value("hops")),
         response);
  });
  server.Get(R"(/api/node/(\d+))", [&graph](const httplib::Request& request,
                                            httplib::Response& response) {
    Send(NodeJson(graph, request.matches[1].str()), response);
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
