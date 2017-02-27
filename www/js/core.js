/* Melo JS core - Core functions for Melo JS
 * Exported functions:
 *  - melo_jsonrpc_request()
 */

function melo_jsonrpc_request(method, params, callback) {
    /*  Prepare JSON-RPC request */
    var request = '{"jsonrpc":"2.0","method":"' + method + '",' +
        '"params":' + params + ',"id":1}';

    /* Send JSON-RPC request */
    $.ajax({
        url: "/rpc",
        method: "POST",
        data: request,
        dataType: "json",
        success: function(data) {
            if (data.error) {
              console.log("[JSON-RPC] ERROR: method '" + method +
                  "' failed with error: \"" + data.error.message + "\"");
              return;
            }
            callback(data.result);
        }
    });
}
