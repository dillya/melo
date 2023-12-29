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
 * @file player.h
 * @brief Player interface definition.
 */

#ifndef MELO_PLAYER_H_
#define MELO_PLAYER_H_

#include <memory>
#include <string>

#include <melo/media.h>
#include <melo/playlist.h>

namespace melo {

/**
 * Interface to implement a media player.
 *
 * The Player class is an interface to let Melo plays some specific media(s)
 * like:
 *  - basic media file,
 *  - remote media files,
 *  - web services medias,
 *  - web radios,
 *  - ...
 *
 * The final class implementation should be added to the Melo global context
 * with Player.add() in order to make it accessible to Browser and Playlist.
 *
 * Thus, a player will be referenced by a Browser implementation and Playlist
 * through its unique ID. If the player is not available, the media will be
 * simply dropped and an error will be returned.
 */
class Player {
 public:
  /**
   * Player description structure.
   *
   * This structure contains all details and informations about a player.
   */
  struct Info {
    const std::string name;        /**< Displayed name of the player. */
    const std::string description; /**< Description of the player. */
  };

  /** Default description string. */
  const static std::string kDefaultDescription;

  /**
   * Add a new player to the global context.
   *
   * This function must be called to add / register a new player to Melo global
   * context and let it accessible from internal API.
   *
   * If the ID is already used by another player, the function will fails.
   *
   * @note A unique ID using only <b>lowercase</b> and <b>".-_"</b> characters
   * should be provided in `id`.
   *
   * @param[in] id The player unique ID to use
   * @param[in] player The player reference
   * @return `true` if the player has been added successfully, `false`
   * otherwise.
   *
   * @see is_valid_id() for more details about unique ID.
   */
  static bool add(const std::string &id, const std::shared_ptr<Player> &player);
  /**
   * Remove a player from the global context.
   *
   * This function can be called to remove / unregister a player from Melo
   * global context: after this call, the player will be not accessible with
   * get_by_id().
   *
   * @param[in] id The unique ID of the player to remove
   * @return `true` if the player has been removed successfully, `false`
   * otherwise.
   */
  static bool remove(const std::string &id);

  /**
   * Check if a player is available in global context.
   *
   * @param[in] id The unique ID of the player to lookup
   * @return `true` if the player is available, `false` otherwise.
   */
  static bool has(const std::string &id);
  /**
   * Get a reference to a player from global context.
   *
   * @param[in] id The unique ID of the player to get
   * @return A new reference to a `player` or `nullptr` if not found.
   */
  static std::shared_ptr<Player> get_by_id(const std::string &id);

  /**
   * Get player informations.
   *
   * This function must be implemented and must return a constant `Info`
   * structure which does not change during runtime.
   *
   * @return A constant Player::Info reference.
   */
  virtual const Info &get_info() const = 0;

  /**
   * Get player displayed name.
   *
   * @return A `std::string` containing the player name.
   */
  inline const std::string &get_name() const { return get_info().name; }
  /**
   * Get player description.
   *
   * @return A `std::string` containing the description of a player.
   */
  inline const std::string &get_description() const {
    return get_info().description;
  }

  /**
   * Play a media / playlist.
   *
   * TODO
   */
  virtual bool play(const std::shared_ptr<Playlist> &playlist) = 0;
  /**
   * Reset a player.
   *
   * TODO
   */
  virtual bool reset() = 0;

  // TODO: add mutex
  inline Media get_media() const { return media_; }

 protected:
  bool update_media(const Media &media);

 private:
  Media media_;
};

}  // namespace melo

#endif  // MELO_PLAYER_H_
