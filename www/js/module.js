/* Melo JS module - Module functions for Melo JS
 *
 * Exported functions:
 *  - melo_module_update_list()
 */

function melo_module_click(eventData) {
    $(this).next("ul").toggleClass("nav-sub-show");
    return false;
}

function melo_module_click_browser(eventData) {
    /* TODO */
    melo_get_browser(eventData.data);
    return false;
}

function melo_module_click_settings(eventData) {
    /* TODO */
    melo_get_config(eventData.data);
    return false;
}

function melo_module_render_list(result) {
    /* Generate module list */
    var list = $("#nav-modules").html("");
    for (var i = 0; i < result.length; i++) {
        var browser_list = result[i].browser_list;

        /* Create module item */
        var item = $(
            '<li>' +
            '  <a class="nav-link" href="#">' + result[i].name + '</a>' +
            '  <ul class="nav-sub"></ul>' +
            '</li>');
        item.children('a').click(result[i].id, melo_module_click);
        var blist = item.children('ul');

        /* Create browser items */
        if (browser_list) {
            for (var j = 0; j < browser_list.length; j++) {
                var bitem = $('<li>' +
                              '  <a class="nav-sub-link" href="#">' +
                                   browser_list[j].name +
                              '  </a>' +
                              '</li>');
                bitem.children('a').click(browser_list[j],
                    melo_module_click_browser);
                blist.append(bitem);
            }
        }

        /* Add settings to sub list */
        if (result[i].config_id) {
            var sitem = $(
                '<li><a class="nav-sub-link" href="#">Settings</a></li>');
            sitem.children('a').click(result[i].id, melo_module_click_settings);
            blist.append(sitem);
        }

        /* Append module to menu */
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
