/* Helper to do JSON-RPC 2.0 call */
function jsonrpc_call(method, params, callback) {
  var request = {};
  request.jsonrpc = "2.0";
  request.method = method;
  request.params = params;
  request.id = 1;

  $.post("/rpc", JSON.stringify(request), callback, "json");
}

function melo_update_list() {
  jsonrpc_call("module.get_list", JSON.parse('[["full"]]'), function(response) {
    if (response.error || !response.result)
      return;

    $("#module_list").html("");
    for (var i = 0; i < response.result.length; i++) {
      var mod = $('<li>' + response.result[i].name + ' [' +
                           response.result[i].id + '] (' +
                           '<a class="info" href="#">info</a>' +
                           ' | <a class="browsers" href="#">browsers</a>' +
                  ')<ul class=browser_list></ul></li>');
      mod.children("a.info").click(response.result[i].id, function(e) {
        melo_get_info(e.data);
        return false;
      });
      mod.children("a.browsers").click(
                       [response.result[i].id, mod.children("ul.browser_list")],
                       function(e) {
        melo_get_browsers(e.data[0], e.data[1]);
        return false;
      });
      $("#module_list").append(mod);
    }
  });
}

function melo_get_info(id) {
  jsonrpc_call("module.get_info", JSON.parse('["' + id + '",["full"]]'),
               function(response) {
    if (response.error || !response.result)
      return;

    $("#module_info_name").html(response.result.name);
    $("#module_info_descr").html(response.result.description);
  });
}

function melo_get_browsers(id, ul) {
  jsonrpc_call("module.get_browser_list",
               JSON.parse('["' + id + '",["name"]]'),
               function(response) {
    if (response.error || !response.result)
      return;

    ul.html("");
    for (var i = 0; i < response.result.length; i++) {
      var bro = $('<li>' + response.result[i].name + ' [' +
                           response.result[i].id +'] (' +
                           '<a class="info" href="#">info</a> | ' +
                           '<a class="open" href="#">open</a>)</li>');
      bro.children("a.info").click(response.result[i].id, function(e) {
        melo_get_browser_info(e.data);
        return false;
      });
      bro.children("a.open").click(response.result[i].id, function(e) {
        melo_get_browser_list(e.data, "/");
        return false;
      });
      ul.append(bro);
    }
  });
}

function melo_get_browser_info(id) {
  jsonrpc_call("browser.get_info", JSON.parse('["' + id + '",["full"]]'),
               function(response) {
    if (response.error || !response.result)
      return;

    $("#browser_info_name").html(response.result.name);
    $("#browser_info_descr").html(response.result.description);
  });
}

var melo_browser_current_id = "";
var melo_browser_current_path = "";

function melo_get_browser_list(id, path) {
  jsonrpc_call("browser.get_list", JSON.parse('["' + id + '","' + path + '"]'),
               function(response) {
    if (response.error || !response.result)
      return;

    melo_browser_current_id = id;
    melo_browser_current_path = path;

    $('#browser_list').html("");
    for (var i = 0; i < response.result.length; i++) {
      var item = $('<li><a href="#">' + response.result[i].name + '</a> [' +
                                        response.result[i].type + ']</li>');
      if (response.result[i].type = "directory") {
        var npath = path + response.result[i].name + "/";
        item.children("a").click([id, npath], function(e) {
          melo_get_browser_list(e.data[0], e.data[1]);
          return false;
        });
      } else {
        item.children("a").click(response, function(e) {
          return false;
        });
      }
      $('#browser_list').append(item);
    }
  });
}

function melo_browser_previous() {
  var path = melo_browser_current_path;
  var n = path.lastIndexOf('/', path.length - 2);
  console.log (path + "..." + n);
  if (n == -1)
    return;
  path = path.substring(0, n + 1);
  melo_get_browser_list(melo_browser_current_id, path);
}

$(document).ready(function() {
  /* Load module list */
  melo_update_list();

  /* Add click events */
  $("#module_refresh").click(function() {melo_update_list(); return false;});
  $("#browser_prev").click(function() {melo_browser_previous(); return false;});
});
