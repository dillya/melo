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
 * @file plugin.h
 * @brief Plugin interface definition.
 */

#ifndef MELO_PLUGIN_H_
#define MELO_PLUGIN_H_

#include <melo/browser.h>
#include <melo/core.h>
#include <melo/player.h>

#ifdef _WIN32
#define MELO_EXPORT __declspec(dllexport)
#else /* !_WIN32 */
#define MELO_EXPORT __attribute__((visibility("default")))
#endif /* !_WIN32 */

namespace melo {

class Plugin {
 public:
  typedef bool (*EntryPoint)(const melo::Plugin &plugin);

  enum Type { NATIVE_PLUGIN, PYTHON_PLUGIN };

  struct Manifest {
    static const std::string kFilename;
    static const std::string kDefaultEntryPoint;

    std::string name;
    std::string version;

    std::string melo_version;

    Type type;
    std::string filename;
    std::string entry_point;

    static bool parse(const std::string &path, Manifest &manifest,
                      std::string &error);
  };

  Plugin(const Manifest &manifest, Core &core)
      : manifest_(manifest), core_(core) {}

  bool add_browser(const std::string &id,
                   const std::shared_ptr<Browser> &Browser) const;
  bool remove_browser(const std::string &id) const;

  bool add_player(const std::string &id,
                  const std::shared_ptr<Player> &player) const;
  bool remove_player(const std::string &id) const;

 private:
  const Manifest manifest_;
  Core &core_;
};

}  // namespace melo

#endif  // MELO_PLUGIN_H_
