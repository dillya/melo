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
 * @file browser.h
 * @brief Browser interface definition.
 */

#ifndef MELO_BROWSER_H_
#define MELO_BROWSER_H_

#include <memory>
#include <string>

#include <melo/media.h>
#include <melo/request.h>

namespace melo {

/**
 * Interface to expose media(s) to users.
 *
 * The Browser class is an interface to expose media(s) from many kind of
 * sources like:
 *  - file system,
 *  - internal library,
 *  - remote libraries (local network or website),
 *  - ...
 *
 * The final class implementation should be added to the Melo global context
 * with Browser.add() in order to make it accessible to final user.
 */
class Browser {
 public:
  /**
   * Browser description structure.
   *
   * This structure contains all details and informations about a browser.
   */
  struct Info {
    const std::string name;        /**< Displayed name of the browser. */
    const std::string description; /**< Description of the browser. */
  };

  /** Default description string. */
  const static std::string kDefaultDescription;

  /**
   * Add a new browser to the global context.
   *
   * This function must be called to add / register a new browser to Melo global
   * context and let it accessible from internal API.
   *
   * If the ID is already used by another browser, the function will fails.
   *
   * @note A unique ID using only <b>lowercase</b> and <b>".-_"</b> characters
   * should be provided in `id`.
   *
   * @param[in] id The browser unique ID to use
   * @param[in] browser The browser reference
   * @return `true` if the browser has been added successfully, `false`
   * otherwise.
   *
   * @see is_valid_id() for more details about unique ID.
   */
  static bool add(const std::string &id,
                  const std::shared_ptr<Browser> &browser);
  /**
   * Remove a browser from the global context.
   *
   * This function can be called to remove / unregister a browser from Melo
   * global context: after this call, the browser will be not accessible with
   * get_by_id().
   *
   * @param[in] id The unique ID of the browser to remove
   * @return `true` if the browser has been removed successfully, `false`
   * otherwise.
   */
  static bool remove(const std::string &id);

  /**
   * Check if a browser is available in global context.
   *
   * @param[in] id The unique ID of the browser to lookup
   * @return `true` if the browser is available, `false` otherwise.
   */
  static bool has(const std::string &id);
  /**
   * Get a reference to a browser from global context.
   *
   * @param[in] id The unique ID of the browser to get
   * @return A new reference to a `Browser` or `nullptr` if not found.
   */
  static std::shared_ptr<Browser> get_by_id(const std::string &id);

  /**
   * Get browser informations.
   *
   * This function must be implemented and must return a constant `Info`
   * structure which does not change during runtime.
   *
   * @return A constant Browser::Info reference.
   */
  virtual const Info &get_info() const = 0;

  /**
   * TODO
   */
  virtual bool handle_request(
      const std::shared_ptr<Request> &request) const = 0;

  /**
   * Get browser displayed name.
   *
   * @return A `std::string` containing the browser name.
   */
  inline const std::string &get_name() const { return get_info().name; }
  /**
   * Get browser description.
   *
   * @return A `std::string` containing the description of a browser.
   */
  inline const std::string &get_description() const {
    return get_info().description;
  }
};

}  // namespace melo

#endif  // MELO_BROWSER_H_
