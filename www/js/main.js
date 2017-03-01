/* Melo JS main - Main task for Melo JS */

$(document).ready(function() {
    /* Init player */
    melo_player_init();

    /* Load Module list for menu */
    melo_module_update_list();

    /* FIXME */
    melo_player_update_list();
    /* Start player timer */
    melo_player_timer_start(0.8);
});
