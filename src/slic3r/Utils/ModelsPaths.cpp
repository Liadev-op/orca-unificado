#include "ModelsPaths.hpp"

#include "libslic3r/Utils.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/log/trivial.hpp>
#include <boost/nowide/fstream.hpp>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace Slic3r {

static bool ensure_dir(const boost::filesystem::path &p)
{
    boost::system::error_code ec;
    if (boost::filesystem::exists(p, ec))
        return boost::filesystem::is_directory(p, ec);
    boost::filesystem::create_directories(p, ec);
    if (ec) {
        BOOST_LOG_TRIVIAL(warning) << "models path: cannot create " << p.string() << ": " << ec.message();
        return false;
    }
    return true;
}

static bool dir_is_writable(const boost::filesystem::path &dir)
{
    boost::system::error_code ec;
    if (!ensure_dir(dir))
        return false;
    auto probe = dir / ".write_test";
    {
        boost::nowide::ofstream out(probe.string().c_str());
        if (!out)
            return false;
        out << "ok";
    }
    boost::filesystem::remove(probe, ec);
    return true;
}

boost::filesystem::path exe_dir()
{
#ifdef _WIN32
    wchar_t buf[MAX_PATH] = {0};
    DWORD   n             = ::GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH)
        return {};
    return boost::filesystem::path(buf).parent_path();
#else
    boost::system::error_code ec;
    auto p = boost::filesystem::read_symlink("/proc/self/exe", ec);
    if (!ec)
        return p.parent_path();
    return {};
#endif
}

boost::filesystem::path models_root_dir(bool create)
{
    auto beside_exe = exe_dir() / "models";
    if (dir_is_writable(beside_exe))
        return beside_exe;

    boost::filesystem::path fallback;
    if (!data_dir().empty())
        fallback = boost::filesystem::path(data_dir()) / "models";
    if (create)
        ensure_dir(fallback);
    BOOST_LOG_TRIVIAL(info) << "models path: using fallback " << fallback.string();
    return fallback;
}

boost::filesystem::path models_provider_dir(const std::string &provider_id, bool create)
{
    auto p = models_root_dir(create) / provider_id;
    if (create)
        ensure_dir(p);
    return p;
}

boost::filesystem::path models_converted_dir(bool create)
{
    auto p = models_root_dir(create) / "converted";
    if (create)
        ensure_dir(p);
    return p;
}

boost::filesystem::path webview_profile_dir(const std::string &provider_id, bool create)
{
    boost::filesystem::path p;
    if (!data_dir().empty())
        p = boost::filesystem::path(data_dir()) / "webview" / provider_id;
    else
        p = exe_dir() / "data_dir" / "webview" / provider_id;
    if (create)
        ensure_dir(p);
    return p;
}

boost::filesystem::path converter_resources_dir()
{
    return boost::filesystem::path(resources_dir()) / "converter";
}

std::string sanitize_filename(const std::string &name)
{
    std::string out;
    out.reserve(name.size());
    for (unsigned char c : name) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
            out.push_back('_');
        else if (c < 32)
            continue;
        else
            out.push_back(static_cast<char>(c));
    }
    boost::trim(out);
    if (out.empty() || out == "." || out == "..")
        out = "model";
    if (out.size() > 120)
        out.resize(120);
    return out;
}

} // namespace Slic3r
