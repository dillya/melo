/* Melo JS player - Player functions for Melo JS
 *
 * Exported functions:
 *  - melo_player_init()
 *  - melo_player_timer_start()
 *  - melo_player_timer_stop()
 *  - melo_player_update_list()
 */

/* Constants */
var melo_player_position_length = 220;
var melo_player_max_handle_position = 216;
var melo_player_volume_handle_half_width = 5;

/* Timer */
var melo_player_timer = null;

/* Current player */
var melo_player_current = {
    id: "",
    playlist_id: "",
    controls: null,
    state: "none",
    is_playing: false,
    name: "",
    ts: 0,
    pos: 0,
    duration: 0
};

/*
 * Helpers
 */
function melo_player_print_time(time_s) {
    time_s = Math.floor(time_s);
    var h = Math.floor(time_s / 3600);
    var m = Math.floor(time_s / 60) - (h * 60);
    var s = time_s - (h * 3600) - (m * 60);
    if (!h)
        return ('0' + m).slice(-2) + ":" + ('0' + s).slice(-2);
    return ('0' + h).slice(-2) + ":" + ('0' + m).slice(-2) + ":" +
        ('0' + s).slice(-2);
}

/*
 * Control helpers
 */
function melo_player_set_state(id, state) {
    melo_jsonrpc_request("player.set_state", '["' + id + '","' + state + '"]',
        function(result) {
            melo_player_update_state(result.state, null);
            /* FIXME */
            melo_player_update_player();
        }
    );
}

function melo_player_prev(id) {
    melo_jsonrpc_request("player.prev", '["' + id + '"]',
        function(result) {
            /* TODO */
            melo_player_update_player();
        }
    );
}

function melo_player_next(id) {
    melo_jsonrpc_request("player.next", '["' + id + '"]',
        function(result) {
            /* TODO */
            melo_player_update_player();
        }
    );
}

function melo_player_seek(id, pos) {
    melo_jsonrpc_request("player.set_pos", '["' + id + '",' + pos + ']',
        function(result) {
            /* TODO */
        }
    );
}

function melo_player_set_volume(id, vol) {
    melo_jsonrpc_request("player.set_volume", '["' + id + '",' + vol + ']',
        function(result) {
            /* TODO */
        }
    );
}

function melo_player_set_mute(id, mute) {
    melo_jsonrpc_request("player.set_mute", '["' + id + '",' + mute + ']',
        function(result) {
            melo_player_update_mute(result.mute);
        }
    );
}

/*
 * Player Rendering
 */
function melo_player_update_control(playlist, controls) {
    melo_player_current.controls = controls;

    if (playlist && playlist != "")
        $("#player-playlist").removeClass("player-button-disabled");
    else
        $("#player-playlist").addClass("player-button-disabled");
    if (controls.state) {
        $("#player-play").removeClass("player-button-disabled");
        $("#player-stop").show();
    } else {
        $("#player-play").addClass("player-button-disabled");
        $("#player-stop").hide();
    }
    if (controls.prev)
        $("#player-prev").removeClass("player-button-disabled");
    else
        $("#player-prev").addClass("player-button-disabled");
    if (controls.next)
        $("#player-next").removeClass("player-button-disabled");
    else
        $("#player-next").addClass("player-button-disabled");
    if (controls.volume)
        $("#player-volume-overlay").hide();
    else
        $("#player-volume-overlay").show();
    if (controls.mute)
        $("#player-mute-overlay").hide();
    else
        $("#player-mute-overlay").show();
}

function melo_player_update_state(state, result) {
    melo_player_current.state = state;

    /* Display message */
    if (state == "loading")
        $("#player-status").text("Loading...").show();
    else if (state == "buffering")
        $("#player-status")
            .text("Buffering... (" + result.buffer + " %)").show();
    else if (state == "error")
        $("#player-status").text("Error: " + result.error).show();
    else
        $("#player-status").text("").hide();

    /* Player is playing */
    if (state == "loading" || state == "buffering" || state == "playing") {
        $("#player-play").children()
            .attr("d", "M4 0 h10 v32 h-10 Z M18 0 h10 v32 h-10 Z");
        melo_player_current.is_playing = true;
        $("#player-stop").removeClass("player-button-disabled");
    } else {
        $("#player-play").children().attr("d", "M4.3 0 V32 L32 16 Z");
        melo_player_current.is_playing = false;

        /* Player is stopped */
        if (state != "paused")
            $("#player-stop").addClass("player-button-disabled");
        else
            $("#player-stop").removeClass("player-button-disabled");
    }

    /* Update playlist control */
    if (result) {
        if (result.has_prev)
            $("#player-prev").removeClass("player-button-disabled");
        else
            $("#player-prev").addClass("player-button-disabled");
        if (result.has_next)
            $("#player-next").removeClass("player-button-disabled");
        else
            $("#player-next").addClass("player-button-disabled");
    }
}

function melo_player_update_mute(mute) {
    melo_player_current.mute =  mute;

    if (mute) {
        $("#player-mute").children("line").show();
        $("#player-volume-overlay").show();
    } else {
        $("#player-mute").children("line").hide();
        if (melo_player_current.controls.volume)
            $("#player-volume-overlay").hide();
    }
}

/*
 * Player handling
 */
function melo_player_render_player(result) {
    /* Update media details */
    if (result.name != melo_player_current.name || result.tags) {
        var title = result.name;
        var artist = "";
        var cover = "";

        /* Update tags related */
        if (result.tags) {
            /* Set details according to tags */
            cover = result.tags.cover_url +'?t=' + result.tags.timestamp;
            if (result.tags.title) {
              title = result.tags.title;
              artist = result.tags.artist || "";
            }

            /* Update current timestamp */
            melo_player_current.ts = result.tags.timestamp;
        }

        /* Update cover and track */
        $("#player-cover-img").attr("src", cover);
        $("#player-track").html(
            '<h2>' + title + '</h2><h3>' + artist + '</h3>');

        /* Update current media name */
        melo_player_current.name = result.name;
    }

    /* Update state */
    melo_player_update_state(result.state, result);

    /* Update position */
    melo_player_current.duration = result.duration;
    var width = 0;
    var time = melo_player_print_time(result.pos / 1000);
    if (result.duration) {
        width = result.pos * 100 / result.duration;
        time += ' / ' + melo_player_print_time(result.duration / 1000);
    }
    $("#player-position-bar").width(width + "%");
    $("#player-time").text(time);

    /* Update volume */
    width = result.volume * 100;
    var pos = $("#player-volume").width() * result.volume +
        $("#player-volume").offset().left -
        melo_player_volume_handle_half_width;
    $("#player-volume-value").width(width + "%");
    $("#player-volume-handle").offset({left: pos });
    melo_player_update_mute(result.mute);
}

function melo_player_update_player() {
    /* Empty player status */
    if (melo_player_current.id == "") {
        $("#player-track").html("");
        $("#player-cover-img").attr("src", "");
        $("#player-position-bar").width(0);
        $("#player-status").text("").hide();
        $("#player-time").text("00:00 / 00:00");
        melo_player_current.playlist_id = "";
        melo_player_current.duration = 0;
        melo_player_current.name = "";
        melo_player_current.ts = 0;
        return;
    }

    /* Get player status */
    melo_jsonrpc_request("player.get_status",
        '{"id": "' + melo_player_current.id + '",' +
        '"fields": ["full"],' +
        '"tags": ["title","artist","album","cover_url"],' +
        '"tags_ts": ' + melo_player_current.ts + '}',
        function(result) {
            melo_player_render_player(result);
        },
        melo_player_update_list);
}

function melo_player_render_list(result) {
    var player_found = false;

    /* Get current tabs */
    var tabs = $("#player-tabs");

    /* Update all player tabs */
    for (var i = 0; i < result.length; i++) {
        var id = result[i].id;
        var name = result[i].name || "No name";
        var img = "";

        /* Find tab */
        var tab = tabs.find("#player-tab_" + id);
        if (!tab || !tab.length) {
            /* Create new tab */
            tab = $(
                '<div id="player-tab_' + id + '" class="player-tab">' +
                '  <div class="player-tab-cover"><img src=""></div>' +
                '  <span>' + name + '</span>' +
                '</div>');

            /* Add event handlers on tab element and store ID */
            tab.children(":first")
                .click(melo_player_tab_click)
               .hover(
                    function() {
                        $(this).next().show();
                    }, function() {
                        $(this).next().hide();
                    })
                .data("id", id)
                .data("playlist_id", result[i].playlist)
                .data("controls", result[i].controls);

            /* Insert new tab */
            if (i) {
                tabs.children("#player-tab_" + result[i-1].id).after(tab);
            } else
                tabs.prepend(tab);
        }

        /* Process tags */
        if (result[i].status.tags) {
            /* Get cover URL */
            if (result[i].status.tags.cover_url)
                img = result[i].status.tags.cover_url +'?t=' +
                    result[i].status.tags.timestamp;
        }

        /* Update player tab */
        tab.find("img").attr("src", img);

        /* Current player */
        if (result[i].id == melo_player_current.id) {
            tab.children(":first").addClass("player-tab-active");
            player_found = true;
        }
    }

    /* Remove old player tabs */
    tab = tabs.children();
    for (var i = 0, j = 0; i < tab.length; i++) {
        if (j < result.length && tab[i].id == "player-tab_" + result[j].id) {
            j++;
            continue;
        }

        /* Current player */
        if (tab[i].id == "player-tab_" + melo_player_current.id)
            player_found = false;

        /* Remove tab */
        tab[i].remove();
    }

    /* Update current player */
    if (player_found == false) {
        if (result.length > 0) {
            melo_player_current.ts = 0;
            melo_player_current.id = result[0].id;
            melo_player_current.playlist_id = result[0].playlist;
            tabs.find(".player-tab-cover:first").addClass("player-tab-active");
            melo_player_update_control(result[0].playlist, result[0].controls);
        } else
            melo_player_current.id = "";
    }
    melo_player_update_player();
}

function melo_player_update_list() {
    /* Get player list */
    melo_jsonrpc_request("player.get_list",
        '[["full"],["state","tags"],["cover_url"]]',
        function(result) {
            /* Update player list */
            melo_player_render_list(result);
        }
    );
}

/*
 * Timer for status poll
 */
function melo_player_timer_event() {
    if ($("#player-ptabs").is(":visible")) {
        /* Update whole player list */
        melo_player_update_list();
    } else {
        /* Update current player */
        melo_player_update_player();
    }
}

function melo_player_timer_start(time_s) {
    if (melo_player_timer)
        return;
    time_s = time_s || 1;
    melo_player_timer_event();
    melo_player_timer = setInterval(melo_player_timer_event, time_s * 1000);
}

function melo_player_timer_stop() {
    if (!melo_player_timer)
        return;
    clearInterval(melo_player_timer);
    melo_player_timer = null;
}

/*
 * Event handlers
 */
function melo_player_toggle_cover_tabs(eventData) {
    /* Update player list before showing */
    if ($("#player-ptabs").not(":visible"))
        melo_player_update_list();

    /* Toggle player list */
    $("#player-ptabs").fadeToggle();
}

function melo_player_tab_click(eventData) {
    /* Change active tab */
    $(".player-tab-active").removeClass("player-tab-active");
    $(this).addClass("player-tab-active");

    /* Update main cover */
    $("#player-cover-img").attr("src", $(this).children(":first").attr("src"));

    /* Update player */
    melo_player_current.id = $(this).data("id");
    melo_player_current.playlist_id = $(this).data("playlist_id");
    melo_player_current.ts = 0;
    melo_player_update_control(melo_player_current.playlist_id,
        $(this).data("controls"));
    melo_player_update_player();
    return false;
}

function melo_player_position_click(eventData) {
    /* No seek available */
    if (!melo_player_current.duration)
        return false;

    /* Get cursor position */
    var x = eventData.pageX;
    if (x > melo_player_position_length)
        x = melo_player_position_length;
    if (x < 0)
        x = 0;

    /* Seek */
    var pos = (x - $(this).offset().left) * melo_player_current.duration /
        $(this).width();
    melo_player_seek(melo_player_current.id, Math.floor(pos));

    /* Update player position */
    var width = (x - $(this).offset().left) * 100 / $(this).width();
    $("#player-position-bar").width(width + "%");
    return false;
}

function melo_player_position_move(eventData) {
    /* No seek available */
    if (!melo_player_current.duration)
        return;

    /* Get cursor position */
    var x = eventData.pageX;
    if (x > melo_player_position_length)
        x = melo_player_position_length;
    if (x < 0)
        x = 0;
    var p = Math.floor(x * melo_player_current.duration /
                melo_player_position_length / 1000);

    /* Update handle position */
    if (x > melo_player_max_handle_position)
        x = melo_player_max_handle_position;
    $("#player-position-handle").offset({left: x});

    /* Update time in handle */
    var time = $("#player-time-handle");
    time.text(melo_player_print_time(p));
    var width = time.outerWidth();
    x -= (width / 2) - 2;
    if (x < 0)
        x = 0;
    else if (x > melo_player_position_length - width)
        x = melo_player_position_length - width;
    time.offset({left: x});
}

function melo_player_show_handle(eventData) {
    if (!melo_player_current.duration)
        return;
    $("#player-position-handle").show();
    $("#player-time-handle").show();
}

function melo_player_hide_handle(eventData) {
    $("#player-position-handle").hide();
    $("#player-time-handle").hide();
}

function melo_player_play_click(eventData) {
    melo_player_set_state(melo_player_current.id,
        melo_player_current.is_playing ? "paused" : "playing");
}

function melo_player_stop_click(eventData) {
    melo_player_set_state(melo_player_current.id, "stopped");
}

function melo_player_prev_click(eventData) {
    melo_player_prev(melo_player_current.id);
}

function melo_player_next_click(eventData) {
    melo_player_next(melo_player_current.id);
}

function melo_player_volume_click(eventData) {
    /* Calculate volume */
    var x = eventData.pageX - $(this).offset().left;
    if (x > $(this).width())
        x = $(this).width();
    if (x < 0)
        x = 0;
    var width = x * 100 / $(this).width();
    var pos = x - melo_player_volume_handle_half_width + $(this).offset().left;

    /* Set volume */
    melo_player_set_volume(melo_player_current.id, width / 100);

    /* Update volume */
    $("#player-volume-value").width(width + "%");
    $("#player-volume-handle").offset({left: pos });
}

function melo_player_volume_mousedown(evenData) {
    $(this).mousemove(melo_player_volume_click);
}

function melo_player_volume_mouseup(eventData) {
    $(this).off("mousemove");
}

function melo_player_mute_click(eventData) {
    melo_player_set_mute(melo_player_current.id, !melo_player_current.mute);
}

function melo_player_playlist_click(eventData) {
    /* TODO */
    melo_get_playlist_list(melo_player_current.playlist_id, null);
}

/*
 * Init
 */
function melo_player_init() {
    /* Add event handler for cover tabs */
    $("#player-cover").click(melo_player_toggle_cover_tabs);

    /* Add event handlers on position */
    $("#player-position-overlay")
        .click(melo_player_position_click)
        .hover(melo_player_show_handle, melo_player_hide_handle)
        .mousemove(melo_player_position_move);

    /* Add event handlers on position (for touch screen) */
    /* FIXME */
    $("#player-position-overlay")
        .bind("touchstart", function (eventData) {
            var evData = eventData.originalEvent.touches[0] ||
                eventData.originalEvent.changedTouches[0];
            melo_player_show_handle();
            melo_player_position_move(evData);
        })
        .bind("touchmove", function (eventData) {
            var evData = eventData.originalEvent.touches[0] ||
                eventData.originalEvent.changedTouches[0];
            eventData.preventDefault();
            melo_player_position_move(evData);
        })
        .bind("touchend", function (eventData) {
            var evData = eventData.originalEvent.touches[0] ||
                eventData.originalEvent.changedTouches[0];
            melo_player_hide_handle();
            melo_player_position_click(evData);
        });

    /* Add event handler for player control */
    $("#player-play").click(melo_player_play_click);
    $("#player-stop").click(melo_player_stop_click);
    $("#player-prev").click(melo_player_prev_click);
    $("#player-next").click(melo_player_next_click);
    $("#player-playlist").click(melo_player_playlist_click);

    /* Add event handler for volume control */
    $("#player-volume")
        .click(melo_player_volume_click)
        .mousedown(melo_player_volume_mousedown)
        .mouseup(melo_player_volume_mouseup)
        .mouseleave(melo_player_volume_mouseup);
    $("#player-mute").click(melo_player_mute_click);

    /* Add event handler for volume control (for touch screen) */
    /* FIXME */
/*    $("#player-volume")
        .bind("touchstart", function (eventData) {
            evData.preventDefault();
            $(this).bind("touchmove", function (eventData) {
                var evData = eventData.originalEvent.touches[0] ||
                    eventData.originalEvent.changedTouches[0];
                eventData.preventDefault();
                melo_player_volume_click(evData);
            });
        })
        .bind("touchend", function (eventData) {
            eventData.preventDefault();
            $(this).off("touchmove");
        });
*/
}
