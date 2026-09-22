#ifndef slic3r_GUI_WebModelsPanel_hpp_
#define slic3r_GUI_WebModelsPanel_hpp_

#include <wx/panel.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "slic3r/Utils/BambuToU1Converter.hpp"
#include "slic3r/Utils/ModelProvider.hpp"

class Button;
class wxBoxSizer;
class wxCheckBox;
class wxListBox;
class wxStaticText;
class wxTextCtrl;

namespace Slic3r {
namespace GUI {

class WebView2Host;

class WebModelsPanel : public wxPanel
{
public:
    WebModelsPanel(wxWindow *parent);
    ~WebModelsPanel() override = default;

    bool Show(bool show) override;
    void msw_rescale();
    void convert_file_dialog();

private:
    void BuildUi();
    void EnsureProviderHost(const std::string &id);
    void SelectProvider(const std::string &id);
    void ShowLocal();
    void RefreshLocalList();
    void UpdateChrome();
    void OnBack();
    void OnForward();
    void OnDownloadManual();
    void OnConvert();
    void OnOpenPrepare();
    bool ConvertPath(const std::string &src, std::string *out_converted);
    void OpenInPrepare(const std::string &path, bool as_project);
    wxString SuggestDownloadPath(const std::string &provider_id, const wxString &uri, const wxString &suggested);
    void OnDownloadFinished(const std::string &provider_id, const wxString &path);
    bool PickFilamentsIfNeeded(const BambuToU1Converter::Result &first, std::vector<BambuToU1Converter::Pick> &picks);

    std::vector<std::unique_ptr<ModelProvider>> m_providers;
    std::map<std::string, WebView2Host *>       m_hosts;
    std::string                                 m_current_id{"makerworld"};
    std::string                                 m_last_file;
    std::string                                 m_last_converted;
    bool                                        m_built{false};

    wxPanel      *m_chip_bar{nullptr};
    wxPanel      *m_browser_stack{nullptr};
    wxPanel      *m_local_panel{nullptr};
    wxListBox    *m_local_list{nullptr};
    wxTextCtrl   *m_url{nullptr};
    wxStaticText *m_status{nullptr};
    Button       *m_btn_back{nullptr};
    Button       *m_btn_fwd{nullptr};
    Button       *m_btn_dl{nullptr};
    Button       *m_btn_convert{nullptr};
    Button       *m_btn_open{nullptr};
    wxCheckBox   *m_chk_convert{nullptr};
    wxBoxSizer   *m_stack_sizer{nullptr};
    std::map<std::string, Button *> m_chips;
};

} // namespace GUI
} // namespace Slic3r

#endif
