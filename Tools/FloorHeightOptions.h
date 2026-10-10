#pragma once
#include "FloorHeightCore.h"
#include <filesystem>
namespace floorheight {
enum class Action { Show,Apply,Restore,Help };
struct Options { Action action=Action::Show; double target=1.905; int delay=5; std::filesystem::path stateDirectory;std::string backupId; };
inline bool ValidBackupId(const std::string& id) {
    return !id.empty() && id.size()<100 && std::all_of(id.begin(),id.end(),[](char c){return (c>='0' && c<='9') || c=='-';});
}
inline Options Parse(const std::vector<std::wstring>& arguments) {
    Options out;bool show=false,restore=false,help=false,feet=false,inches=false,meters=false,cm=false,delay=false,state=false,backup=false;
    double ft=0,in=0,metric=0;
    for(size_t i=0;i<arguments.size();i++) {
        const auto& arg=arguments[i];
        auto once=[](bool& seen){if(seen)throw std::invalid_argument("Option repeated.");seen=true;};
        auto value=[&]()->std::wstring {if(i+1>=arguments.size())throw std::invalid_argument("Option value missing.");return arguments[++i];};
        auto number=[&](){auto text=value();size_t used=0;double n=std::stod(text,&used);if(used!=text.size() || !std::isfinite(n))throw std::invalid_argument("Invalid numeric height/value.");return n;};
        if(arg==L"--show")once(show);
        else if(arg==L"--restore")once(restore);
        else if(arg==L"--help")once(help);
        else if(arg==L"--feet"){once(feet);ft=number();}
        else if(arg==L"--inches"){once(inches);in=number();}
        else if(arg==L"--meters"){once(meters);metric=number();}
        else if(arg==L"--centimeters"){once(cm);metric=number();}
        else if(arg==L"--delay"){once(delay);double n=number();if(n<0 || n>60 || n!=std::floor(n))throw std::invalid_argument("Delay must be whole seconds from 0 to 60.");out.delay=static_cast<int>(n);}
        else if(arg==L"--state-dir"){once(state);auto path=value();if(path.empty())throw std::invalid_argument("State directory is empty.");out.stateDirectory=path;}
        else if(arg==L"--backup-id"){once(backup);auto text=value();for(wchar_t c:text) {if(c>127)throw std::invalid_argument("Invalid backup ID.");out.backupId+=static_cast<char>(c);}if(!ValidBackupId(out.backupId))throw std::invalid_argument("Invalid backup ID.");}
        else throw std::invalid_argument("Unknown option. Use --help.");
    }
    const int actions=static_cast<int>(show)+static_cast<int>(restore)+static_cast<int>(help)+static_cast<int>(feet)+static_cast<int>(meters)+static_cast<int>(cm);
    if(actions>1 || (inches && !feet) || (backup && !restore) || (delay && !(feet || meters || cm)))throw std::invalid_argument("Conflicting or incomplete options.");
    if(help)out.action=Action::Help;
    else if(restore)out.action=Action::Restore;
    else if(feet || meters || cm){out.action=Action::Apply;out.target=feet?FeetInches(ft,in):(cm?Centimeters(metric):Height(metric));}
    return out;
}
}
