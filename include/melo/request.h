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

/**
 * @file request.h
 * @brief Request class definition.
 */

#ifndef MELO_REQUEST_H_
#define MELO_REQUEST_H_

#include <functional>
#include <memory>
#include <string>

namespace melo {

/**
 * Request class to handle a request and its message.
 *
 * Melo is using an asynchronous request <-> response system to interact with
 * the custom implementations of Browser and Player.
 *
 * TODO: add more details
 */
class Request final : public std::enable_shared_from_this<Request> {
  // Makes Request not constructible from outside
  struct Private {};

 public:
  /**
   * Completion function.
   *
   * This function is called when the Request.complete() call is done, aka, when
   * a request is completed.
   *
   * @param[in] msg The message provided during Request.complete(): it should
   * hold the response
   */
  using Func = std::function<void(const std::string &msg)>;

  /** @private */
  Request(Private, const std::string &msg, const Func &func)
      : msg_(msg), func_(func) {}
  /** @private */
  Request(Private, std::string &&msg, const Func &func)
      : msg_(std::move(msg)), func_(func) {}

  /**
   * Create a new request.
   *
   * This function is used to create a new `Request` (direct constructor is not
   * available) with an optional message and completion function.
   *
   * The completion function hold by `func` is called during complete() call.
   *
   * @note The `std::string` type is used here to hold the data but a message
   * can be purely binary data.
   *
   * @param[in] msg The message to attach to the request
   * @param[in] func The function to call at completion
   * @return A new `Request` reference.
   */
  static inline std::shared_ptr<Request> create(const std::string &msg,
                                                const Func &func) {
    return std::make_shared<Request>(Private(), msg, func);
  }
  /**
   * Create a new request by stealing message.
   *
   * This function does the same as create() but it steals the message.
   *
   * @param[in] msg The message to attach to the request
   * @param[in] func The function to call at completion
   * @return A new `Request` reference.
   */
  static inline std::shared_ptr<Request> create(std::string &&msg,
                                                const Func &func) {
    return std::make_shared<Request>(Private(), std::move(msg), func);
  }

  /**
   * Get message of the request.
   *
   * This function can be used to get the message attached to the request.
   *
   * @note The `std::string` type is used here to hold the data but a message
   * can be purely binary data.
   *
   * @return a `std::string` containing the message attached to the request.
   */
  inline const std::string &get_message() const { return msg_; }

  /**
   * Complete a request.
   *
   * A call to this function will:
   *  - call the function provided in create() with the message hold by `msg`,
   *  - set the request as completed.
   * Any further call to complete() will be skipped.
   *
   * @note If the request has been canceled, this call will fail.
   *
   * @param[in] msg The message to provide to the completed function
   * @return `true` if the request has been completed successfully, `false`
   * otherwise.
   */
  bool complete(const std::string &msg);
  /**
   * Check if the request has been completed.
   *
   * @return `true` if the request has been completed, `false` otherwise.
   */
  inline bool is_completed() const { return completed_; }

  /**
   * Cancel a request.
   *
   * A call to this function will set the request as canceled and completed. Any
   * further call to complete() will be skipped.
   *
   * An synchronous task should check periodically that is_canceled() is `true`
   * to cancel the operation.
   *
   * @note If the request has been completed, this call will fail.
   *
   * @return `true` if the request has been canceled successfully, `false`
   * otherwise.
   */
  bool cancel();
  /**
   * Check if the request has been canceled.
   *
   * @return `true` if the request has been canceled, `false` otherwise.
   */
  inline bool is_canceled() const { return canceled_; }

 private:
  const std::string msg_;
  const Func func_;
  bool completed_ = false;
  bool canceled_ = false;
};

}  // namespace melo

#endif  // MELO_REQUEST_H_
