import melopy

class YoutubeBrowser(melopy.Browser):
    def __init__(self) -> None:
        melopy.Browser.__init__(self)
        self._info = melopy.Browser.Info()

    def get_info(self) -> melopy.Browser.Info:
        return self._info

    def handle_request(self) -> bool:
        # Play media
        melopy.Playlist.play()

        return True
