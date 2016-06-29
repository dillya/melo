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

    /* Generate module list */
    $("#module_list").html("");
    for (var i = 0; i < response.result.length; i++) {
      /* Generate module item */
      var mod = $('<li>' + response.result[i].name + ' [' +
                           response.result[i].id + '] (' +
                           '<a class="info" href="#">info</a>' +
                           ' | <a class="browsers" href="#">browsers</a>' +
                  ')<ul class=browser_list></ul></li>');

      /* Add a link to get module details */
      mod.children("a.info").click(response.result[i].id, function(e) {
        melo_get_info(e.data);
        return false;
      });

      /* Add a link to get browser list attached to the module */
      mod.children("a.browsers").click(
                       [response.result[i].id, mod.children("ul.browser_list")],
                       function(e) {
        melo_get_browsers(e.data[0], e.data[1]);
        return false;
      });

      /* Add item to list */
      $("#module_list").append(mod);
    }
  });
}

function melo_get_info(id) {
  jsonrpc_call("module.get_info", JSON.parse('["' + id + '",["full"]]'),
               function(response) {
    if (response.error || !response.result)
      return;

    /* Display module details */
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

    /* Generate browser list */
    ul.html("");
    for (var i = 0; i < response.result.length; i++) {
      /* Geneate browser item */
      var bro = $('<li>' + response.result[i].name + ' [' +
                           response.result[i].id +'] (' +
                           '<a class="info" href="#">info</a> | ' +
                           '<a class="open" href="#">open</a>)</li>');

      /* Add a link to get browser info */
      bro.children("a.info").click(response.result[i].id, function(e) {
        melo_get_browser_info(e.data);
        return false;
      });

      /* Add a link to get list from browser */
      bro.children("a.open").click(response.result[i].id, function(e) {
        melo_get_browser_list(e.data, "/");
        return false;
      });

      /* Add item to list */
      ul.append(bro);
    }
  });
}

function melo_get_browser_info(id) {
  jsonrpc_call("browser.get_info", JSON.parse('["' + id + '",["full"]]'),
               function(response) {
    if (response.error || !response.result)
      return;

    /* Display browser details */
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

    /* Save current ID and path of the browser */
    melo_browser_current_id = id;
    melo_browser_current_path = path;

    /* Generate list */
    $('#browser_list').html("");
    for (var i = 0; i < response.result.length; i++) {
      var name = response.result[i].name;
      var npath = path + response.result[i].name + "/";

      /* Use full name when available */
      if (response.result[i].full_name != null)
        name = response.result[i].full_name + ' (' + name + ')';

      /* Generate list item */
      var item = $('<li><a href="#">' + name + '</a> [' +
                                        response.result[i].type + ']</li>');

      /* Setup link */
      if (response.result[i].type == "directory" ||
          response.result[i].type == "category") {
        /* Get list of children */
        item.children("a").click([id, npath], function(e) {
          melo_get_browser_list(e.data[0], e.data[1]);
          return false;
        });
      } else {
        /* Do action on file / item */
        item.children("a").click(response, function(e) {
          return false;
        });
      }

      /* Add a link if item is removable */
      if (response.result[i].remove != null) {
        var rm = $('<a href="#">' + response.result[i].remove + '</a>');
        rm.click([id, npath], function(e) {
          melo_browser_remove(e.data[0], e.data[1]);
          return false;
        });
        item.append(" [");
        item.append(rm);
        item.append("]");
      }

      /* Add item */
      $('#browser_list').append(item);
    }
  });
}

function melo_browser_remove(id, path) {
  jsonrpc_call("browser.remove", JSON.parse('["' + id + '","' + path + '"]'),
               function(response) {
    if (response.error || !response.result)
      return;

    /* Update list when remove is done */
    melo_get_browser_list(melo_browser_current_id, melo_browser_current_path);
  });
}

function melo_browser_previous() {
  var path = melo_browser_current_path;
  var n = path.lastIndexOf('/', path.length - 2);

  /* Root */
  if (n == -1)
    return;

  /* Get parent and update list */
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
