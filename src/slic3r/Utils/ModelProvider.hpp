#ifndef slic3r_ModelProvider_hpp_
#define slic3r_ModelProvider_hpp_

#include <memory>
#include <string>
#include <vector>

namespace Slic3r {

// One catalog site shown inside the Models tab. No scrapers, no hidden APIs:
// the user navigates the official website and we intercept their download.
class ModelProvider
{
public:
    virtual ~ModelProvider() = default;

    virtual std::string id() const = 0;
    virtual std::string display_name() const = 0;
    virtual std::string start_url() const = 0;
    virtual std::string user_agent_tag() const = 0;

    // True if this URL looks like a file the user asked the site to download.
    virtual bool matches_download(const std::string &url) const = 0;

    // Folder name under models/<id>/<hint>/
    virtual std::string download_subfolder(const std::string &page_url, const std::string &download_url) const = 0;

    virtual bool offers_u1_convert() const { return false; }
};

class MakerWorldProvider : public ModelProvider
{
public:
    std::string id() const override { return "makerworld"; }
    std::string display_name() const override { return "MakerWorld"; }
    std::string start_url() const override { return "https://makerworld.com/"; }
    std::string user_agent_tag() const override { return "OrcaUnificado-Models"; }
    bool matches_download(const std::string &url) const override;
    std::string download_subfolder(const std::string &page_url, const std::string &download_url) const override;
    bool offers_u1_convert() const override { return true; }
};

class PrintablesProvider : public ModelProvider
{
public:
    std::string id() const override { return "printables"; }
    std::string display_name() const override { return "Printables"; }
    std::string start_url() const override { return "https://www.printables.com/"; }
    std::string user_agent_tag() const override { return "OrcaUnificado-Models"; }
    bool matches_download(const std::string &url) const override;
    std::string download_subfolder(const std::string &page_url, const std::string &download_url) const override;
};

std::vector<std::unique_ptr<ModelProvider>> make_model_providers();

} // namespace Slic3r

#endif
