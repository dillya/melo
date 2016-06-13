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
      var mod = $('<li>' + response.result[i].id + ' -> ' +
                           response.result[i].name +  '</li>');
      mod.click(response.result[i].id, function(e) {
        melo_get_info(e.data);
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

$(document).ready(function() {
  /* Load module list */
  melo_update_list();

  /* Add click events */
  $("#module_refresh").click(function() {melo_update_list(); return false;});
});
