/* Melo JS module - Module functions for Melo JS */

function melo_module_click(data) {
    console.log(data.data);
}

function melo_module_render_list(result) {
    /* Generate module list */
    var list = $("#nav_module_list").html("");
    for (var i = 0; i < result.length; i++) {
        var browser_list = result[i].browser_list;
        var blist = "";
        if (browser_list) {
            for (var j = 0; j < browser_list.length; j++) {
                blist += '<li><a href="' + browser_list[j].id + '">' + browser_list[j].name + '</li>';
            }
        }
        if (result[i].config_id)
          blist += '<li><a href="#">Settings</li>';

        var item = $('<li>' + result[i].name + '<ul>' + blist + '</ul></li>');
        item.click(result[i].id, melo_module_click);
        list.append(item);
    }
}

function melo_module_update_list() {
    /* Get module list */
    melo_jsonrpc_request("module.get_full_list", '[["full"],["full"],["full"]]',
        function(result) {
            /* Update module list */
            melo_module_render_list(result);
        }
    );
}
