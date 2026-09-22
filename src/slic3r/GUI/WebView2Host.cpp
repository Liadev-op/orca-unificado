#include "WebView2Host.hpp"

#include "slic3r/GUI/GUI_App.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/Widgets/WebView.hpp"

#include <boost/log/trivial.hpp>
#include <boost/nowide/convert.hpp>
#include <iomanip>

#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/webview.h>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wrl.h>
#include <wrl/event.h>
#include <WebView2.h>
#pragma comment(lib, "ole32.lib")
#endif

wxDEFINE_EVENT(EVT_MODELS_BROWSER_READY, wxCommandEvent);
wxDEFINE_EVENT(EVT_MODELS_BROWSER_NAVIGATED, wxCommandEvent);
wxDEFINE_EVENT(EVT_MODELS_BROWSER_DOWNLOAD, wxCommandEvent);
wxDEFINE_EVENT(EVT_MODELS_BROWSER_DOWNLOAD_FAIL, wxCommandEvent);

namespace Slic3r {
namespace GUI {

#ifdef _WIN32

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

struct WebView2Host::Native {
    WebView2Host                 *host{nullptr};
    ComPtr<ICoreWebView2Environment> env;
    ComPtr<ICoreWebView2Controller>  controller;
    ComPtr<ICoreWebView2>            webview;
    ComPtr<ICoreWebView2_4>          webview4;
    EventRegistrationToken           download_token{};
    EventRegistrationToken           nav_token{};
    EventRegistrationToken           src_token{};
    EventRegistrationToken           newwin_token{};
    EventRegistrationToken           history_token{};
    bool                             dying{false};
};

static wxString lpwstr_to_wx(LPCWSTR s)
{
    if (!s)
        return {};
    return wxString(s);
}

static void free_co_str(LPWSTR s)
{
    if (s)
        ::CoTaskMemFree(s);
}

static RECT wx_to_rect(const wxRect &r)
{
    RECT rc;
    rc.left   = r.x;
    rc.top    = r.y;
    rc.right  = r.x + r.width;
    rc.bottom = r.y + r.height;
    return rc;
}

#endif // _WIN32

WebView2Host::WebView2Host(wxWindow *parent, const wxString &user_data_folder, const wxString &user_agent)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL | wxBORDER_NONE)
    , m_user_data(user_data_folder)
    , m_user_agent(user_agent)
{
    SetBackgroundColour(*wxWHITE);
    Bind(wxEVT_SIZE, &WebView2Host::OnSize, this);
#ifdef _WIN32
    m_native = new Native;
    m_native->host = this;
    CallAfter([this] { StartWebView2(); });
#else
    BindWxFallback();
#endif
}

WebView2Host::~WebView2Host()
{
    DestroyNative();
}

void WebView2Host::DestroyNative()
{
#ifdef _WIN32
    if (!m_native)
        return;
    m_native->dying = true;
    if (m_native->webview4 && m_native->download_token.value)
        m_native->webview4->remove_DownloadStarting(m_native->download_token);
    if (m_native->controller)
        m_native->controller->Close();
    delete m_native;
    m_native = nullptr;
#endif
}

void WebView2Host::OnSize(wxSizeEvent &evt)
{
    ResizeNative();
    evt.Skip();
}

void WebView2Host::ResizeNative()
{
#ifdef _WIN32
    if (!m_native || !m_native->controller)
        return;
    wxRect r = GetClientRect();
    RECT   rc = wx_to_rect(r);
    m_native->controller->put_Bounds(rc);
#endif
}

void WebView2Host::Navigate(const wxString &url)
{
    m_pending_url = url;
#ifdef _WIN32
    if (m_native && m_native->webview)
        m_native->webview->Navigate(url.wc_str());
#else
    if (m_browser)
        m_browser->LoadURL(url);
#endif
}

void WebView2Host::GoBack()
{
#ifdef _WIN32
    if (m_native && m_native->webview)
        m_native->webview->GoBack();
#else
    if (m_browser && m_browser->CanGoBack())
        m_browser->GoBack();
#endif
}

void WebView2Host::GoForward()
{
#ifdef _WIN32
    if (m_native && m_native->webview)
        m_native->webview->GoForward();
#else
    if (m_browser && m_browser->CanGoForward())
        m_browser->GoForward();
#endif
}

bool WebView2Host::CanGoBack() const
{
#ifdef _WIN32
    return m_can_back;
#else
    return m_browser && m_browser->CanGoBack();
#endif
}

bool WebView2Host::CanGoForward() const
{
#ifdef _WIN32
    return m_can_fwd;
#else
    return m_browser && m_browser->CanGoForward();
#endif
}

wxString WebView2Host::CurrentUrl() const
{
#ifdef _WIN32
    return m_current_url;
#else
    return m_browser ? m_browser->GetCurrentURL() : m_current_url;
#endif
}

void WebView2Host::BindWxFallback()
{
#ifndef _WIN32
    m_browser = WebView::CreateWebView(this, m_pending_url.empty() ? wxString("about:blank") : m_pending_url);
    auto *s   = new wxBoxSizer(wxVERTICAL);
    if (m_browser) {
        s->Add(m_browser, 1, wxEXPAND);
        m_browser->Bind(wxEVT_WEBVIEW_NAVIGATED, [this](wxWebViewEvent &e) {
            m_current_url = e.GetURL();
            m_ready       = true;
            wxCommandEvent ev(EVT_MODELS_BROWSER_NAVIGATED);
            ev.SetEventObject(this);
            ev.SetString(m_current_url);
            wxPostEvent(this, ev);
        });
    } else {
        s->Add(new wxStaticText(this, wxID_ANY, _L("WebView is not available on this system.")), 1, wxALIGN_CENTER);
    }
    SetSizer(s);
    m_ready = m_browser != nullptr;
    wxCommandEvent ev(EVT_MODELS_BROWSER_READY);
    ev.SetEventObject(this);
    wxPostEvent(this, ev);
#endif
}

#ifdef _WIN32

void WebView2Host::StartWebView2()
{
    if (!m_native || m_native->dying)
        return;
    HWND hwnd = GetHWND();
    if (!hwnd)
        return;

    std::wstring user_data = m_user_data.ToStdWstring();
    HRESULT      hr        = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, user_data.empty() ? nullptr : user_data.c_str(), nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this](HRESULT result, ICoreWebView2Environment *env) -> HRESULT {
                if (!m_native || m_native->dying)
                    return S_OK;
                if (FAILED(result) || !env) {
                    BOOST_LOG_TRIVIAL(error) << "Models WebView2 environment failed: " << std::hex << result;
                    return S_OK;
                }
                m_native->env = env;
                HWND hwnd2    = GetHWND();
                env->CreateCoreWebView2Controller(
                    hwnd2,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [this](HRESULT result, ICoreWebView2Controller *controller) -> HRESULT {
                            if (!m_native || m_native->dying)
                                return S_OK;
                            if (FAILED(result) || !controller)
                                return S_OK;
                            m_native->controller = controller;
                            controller->get_CoreWebView2(&m_native->webview);
                            if (!m_native->webview)
                                return S_OK;

                            ICoreWebView2Settings *settings = nullptr;
                            if (SUCCEEDED(m_native->webview->get_Settings(&settings)) && settings) {
                                settings->put_AreDefaultContextMenusEnabled(TRUE);
                                settings->put_AreDevToolsEnabled(TRUE);
                                ICoreWebView2Settings2 *s2 = nullptr;
                                if (SUCCEEDED(settings->QueryInterface(IID_PPV_ARGS(&s2))) && s2) {
                                    if (!m_user_agent.empty())
                                        s2->put_UserAgent(m_user_agent.wc_str());
                                    s2->Release();
                                }
                                settings->Release();
                            }

                            m_native->webview.As(&m_native->webview4);
                            if (m_native->webview4) {
                                m_native->webview4->add_DownloadStarting(
                                    Callback<ICoreWebView2DownloadStartingEventHandler>(
                                        [this](ICoreWebView2 *, ICoreWebView2DownloadStartingEventArgs *args) -> HRESULT {
                                            if (!m_native || m_native->dying || !args)
                                                return S_OK;
                                            ICoreWebView2DownloadOperation *op = nullptr;
                                            args->get_DownloadOperation(&op);
                                            LPWSTR uri = nullptr;
                                            LPWSTR sug = nullptr;
                                            if (op) {
                                                op->get_Uri(&uri);
                                                op->get_ResultFilePath(&sug);
                                            }
                                            wxString uri_s = lpwstr_to_wx(uri);
                                            wxString sug_s = lpwstr_to_wx(sug);
                                            free_co_str(uri);
                                            free_co_str(sug);

                                            wxString dest = m_next_download;
                                            m_next_download.clear();
                                            if (dest.empty() && m_suggest)
                                                dest = m_suggest(uri_s, sug_s);
                                            if (!dest.empty()) {
                                                args->put_ResultFilePath(dest.wc_str());
                                                args->put_Handled(TRUE);
                                            }
                                            if (op) {
                                                op->add_StateChanged(
                                                    Callback<ICoreWebView2StateChangedEventHandler>(
                                                        [this, dest](ICoreWebView2DownloadOperation *sender, IUnknown *) -> HRESULT {
                                                            if (!m_native || m_native->dying || !sender)
                                                                return S_OK;
                                                            COREWEBVIEW2_DOWNLOAD_STATE st;
                                                            sender->get_State(&st);
                                                            if (st == COREWEBVIEW2_DOWNLOAD_STATE_COMPLETED) {
                                                                LPWSTR path = nullptr;
                                                                sender->get_ResultFilePath(&path);
                                                                wxString p = lpwstr_to_wx(path);
                                                                free_co_str(path);
                                                                if (p.empty())
                                                                    p = dest;
                                                                wxCommandEvent ev(EVT_MODELS_BROWSER_DOWNLOAD);
                                                                ev.SetEventObject(this);
                                                                ev.SetString(p);
                                                                wxPostEvent(this, ev);
                                                            } else if (st == COREWEBVIEW2_DOWNLOAD_STATE_INTERRUPTED) {
                                                                wxCommandEvent ev(EVT_MODELS_BROWSER_DOWNLOAD_FAIL);
                                                                ev.SetEventObject(this);
                                                                ev.SetString(_L("Download interrupted (login, paywall or network)."));
                                                                wxPostEvent(this, ev);
                                                            }
                                                            return S_OK;
                                                        })
                                                        .Get(),
                                                    nullptr);
                                                op->Release();
                                            }
                                            return S_OK;
                                        })
                                        .Get(),
                                    &m_native->download_token);
                            }

                            m_native->webview->add_NavigationCompleted(
                                Callback<ICoreWebView2NavigationCompletedEventHandler>(
                                    [this](ICoreWebView2 *sender, ICoreWebView2NavigationCompletedEventArgs *args) -> HRESULT {
                                        if (!m_native || m_native->dying || !sender)
                                            return S_OK;
                                        LPWSTR uri = nullptr;
                                        sender->get_Source(&uri);
                                        m_current_url = lpwstr_to_wx(uri);
                                        free_co_str(uri);
                                        BOOL ok = TRUE;
                                        if (args)
                                            args->get_IsSuccess(&ok);
                                        BOOL back = FALSE, fwd = FALSE;
                                        sender->get_CanGoBack(&back);
                                        sender->get_CanGoForward(&fwd);
                                        m_can_back = back == TRUE;
                                        m_can_fwd  = fwd == TRUE;
                                        wxCommandEvent ev(EVT_MODELS_BROWSER_NAVIGATED);
                                        ev.SetEventObject(this);
                                        ev.SetString(m_current_url);
                                        ev.SetInt(ok ? 1 : 0);
                                        wxPostEvent(this, ev);
                                        return S_OK;
                                    })
                                    .Get(),
                                &m_native->nav_token);

                            m_native->webview->add_NewWindowRequested(
                                Callback<ICoreWebView2NewWindowRequestedEventHandler>(
                                    [this](ICoreWebView2 *, ICoreWebView2NewWindowRequestedEventArgs *args) -> HRESULT {
                                        if (!args || !m_native || !m_native->webview)
                                            return S_OK;
                                        LPWSTR uri = nullptr;
                                        args->get_Uri(&uri);
                                        wxString u = lpwstr_to_wx(uri);
                                        free_co_str(uri);
                                        args->put_Handled(TRUE);
                                        if (!u.empty())
                                            m_native->webview->Navigate(u.wc_str());
                                        return S_OK;
                                    })
                                    .Get(),
                                &m_native->newwin_token);

                            m_ready = true;
                            ResizeNative();
                            controller->put_IsVisible(TRUE);
                            if (!m_pending_url.empty())
                                m_native->webview->Navigate(m_pending_url.wc_str());
                            wxCommandEvent ev(EVT_MODELS_BROWSER_READY);
                            ev.SetEventObject(this);
                            wxPostEvent(this, ev);
                            return S_OK;
                        })
                        .Get());
                return S_OK;
            })
            .Get());

    if (FAILED(hr))
        BOOST_LOG_TRIVIAL(error) << "CreateCoreWebView2EnvironmentWithOptions hr=" << std::hex << hr;
}

#endif // _WIN32

} // namespace GUI
} // namespace Slic3r
