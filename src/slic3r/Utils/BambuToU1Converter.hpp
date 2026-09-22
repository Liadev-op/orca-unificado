#ifndef slic3r_BambuToU1Converter_hpp_
#define slic3r_BambuToU1Converter_hpp_

#include <map>
#include <string>
#include <vector>
#include <boost/filesystem/path.hpp>

namespace Slic3r {

// In-process port of josuanbn/bl2u1 (GPL-3.0). Converts a Bambu/Bambu Studio
// (and MakerWorld) 3MF project into a Snapmaker U1 project 3MF, keeping
// color painting. Does not call nbn.cat and does not invoke Python.
class BambuToU1Converter
{
public:
    struct Filament {
        std::string id;
        std::string color;
        std::string type;
    };

    struct Pick {
        std::string id;
        std::string color;
        std::string type;
    };

    struct Result {
        bool        ok{false};
        bool        need_filament_pick{false};
        bool        used_supports{false};
        std::string error;
        std::string output_path;
        std::vector<Filament> filaments;
    };

    static constexpr int k_target_filaments = 4;

    static bool is_bambu_project(const boost::filesystem::path &src_3mf);
    static std::vector<Filament> analyze(const boost::filesystem::path &src_3mf, std::string *error = nullptr);

    // keep_picks empty => keep the first 4 filaments (or all if fewer).
    // If the source has more than 4 and keep_picks is empty, returns need_filament_pick.
    static Result convert(const boost::filesystem::path &src_3mf,
                          const boost::filesystem::path &dest_dir,
                          const std::vector<Pick>       &keep_picks = {});

    // Builds a tiny Bambu-like 3MF, converts it, rejects STL. For CI / debug.
    static bool smoke_test(std::string &error);
};

} // namespace Slic3r

#endif
