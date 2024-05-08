#include "Udawa.h"

Udawa::Udawa() {
    
}

void Udawa::begin(){
    _wiFiHelper.begin("FISIP", "Fisip2019%", "UDAWA", "defaultkey", "sudarsan");
    _wiFiHelper.addOnConnectedCallback(std::bind(&Udawa::_onWiFiConnected, this));
}

void Udawa::run(){
    _wiFiHelper.run();
}

void Udawa::_onWiFiConnected(){

}
void Udawa::_onWiFiDisconnected(){

}
void Udawa::_onWiFiGotIP(){
    
}