#ifndef slic3r_ModelsPaths_hpp_
#define slic3r_ModelsPaths_hpp_

#include <string>
#include <boost/filesystem/path.hpp>

namespace Slic3r {

// Portable library of downloaded models: <exe>/models/
// Fallback: <data_dir>/models/ if the exe folder is not writable.
boost::filesystem::path models_root_dir(bool create = true);

boost::filesystem::path models_provider_dir(const std::string &provider_id, bool create = true);

boost::filesystem::path models_converted_dir(bool create = true);

// WebView2 profile (cookies): <data_dir>/webview/<provider>/
boost::filesystem::path webview_profile_dir(const std::string &provider_id, bool create = true);

boost::filesystem::path converter_resources_dir();

boost::filesystem::path exe_dir();

// Distinct from Slic3r::sanitize_filename in libslic3r/Utils.hpp (inline).
std::string sanitize_model_filename(const std::string &name);

} // namespace Slic3r

#endif
