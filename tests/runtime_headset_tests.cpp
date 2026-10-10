#include "../CustomHeadsetOpenVR/src/Tracking/RuntimeHeadsetSelection.h"
#include <iostream>
#include <stdexcept>
static void Check(bool ok,const char* m){if(!ok)throw std::runtime_error(m);}
int main(){try{
    dreamair::RuntimeHeadsetSelection s;
    s.Observe("Pimax Dream Air","Pimax","hmd","dream",false);
    s.Observe("Pimax Dream Air","Pimax","controller","controller",true);
    s.Observe("Vive","HTC","hmd","other-hmd",true);
    s.Observe("REF-HMD","Pimax","hmd","crystal",false);
    s.Observe("REF-HMD","Pimax","hmd","dream-ref",true);
    s.Observe("Pimax Dream Air","Pimax","hmd","",true);
    Check(s.Contains("dream")&&s.Contains("dream-ref"),"target headset not identified");
    Check(!s.Contains("controller")&&!s.Contains("other-hmd")&&!s.Contains("crystal")&&!s.Contains(""),"unrelated device suppressed");
    Check(!s.Contains("PimaxSlamCustomHMD"),"native headset suppressed");
    std::cout<<"PASS Dream Air identity, REF-HMD type guard, controller and unrelated HMD isolation\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
