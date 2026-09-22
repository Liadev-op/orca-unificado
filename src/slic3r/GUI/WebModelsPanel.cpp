#include "WebModelsPanel.hpp"

#include "WebView2Host.hpp"
#include "GUI_App.hpp"
#include "I18N.hpp"
#include "MainFrame.hpp"
#include "MsgDialog.hpp"
#include "Plater.hpp"
#include "Widgets/Button.hpp"
#include "Widgets/Label.hpp"
#include "Widgets/StateColor.hpp"
#include "format.hpp"
#include "GUI.hpp"
#include "wxExtensions.hpp"

#include "slic3r/Utils/ModelsPaths.hpp"
#include "libslic3r/Format/bbs_3mf.hpp"
#include "libslic3r.h"

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/log/trivial.hpp>
#include <boost/nowide/convert.hpp>

#include <wx/checkbox.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/listbox.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textdlg.h>
#include <wx/textctrl.h>
#include <wx/wupdlock.h>

namespace Slic3r {
namespace GUI {

static Button *make_chip(wxWindow *parent, const wxString &label)
{
    auto *b = new Button(parent, label);
    b->SetFont(Label::Body_13);
    b->SetMinSize(wxSize(parent->FromDIP(110), parent->FromDIP(28)));
    b->SetCornerRadius(parent->FromDIP(14));
    StateColor bg(std::make_pair(wxColour(0, 150, 136), (int) StateColor::Checked),
                  std::make_pair(wxColour(232, 232, 232), (int) StateColor::Hovered),
                  std::make_pair(wxColour(245, 245, 245), (int) StateColor::Normal));
    StateColor fg(std::make_pair(*wxWHITE, (int) StateColor::Checked),
                  std::make_pair(wxColour(50, 50, 50), (int) StateColor::Normal));
    b->SetBackgroundColor(bg);
    b->SetTextColor(fg);
    return b;
}

static Button *make_bar_btn(wxWindow *parent, const wxString &label)
{
    auto *b = new Button(parent, label);
    b->SetFont(Label::Body_12);
    b->SetStyle(ButtonStyle::Regular, ButtonType::Compact);
    b->SetMinSize(wxSize(parent->FromDIP(88), parent->FromDIP(26)));
    return b;
}

WebModelsPanel::WebModelsPanel(wxWindow *parent)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL)
{
    m_providers = make_model_providers();
    SetBackgroundColour(*wxWHITE);
    BuildUi();
}

void WebModelsPanel::BuildUi()
{
    auto *root = new wxBoxSizer(wxVERTICAL);

    m_chip_bar = new wxPanel(this);
    m_chip_bar->SetBackgroundColour(wxColour(250, 250, 250));
    auto *chips = new wxBoxSizer(wxHORIZONTAL);
    chips->Add(new wxStaticText(m_chip_bar, wxID_ANY, _L("Models")), 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, FromDIP(12));
    for (const auto &p : m_providers) {
        auto *chip = make_chip(m_chip_bar, wxString::FromUTF8(p->display_name()));
        m_chips[p->id()] = chip;
        const std::string id = p->id();
        chip->Bind(wxEVT_BUTTON, [this, id](wxCommandEvent &) { SelectProvider(id); });
        chips->Add(chip, 0, wxALIGN_CENTER_VERTICAL | wxALL, FromDIP(6));
    }
    auto *local = make_chip(m_chip_bar, _L("Local"));
    m_chips["local"] = local;
    local->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { ShowLocal(); });
    chips->Add(local, 0, wxALIGN_CENTER_VERTICAL | wxALL, FromDIP(6));
    auto *hint = new wxStaticText(m_chip_bar, wxID_ANY,
                                  _L("Does not replace Snapmaker Space (Project / Model tab)."));
    hint->SetForegroundColour(wxColour(120, 120, 120));
    chips->Add(hint, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(8));
    m_chip_bar->SetSizer(chips);
    root->Add(m_chip_bar, 0, wxEXPAND);

    m_browser_stack = new wxPanel(this);
    m_browser_stack->SetBackgroundColour(*wxWHITE);
    m_stack_sizer = new wxBoxSizer(wxVERTICAL);
    m_browser_stack->SetSizer(m_stack_sizer);
    root->Add(m_browser_stack, 1, wxEXPAND);

    m_local_panel = new wxPanel(m_browser_stack);
    auto *ls      = new wxBoxSizer(wxVERTICAL);
    ls->Add(new wxStaticText(m_local_panel, wxID_ANY,
                             _L("Files already saved under models/ (makerworld, printables, converted).")),
            0, wxALL, FromDIP(8));
    m_local_list = new wxListBox(m_local_panel, wxID_ANY);
    ls->Add(m_local_list, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(8));
    m_local_list->Bind(wxEVT_LISTBOX, [this](wxCommandEvent &e) {
        m_last_file = e.GetString().ToUTF8().data();
        m_last_converted.clear();
        UpdateChrome();
    });
    m_local_list->Bind(wxEVT_LISTBOX_DCLICK, [this](wxCommandEvent &) { OnOpenPrepare(); });
    m_local_panel->SetSizer(ls);
    m_stack_sizer->Add(m_local_panel, 1, wxEXPAND);
    m_local_panel->Hide();

    auto *bar = new wxPanel(this);
    bar->SetBackgroundColour(wxColour(248, 248, 248));
    auto *bs = new wxBoxSizer(wxHORIZONTAL);
    m_btn_back = make_bar_btn(bar, _L("Back"));
    m_btn_fwd  = make_bar_btn(bar, _L("Forward"));
    m_btn_dl   = make_bar_btn(bar, _L("Save to portable"));
    m_btn_convert = make_bar_btn(bar, _L("Convert to U1"));
    m_btn_open = make_bar_btn(bar, _L("Open in Prepare"));
    m_btn_back->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { OnBack(); });
    m_btn_fwd->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { OnForward(); });
    m_btn_dl->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { OnDownloadManual(); });
    m_btn_convert->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { OnConvert(); });
    m_btn_open->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { OnOpenPrepare(); });
    m_chk_convert = new wxCheckBox(bar, wxID_ANY, _L("Convert to U1 after download"));
    m_chk_convert->SetValue(true);
    m_url = new wxTextCtrl(bar, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER | wxTE_READONLY);
    m_status = new wxStaticText(bar, wxID_ANY, _L("MakerWorld: anonymous until you sign in on the site."));
    m_status->SetForegroundColour(wxColour(90, 90, 90));
    bs->Add(m_btn_back, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(8));
    bs->Add(m_btn_fwd, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(4));
    bs->Add(m_url, 1, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, FromDIP(8));
    bs->Add(m_btn_dl, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(4));
    bs->Add(m_chk_convert, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(4));
    bs->Add(m_btn_convert, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(4));
    bs->Add(m_btn_open, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
    auto *bar_wrap = new wxBoxSizer(wxVERTICAL);
    bar_wrap->Add(bs, 0, wxEXPAND | wxTOP, FromDIP(4));
    bar_wrap->Add(m_status, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(8));
    bar->SetSizer(bar_wrap);
    root->Add(bar, 0, wxEXPAND);

    SetSizer(root);
    m_built = true;
}

bool WebModelsPanel::Show(bool show)
{
    bool r = wxPanel::Show(show);
    if (show && m_current_id != "local")
        EnsureProviderHost(m_current_id);
    if (show)
        SelectProvider(m_current_id);
    return r;
}

void WebModelsPanel::EnsureProviderHost(const std::string &id)
{
    if (m_hosts.count(id))
        return;
    ModelProvider *prov = nullptr;
    for (auto &p : m_providers) {
        if (p->id() == id) {
            prov = p.get();
            break;
        }
    }
    if (!prov)
        return;

    const auto profile = webview_profile_dir(id, true);
    wxString ua = wxString::Format(
        "OrcaUnificado/%s (%s) Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
        "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36 Edg/120.0.0.0",
        SLIC3R_VERSION, prov->user_agent_tag());
    auto *host = new WebView2Host(m_browser_stack, wxString::FromUTF8(profile.string()), ua);
    m_hosts[id] = host;
    m_stack_sizer->Add(host, 1, wxEXPAND);
    host->Hide();

    host->SetDownloadPathSuggester([this, id](const wxString &uri, const wxString &suggested) {
        return SuggestDownloadPath(id, uri, suggested);
    });
    host->Bind(EVT_MODELS_BROWSER_NAVIGATED, [this, id](wxCommandEvent &e) {
        if (m_current_id != id)
            return;
        m_url->SetValue(e.GetString());
        UpdateChrome();
    });
    host->Bind(EVT_MODELS_BROWSER_DOWNLOAD, [this, id](wxCommandEvent &e) { OnDownloadFinished(id, e.GetString()); });
    host->Bind(EVT_MODELS_BROWSER_DOWNLOAD_FAIL, [this](wxCommandEvent &e) {
        m_status->SetLabel(e.GetString());
        WarningDialog(this, e.GetString(), _L("Models"), wxOK).ShowModal();
    });
    host->Bind(EVT_MODELS_BROWSER_READY, [this, id, prov](wxCommandEvent &) {
        if (prov)
            m_hosts[id]->Navigate(wxString::FromUTF8(prov->start_url()));
    });
}

void WebModelsPanel::SelectProvider(const std::string &id)
{
    m_current_id = id;
    for (auto &kv : m_chips)
        kv.second->SetValue(kv.first == id);

    if (id == "local") {
        for (auto &h : m_hosts)
            h.second->Hide();
        m_local_panel->Show();
        RefreshLocalList();
        m_browser_stack->Layout();
        Layout();
        UpdateChrome();
        return;
    }

    m_local_panel->Hide();
    try {
        EnsureProviderHost(id);
    } catch (...) {
        BOOST_LOG_TRIVIAL(error) << "Models: failed to create host for " << id;
        m_status->SetLabel(wxString::Format(_L("Could not open %s. MakerWorld still works if this is Printables."),
                                            wxString::FromUTF8(id)));
        return;
    }
    for (auto &h : m_hosts)
        h.second->Show(h.first == id);
    m_browser_stack->Layout();
    Layout();
    UpdateChrome();
}

void WebModelsPanel::ShowLocal() { SelectProvider("local"); }

void WebModelsPanel::RefreshLocalList()
{
    m_local_list->Clear();
    namespace fs = boost::filesystem;
    auto add_dir = [this](const fs::path &root) {
        boost::system::error_code ec;
        if (!fs::exists(root, ec))
            return;
        for (fs::recursive_directory_iterator it(root, ec), end; it != end && !ec; it.increment(ec)) {
            if (!fs::is_regular_file(it->path(), ec))
                continue;
            auto ext = it->path().extension().string();
            boost::to_lower(ext);
            if (ext == ".3mf" || ext == ".stl" || ext == ".zip" || ext == ".obj")
                m_local_list->Append(wxString::FromUTF8(it->path().string()));
        }
    };
    add_dir(models_root_dir(true));
}

void WebModelsPanel::UpdateChrome()
{
    WebView2Host *host = nullptr;
    auto it = m_hosts.find(m_current_id);
    if (it != m_hosts.end())
        host = it->second;
    const bool web = m_current_id != "local" && host;
    m_btn_back->Enable(web && host->CanGoBack());
    m_btn_fwd->Enable(web && host->CanGoForward());
    if (web)
        m_url->SetValue(host->CurrentUrl());
    else if (m_current_id == "local")
        m_url->SetValue(wxString::FromUTF8(models_root_dir(false).string()));

    bool is_mw = m_current_id == "makerworld";
    m_chk_convert->Enable(is_mw || (!m_last_file.empty() && boost::algorithm::iends_with(m_last_file, ".3mf")));
    m_btn_convert->Enable(!m_last_file.empty() && boost::algorithm::iends_with(m_last_file, ".3mf"));
    m_btn_open->Enable(!m_last_file.empty() || !m_last_converted.empty());

    wxString sess;
    if (m_current_id == "makerworld")
        sess = _L("MakerWorld: session cookies live in data_dir/webview/makerworld (not Orca Cloud, not Snapmaker).");
    else if (m_current_id == "printables")
        sess = _L("Printables: session cookies live in data_dir/webview/printables.");
    else
        sess = _L("Local library next to the exe (models/).");
    if (!m_last_file.empty())
        sess += "  " + wxString::Format(_L("Last file: %s"), wxString::FromUTF8(boost::filesystem::path(m_last_file).filename().string()));
    m_status->SetLabel(sess);
}

void WebModelsPanel::OnBack()
{
    auto it = m_hosts.find(m_current_id);
    if (it != m_hosts.end())
        it->second->GoBack();
}

void WebModelsPanel::OnForward()
{
    auto it = m_hosts.find(m_current_id);
    if (it != m_hosts.end())
        it->second->GoForward();
}

wxString WebModelsPanel::SuggestDownloadPath(const std::string &provider_id, const wxString &uri, const wxString &suggested)
{
    ModelProvider *prov = nullptr;
    for (auto &p : m_providers) {
        if (p->id() == provider_id)
            prov = p.get();
    }
    if (!prov)
        return {};
    std::string page = m_hosts.count(provider_id) ? m_hosts[provider_id]->CurrentUrl().ToUTF8().data() : "";
    auto        sub  = prov->download_subfolder(page, uri.ToUTF8().data());
    auto        dir  = models_provider_dir(provider_id, true) / sub;
    boost::system::error_code ec;
    boost::filesystem::create_directories(dir, ec);
    wxFileName fn(suggested);
    std::string name = sanitize_model_filename(fn.GetFullName().ToUTF8().data());
    if (name.empty() || name == "model") {
        wxFileName fu(uri);
        name = sanitize_model_filename(fu.GetFullName().ToUTF8().data());
    }
    if (name.empty() || name.find('.') == std::string::npos)
        name = "model.3mf";
    return wxString::FromUTF8((dir / name).string());
}

void WebModelsPanel::OnDownloadFinished(const std::string &provider_id, const wxString &path)
{
    m_last_file      = path.ToUTF8().data();
    m_last_converted.clear();
    m_status->SetLabel(wxString::Format(_L("Saved to %s"), path));
    BOOST_LOG_TRIVIAL(info) << "Models download: " << m_last_file << " provider=" << provider_id;
    bool do_convert = m_chk_convert->GetValue();
    ModelProvider *prov = nullptr;
    for (auto &p : m_providers)
        if (p->id() == provider_id)
            prov = p.get();
    if (do_convert && prov && prov->offers_u1_convert() && boost::algorithm::iends_with(m_last_file, ".3mf") &&
        BambuToU1Converter::is_bambu_project(m_last_file)) {
        std::string converted;
        if (ConvertPath(m_last_file, &converted))
            m_last_converted = converted;
    }
    UpdateChrome();
}

void WebModelsPanel::OnDownloadManual()
{
    wxFileDialog dlg(this, _L("Copy a downloaded model into the portable models folder"), wxEmptyString, wxEmptyString,
                     "3MF/STL (*.3mf;*.stl;*.zip)|*.3mf;*.stl;*.zip", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() != wxID_OK)
        return;
    auto src = into_path(dlg.GetPath());
    auto id  = m_current_id == "local" ? std::string("makerworld") : m_current_id;
    auto dest_dir = models_provider_dir(id, true) / "manual";
    boost::system::error_code ec;
    boost::filesystem::create_directories(dest_dir, ec);
    auto dest = dest_dir / src.filename();
    boost::filesystem::copy_file(src, dest, boost::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        ErrorDialog(this, wxString::FromUTF8(ec.message()), false).ShowModal();
        return;
    }
    OnDownloadFinished(id, wxString::FromUTF8(dest.string()));
}

bool WebModelsPanel::PickFilamentsIfNeeded(const BambuToU1Converter::Result &first, std::vector<BambuToU1Converter::Pick> &picks)
{
    if (!first.need_filament_pick)
        return true;
    wxDialog dlg(this, wxID_ANY, _L("U1 has 4 slots — pick filaments to keep"), wxDefaultPosition, wxDefaultSize,
                 wxDEFAULT_DIALOG_STYLE);
    auto *s = new wxBoxSizer(wxVERTICAL);
    s->Add(new wxStaticText(&dlg, wxID_ANY, _L("This Bambu project has more than 4 filaments. Keep up to 4:")), 0, wxALL, 8);
    std::vector<wxCheckBox *> boxes;
    for (const auto &f : first.filaments) {
        auto label = wxString::Format("%s  %s  %s", wxString::FromUTF8(f.id), wxString::FromUTF8(f.type),
                                      wxString::FromUTF8(f.color));
        auto *cb   = new wxCheckBox(&dlg, wxID_ANY, label);
        cb->SetValue(boxes.size() < 4);
        boxes.push_back(cb);
        s->Add(cb, 0, wxLEFT | wxRIGHT | wxBOTTOM, 8);
    }
    auto *btns = dlg.CreateButtonSizer(wxOK | wxCANCEL);
    s->Add(btns, 0, wxEXPAND | wxALL, 8);
    dlg.SetSizerAndFit(s);
    if (dlg.ShowModal() != wxID_OK)
        return false;
    picks.clear();
    for (size_t i = 0; i < boxes.size(); ++i) {
        if (!boxes[i]->GetValue())
            continue;
        BambuToU1Converter::Pick p;
        p.id    = first.filaments[i].id;
        p.color = first.filaments[i].color;
        p.type  = first.filaments[i].type;
        picks.push_back(p);
        if (picks.size() == 4)
            break;
    }
    if (picks.empty()) {
        WarningDialog(this, _L("Keep at least one filament."), _L("Convert to U1"), wxOK).ShowModal();
        return false;
    }
    return true;
}

bool WebModelsPanel::ConvertPath(const std::string &src, std::string *out_converted)
{
    if (!BambuToU1Converter::is_bambu_project(src)) {
        WarningDialog(this,
                      _L("This file is not a Bambu/MakerWorld project 3MF. STL and Printables files should be imported as geometry."),
                      _L("Convert to U1"), wxOK)
            .ShowModal();
        return false;
    }
    auto first = BambuToU1Converter::convert(src, models_converted_dir(true), {});
    std::vector<BambuToU1Converter::Pick> picks;
    if (first.need_filament_pick) {
        if (!PickFilamentsIfNeeded(first, picks))
            return false;
        first = BambuToU1Converter::convert(src, models_converted_dir(true), picks);
    }
    if (!first.ok) {
        ErrorDialog(this, wxString::FromUTF8(first.error), false).ShowModal();
        return false;
    }
    if (out_converted)
        *out_converted = first.output_path;
    m_status->SetLabel(wxString::Format(_L("Converted to %s"), wxString::FromUTF8(first.output_path)));
    InfoDialog(this, _L("Convert to U1"),
               wxString::Format(_L("Wrote %s\nColor painting is kept. Open it as a project so the machine stays U1.\n\nConverter logic: josuanbn/bl2u1 (GPL-3.0), ported in-process."),
                                wxString::FromUTF8(first.output_path)))
        .ShowModal();
    return true;
}

void WebModelsPanel::OnConvert()
{
    if (m_last_file.empty()) {
        convert_file_dialog();
        return;
    }
    ConvertPath(m_last_file, &m_last_converted);
    UpdateChrome();
}

void WebModelsPanel::convert_file_dialog()
{
    wxFileDialog dlg(this, _L("Convert Bambu 3MF to Snapmaker U1"), wxEmptyString, wxEmptyString, "3MF (*.3mf)|*.3mf",
                     wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() != wxID_OK)
        return;
    m_last_file = into_u8(dlg.GetPath());
    ConvertPath(m_last_file, &m_last_converted);
    UpdateChrome();
}

void WebModelsPanel::OpenInPrepare(const std::string &path, bool as_project)
{
    auto *plater = wxGetApp().plater();
    if (!plater || path.empty())
        return;
    wxString wpath = wxString::FromUTF8(path);
    if (as_project)
        plater->load_project(wpath);
    else {
        std::vector<boost::filesystem::path> files{boost::filesystem::path(path)};
        plater->load_files(files, LoadStrategy::LoadModel);
    }
    if (wxGetApp().mainframe)
        wxGetApp().mainframe->select_tab(MainFrame::tp3DEditor);
}

void WebModelsPanel::OnOpenPrepare()
{
    if (!m_last_converted.empty()) {
        OpenInPrepare(m_last_converted, true);
        return;
    }
    if (m_last_file.empty())
        return;
    bool bambu = boost::algorithm::iends_with(m_last_file, ".3mf") && BambuToU1Converter::is_bambu_project(m_last_file);
    if (bambu && m_chk_convert->GetValue()) {
        if (ConvertPath(m_last_file, &m_last_converted) && !m_last_converted.empty()) {
            OpenInPrepare(m_last_converted, true);
            return;
        }
    }
    OpenInPrepare(m_last_file, false);
}

void WebModelsPanel::msw_rescale()
{
    Layout();
    Refresh();
}

} // namespace GUI
} // namespace Slic3r
