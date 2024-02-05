import logging

import melopy as melo

_logger = logging.getLogger(__name__)

def entry_point(plugin: melo.Plugin) -> str:
    # Add player
    if not plugin.add_player("melo.webplayer.player", None):
        _logger.error("failed to register player")
        return False

    return True
