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

#include <melo/browser.h>

class MyTestBrowser final : public melo::Browser {
 public:
  MyTestBrowser(const std::string &name) : info_{name, ""} {}

  inline const Info &get_info() const override { return info_; }

  inline bool handle_request(
      const std::shared_ptr<melo::Request> &req) const override {
    // Return request message
    req->complete(req->get_message());
    return true;
  }

 private:
  const Info info_;
};

TEST(BrowserTest, Info) {
  const std::string name = "browser_info";
  MyTestBrowser browser(name);

  // Test info
  auto &ref = browser.get_info();
  EXPECT_EQ(ref.name, name);
  EXPECT_EQ(ref.description, "");

  // Test helpers
  EXPECT_EQ(browser.get_name(), name);
  EXPECT_EQ(browser.get_description(), "");
}

TEST(BrowserTest, AddRemove) {
  const std::string name = "add_remove_browser";
  auto browser = std::make_shared<MyTestBrowser>(name);

  // Not yet registered
  const std::string id = "add.remove.browser";
  EXPECT_FALSE(melo::Browser::has(id));

  // Adding browser with invalid ID
  const std::string invalid_id = "(add remove#browser";
  EXPECT_FALSE(melo::Browser::add(invalid_id, browser));

  // Adding browser
  EXPECT_TRUE(melo::Browser::add(id, browser));

  // Browser is available
  auto ref = melo::Browser::get_by_id(id);
  EXPECT_NE(ref, nullptr);
  EXPECT_EQ(ref->get_name(), name);

  // Cannot add twice
  EXPECT_FALSE(melo::Browser::add(id, browser));

  // Remove browser
  EXPECT_TRUE(melo::Browser::remove(id));
  EXPECT_FALSE(melo::Browser::remove(id));

  // Not in list anymore
  ref = melo::Browser::get_by_id(id);
  EXPECT_EQ(ref, nullptr);
}

TEST(BrowserTest, HandleRequest) {
  const std::string name = "handle_request_browser";
  auto browser = std::make_shared<MyTestBrowser>(name);

  // Add browser
  const std::string id = "handle.request.browser";
  melo::Browser::add(id, browser);

  // Prepare message and callback
  std::string msg = "test message";
  bool done = false;
  auto func = [msg, &done](const std::string &resp) {
    // Should get same message
    EXPECT_EQ(msg, resp);
    done = true;
  };

  // Create new request with move
  auto req = melo::Request::create(std::move(msg), func);
  EXPECT_TRUE(msg.empty());

  // Handle request
  browser->handle_request(req);
  EXPECT_TRUE(done);
}
