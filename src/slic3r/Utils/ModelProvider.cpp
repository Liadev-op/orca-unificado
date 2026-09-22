#include "ModelProvider.hpp"

#include "ModelsPaths.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/filesystem/path.hpp>
#include <regex>

namespace Slic3r {

static std::string first_capture(const std::string &text, const std::regex &re)
{
    std::smatch m;
    if (std::regex_search(text, m, re) && m.size() > 1)
        return m[1].str();
    return {};
}

bool MakerWorldProvider::matches_download(const std::string &url) const
{
    std::string u = url;
    boost::algorithm::to_lower(u);
    if (u.find("makerworld.com") == std::string::npos && u.find("bambulab.com") == std::string::npos &&
        u.find("aliyuncs.com") == std::string::npos && u.find("amazonaws.com") == std::string::npos)
        return boost::algorithm::ends_with(u, ".3mf") || boost::algorithm::ends_with(u, ".stl") ||
               boost::algorithm::ends_with(u, ".zip");
    return u.find(".3mf") != std::string::npos || u.find(".stl") != std::string::npos || u.find("download") != std::string::npos;
}

std::string MakerWorldProvider::download_subfolder(const std::string &page_url, const std::string &download_url) const
{
    const std::regex re_models(R"(/models/(\d+))", std::regex::icase);
    std::string      id = first_capture(page_url, re_models);
    if (id.empty())
        id = first_capture(download_url, re_models);
    if (id.empty())
        id = sanitize_model_filename(boost::filesystem::path(download_url).stem().string());
    if (id.empty())
        id = "download";
    return id;
}

bool PrintablesProvider::matches_download(const std::string &url) const
{
    std::string u = url;
    boost::algorithm::to_lower(u);
    return u.find(".3mf") != std::string::npos || u.find(".stl") != std::string::npos || u.find(".zip") != std::string::npos ||
           u.find("media.printables.com") != std::string::npos || u.find("/download/") != std::string::npos;
}

std::string PrintablesProvider::download_subfolder(const std::string &page_url, const std::string &download_url) const
{
    const std::regex re_model(R"(/model/(\d+))", std::regex::icase);
    std::string      id = first_capture(page_url, re_model);
    if (id.empty())
        id = first_capture(download_url, re_model);
    if (id.empty())
        id = sanitize_model_filename(boost::filesystem::path(download_url).stem().string());
    if (id.empty())
        id = "download";
    return id;
}

std::vector<std::unique_ptr<ModelProvider>> make_model_providers()
{
    std::vector<std::unique_ptr<ModelProvider>> out;
    out.emplace_back(std::make_unique<MakerWorldProvider>());
    out.emplace_back(std::make_unique<PrintablesProvider>());
    return out;
}

} // namespace Slic3r
