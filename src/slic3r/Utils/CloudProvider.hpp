#ifndef __CLOUD_PROVIDER_HPP__
#define __CLOUD_PROVIDER_HPP__

#include <string>
#include <vector>

namespace Slic3r {

// Provider ids as in Orca Slicer tag v2.4.2 (ICloudServiceAgent.hpp).
// CloudProvider.hpp did not exist as a separate file on that tag; M1 extracts
// the constants so the SnOrca tree can include them without replacing NetworkAgent.
static const std::string ORCA_CLOUD_PROVIDER("orca");
static const std::string BBL_CLOUD_PROVIDER("bbl");

struct CloudEvent {
    std::string provider;  // ORCA_CLOUD_PROVIDER or BBL_CLOUD_PROVIDER
};

enum BundleType {
    Default = 0,
    Local,
    Subscribed,
};

// Minimal bundle metadata used by OrcaCloudServiceAgent::get_shared_bundle.
// The plugin hub / PresetBundleMetadata system from vanilla is NOT ported in M1.
struct BundleMetadata {
    std::string              id;
    std::string              name;
    std::string              version;
    std::string              description;
    std::string              author;
    long long                imported_time{0};
    long long                updated_time{0};
    BundleType               bundle_type{Default};
    std::string              path;
    std::vector<std::string> print_presets;
    std::vector<std::string> filament_presets;
    std::vector<std::string> printer_presets;
    bool                     is_subscribed{false};
    bool                     update_available{false};
    bool                     not_found{false};
    bool                     unauthorized{false};
};

} // namespace Slic3r

#endif // __CLOUD_PROVIDER_HPP__
