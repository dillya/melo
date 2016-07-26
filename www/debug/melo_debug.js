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
    $("#player_list").html("");
    for (var i = 0; i < response.result.length; i++) {
      /* Generate module item */
      var mod = $('<li>' + response.result[i].name + ' [' +
                           response.result[i].id + '] (' +
                           '<a class="info" href="#">info</a>' +
                           ' | <a class="browsers" href="#">browsers</a>' +
                           ' | <a class="config" href="#">config</a>' +
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

      /* Add a link to get module config */
      mod.children("a.config").click(response.result[i].config_id, function(e) {
        melo_get_config(e.data);
        return false;
      });

      /* Add item to list */
      $("#module_list").append(mod);

      /* Add players of module */
      melo_add_players(response.result[i].id, response.result[i].name);
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
        melo_get_browser_list(e.data, "/", 0, 0);
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
var melo_browser_current_off = 0;
var melo_browser_current_count = 0;

function melo_get_browser_list(id, path, off, count) {
  /* Set navigation */
  if (count == 0)
    count = 100;
  melo_browser_current_off = off;
  melo_browser_current_count = count;

  /* Update navigation */
  $('#bnav').html('Offset: ' + off + ', Count: ' + count + ' | ');
  if (off > 0)
    $('#bnav').append('<a class="prev" href="">&lt;</a> ');
  $('#bnav').append('<a class="next" href="">&gt;</a>');

  /* Add links for navigation */
  if (off > 0) {
    var prev_off = off - count;
    if (prev_off < 0)
      prev_off = 0;
    $('#bnav').children("a.prev").click([id, path, prev_off, count], function(e) {
      melo_get_browser_list(e.data[0], e.data[1], e.data[2], e.data[3]);
      return false;
    });
  }
  $('#bnav').children("a.next").click([id, path, off+count, count], function(e) {
    melo_get_browser_list(e.data[0], e.data[1], e.data[2], e.data[3]);
    return false;
  });

  /* Do request */
  jsonrpc_call("browser.get_list", JSON.parse('["' + id + '","' + path + '",' +
                                                off + ',' + count + ']'),
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
      var fpath = path + response.result[i].name;

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
          melo_get_browser_list(e.data[0], e.data[1], 0, 0);
          return false;
        });
      } else {
        /* Do action on file / item */
        item.children("a").click([id, fpath], function(e) {
          melo_browser_action("play", e.data[0], e.data[1], false);
          return false;
        });
      }

      /* Add a link if item is addable */
      if (response.result[i].add != null) {
        item.append(' [<a class="add" href="#">' + response.result[i].add + '</a>]');
        item.children('a.add').click([id, fpath], function(e) {
          melo_browser_action("add", e.data[0], e.data[1], false);
          return false;
        });
      }

      /* Add a link if item is removable */
      if (response.result[i].remove != null) {
        item.append(' [<a class="rm" href="#">' + response.result[i].remove + '</a>]');
        item.children('a.rm').click([id, npath], function(e) {
          melo_browser_action("remove", e.data[0], e.data[1], true);
          return false;
        });
      }

      /* Add item */
      $('#browser_list').append(item);
    }
  });
}

function melo_browser_action(action, id, path, update) {
  jsonrpc_call("browser." + action, JSON.parse('["' + id + '","' + path + '"]'),
               function(response) {
    if (response.error || !response.result)
      return;

    /* Update list when remove is done */
    if (update)
      melo_get_browser_list(melo_browser_current_id, melo_browser_current_path,
                            melo_browser_current_off, melo_browser_current_count);
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
  melo_get_browser_list(melo_browser_current_id, path, 0, 0);
}

var players = [];
var players_timer = null;

function melo_add_players(id, name) {
  /* Remove all players and there timers */
  if (players_timer != null)
      clearInterval(players_timer);
  players = [];

  jsonrpc_call("module.get_player_list", JSON.parse('["' + id + '",["full"]]'),
               function(response) {
    if (response.error || !response.result)
      return;

    /* Add players */
    $("#player_list").append('<p><span class="title">' + name + ':</span></p>');
    for (var i = 0; i < response.result.length; i++) {
      var id = response.result[i].id;
      var playlist_id = response.result[i].playlist;

      /* Add player to list */
      var player = $('<p> + <span class="title">' + id + '</span>' +
                            ' [<a href="#">refresh</a>]</p>');
      var stat = $('<p>' +
                     '<div>' +
                        '<img class="player_cover" src="" alt="cover">' +
                        '<div class="player_pos">' +
                           '<div class="player_cursor"></div>' +
                        '</div>' +
                        '<a class="playlist" href="#">Playlist &gt;</a><br>' +
                        '<a class="prev" href="#">&lt; Previous</a> | ' +
                        '<a class="play_pause" href="#">Play</a> | ' +
                        '<a class="stop" href="#">Stop</a> | ' +
                        '<a class="next" href="#">Next &gt;</a><br>' +
                        'State: <span class="state"></span><br>' +
                        'Name: <span class="name"></span><br>' +
                        'Pos: <span class="pos"></span><br>' +
                        '<span class="under">Tags:</span><br>' +
                        'Title: <span class="ttitle"></span><br>' +
                        'Artist: <span class="artist"></span><br>' +
                        'Alnum: <span class="album"></span><br>' +
                        'Genre: <span class="genre"></span><br>' +
                        'Date: <span class="date"></span><br>' +
                        'Track: <span class="track"></span>' +
                     '</div>' +
                   '</p>');

      /* Add player to list */
      var play = {};
      play.id = id;
      play.element = stat;
      play.tags_ts = 0;
      players.push(play);

      /* Add a link for refresh */
      player.children("a").click(play, function(e) {
        melo_update_player(e.data);
        return false;
      });

      /* Add a link to display playlist */
      if (playlist_id != null) {
        stat.find("a.playlist").click([playlist_id, play], function(e) {
          melo_get_playlist_list(e.data[0], e.data[1]);

          /* Update poll */
          playlist_poll_id = playlist_id;
          playlist_poll_player = play;

          return false;
        });
      }

      /* Add a link for previous and next */
      stat.find("a.prev").click([id, play], function(e) {
        melo_player_action("prev", e.data[0], e.data[1]);
        return false;
      });
      stat.find("a.next").click([id, play], function(e) {
        melo_player_action("next", e.data[0], e.data[1]);
        return false;
      });

      /* Get status */
      melo_update_player(play);

      /* Add player */
      $("#player_list").append(player);
      $("#player_list").append(stat);
    }
  });
}

function melo_update_player(play) {
  var id = play.id;
  var player = play.element;
  var tags_ts = play.tags_ts;

  /* Get status */
  jsonrpc_call("player.get_status",
               JSON.parse('["' + id + '",["full"],["full"],' + tags_ts + ']'),
               function(response) {
    if (response.error || !response.result)
      return;
    var s = response.result;
    var l = (s.state == "playing") ? "Pause" : "Play";
    var ns = (s.state == "playing") ? "paused" : "playing";

    /* Update player status */
    player.children('.play_pause').text(l);
    player.children('.state').text(s.state);
    player.children('.name').text(s.name);
    player.children('.pos').text(s.pos + ' / ' + s.duration);

    /* Set width of player cursor */
    player.find("div.player_cursor").width((s.pos * 100 / s.duration) + "%");

    /* Add action for seek */
    player.children("div.player_pos").unbind().click([id, s.duration], function(e) {
      var pos = e.data[1] * (e.pageX - $(this).offset().left) / $(this).width();
      pos = parseInt(pos, 10);
      melo_player_seek(e.data[0], pos);
      $(this).children("div.player_cursor").width((pos * 100 / e.data[1]) + "%");
      return false;
    });

    /* Add link to play / pause */
    player.find("a.play_pause").unbind().click([id, ns, play], function(e) {
      melo_set_player_state(e.data[0], e.data[1], e.data[2]);
      return false;
    });

    /* Add link to stop */
    player.find("a.stop").unbind().click([id, "stopped", play], function(e) {
      melo_set_player_state(e.data[0], e.data[1], e.data[2]);
      return false;
    });

    /* Update tags if tags are available and timestamp has changed */
    if (s.tags != null && s.tags.timestamp > tags_ts) {
      /* Update tags timestamp */
      play.tags_ts;
      play.tags_ts = s.tags.timestamp;

      /* Update tags */
      player.children('.ttitle').text(s.tags.title);
      player.children('.artist').text(s.tags.artist);
      player.children('.album').text(s.tags.album);
      player.children('.genre').text(s.tags.genre);
      player.children('.date').text(s.tags.date);
      player.children('.track').text(s.tags.track + ' / ' + s.tags.tracks);

      /* Update image src if available */
      var img_src = "";
      if (s.tags.cover != null)
        img_src = "data:" + s.tags.cover_type + ";base64," + s.tags.cover;
      player.children("img.player_cover").attr('src', img_src);
    }
  });
}

function melo_player_poll(state) {
  /* Enable / Disable timer for player polling */
  if (state) {
    /* Enable */
    if (players_timer == null) {
      players_timer = setInterval(function() {
        for (var i = 0; i < players.length; i++)
          melo_update_player(players[i]);
       }, 1000);
    }
  } else {
    /* Disable */
    if (players_timer != null) {
      clearInterval(players_timer);
      players_timer = null;
    }
  }
}

function melo_set_player_state(id, state, play) {
  jsonrpc_call("player.set_state", JSON.parse('["' + id + '","' + state + '"]'),
               function(response) {
    if (response.error || !response.result)
      return;

    /* Update player status */
    melo_update_player(play);
  });
}

function melo_player_action(action, id, play) {
  jsonrpc_call("player." + action, JSON.parse('["' + id + '"]'),
               function(response) {
    if (response.error || !response.result)
      return;

    /* Update player status */
    melo_update_player(play);
  });
}

function melo_player_seek(id, pos) {
  jsonrpc_call("player.set_pos", JSON.parse('["' + id + '",' + pos + ']'),
               function(response) {
    if (response.error || !response.result)
      return;
  });
}

function melo_get_playlist_list(id, play) {
  jsonrpc_call("playlist.get_list", JSON.parse('["' + id + '"]'),
               function(response) {
    if (response.error || !response.result)
      return;

    var current = response.result.current;
    var list = response.result.list;

    /* Generate list */
    $('#playlist_list').html("");
    for (var i = 0; i < list.length; i++) {
      var l = list[i];
      var name = l.name;

      /* Use full name when available */
      if (l.full_name != null)
        name = l.full_name + ' (' + l.name + ')';

      /* Generate list item */
      var item = $('<li><a class="play" href="#">' + name + '</a></li>');

      /* Set as current */
      if (l.name == current)
        item.addClass("current");

      /* Add link to play */
      item.children("a.play").click([id, l.name, play], function(e) {
        melo_playlist_play(e.data[0], e.data[1], e.data[2]);
        return false;
      });

      /* Add link to remove */
      if (l.can_remove) {
        /* Add link to item */
        item.append(' [<a class="remove" href="#">remove</a>]');
        item.children("a.remove").click([id, l.name, play], function(e) {
          melo_playlist_remove(e.data[0], e.data[1], e.data[2]);
          return false;
        });
      }

      /* Add item */
      $('#playlist_list').append(item);
    }
  });
}

function melo_playlist_play(id, name, play) {
  jsonrpc_call("playlist.play", JSON.parse('["' + id + '","' + name + '"]'),
               function(response) {
    if (response.error || !response.result)
      return;

    /* Update playlist and player */
    melo_update_player (play);
    melo_get_playlist_list (id, play);
  });
}

function melo_playlist_remove(id, name, play) {
  jsonrpc_call("playlist.remove", JSON.parse('["' + id + '","' + name + '"]'),
               function(response) {
    if (response.error || !response.result)
      return;

    /* Update playlist and player */
    melo_update_player (play);
    melo_get_playlist_list (id, play);
  });
}

var playlist_poll_id = "";
var playlist_poll_player = null;
var playlist_poll_timer = null;

function melo_playlist_poll(state) {
  /* Enable / Disable timer for playlist polling */
  if (state) {
    /* Enable */
    if (playlist_poll_id != "" && playlist_poll_player != null) {
      playerlist_poll_timer = setInterval(function() {
        melo_get_playlist_list(playlist_poll_id, playlist_poll_player);
      }, 1000);
    }
  } else {
    /* Disable */
    if (playerlist_poll_timer != null) {
      clearInterval(playerlist_poll_timer);
      playerlist_poll_timer= null;
    }
  }
}

function melo_get_config(id) {
  jsonrpc_call("config.get", JSON.parse('["' + id + '"]'),
               function(response) {
    if (response.error || !response.result)
      return;
    var groups = response.result

    /* Display configuration */
    $("#config").html("");
    for (var i = 0; i < groups.length; i++) {
      var items = groups[i].items;

      /* Create a new form */
      var group = $('<form></form>');

      /* Display group name */
      group.append('<p>' +
                     '<span class="title">' + groups[i].name + ' (' +
                                              groups[i].id + ')' +
                     '</span>' +
                   '</p><p>');

      /* Display all items */
      for (var j = 0; j < items.length; j++) {
        var item_id = items[j].id;
        var name = items[j].name;
        var value = items[j].val;
        var ro = items[j].read_only;
        var element = items[j].element;

        /* Add new form element */
        if (item_id == null)
          group.append('<span class="under">' + name + ':</span><br>');
        else if (element == "checkbox")
          group.append('<input type="checkbox" id="' + item_id + '"' +
                              (value ? " checked": "") + (ro ? " readonly" : "") + '>' +
                              '<label for="' + item_id + '">' + name + '</label><br>');
        else
          group.append(name + ': <input type="' + element + '" id="' + item_id + '"' +
                                 ' name="' + item_id + '" value="' + (value ? value : "") + '"' +
                                 (ro ? " readonly" : "") + '><br>');
      }

      /* Add a save button */
      group.append('<input type="submit" value="Save"><br>');
      group.append('</p>');

      /* Add action for save button */
      group.submit([id, groups[i].id, items, group], function(e) {
        melo_set_config (e.data[0], e.data[1], e.data[2], e.data[3]);
        e.preventDefault();
      })

      /* Close form */
      $("#config").append(group);
    }
  });
}

function melo_set_config(id, group_id, items, form) {
  var params = [];
  var group_list = [];
  var group = {};
  var list = [];

  /* Parse form */
  for (var i = 0; i < items.length; i++) {
    if (items[i].id == null || items[i].read_only)
      continue;

    /* Get children value */
    var obj = {};
    obj.id = items[i].id;
    if (items[i].element == "checkbox")
      obj.val = form.children('#' + items[i].id).is(':checked');
    else if (items[i].element == "number")
      obj.val = parseInt(form.children('#' + items[i].id).val());
    else
      obj.val = form.children('#' + items[i].id).val();

    /* Add item to list */
    list.push(obj);
  }

  /* Generate group */
  group.id = group_id;
  group.list = list;
  group_list.push(group);

  /* Generate params */
  params.push(id);
  params.push(group_list);

  /* Send new configuration */
  jsonrpc_call("config.set", params, function(response) {
    if (response.error || !response.result)
      return;

    /* Update */
    melo_get_config(id);
  });
}

$(document).ready(function() {
  /* Load module list */
  melo_update_list();

  /* Add click events */
  $("#main_config").click(function() {melo_get_config("main"); return false;});
  $("#module_refresh").click(function() {melo_update_list(); return false;});
  $("#browser_prev").click(function() {melo_browser_previous(); return false;});
  $("#player_poll").change(function() {melo_player_poll(this.checked); return false;});
  $("#playlist_poll").change(function() {melo_playlist_poll(this.checked); return false;});
  $("#playlist_refresh").click(function() {melo_get_playlist_list(playlist_poll_id, playlist_poll_player);return false;});
});
