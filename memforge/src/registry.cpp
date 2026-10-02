#include <stdexcept>

#include "memforge/plugin.hpp"
#include "plugins/plugins.hpp"

namespace mf {
namespace {

using Factory = std::unique_ptr<Plugin> (*)();
const Factory kFactories[] = {
    make_strings_plugin, make_netioc_plugin, make_patscan_plugin, make_pescan_plugin,
    make_pooltag_plugin, make_dtbscan_plugin, make_entropy_plugin,
};

}  // namespace

std::vector<PluginInfo> available_plugins() {
  std::vector<PluginInfo> out;
  for (Factory f : kFactories) {
    auto p = f();
    out.push_back({p->name(), p->description()});
  }
  return out;
}

std::vector<std::unique_ptr<Plugin>> create_plugins(const std::vector<std::string>& names,
                                                    const Options& opts) {
  std::vector<std::string> wanted;
  for (const auto& n : names) {
    if (n == "all") {
      for (const auto& info : available_plugins()) wanted.push_back(info.name);
    } else {
      wanted.push_back(n);
    }
  }
  std::vector<std::unique_ptr<Plugin>> out;
  for (const auto& n : wanted) {
    bool dup = false;
    for (const auto& p : out) dup |= n == p->name();
    if (dup) continue;
    std::unique_ptr<Plugin> made;
    for (Factory f : kFactories) {
      auto p = f();
      if (n == p->name()) {
        made = std::move(p);
        break;
      }
    }
    if (!made) throw std::invalid_argument("unknown plugin: " + n);
    made->configure(opts);
    out.push_back(std::move(made));
  }
  if (out.empty()) throw std::invalid_argument("no plugins selected");
  return out;
}

}  // namespace mf
