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

#include <melo/utils.h>

TEST(UtilsTest, ValidId) {
  // Valid IDs
  const std::string valid_chars = "abcdefghijklmnopqrstuvwxyz0123456789.-_";
  for (auto id : valid_chars)
    EXPECT_TRUE(melo::is_valid_id(std::string(1, id)));
  EXPECT_TRUE(melo::is_valid_id("a.valid.id"));
  EXPECT_TRUE(melo::is_valid_id("a.super_valid.id-2"));

  // Invalid IDs
  const std::string invalid_chars =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ`~!@#$%^&*()+=[]{}\\|;:'\",/<>? ";
  for (auto id : invalid_chars)
    EXPECT_FALSE(melo::is_valid_id(std::string(1, id)));
  EXPECT_FALSE(melo::is_valid_id("a very bad ID"));
}
