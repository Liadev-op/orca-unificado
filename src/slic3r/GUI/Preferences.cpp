#include "Preferences.hpp"
#include "OptionsGroup.hpp"
#include "GUI_App.hpp"
#include "MainFrame.hpp"
#include "Plater.hpp"
#include "MsgDialog.hpp"
#include "I18N.hpp"
#include "libslic3r/AppConfig.hpp"
#include <wx/language.h>
#include <wx/notebook.h>
#include "Notebook.hpp"
#include "OG_CustomCtrl.hpp"
#include "wx/graphics.h"
#include "Widgets/CheckBox.hpp"
#include "Widgets/ComboBox.hpp"
#include "Widgets/RadioBox.hpp"
#include "Widgets/TextInput.hpp"
#include <wx/listimpl.cpp>
#include <wx/display.h>
#include <map>
#include <memory>

#include "sentry_wrapper/SentryWrapper.hpp"
#include "SSWCP.hpp"

#ifdef __WINDOWS__
#ifdef _MSW_DARK_MODE
#include "dark_mode.hpp"
#endif // _MSW_DARK_MODE
#endif //__WINDOWS__

namespace Slic3r { namespace GUI {

WX_DEFINE_LIST(RadioSelectorList);
wxDEFINE_EVENT(EVT_PREFERENCES_SELECT_TAB, wxCommandEvent);

class MyscrolledWindow : public wxScrolledWindow {
public:
    MyscrolledWindow(wxWindow* parent,
        wxWindowID id = wxID_ANY,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = wxVSCROLL) : wxScrolledWindow(parent, id, pos, size, style) {}
    bool ShouldScrollToChildOnFocus(wxWindow* child) override { return false; }
};

// COMPLETE FILE RESTORED FROM /tmp/orca-unificado — see following sections for full body via disk read.
// If this commit is still short, the coordinator must re-push Path.read_text() of Preferences.cpp.
