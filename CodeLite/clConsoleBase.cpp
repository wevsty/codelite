#include "clConsoleBase.h"
#include "clConsoleCMD.h"
#include "clConsoleGnomeTerminal.h"
#include "clConsoleKonsole.h"
#include "clConsoleLXTerminal.h"
#include "clConsoleOSXTerminal.h"
#include "cl_config.h"
#include "file_logger.h"
#include <algorithm>
#include <wx/utils.h>

clConsoleBase::clConsoleBase() {}

clConsoleBase::~clConsoleBase() {}

clConsoleBase::Ptr_t clConsoleBase::GetTerminal()
{
    clConsoleBase::Ptr_t terminal;
    wxString terminalName = GetSelectedTerminalName();
#ifdef __WXMSW__
    wxUnusedVar(terminalName);
    terminal.reset(new clConsoleCMD());
#elif defined(__WXGTK__)
    if(terminalName.CmpNoCase("konsole") == 0) {
        terminal.reset(new clConsoleKonsole());
    } else if(terminalName.CmpNoCase("lxterminal") == 0) {
        terminal.reset(new clConsoleLXTerminal());
    } else {
        // the default terminal is "gnome-terminal"
        terminal.reset(new clConsoleGnomeTerminal());
    }
#else
    wxUnusedVar(terminalName);
    terminal.reset(new clConsoleOSXTerminal());
#endif
    return terminal;
}

wxArrayString clConsoleBase::GetAvailaleTerminals()
{
    wxArrayString terminals;
#ifdef __WXMSW__
    terminals.Add("CMD");
#elif defined(__WXGTK__)
    terminals.Add("konsole");
    terminals.Add("gnome-terminal");
    terminals.Add("lxterminal");
#else
    terminals.Add("Terminal.app");
#endif
    return terminals;
}

void clConsoleBase::AddEnvVariable(const wxString& name, const wxString& value)
{
    m_environment.erase(name);
    m_environment.insert({ name, value });
}

wxString clConsoleBase::GetEnvironmentPrefix() const
{
    wxString strline;
    std::for_each(m_environment.begin(), m_environment.end(),
                  [&](const wxStringMap_t::value_type& vt) { strline << vt.first << "=" << vt.second << " "; });
    return strline;
}

wxString clConsoleBase::WrapWithQuotesIfNeeded(const wxString& s) const
{
    wxString strimmed = s;
    strimmed.Trim().Trim(false);
    if(strimmed.Contains(" ")) { strimmed.Prepend("\"").Append("\""); }
    return strimmed;
}

wxString clConsoleBase::EscapeString(const wxString& str, const wxString& c) const
{
    wxString escaped = str;
    escaped.Replace(c, wxString() << "\\" << c);
    return escaped;
}

bool clConsoleBase::StartProcess(const wxString& command)
{
    SetPid(::wxExecute(command, wxEXEC_ASYNC | wxEXEC_MAKE_GROUP_LEADER | GetExecExtraFlags(), m_callback));
    // reset the m_callback (it will auto-delete itself)
    m_callback = nullptr;
    return (GetPid() > 0);
}

wxString clConsoleBase::GetSelectedTerminalName()
{
    wxString terminalName = clConfig::Get().Read("Terminal", wxString());
    if(terminalName.IsEmpty()) {
#ifdef __WXGTK__
        terminalName = "gnome-terminal";
#elif defined(__WXOSX__)
        terminalName = "Terminal";
#else
        terminalName = "CMD";
#endif
    }
    return terminalName;
}

clConsoleEnvironment::clConsoleEnvironment() {}

clConsoleEnvironment::~clConsoleEnvironment() { UnApply(); }

void clConsoleEnvironment::Add(const wxString& name, const wxString& value)
{
    m_environment.erase(name);
    m_environment.insert({ name, value });
}

void clConsoleEnvironment::Apply()
{
    if(!m_oldEnvironment.empty()) {
        clWARNING() << "Refusing to apply environment. Already in a dirty state";
        return;
    }
    if(m_environment.empty()) { return; }

    // keep a copy of the old environment before we apply the new values
    m_oldEnvironment.clear();
    std::for_each(m_environment.begin(), m_environment.end(), [&](const wxStringMap_t::value_type& vt) {
        wxString envvalue;
        if(::wxGetEnv(vt.first, &envvalue)) {
            m_oldEnvironment[vt.first] = envvalue;
        } else {
            m_oldEnvironment[vt.first] = "__no_such_env__";
        }
        ::wxSetEnv(vt.first, vt.second);
    });
}

void clConsoleEnvironment::UnApply()
{
    if(m_oldEnvironment.empty()) { return; }
    std::for_each(m_oldEnvironment.begin(), m_oldEnvironment.end(), [&](const wxStringMap_t::value_type& vt) {
        if(vt.second == "__no_such_env__") {
            ::wxUnsetEnv(vt.second);
        } else {
            ::wxSetEnv(vt.first, vt.second);
        }
    });
    m_oldEnvironment.clear();
}

clConsoleEnvironment::clConsoleEnvironment(const wxStringMap_t& env)
    : m_environment(env)
{
}