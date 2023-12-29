import melopy

from youtube_browser import YoutubeBrowser

def main() -> None:
    # Add new Youtube browser
    melopy.Browser.add("youtube.browser", YoutubeBrowser())
