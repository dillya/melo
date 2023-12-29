// Copyright (C) 2023 Alexandre Dilly <dillya@sparod.com>
//
// This program is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any
// later version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#include <gtest/gtest.h>

#include <melo/player.h>
#include <melo/playlist.h>

class TestPlayer final : public melo::Player {
 public:
  inline const melo::Player::Info &get_info() const override { return info_; }

  inline bool play(const std::shared_ptr<melo::Playlist> &playlist) override {
    playlist_ = playlist;
    update_media(playlist->get_current());
    return true;
  }
  inline bool reset() override {
    playlist_ = nullptr;
    return true;
  }

  inline const std::shared_ptr<melo::Playlist> &get_playlist() const {
    return playlist_;
  }

 private:
  melo::Player::Info info_ = {};
  std::shared_ptr<melo::Playlist> playlist_;
};

TEST(PlaylistTest, AddRemove) {
  // Register dummy player
  auto player = std::make_shared<TestPlayer>();
  EXPECT_TRUE(melo::Player::add("test.player", player));
  EXPECT_TRUE(melo::Player::add("test.another.player", player));

  // Add single media
  melo::Media media{"test.player", "protocol://an_uri"};
  EXPECT_TRUE(melo::Playlist::add(media));

  // Check list content
  EXPECT_EQ(melo::Playlist::get_playlist_count(), 1);
  auto playlist = melo::Playlist::get_playlist(0);
  EXPECT_NE(playlist, nullptr);
  EXPECT_EQ(playlist->get_uri(), "protocol://an_uri");
  EXPECT_EQ(melo::Playlist::get_playlist(1), nullptr);

  // Add list of media
  melo::Media parent{"test.player", "protocol://a_list_uri"};
  std::vector<melo::Media> list{
      {"test.player", "protocol://another_uri"},
      {"test.player", "protocol://a_second_uri"},
      {"test.player", "protocol://a_third_uri"},
  };
  EXPECT_TRUE(melo::Playlist::add(parent, list));

  // Check list content
  EXPECT_EQ(melo::Playlist::get_playlist_count(), 2);
  playlist = melo::Playlist::get_playlist(0);
  EXPECT_NE(playlist, nullptr);
  EXPECT_EQ(playlist->get_uri(), "protocol://an_uri");
  playlist = melo::Playlist::get_playlist(1);
  EXPECT_NE(playlist, nullptr);
  EXPECT_EQ(playlist->get_uri(), "protocol://a_list_uri");
  EXPECT_EQ(playlist->get_count(), 3);
  EXPECT_EQ(playlist->get_uri(0), "protocol://another_uri");
  EXPECT_EQ(playlist->get_uri(1), "protocol://a_second_uri");
  EXPECT_EQ(playlist->get_uri(2), "protocol://a_third_uri");
  EXPECT_EQ(melo::Playlist::get_playlist(2), nullptr);

  // Add empty list of media
  std::vector<melo::Media> empty_list;
  EXPECT_TRUE(melo::Playlist::add(parent, empty_list));

  // Check list content
  EXPECT_EQ(melo::Playlist::get_playlist_count(), 3);
  playlist = melo::Playlist::get_playlist(0);
  EXPECT_NE(playlist, nullptr);
  EXPECT_EQ(playlist->get_uri(), "protocol://an_uri");
  playlist = melo::Playlist::get_playlist(1);
  EXPECT_NE(playlist, nullptr);
  EXPECT_EQ(playlist->get_uri(), "protocol://a_list_uri");
  EXPECT_EQ(playlist->get_count(), 3);
  playlist = melo::Playlist::get_playlist(2);
  EXPECT_NE(playlist, nullptr);
  EXPECT_EQ(playlist->get_uri(), "protocol://a_list_uri");
  EXPECT_EQ(playlist->get_count(), 0);
  EXPECT_EQ(melo::Playlist::get_playlist(3), nullptr);

  // Add invalid media (no player)
  melo::Media invalid_media{"test.invalid.player", "protocol://an_uri"};
  EXPECT_FALSE(melo::Playlist::add(invalid_media));
  EXPECT_EQ(melo::Playlist::get_playlist_count(), 3);

  // Add invalid list of media
  melo::Media invalid_parent{"test.invalid.player", "protocol://an_uri"};
  std::vector<melo::Media> invalid_list{
      {"test.invalid.player", "protocol://another_uri"},
      {"test.invalid.player", "protocol://a_second_uri"},
  };
  EXPECT_FALSE(melo::Playlist::add(invalid_parent, invalid_list));
  EXPECT_EQ(melo::Playlist::get_playlist_count(), 3);

  // Add mixed list of media
  std::vector<melo::Media> invalid_mixed_list{
      {"test.player", "protocol://another_uri"},
      {"test.another.player", "protocol://a_second_uri"},
  };
  EXPECT_FALSE(melo::Playlist::add(parent, invalid_mixed_list));
  EXPECT_EQ(melo::Playlist::get_playlist_count(), 3);

  // Check media remove
  EXPECT_TRUE(melo::Playlist::remove(0));
  EXPECT_EQ(melo::Playlist::get_playlist_count(), 2);
  playlist = melo::Playlist::get_playlist(0);
  EXPECT_NE(playlist, nullptr);
  EXPECT_EQ(playlist->get_uri(), "protocol://a_list_uri");
  EXPECT_EQ(playlist->get_count(), 3);
  playlist = melo::Playlist::get_playlist(1);
  EXPECT_NE(playlist, nullptr);
  EXPECT_EQ(playlist->get_uri(), "protocol://a_list_uri");
  EXPECT_EQ(playlist->get_count(), 0);
  EXPECT_EQ(melo::Playlist::get_playlist(2), nullptr);

  // Check media of list remove
  EXPECT_TRUE(melo::Playlist::remove(0, 1));
  EXPECT_EQ(melo::Playlist::get_playlist_count(), 2);
  playlist = melo::Playlist::get_playlist(0);
  EXPECT_NE(playlist, nullptr);
  EXPECT_EQ(playlist->get_uri(), "protocol://a_list_uri");
  EXPECT_EQ(playlist->get_count(), 2);
  EXPECT_EQ(playlist->get_uri(0), "protocol://another_uri");
  EXPECT_EQ(playlist->get_uri(1), "protocol://a_third_uri");

  // Clear playlist
  melo::Playlist::clear();

  // Remove players
  EXPECT_TRUE(melo::Player::remove("test.player"));
  EXPECT_TRUE(melo::Player::remove("test.another.player"));
}

TEST(PlaylistTest, PlayAdd) {
  // Register dummy player
  auto player = std::make_shared<TestPlayer>();
  EXPECT_TRUE(melo::Player::add("test.player", player));

  // Play a single media
  melo::Media media{"test.player", "protocol://an_uri"};
  EXPECT_TRUE(melo::Playlist::play(media));

  // Check playing URI
  EXPECT_EQ(player->get_media().get_uri(), "protocol://an_uri");

  // Add list of media
  melo::Media parent{"test.player", "protocol://an_uri"};
  std::vector<melo::Media> list{
      {"test.player", "protocol://another_uri"},
      {"test.player", "protocol://a_second_uri"},
      {"test.player", "protocol://a_third_uri"},
  };
  EXPECT_TRUE(melo::Playlist::add(parent, list));

  // Check playing URI
  EXPECT_EQ(player->get_media().get_uri(), "protocol://an_uri");

  // Clear playlist
  melo::Playlist::clear();

  // Remove players
  EXPECT_TRUE(melo::Player::remove("test.player"));
}

TEST(PlaylistTest, PrevNext) {
  // Register dummy player
  auto player_a = std::make_shared<TestPlayer>();
  auto player_b = std::make_shared<TestPlayer>();
  EXPECT_TRUE(melo::Player::add("test.player.a", player_a));
  EXPECT_TRUE(melo::Player::add("test.player.b", player_b));

  // Play a single media
  melo::Media media_a{"test.player.a", "a"};
  EXPECT_TRUE(melo::Playlist::play(media_a));

  // Add list of media
  melo::Media media_b{"test.player.b", "b"};
  std::vector<melo::Media> list_b{
      {"test.player.b", "b0"},
      {"test.player.b", "b1"},
      {"test.player.b", "b2"},
  };
  EXPECT_TRUE(melo::Playlist::add(media_b, list_b));

  // Add a single media
  melo::Media media_c{"test.player.a", "c"};
  EXPECT_TRUE(melo::Playlist::add(media_c));

  // Test next
  EXPECT_EQ(player_a->get_media().get_uri(), "a");
  EXPECT_NE(player_a->get_playlist(), nullptr);
  EXPECT_EQ(player_b->get_playlist(), nullptr);
  EXPECT_TRUE(melo::Playlist::next());  // Move to b.0
  EXPECT_EQ(player_b->get_media().get_uri(), "b0");
  EXPECT_EQ(player_a->get_playlist(), nullptr);
  EXPECT_NE(player_b->get_playlist(), nullptr);
  EXPECT_TRUE(melo::Playlist::next());  // Move to b.1
  EXPECT_EQ(player_b->get_media().get_uri(), "b1");
  EXPECT_EQ(player_a->get_playlist(), nullptr);
  EXPECT_NE(player_b->get_playlist(), nullptr);
  EXPECT_TRUE(melo::Playlist::next());  // Move to b.2
  EXPECT_EQ(player_b->get_media().get_uri(), "b2");
  EXPECT_EQ(player_a->get_playlist(), nullptr);
  EXPECT_NE(player_b->get_playlist(), nullptr);
  EXPECT_TRUE(melo::Playlist::next());  // Move to c
  EXPECT_EQ(player_a->get_media().get_uri(), "c");
  EXPECT_NE(player_a->get_playlist(), nullptr);
  EXPECT_EQ(player_b->get_playlist(), nullptr);
  EXPECT_FALSE(melo::Playlist::next());  // No more media

  // Test previous
  EXPECT_EQ(player_a->get_media().get_uri(), "c");
  EXPECT_NE(player_a->get_playlist(), nullptr);
  EXPECT_EQ(player_b->get_playlist(), nullptr);
  EXPECT_TRUE(melo::Playlist::previous());  // Move to b.2
  EXPECT_EQ(player_b->get_media().get_uri(), "b2");
  EXPECT_EQ(player_a->get_playlist(), nullptr);
  EXPECT_NE(player_b->get_playlist(), nullptr);
  EXPECT_TRUE(melo::Playlist::previous());  // Move to b.1
  EXPECT_EQ(player_b->get_media().get_uri(), "b1");
  EXPECT_EQ(player_a->get_playlist(), nullptr);
  EXPECT_NE(player_b->get_playlist(), nullptr);
  EXPECT_TRUE(melo::Playlist::previous());  // Move to b.0
  EXPECT_EQ(player_b->get_media().get_uri(), "b0");
  EXPECT_EQ(player_a->get_playlist(), nullptr);
  EXPECT_NE(player_b->get_playlist(), nullptr);
  EXPECT_TRUE(melo::Playlist::previous());  // Move to a
  EXPECT_EQ(player_a->get_media().get_uri(), "a");
  EXPECT_NE(player_a->get_playlist(), nullptr);
  EXPECT_EQ(player_b->get_playlist(), nullptr);
  EXPECT_FALSE(melo::Playlist::previous());  // No more media

  // Test parent next / previous
  EXPECT_EQ(player_a->get_media().get_uri(), "a");
  EXPECT_NE(player_a->get_playlist(), nullptr);
  EXPECT_EQ(player_b->get_playlist(), nullptr);
  EXPECT_TRUE(melo::Playlist::next(true));  // Move to b.0
  EXPECT_EQ(player_b->get_media().get_uri(), "b0");
  EXPECT_EQ(player_a->get_playlist(), nullptr);
  EXPECT_NE(player_b->get_playlist(), nullptr);
  EXPECT_TRUE(melo::Playlist::next(true));  // Move to c
  EXPECT_EQ(player_a->get_media().get_uri(), "c");
  EXPECT_NE(player_a->get_playlist(), nullptr);
  EXPECT_EQ(player_b->get_playlist(), nullptr);
  EXPECT_FALSE(melo::Playlist::next(true));  // No more media
  EXPECT_EQ(player_a->get_media().get_uri(), "c");
  EXPECT_NE(player_a->get_playlist(), nullptr);
  EXPECT_EQ(player_b->get_playlist(), nullptr);
  EXPECT_TRUE(melo::Playlist::previous(true));  // Move to b.2
  EXPECT_EQ(player_b->get_media().get_uri(), "b2");
  EXPECT_EQ(player_a->get_playlist(), nullptr);
  EXPECT_NE(player_b->get_playlist(), nullptr);
  EXPECT_TRUE(melo::Playlist::previous(true));  // Move to a
  EXPECT_EQ(player_a->get_media().get_uri(), "a");
  EXPECT_NE(player_a->get_playlist(), nullptr);
  EXPECT_EQ(player_b->get_playlist(), nullptr);
  EXPECT_FALSE(melo::Playlist::previous());  // No more media

  // Test seek
  EXPECT_TRUE(melo::Playlist::play(1, 1));  // Move to b.1
  EXPECT_EQ(player_b->get_media().get_uri(), "b1");
  EXPECT_EQ(player_a->get_playlist(), nullptr);
  EXPECT_NE(player_b->get_playlist(), nullptr);
  EXPECT_TRUE(melo::Playlist::previous());  // Move to b.0
  EXPECT_EQ(player_b->get_media().get_uri(), "b0");
  EXPECT_EQ(player_a->get_playlist(), nullptr);
  EXPECT_NE(player_b->get_playlist(), nullptr);
  EXPECT_TRUE(melo::Playlist::next(true));  // Move to c
  EXPECT_EQ(player_a->get_media().get_uri(), "c");
  EXPECT_NE(player_a->get_playlist(), nullptr);
  EXPECT_EQ(player_b->get_playlist(), nullptr);

  // Clear playlist
  melo::Playlist::clear();

  // Remove players
  EXPECT_TRUE(melo::Player::remove("test.player.a"));
  EXPECT_TRUE(melo::Player::remove("test.player.b"));
}

TEST(PlaylistTest, RemoveCurrent) {
  // Register dummy player
  auto player = std::make_shared<TestPlayer>();
  EXPECT_TRUE(melo::Player::add("test.player", player));

  // Add a single media
  melo::Media media_a{"test.player", "a"};
  EXPECT_TRUE(melo::Playlist::add(media_a));

  // Add list of media
  melo::Media media_b{"test.player", "b"};
  std::vector<melo::Media> list_b{
      {"test.player", "b0"},
      {"test.player", "b1"},
      {"test.player", "b2"},
  };
  EXPECT_TRUE(melo::Playlist::add(media_b, list_b));

  EXPECT_EQ(melo::Playlist::get_playlist_count(), 2);

  // Remove playing in nested
  EXPECT_TRUE(melo::Playlist::play(1, 1));
  auto playlist = melo::Playlist::get_current_playlist();
  EXPECT_NE(playlist, nullptr);
  EXPECT_EQ(playlist->get_current().get_uri(), "b1");
  EXPECT_TRUE(melo::Playlist::remove(1, 1));
  playlist = melo::Playlist::get_current_playlist();
  EXPECT_NE(playlist, nullptr);
  EXPECT_EQ(playlist->get_current().get_uri(), "b");

  // Remove another
  EXPECT_TRUE(melo::Playlist::play(1, 0));
  playlist = melo::Playlist::get_current_playlist();
  EXPECT_NE(playlist, nullptr);
  EXPECT_EQ(player->get_media().get_uri(), "b0");
  EXPECT_TRUE(melo::Playlist::remove(1, 0));
  playlist = melo::Playlist::get_current_playlist();
  EXPECT_NE(playlist, nullptr);
  EXPECT_EQ(playlist->get_current().get_uri(), "b");

  // Remove another
  EXPECT_TRUE(melo::Playlist::play(1));
  playlist = melo::Playlist::get_current_playlist();
  EXPECT_NE(playlist, nullptr);
  EXPECT_EQ(player->get_media().get_uri(), "b2");
  EXPECT_TRUE(melo::Playlist::remove(1, 0));
  playlist = melo::Playlist::get_current_playlist();
  EXPECT_NE(playlist, nullptr);
  EXPECT_EQ(playlist->get_current().get_uri(), "b");

  // Remove another
  EXPECT_TRUE(melo::Playlist::play(1));
  playlist = melo::Playlist::get_current_playlist();
  EXPECT_NE(playlist, nullptr);
  EXPECT_EQ(player->get_media().get_uri(), "b");
  EXPECT_FALSE(melo::Playlist::remove(1, 0));
  EXPECT_TRUE(melo::Playlist::remove(1));
  playlist = melo::Playlist::get_current_playlist();
  EXPECT_NE(playlist, nullptr);
  EXPECT_EQ(playlist->get_current().get_uri(), "a");
  EXPECT_EQ(melo::Playlist::get_playlist_count(), 1);

  // Remove last
  EXPECT_FALSE(melo::Playlist::play(1));
  EXPECT_TRUE(melo::Playlist::play(0));
  playlist = melo::Playlist::get_current_playlist();
  EXPECT_NE(playlist, nullptr);
  EXPECT_EQ(playlist->get_current().get_uri(), "a");
  EXPECT_FALSE(melo::Playlist::remove(1));
  EXPECT_TRUE(melo::Playlist::remove(0));
  playlist = melo::Playlist::get_current_playlist();
  EXPECT_EQ(playlist, nullptr);
  EXPECT_EQ(melo::Playlist::get_playlist_count(), 0);

  // Clear playlist
  melo::Playlist::clear();

  // Remove player
  EXPECT_TRUE(melo::Player::remove("test.player"));
}

TEST(PlaylistTest, Swap) {
  // Register dummy player
  auto player = std::make_shared<TestPlayer>();
  EXPECT_TRUE(melo::Player::add("test.player", player));

  // Add a single media
  melo::Media media_a{"test.player", "a"};
  EXPECT_TRUE(melo::Playlist::add(media_a));

  // Add list of media
  melo::Media media_b{"test.player", "b"};
  std::vector<melo::Media> list_b{
      {"test.player", "b0"},
      {"test.player", "b1"},
      {"test.player", "b2"},
  };
  EXPECT_TRUE(melo::Playlist::add(media_b, list_b));

  // Add a single media
  melo::Media media_c{"test.player", "c"};
  EXPECT_TRUE(melo::Playlist::add(media_c));

  // Swap sub-elements
  EXPECT_FALSE(melo::Playlist::swap(1, 2, 3));
  EXPECT_FALSE(melo::Playlist::swap(0, 0, 0));
  EXPECT_TRUE(melo::Playlist::swap(1, 1, 0));
  auto playlist = melo::Playlist::get_playlist(1);
  EXPECT_NE(playlist, nullptr);
  EXPECT_EQ(playlist->get_media(0).get_uri(), "b1");
  EXPECT_EQ(playlist->get_media(1).get_uri(), "b0");
  EXPECT_EQ(playlist->get_media(2).get_uri(), "b2");
  EXPECT_TRUE(melo::Playlist::swap(1, 0, 2));
  EXPECT_EQ(playlist->get_media(0).get_uri(), "b2");
  EXPECT_EQ(playlist->get_media(1).get_uri(), "b0");
  EXPECT_EQ(playlist->get_media(2).get_uri(), "b1");

  // Swap sub-elements
  EXPECT_FALSE(melo::Playlist::swap(1, 3));
  EXPECT_TRUE(melo::Playlist::swap(0, 1));
  playlist = melo::Playlist::get_playlist(0);
  EXPECT_NE(playlist, nullptr);
  EXPECT_EQ(playlist->get_current().get_uri(), "b");
  playlist = melo::Playlist::get_playlist(1);
  EXPECT_NE(playlist, nullptr);
  EXPECT_EQ(playlist->get_current().get_uri(), "a");
  EXPECT_TRUE(melo::Playlist::swap(2, 1));
  playlist = melo::Playlist::get_playlist(2);
  EXPECT_NE(playlist, nullptr);
  EXPECT_EQ(playlist->get_current().get_uri(), "a");

  // Clear playlist
  melo::Playlist::clear();

  // Remove player
  EXPECT_TRUE(melo::Player::remove("test.player"));
}
