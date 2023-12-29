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

#include "melo/request.h"

#include "melo/log.h"

namespace melo {

bool Request::complete(const std::string &msg) {
  // Already completed
  if (completed_) {
    MELO_LOGW("request {} has been already completed", (void *)this);
    return false;
  }

  // Call completed function
  if (func_)
    func_(msg);

  // Complete the request
  completed_ = true;

  return true;
}

bool Request::cancel() {
  // Already completed
  if (completed_) {
    MELO_LOGD("request {} has been already completed", (void *)this);
    return false;
  }

  // Already canceled
  if (canceled_) {
    MELO_LOGD("request {} has been already canceled", (void *)this);
    return false;
  }

  // Cancel and complete the request
  completed_ = true;
  canceled_ = true;

  return true;
}

}  // namespace melo
