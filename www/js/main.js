/* Melo JS main - Main task for Melo JS */

$(document).ready(function() {
    /* Init player */
    melo_player_init();

    /* Load module list for menu */
    melo_module_update_list();

    /* Load player list */
    melo_player_update_list();

    /* Start player timer */
    melo_player_timer_start(0.8);

    /* Manage player timer according to document visibility */
    $(document).on('visibilitychange', function() {
        if (document.hidden) {
            /* Stop player timer */
            melo_player_timer_stop();
        } else {
            /* Start player timer */
            melo_player_timer_start(0.8);
        }
    });

    /* TODO */
    $("#open-side").click(function() {
        $("#page-side").show();
        $("#page-overlay").show();
    });
    $("#page-overlay").click(function() {
        $("#page-side").hide();
        $("#page-overlay").hide();
    });
});
