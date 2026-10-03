#pragma once

#include <memory>

#include "memforge/plugin.hpp"

namespace mf {
std::unique_ptr<Plugin> make_strings_plugin();
std::unique_ptr<Plugin> make_patscan_plugin();
std::unique_ptr<Plugin> make_netioc_plugin();
std::unique_ptr<Plugin> make_pescan_plugin();
std::unique_ptr<Plugin> make_entropy_plugin();
std::unique_ptr<Plugin> make_pooltag_plugin();
std::unique_ptr<Plugin> make_dtbscan_plugin();
}  // namespace mf
