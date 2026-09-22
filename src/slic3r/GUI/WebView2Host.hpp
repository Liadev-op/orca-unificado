#ifndef slic3r_GUI_WebView2Host_hpp_
#define slic3r_GUI_WebView2Host_hpp_

#include <wx/panel.h>
#include <wx/event.h>
#include <functional>
#include <string>

class wxWebView;

wxDECLARE_EVENT(EVT_MODELS_BROWSER_READY, wxCommandEvent);
wxDECLARE_EVENT(EVT_MODELS_BROWSER_NAVIGATED, wxCommandEvent);
wxDECLARE_EVENT(EVT_MODELS_BROWSER_DOWNLOAD, wxCommandEvent);
wxDECLARE_EVENT(EVT_MODELS_BROWSER_DOWNLOAD_FAIL, wxCommandEvent);

namespace Slic3r {
namespace GUI {

// Windows: real WebView2 with a per-provider userDataFolder and DownloadStarting.
// Other OS: wxWebView fallback (no download intercept).
class WebView2Host : public wxPanel
{
public:
    WebView2Host(wxWindow *parent, const wxString &user_data_folder, const wxString &user_agent);
    ~WebView2Host() override;

    void        Navigate(const wxString &url);
    void        GoBack();
    void        GoForward();
    bool        CanGoBack() const;
    bool        CanGoForward() const;
    wxString    CurrentUrl() const;
    bool        IsReady() const { return m_ready; }

    // Next DownloadStarting is forced into this full path (UTF-8). Empty = derive from suggested name.
    void        SetNextDownloadPath(const wxString &path_utf8) { m_next_download = path_utf8; }

    using SuggestFn = std::function<wxString(const wxString &uri, const wxString &suggested)>;
    void        SetDownloadPathSuggester(SuggestFn fn) { m_suggest = std::move(fn); }

private:
    void OnSize(wxSizeEvent &evt);
    void BindWxFallback();
    void ResizeNative();
    void DestroyNative();

    wxString  m_user_data;
    wxString  m_user_agent;
    wxString  m_pending_url;
    wxString  m_current_url;
    wxString  m_next_download;
    SuggestFn m_suggest;
    bool      m_ready{false};
    bool      m_can_back{false};
    bool      m_can_fwd{false};

#ifdef _WIN32
    void       StartWebView2();
    struct Native;
    Native    *m_native{nullptr};
#else
    wxWebView *m_browser{nullptr};
#endif
};

} // namespace GUI
} // namespace Slic3r

#endif
