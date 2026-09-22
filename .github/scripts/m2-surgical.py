from pathlib import Path


def must_replace(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise SystemExit(f"missing snippet: {label}")
    return text.replace(old, new, 1)


def patch_converter():
    p = Path("src/slic3r/Utils/BambuToU1Converter.cpp")
    t = p.read_text(encoding="utf-8")
    if "json_as_string" in t and "json_truthy" in t:
        print("converter already patched")
        return
    helpers = r'''
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

'''
    t = must_replace(
        t,
        '#include <sstream>\n\nusing json',
        '#include <sstream>\n#include <stdexcept>\n\nusing json',
        "converter include",
    )
    t = must_replace(
        t,
        'constexpr const char *k_default_profile = "Snapmaker PLA SnapSpeed @U1";\n\nstd::string read_zip_entry',
        'constexpr const char *k_default_profile = "Snapmaker PLA SnapSpeed @U1";\n' + helpers + 'std::string read_zip_entry',
        "converter helpers",
    )
    t = t.replace("open_zip_reader(&zip, p.string())", "open_zip_reader(&zip, path_as_utf8(p))")
    t = t.replace("open_zip_reader(&zin, src.string())", "open_zip_reader(&zin, path_as_utf8(src))")
    t = t.replace("open_zip_writer(&zout, dst.string())", "open_zip_writer(&zout, path_as_utf8(dst))")
    t = t.replace("open_zip_writer(&zip, path.string())", "open_zip_writer(&zip, path_as_utf8(path))")
    t = t.replace("open_zip_reader(&zip, src_3mf.string())", "open_zip_reader(&zip, path_as_utf8(src_3mf))")
    t = t.replace("ifstream in(p.string().c_str())", "ifstream in(path_as_utf8(p).c_str())")
    t = t.replace("ifstream in(jp.string().c_str())", "ifstream in(path_as_utf8(jp).c_str())")
    t = t.replace("ofstream o(stl.string().c_str())", "ofstream o(path_as_utf8(stl).c_str())")
    t = t.replace("std::string stem = src_3mf.stem().string();", "std::string stem = path_as_utf8(src_3mf.stem());")
    t = t.replace("r.output_path = dest.string();", "r.output_path = path_as_utf8(dest);")
    t = must_replace(
        t,
        '        if (it.value("type", "") == type)\n            return it.value("settings_id", k_default_profile);',
        '        if (json_as_string(it, "type") == type) {\n            auto sid = json_as_string(it, "settings_id", k_default_profile);\n            return sid.empty() ? k_default_profile : sid;\n        }',
        "profile_for_type",
    )
    t = must_replace(
        t,
        '        auto colors = cfg.value("filament_colour", json::array());\n        auto types  = cfg.value("filament_type", json::array());',
        '        auto colors = json_as_array(cfg, "filament_colour");\n        auto types  = json_as_array(cfg, "filament_type");',
        "filament arrays",
    )
    t = must_replace(
        t,
        '    if (orig_settings.value("enable_support", "0") == "1" || orig_settings.value("enable_support", 0) == 1)',
        '    if (json_truthy(orig_settings, "enable_support"))',
        "enable_support crash",
    )
    t = must_replace(
        t,
        '    auto diff        = orig_settings.value("different_settings_to_system", json::array());',
        '    auto diff        = json_as_array(orig_settings, "different_settings_to_system");',
        "diff array",
    )
    t = must_replace(
        t,
        'void normalize_filament_arrays(json &cfg)\n{\n    const int n = BambuToU1Converter::k_target_filaments;',
        'void normalize_filament_arrays(json &cfg)\n{\n    if (!cfg.is_object())\n        return;\n    const int n = BambuToU1Converter::k_target_filaments;',
        "normalize guard",
    )
    t = must_replace(
        t,
        'bool BambuToU1Converter::is_bambu_project(const boost::filesystem::path &src_3mf)\n{\n    mz_zip_archive zip;\n    mz_zip_zero_struct(&zip);\n    if (!open_zip_reader(&zip, path_as_utf8(src_3mf)))\n        return false;\n    bool ok = zip_has(zip, "Metadata/project_settings.config");\n    close_zip_reader(&zip);\n    return ok;\n}',
        'bool BambuToU1Converter::is_bambu_project(const boost::filesystem::path &src_3mf)\n{\n    try {\n        mz_zip_archive zip;\n        mz_zip_zero_struct(&zip);\n        if (!open_zip_reader(&zip, path_as_utf8(src_3mf)))\n            return false;\n        bool ok = zip_has(zip, "Metadata/project_settings.config");\n        close_zip_reader(&zip);\n        return ok;\n    } catch (...) {\n        return false;\n    }\n}',
        "is_bambu try",
    )
    t = must_replace(
        t,
        '{\n    Result r;\n    std::string err;',
        '{\n    Result r;\n    try {\n    std::string err;',
        "convert try",
    )
    t = must_replace(
        t,
        '    BOOST_LOG_TRIVIAL(info) << "BambuToU1: wrote " << r.output_path << " supports=" << has_support;\n    return r;\n}',
        '    BOOST_LOG_TRIVIAL(info) << "BambuToU1: wrote " << r.output_path << " supports=" << has_support;\n    return r;\n    } catch (const std::exception &e) {\n        BOOST_LOG_TRIVIAL(error) << "BambuToU1: convert exception: " << e.what();\n        r.ok = false;\n        r.error = std::string("Conversion failed: ") + e.what();\n        return r;\n    } catch (...) {\n        BOOST_LOG_TRIVIAL(error) << "BambuToU1: convert unknown exception";\n        r.ok = false;\n        r.error = "Conversion failed (unknown error).";\n        return r;\n    }\n}',
        "convert catch",
    )
    if 'value("enable_support", "0")' in t:
        raise SystemExit("enable_support still throws")
    p.write_text(t, encoding="utf-8")
    print("patched converter")


def patch_webview():
    p = Path("src/slic3r/GUI/WebView2Host.cpp")
    t = p.read_text(encoding="utf-8")
    if "put_NewWindow" in t and "SetDefaultDownloadFolder" in t:
        print("webview already patched")
        return
    t = must_replace(
        t,
        '#include <boost/nowide/convert.hpp>\n#include <iomanip>\n\n#include <wx/sizer.h>',
        '#include <boost/nowide/convert.hpp>\n#include <exception>\n#include <iomanip>\n\n#include <wx/filename.h>\n#include <wx/sizer.h>',
        "webview includes",
    )
    t = must_replace(
        t,
        '    m_native->controller->put_Bounds(rc);\n#endif\n}\n\nvoid WebView2Host::Navigate',
        '''    m_native->controller->put_Bounds(rc);
#endif
}

void WebView2Host::SetDefaultDownloadFolder(const wxString &folder)
{
    m_default_dl_folder = folder;
#ifdef _WIN32
    ApplyDefaultDownloadFolder();
#endif
}

#ifdef _WIN32
void WebView2Host::ApplyDefaultDownloadFolder()
{
    if (m_default_dl_folder.empty() || !m_native || !m_native->webview)
        return;
    try {
        ComPtr<ICoreWebView2_13> wv13;
        if (FAILED(m_native->webview.As(&wv13)) || !wv13)
            return;
        ComPtr<ICoreWebView2Profile> profile;
        if (FAILED(wv13->get_Profile(&profile)) || !profile)
            return;
        HRESULT hr = profile->put_DefaultDownloadFolderPath(m_default_dl_folder.wc_str());
        BOOST_LOG_TRIVIAL(info) << "Models WebView2 DefaultDownloadFolderPath hr=" << std::hex << hr;
    } catch (...) {
        BOOST_LOG_TRIVIAL(error) << "Models WebView2: DefaultDownloadFolderPath threw";
    }
}
#endif

void WebView2Host::Navigate''',
        "SetDefaultDownloadFolder",
    )
    t = must_replace(
        t,
        '                            m_native->webview.As(&m_native->webview4);\n                            if (m_native->webview4) {',
        '                            m_native->webview.As(&m_native->webview4);\n                            ApplyDefaultDownloadFolder();\n                            if (m_native->webview4) {',
        "apply folder on start",
    )
    t = must_replace(
        t,
        '                                        args->put_Handled(TRUE);\n                                        if (!u.empty())\n                                            m_native->webview->Navigate(u.wc_str());\n                                        return S_OK;',
        '''                                        HRESULT hr = args->put_NewWindow(m_native->webview.Get());
                                        args->put_Handled(TRUE);
                                        if (FAILED(hr) && !u.empty() && !u.StartsWith("about:") && !u.StartsWith("blob:"))
                                            m_native->webview->Navigate(u.wc_str());
                                        BOOST_LOG_TRIVIAL(info) << "Models NewWindowRequested uri=" << u.ToUTF8().data()
                                                                << " put_NewWindow hr=" << std::hex << hr;
                                        return S_OK;''',
        "put_NewWindow",
    )
    p.write_text(t, encoding="utf-8")
    print("patched webview")


def patch_panel():
    p = Path("src/slic3r/GUI/WebModelsPanel.cpp")
    t = p.read_text(encoding="utf-8")
    if "SetDefaultDownloadFolder" in t and "ConvertPath" in t and "try {" in t:
        print("panel already has download folder")
    t = t.replace(
        "auto *host = new WebView2Host(m_browser_stack, wxString::FromUTF8(profile.string()), ua);",
        "auto *host = new WebView2Host(m_browser_stack, from_path(profile), ua);",
    )
    if "SetDefaultDownloadFolder" not in t:
        t = must_replace(
            t,
            "    host->Hide();\n\n    host->SetDownloadPathSuggester",
            "    host->Hide();\n\n    host->SetDefaultDownloadFolder(from_path(models_provider_dir(id, true)));\n    host->SetDownloadPathSuggester",
            "panel default folder",
        )
    t = t.replace(
        "return wxString::FromUTF8((dir / name).string());",
        "return from_path(dir / name);",
    )
    t = t.replace(
        "m_last_file = e.GetString().ToUTF8().data();",
        "m_last_file = into_u8(e.GetString());",
    )
    t = t.replace(
        "m_local_list->Append(wxString::FromUTF8(it->path().string()));",
        "m_local_list->Append(from_path(it->path()));",
    )
    old_convert = """bool WebModelsPanel::ConvertPath(const std::string &src, std::string *out_converted)
{
    if (!BambuToU1Converter::is_bambu_project(src)) {"""
    new_convert = """bool WebModelsPanel::ConvertPath(const std::string &src, std::string *out_converted)
{
    try {
    if (src.empty()) {
        WarningDialog(this, _L("No file selected."), _L("Convert to U1"), wxOK).ShowModal();
        return false;
    }
    boost::filesystem::path src_path(src);
    if (!BambuToU1Converter::is_bambu_project(src_path)) {"""
    if old_convert in t:
        t = t.replace(old_convert, new_convert, 1)
        t = t.replace(
            "    auto first = BambuToU1Converter::convert(src, models_converted_dir(true), {});",
            "    auto first = BambuToU1Converter::convert(src_path, models_converted_dir(true), {});",
            1,
        )
        t = t.replace(
            "        first = BambuToU1Converter::convert(src, models_converted_dir(true), picks);",
            "        first = BambuToU1Converter::convert(src_path, models_converted_dir(true), picks);",
            1,
        )
        t = must_replace(
            t,
            "    return true;\n}\n\nvoid WebModelsPanel::OnConvert()",
            """    return true;
    } catch (const std::exception &e) {
        BOOST_LOG_TRIVIAL(error) << "Models ConvertPath: " << e.what();
        ErrorDialog(this, wxString::Format(_L("Convert to U1 failed: %s\\nThe app stayed open."), from_u8(e.what())), false).ShowModal();
        return false;
    } catch (...) {
        ErrorDialog(this, _L("Convert to U1 failed (unknown error). The app stayed open."), false).ShowModal();
        return false;
    }
}

void WebModelsPanel::OnConvert()""",
            "ConvertPath catch",
        )
    p.write_text(t, encoding="utf-8")
    print("patched panel")


def patch_mainframe():
    p = Path("src/slic3r/GUI/MainFrame.cpp")
    t = p.read_text(encoding="utf-8")
    old = """            [this](wxCommandEvent &) {
                if (m_models_panel)
                    m_models_panel->convert_file_dialog();
            },"""
    new = """            [this](wxCommandEvent &) {
                try {
                    if (m_models_panel)
                        m_models_panel->convert_file_dialog();
                } catch (...) {
                    wxMessageBox(_L("Convert to U1 failed. The app stayed open."), _L("Orca Unificado"), wxOK | wxICON_ERROR);
                }
            },"""
    if "Convert to U1 failed. The app stayed open." in t:
        print("mainframe already patched")
        return
    t = must_replace(t, old, new, "mainframe")
    p.write_text(t, encoding="utf-8")
    print("patched mainframe")


if __name__ == "__main__":
    patch_converter()
    patch_webview()
    patch_panel()
    patch_mainframe()
    conv = Path("src/slic3r/Utils/BambuToU1Converter.cpp").read_text(encoding="utf-8")
    if "json_as_string" not in conv or 'value("enable_support", "0")' in conv:
        raise SystemExit("converter guard failed")
    wv = Path("src/slic3r/GUI/WebView2Host.cpp").read_text(encoding="utf-8")
    if "put_NewWindow" not in wv or "SetDefaultDownloadFolder" not in wv:
        raise SystemExit("webview guard failed")
    print("all surgical patches ok")
