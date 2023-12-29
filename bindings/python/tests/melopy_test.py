# Entry point for Python bindings for Melo

import sys

import melopy
import proto.browser_pb2 as melopy_proto

class MyBrowser(melopy.Browser):
    def __init__(self) -> None:
        melopy.Browser.__init__(self)
        self._info = melopy.Browser.Info()

    def get_info(self) -> melopy.Browser.Info:
        return self._info

    def handle_request(self, req: melopy.Request) -> bool:
        req.complete(req.get_message())
        return True

def main() -> int:
    # Print version
    print(melopy.get_version())

    # Print MyBrowser name
    browser = MyBrowser()
    print(browser.get_name())

    # Add MyBrowser
    browser_id = "my.browser"
    melopy.Browser.add(browser_id, browser)

    # Get MyBrowser by ID
    ref = melopy.Browser.get_by_id(browser_id)
    if ref is not None:
        print(ref.get_name())
    if melopy.Browser.has(browser_id):
        print(f"has {browser_id}")

    # Create dummy message
    preq = melopy_proto.Request()
    preq.dummy = "plop"

    # Handle request
    def func(msg: bytes) -> None:
        preq = melopy_proto.Request()
        preq.ParseFromString(msg)
        print("complete:", preq.dummy)
    req = melopy.Request.create(preq.SerializeToString(), func)
    ref.handle_request(req)

    # Get invalid browser by ID
    invalid_id = "invalid.browser"
    ref = melopy.Browser.get_by_id(invalid_id)
    if ref is None:
        print(f"cannot get {invalid_id}")
    if not melopy.Browser.has(invalid_id):
        print(f"doesn't have {invalid_id}")

    # Remove MyBrowser
    melopy.Browser.remove(browser_id)

    return 0

if __name__ == "__main__":
    sys.exit(main())
