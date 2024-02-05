
#include <melo/log.h>
#include <melo/plugin.h>

class FilePlayer final : public melo::Player {
 public:
  const Info &get_info() const override { return info_; }
  bool play(const std::shared_ptr<melo::Playlist> &playlist) override {
    return false;
  }
  bool reset() override { return false; }

 private:
  const Info info_ = {
      .name = "File player",
      .description = "Can play any file",
  };
};

class FileBrowser final : public melo::Browser {
 public:
  const Info &get_info() const override { return info_; }
  bool handle_request(
      const std::shared_ptr<melo::Request> &request) const override {
    return false;
  }

 private:
  const Info info_ = {
      .name = "File browser",
      .description = "Can browse local / remote file system",
  };
};

extern "C" {
MELO_EXPORT
bool entry_point(const melo::Plugin &plugin);
};

bool entry_point(const melo::Plugin &plugin) {
  // Create player and browser
  auto player = std::make_shared<FilePlayer>();
  auto browser = std::make_shared<FileBrowser>();

  // Add player
  if (!plugin.add_player("melo.file.player", player)) {
    MELO_LOGE("failed to register file player");
    return false;
  }

  // Add browser
  if (!plugin.add_browser("melo.file.browser", browser)) {
    MELO_LOGE("failed to register file browser");
    return false;
  }

  return true;
}
