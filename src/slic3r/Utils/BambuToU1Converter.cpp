#include "BambuToU1Converter.hpp"
#include "ModelsPaths.hpp"

#include "libslic3r/miniz_extension.hpp"
#include "nlohmann/json.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/log/trivial.hpp>
#include <boost/nowide/fstream.hpp>
#include <cstdio>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <map>
#include <regex>
#include <sstream>
#include <stdexcept>

using json = nlohmann::json;

namespace Slic3r {

namespace {

constexpr const char *k_default_profile = "Snapmaker PLA SnapSpeed @U1";

std::string json_as_string(const json &j, const char *key, const std::string &def = {})
{
    if (!j.is_object() || !j.contains(key))
        return def;
    const auto &v = j.at(key);
    try {
        if (v.is_string())
            return v.get<std::string>();
        if (v.is_boolean())
            return v.get<bool>() ? "1" : "0";
        if (v.is_number_integer())
            return std::to_string(v.get<long long>());
        if (v.is_number())
            return std::to_string(v.get<double>());
    } catch (...) {}
    return def;
}

bool json_truthy(const json &j, const char *key)
{
    if (!j.is_object() || !j.contains(key))
        return false;
    const auto &v = j.at(key);
    try {
        if (v.is_string()) {
            const auto s = v.get<std::string>();
            return s == "1" || s == "true" || s == "True" || s == "TRUE";
        }
        if (v.is_boolean())
            return v.get<bool>();
        if (v.is_number())
            return v.get<double>() != 0.0;
    } catch (...) {}
    return false;
}

json json_as_array(const json &j, const char *key)
{
    if (!j.is_object() || !j.contains(key))
        return json::array();
    const auto &v = j.at(key);
    if (v.is_array())
        return v;
    if (v.is_string()) {
        json arr = json::array();
        arr.push_back(v.get<std::string>());
        return arr;
    }
    return json::array();
}

std::string read_zip_entry(mz_zip_archive &zip, const char *name)
{
    int idx = mz_zip_reader_locate_file(&zip, name, nullptr, 0);
    if (idx < 0)
        return {};
    size_t size = 0;
    void  *p    = mz_zip_reader_extract_to_heap(&zip, idx, &size, 0);
    if (!p)
        return {};
    std::string s(static_cast<const char *>(p), size);
    mz_free(p);
    return s;
}

bool zip_has(mz_zip_archive &zip, const char *name)
{
    return mz_zip_reader_locate_file(&zip, name, nullptr, 0) >= 0;
}

std::string normalize_color(std::string color)
{
    boost::trim(color);
    if (!color.empty() && color[0] == '#')
        color.erase(color.begin());
    if (color.size() == 8)
        color = color.substr(0, 6);
    if (color.size() != 6)
        return "#000000";
    for (char c : color) {
        if (!std::isxdigit(static_cast<unsigned char>(c)))
            return "#000000";
    }
    boost::to_upper(color);
    return "#" + color;
}

json load_filament_map()
{
    json arr = json::array();
    auto p   = converter_resources_dir() / "u1_filament_map.json";
    boost::nowide::ifstream in(path_as_utf8(p).c_str());
    if (in) {
        try {
            in >> arr;
        } catch (...) {
            arr = json::array();
        }
    }
    if (arr.empty()) {
        arr = json::array({
            {{"type", "PLA"}, {"settings_id", k_default_profile}},
            {{"type", "PETG"}, {"settings_id", "Snapmaker PETG HF"}},
            {{"type", "PETG-HF"}, {"settings_id", "Snapmaker PETG HF"}},
            {{"type", "ABS"}, {"settings_id", "Snapmaker ABS @U1 0.4 nozzle"}},
            {{"type", "TPU"}, {"settings_id", "Snapmaker TPU @U1 0.4 nozzle"}},
        });
    }
    return arr;
}

std::string profile_for_type(const json &map, const std::string &type)
{
    for (const auto &it : map) {
        if (json_as_string(it, "type") == type) {
            auto sid = json_as_string(it, "settings_id", k_default_profile);
            return sid.empty() ? k_default_profile : sid;
        }
    }
    return k_default_profile;
}

std::vector<BambuToU1Converter::Filament> parse_filaments_from_contents(const std::string &slice_info, const std::string &project_settings)
{
    std::vector<BambuToU1Converter::Filament> filaments;
    static const std::regex fil_re(
        R"(<filament\b([^>]*)/?>)",
        std::regex::icase);
    // MSVC C2001: raw-string R"(...)" cannot contain )" — use a custom delimiter.
    static const std::regex attr_re(R"re(([a-zA-Z_]+)\s*=\s*"([^"]*)")re");
    for (std::sregex_iterator it(slice_info.begin(), slice_info.end(), fil_re), end; it != end; ++it) {
        BambuToU1Converter::Filament f;
        std::string attrs = (*it)[1].str();
        for (std::sregex_iterator a(attrs.begin(), attrs.end(), attr_re), ae; a != ae; ++a) {
            std::string key = (*a)[1].str();
            std::string val = (*a)[2].str();
            boost::to_lower(key);
            if (key == "id")
                f.id = val;
            else if (key == "color")
                f.color = normalize_color(val);
            else if (key == "type")
                f.type = val;
        }
        if (f.id.empty())
            continue;
        if (f.type.empty())
            f.type = "PLA";
        if (f.color.empty())
            f.color = "#000000";
        filaments.push_back(std::move(f));
    }
    if (!filaments.empty() || project_settings.empty())
        return filaments;
    try {
        json cfg = json::parse(project_settings);
        auto colors = json_as_array(cfg, "filament_colour");
        auto types  = json_as_array(cfg, "filament_type");
        for (size_t i = 0; i < colors.size(); ++i) {
            BambuToU1Converter::Filament f;
            f.id    = std::to_string(i + 1);
            f.color = normalize_color(colors[i].is_string() ? colors[i].get<std::string>() : "");
            f.type  = (i < types.size() && types[i].is_string()) ? types[i].get<std::string>() : "PLA";
            filaments.push_back(std::move(f));
        }
    } catch (...) {}
    return filaments;
}

json load_u1_template(bool supports)
{
    const char *name = supports ? "u1_template_supports.3mf" : "u1_template.3mf";
    auto        p    = converter_resources_dir() / name;
    mz_zip_archive zip;
    mz_zip_zero_struct(&zip);
    json cfg;
    if (open_zip_reader(&zip, path_as_utf8(p))) {
        auto raw = read_zip_entry(zip, "Metadata/project_settings.config");
        close_zip_reader(&zip);
        if (!raw.empty()) {
            try {
                cfg = json::parse(raw);
            } catch (...) {}
        }
    }
    if (cfg.is_null() || cfg.empty()) {
        auto jp = converter_resources_dir() / (supports ? "u1_template_supports.json" : "u1_template.json");
        boost::nowide::ifstream in(path_as_utf8(jp).c_str());
        if (in) {
            try {
                in >> cfg;
            } catch (...) {}
        }
    }
    if (cfg.is_null() || cfg.empty()) {
        cfg = json::object();
        cfg["printer_model"]        = "Snapmaker U1";
        cfg["printer_settings_id"]  = "Snapmaker U1 (0.4 nozzle)";
        cfg["print_settings_id"]    = "0.20mm Standard @Snapmaker U1 (0.4 nozzle)";
        cfg["enable_support"]       = supports ? "1" : "0";
        cfg["support_type"]         = "tree(auto)";
    }
    return cfg;
}

std::string replace_printer_model_id(std::string xml)
{
    static const std::regex re(R"re(key="printer_model_id"\s+value="[^"]*")re");
    if (std::regex_search(xml, re))
        return std::regex_replace(xml, re, "key=\"printer_model_id\" value=\"Snapmaker U1\"");
    static const std::regex re2(R"(key='printer_model_id'\s+value='[^']*')");
    return std::regex_replace(xml, re2, "key=\"printer_model_id\" value=\"Snapmaker U1\"");
}

std::string rewrite_slice_info(const std::string &xml,
                               const std::map<std::string, BambuToU1Converter::Pick> &picks,
                               std::map<std::string, std::string> &id_mapping)
{
    std::string out = replace_printer_model_id(xml);
    static const std::regex fil_re(R"(<filament\b[^>]*/?>)", std::regex::icase);
    std::string rebuilt;
    rebuilt.reserve(out.size());
    std::sregex_iterator it(out.begin(), out.end(), fil_re), end;
    std::string::const_iterator last = out.begin();
    int new_id = 1;
    bool wrote_any = false;
    std::string indent = "    ";
    for (; it != end; ++it) {
        rebuilt.append(last, (*it)[0].first);
        last = (*it)[0].second;
        std::string tag = it->str();
        static const std::regex id_re(R"re(\bid\s*=\s*"([^"]*)")re", std::regex::icase);
        std::smatch m;
        std::string old_id;
        if (std::regex_search(tag, m, id_re))
            old_id = m[1].str();
        auto found = picks.find(old_id);
        if (found == picks.end())
            continue;
        const auto &pk = found->second;
        id_mapping[old_id] = std::to_string(new_id);
        std::ostringstream oss;
        oss << "<filament id=\"" << new_id << "\" type=\"" << pk.type << "\" color=\"" << pk.color
            << "\" used_m=\"0\" used_g=\"0\" />";
        rebuilt += oss.str();
        wrote_any = true;
        ++new_id;
    }
    rebuilt.append(last, out.cend());
    if (!wrote_any && picks.empty())
        return rebuilt;

    // Pad dummy white PLA to 4.
    if (new_id <= BambuToU1Converter::k_target_filaments) {
        std::string pad;
        for (; new_id <= BambuToU1Converter::k_target_filaments; ++new_id) {
            pad += "\n    <filament id=\"" + std::to_string(new_id) +
                   "\" type=\"PLA\" color=\"#FFFFFFFF\" used_m=\"0\" used_g=\"0\" />";
        }
        // Insert before last </plate> if present, else before </config>.
        auto pos = rebuilt.rfind("</plate>");
        if (pos == std::string::npos)
            pos = rebuilt.rfind("</config>");
        if (pos != std::string::npos)
            rebuilt.insert(pos, pad + "\n  ");
        else
            rebuilt += pad;
    }
    (void) indent;
    return rebuilt;
}

std::string rewrite_model_settings(const std::string &xml, const std::map<std::string, std::string> &id_mapping)
{
    std::string out = xml;
    static const std::regex re(R"re(<metadata\s+key="extruder"\s+value="([^"]*)")re", std::regex::icase);
    std::string result;
    result.reserve(out.size());
    std::sregex_iterator it(out.begin(), out.end(), re), end;
    std::string::const_iterator last = out.begin();
    for (; it != end; ++it) {
        result.append(last, (*it)[0].first);
        std::string old = (*it)[1].str();
        auto found = id_mapping.find(old);
        std::string neu = found != id_mapping.end() ? found->second : old;
        result += "<metadata key=\"extruder\" value=\"" + neu + "\"";
        last = (*it)[0].second;
    }
    result.append(last, out.cend());
    return result;
}

void normalize_filament_arrays(json &cfg)
{
    if (!cfg.is_object())
        return;
    const int n = BambuToU1Converter::k_target_filaments;
    for (auto it = cfg.begin(); it != cfg.end(); ++it) {
        if (!boost::algorithm::starts_with(it.key(), "filament_"))
            continue;
        if (!it->is_array() || it->empty())
            continue;
        auto arr = *it;
        if ((int) arr.size() < n) {
            auto last = arr.back();
            while ((int) arr.size() < n)
                arr.push_back(last);
        } else if ((int) arr.size() > n) {
            json trimmed = json::array();
            for (int i = 0; i < n; ++i)
                trimmed.push_back(arr.at(i));
            arr = std::move(trimmed);
        }
        *it = arr;
    }
}

bool write_converted_zip(const boost::filesystem::path &src,
                         const boost::filesystem::path &dst,
                         const std::string             &slice_info,
                         const std::string             &model_settings,
                         const std::string             &project_settings)
{
    mz_zip_archive zin;
    mz_zip_archive zout;
    mz_zip_zero_struct(&zin);
    mz_zip_zero_struct(&zout);
    if (!open_zip_reader(&zin, path_as_utf8(src)))
        return false;
    if (!open_zip_writer(&zout, path_as_utf8(dst))) {
        close_zip_reader(&zin);
        return false;
    }

    const mz_uint n = mz_zip_reader_get_num_files(&zin);
    bool ok = true;
    for (mz_uint i = 0; i < n && ok; ++i) {
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&zin, i, &st)) {
            ok = false;
            break;
        }
        std::string name = st.m_filename ? st.m_filename : "";
        if (name.find("..") != std::string::npos) {
            BOOST_LOG_TRIVIAL(warning) << "BambuToU1: skip suspicious zip entry " << name;
            continue;
        }
        if (st.m_is_directory)
            continue;

        const void *payload = nullptr;
        size_t      size    = 0;
        std::string owned;
        if (name == "Metadata/slice_info.config") {
            owned   = slice_info;
            payload = owned.data();
            size    = owned.size();
        } else if (name == "Metadata/model_settings.config") {
            owned   = model_settings;
            payload = owned.data();
            size    = owned.size();
        } else if (name == "Metadata/project_settings.config") {
            owned   = project_settings;
            payload = owned.data();
            size    = owned.size();
        } else {
            size_t extracted = 0;
            void  *p         = mz_zip_reader_extract_to_heap(&zin, i, &extracted, 0);
            if (!p) {
                ok = false;
                break;
            }
            ok = mz_zip_writer_add_mem(&zout, name.c_str(), p, extracted, MZ_DEFAULT_COMPRESSION) != 0;
            mz_free(p);
            continue;
        }
        ok = mz_zip_writer_add_mem(&zout, name.c_str(), payload, size, MZ_DEFAULT_COMPRESSION) != 0;
    }

    if (ok)
        ok = mz_zip_writer_finalize_archive(&zout) != 0;
    close_zip_writer(&zout);
    close_zip_reader(&zin);
    if (!ok) {
        boost::system::error_code ec;
        boost::filesystem::remove(dst, ec);
    }
    return ok;
}

std::string make_minimal_bambu_3mf(const boost::filesystem::path &path, int n_filaments, bool with_support)
{
    json project;
    project["filament_colour"] = json::array();
    project["filament_type"]   = json::array();
    project["different_settings_to_system"] = json::array();
    if (with_support)
        project["different_settings_to_system"].push_back("enable_support");
    std::ostringstream slice;
    slice << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<config>\n  <plate>\n";
    slice << "    <metadata key=\"printer_model_id\" value=\"BL-P001\"/>\n";
    for (int i = 0; i < n_filaments; ++i) {
        char col[8];
        std::snprintf(col, sizeof(col), "#%02X00%02X", (i * 40) & 0xFF, (i * 80) & 0xFF);
        project["filament_colour"].push_back(std::string(col) + "FF");
        project["filament_type"].push_back("PLA");
        slice << "    <filament id=\"" << (i + 1) << "\" type=\"PLA\" color=\"" << col
              << "\" used_m=\"0\" used_g=\"0\" />\n";
    }
    slice << "  </plate>\n</config>\n";
    std::string model =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<config>\n  <object>\n"
        "    <metadata key=\"extruder\" value=\"1\"/>\n  </object>\n</config>\n";
    std::string proj = project.dump(4);

    mz_zip_archive zip;
    mz_zip_zero_struct(&zip);
    if (!open_zip_writer(&zip, path_as_utf8(path)))
        return "open writer";
    bool ok = mz_zip_writer_add_mem(&zip, "Metadata/slice_info.config", slice.str().data(), slice.str().size(), MZ_DEFAULT_COMPRESSION) &&
              mz_zip_writer_add_mem(&zip, "Metadata/model_settings.config", model.data(), model.size(), MZ_DEFAULT_COMPRESSION) &&
              mz_zip_writer_add_mem(&zip, "Metadata/project_settings.config", proj.data(), proj.size(), MZ_DEFAULT_COMPRESSION) &&
              mz_zip_writer_add_mem(&zip, "3D/3dmodel.model", "<model/>", 8, MZ_DEFAULT_COMPRESSION) &&
              mz_zip_writer_finalize_archive(&zip);
    close_zip_writer(&zip);
    return ok ? std::string() : "write zip";
}

} // namespace

bool BambuToU1Converter::is_bambu_project(const boost::filesystem::path &src_3mf)
{
    try {
        mz_zip_archive zip;
        mz_zip_zero_struct(&zip);
        if (!open_zip_reader(&zip, path_as_utf8(src_3mf)))
            return false;
        bool ok = zip_has(zip, "Metadata/project_settings.config");
        close_zip_reader(&zip);
        return ok;
    } catch (...) {
        return false;
    }
}

std::vector<BambuToU1Converter::Filament> BambuToU1Converter::analyze(const boost::filesystem::path &src_3mf, std::string *error)
{
    mz_zip_archive zip;
    mz_zip_zero_struct(&zip);
    if (!open_zip_reader(&zip, path_as_utf8(src_3mf))) {
        if (error)
            *error = "Not a valid 3MF/ZIP archive.";
        return {};
    }
    auto slice = read_zip_entry(zip, "Metadata/slice_info.config");
    auto proj  = read_zip_entry(zip, "Metadata/project_settings.config");
    close_zip_reader(&zip);
    if (slice.empty() && proj.empty()) {
        if (error)
            *error = "Not a Bambu/MakerWorld project 3MF (missing Metadata). STL and Printables geometry 3MF cannot be converted.";
        return {};
    }
    auto fils = parse_filaments_from_contents(slice, proj);
    if (fils.empty() && error)
        *error = "Could not parse filaments from the uploaded file.";
    return fils;
}

BambuToU1Converter::Result BambuToU1Converter::convert(const boost::filesystem::path &src_3mf,
                                                       const boost::filesystem::path &dest_dir,
                                                       const std::vector<Pick>       &keep_picks)
{
    Result r;
    try {
    std::string err;
    auto filaments = analyze(src_3mf, &err);
    r.filaments    = filaments;
    if (filaments.empty()) {
        r.error = err.empty() ? "Could not parse filaments from the uploaded file." : err;
        return r;
    }

    std::vector<Pick> picks = keep_picks;
    if (picks.empty()) {
        if ((int) filaments.size() > k_target_filaments) {
            r.need_filament_pick = true;
            r.error              = "This project has more than 4 filaments. Choose which 4 stay for the U1.";
            return r;
        }
        for (const auto &f : filaments) {
            Pick p;
            p.id    = f.id;
            p.color = f.color;
            p.type  = f.type.empty() ? "PLA" : f.type;
            picks.push_back(p);
        }
    }
    if ((int) picks.size() > k_target_filaments)
        picks.resize(k_target_filaments);

    mz_zip_archive zip;
    mz_zip_zero_struct(&zip);
    if (!open_zip_reader(&zip, path_as_utf8(src_3mf))) {
        r.error = "Not a valid 3MF/ZIP archive.";
        return r;
    }
    auto orig_proj_raw = read_zip_entry(zip, "Metadata/project_settings.config");
    auto slice_raw     = read_zip_entry(zip, "Metadata/slice_info.config");
    auto model_raw     = read_zip_entry(zip, "Metadata/model_settings.config");
    close_zip_reader(&zip);

    json orig_settings = json::object();
    if (!orig_proj_raw.empty()) {
        try {
            orig_settings = json::parse(orig_proj_raw);
        } catch (const std::exception &e) {
            r.error = std::string("Could not read project settings: ") + e.what();
            return r;
        }
    }

    bool has_support = false;
    auto diff        = json_as_array(orig_settings, "different_settings_to_system");
    if (diff.is_array()) {
        for (const auto &s : diff) {
            if (s.is_string() && s.get<std::string>().find("enable_support") != std::string::npos)
                has_support = true;
        }
    }
    if (json_truthy(orig_settings, "enable_support"))
        has_support = true;

    json combined = load_u1_template(has_support);
    r.used_supports = has_support;

    std::map<std::string, Pick> pick_map;
    for (auto &p : picks) {
        p.color = normalize_color(p.color);
        if (p.color.size() == 7)
            p.color += "FF";
        boost::to_upper(p.color);
        if (p.color[0] != '#')
            p.color = "#" + p.color;
        pick_map[p.id] = p;
    }

    std::map<std::string, std::string> id_mapping;
    std::string new_slice = rewrite_slice_info(slice_raw.empty() ? std::string("<?xml version=\"1.0\"?><config><plate></plate></config>") : slice_raw,
                                               pick_map, id_mapping);
    std::string new_model = rewrite_model_settings(model_raw, id_mapping);

    json map               = load_filament_map();
    json new_colors        = json::array();
    json new_types         = json::array();
    json new_settings_ids  = json::array();
    for (const auto &p : picks) {
        new_colors.push_back(p.color);
        new_types.push_back(p.type);
        new_settings_ids.push_back(profile_for_type(map, p.type));
    }
    while ((int) new_colors.size() < k_target_filaments) {
        new_colors.push_back("#FFFFFFFF");
        new_types.push_back("PLA");
        new_settings_ids.push_back(k_default_profile);
    }
    combined["filament_colour"]      = new_colors;
    combined["filament_type"]        = new_types;
    combined["filament_settings_id"] = new_settings_ids;
    combined["printer_model"]        = "Snapmaker U1";
    combined["printer_settings_id"]  = "Snapmaker U1 (0.4 nozzle)";
    normalize_filament_arrays(combined);
    std::string new_proj = combined.dump(4);

    boost::system::error_code ec;
    boost::filesystem::create_directories(dest_dir, ec);
    std::string stem = path_as_utf8(src_3mf.stem());
    boost::replace_all(stem, "-U1", "");
    auto dest = dest_dir / (sanitize_model_filename(stem) + "-U1.3mf");
    if (!write_converted_zip(src_3mf, dest, new_slice, new_model, new_proj)) {
        r.error = "Conversion failed while writing the U1 3MF.";
        return r;
    }
    r.ok          = true;
    r.output_path = path_as_utf8(dest);
    BOOST_LOG_TRIVIAL(info) << "BambuToU1: wrote " << r.output_path << " supports=" << has_support;
    return r;
    } catch (const std::exception &e) {
        BOOST_LOG_TRIVIAL(error) << "BambuToU1: convert exception: " << e.what();
        r.ok = false;
        r.error = std::string("Conversion failed: ") + e.what();
        return r;
    } catch (...) {
        BOOST_LOG_TRIVIAL(error) << "BambuToU1: convert unknown exception";
        r.ok = false;
        r.error = "Conversion failed (unknown error).";
        return r;
    }
}

bool BambuToU1Converter::smoke_test(std::string &error)
{
    auto tmp = boost::filesystem::temp_directory_path() / "orcaunificado-u1-smoke";
    boost::system::error_code ec;
    boost::filesystem::create_directories(tmp, ec);
    auto src1 = tmp / "one.3mf";
    auto src4 = tmp / "four.3mf";
    auto src5 = tmp / "five.3mf";
    auto srcs = tmp / "sup.3mf";
    auto stl  = tmp / "nope.stl";
    if (auto e = make_minimal_bambu_3mf(src1, 1, false); !e.empty()) {
        error = "make 1-color: " + e;
        return false;
    }
    if (auto e = make_minimal_bambu_3mf(src4, 4, false); !e.empty()) {
        error = "make 4-color: " + e;
        return false;
    }
    if (auto e = make_minimal_bambu_3mf(src5, 5, false); !e.empty()) {
        error = "make 5-color: " + e;
        return false;
    }
    if (auto e = make_minimal_bambu_3mf(srcs, 2, true); !e.empty()) {
        error = "make supports: " + e;
        return false;
    }
    {
        boost::nowide::ofstream o(path_as_utf8(stl).c_str());
        o << "solid nope\nendsolid nope\n";
    }
    if (is_bambu_project(stl)) {
        error = "STL was accepted as a Bambu project";
        return false;
    }
    auto r1 = convert(src1, tmp / "out");
    if (!r1.ok) {
        error = "1-color: " + r1.error;
        return false;
    }
    auto r4 = convert(src4, tmp / "out");
    if (!r4.ok) {
        error = "4-color: " + r4.error;
        return false;
    }
    auto r5 = convert(src5, tmp / "out");
    if (!r5.need_filament_pick) {
        error = "5-color should ask for a pick";
        return false;
    }
    std::vector<Pick> keep;
    for (int i = 0; i < 4; ++i) {
        Pick p;
        p.id    = r5.filaments[i].id;
        p.color = r5.filaments[i].color;
        p.type  = r5.filaments[i].type;
        keep.push_back(p);
    }
    r5 = convert(src5, tmp / "out", keep);
    if (!r5.ok) {
        error = "5-color convert: " + r5.error;
        return false;
    }
    auto rs = convert(srcs, tmp / "out");
    if (!rs.ok) {
        error = "supports: " + rs.error;
        return false;
    }
    if (!rs.used_supports) {
        error = "supports template was not selected";
        return false;
    }
    return true;
}

} // namespace Slic3r
