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

#include <melo/request.h>

TEST(RequestTest, Create) {
  // Prepare message and callback
  const std::string msg = "test message";
  bool done = false;
  auto func = [&msg, &done](const std::string &resp) {
    // Should get same message
    EXPECT_EQ(msg, resp);
    done = true;
  };

  // Create new request
  auto req = melo::Request::create(msg, func);

  // Test complete
  EXPECT_FALSE(done);
  EXPECT_FALSE(req->is_completed());
  EXPECT_TRUE(req->complete(req->get_message()));
  EXPECT_TRUE(req->is_completed());
  EXPECT_TRUE(done);
  EXPECT_FALSE(req->complete(req->get_message()));
}

TEST(RequestTest, CreateMove) {
  // Prepare message and callback
  std::string msg = "test message with move";
  bool done = false;
  auto func = [msg, &done](const std::string &resp) {
    // Should get same message
    EXPECT_EQ(msg, resp);
    done = true;
  };

  // Create new request with move
  auto req = melo::Request::create(std::move(msg), func);
  EXPECT_TRUE(msg.empty());

  // Test complete
  EXPECT_FALSE(done);
  EXPECT_FALSE(req->is_completed());
  EXPECT_TRUE(req->complete(req->get_message()));
  EXPECT_TRUE(req->is_completed());
  EXPECT_TRUE(done);
  EXPECT_FALSE(req->complete(req->get_message()));
}

TEST(RequestTest, CreateEmpty) {
  // Create new request
  auto req = melo::Request::create({}, nullptr);

  // Test complete
  EXPECT_FALSE(req->is_completed());
  EXPECT_TRUE(req->complete({}));
  EXPECT_TRUE(req->is_completed());
  EXPECT_FALSE(req->complete({}));
}

TEST(RequestTest, Cancel) {
  // Create new request
  auto req = melo::Request::create({}, nullptr);

  // Test cancel
  EXPECT_FALSE(req->is_completed());
  EXPECT_FALSE(req->is_canceled());
  EXPECT_TRUE(req->cancel());
  EXPECT_TRUE(req->is_canceled());
  EXPECT_TRUE(req->is_completed());
  EXPECT_FALSE(req->cancel());
  EXPECT_FALSE(req->complete({}));

  // Recreate new request
  req = melo::Request::create({}, nullptr);

  // Test cancel after complete
  EXPECT_TRUE(req->complete({}));
  EXPECT_FALSE(req->cancel());
  EXPECT_FALSE(req->is_canceled());
  EXPECT_TRUE(req->is_completed());
}
