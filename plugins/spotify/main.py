import logging

import melopy as melo

_logger = logging.getLogger(__name__)

class SpotifyPlayer(melo.Player):
    def __init__(self) -> None:
        melo.Player.__init__(self)
        self._info = melo.Player.Info(
          "Spotify player",
          "Player embedding all Spotify features",
        )

    def get_info(self) -> melo.Player.Info:
        return self._info

# Create new player
player = SpotifyPlayer()

def entry_point(plugin: melo.Plugin) -> str:

    # Add player
    if not plugin.add_player("melo.spotify.player", player):
        _logger.error("failed to register player")
        return False

    return True
