#include "UdawaConfig.h"


UdawaGenericConfig::UdawaGenericConfig(){
    char* deviceId = new char[16];
    uint64_t chipid = ESP.getEfuseMac();
    sprintf(deviceId, "%04X%08X",(uint16_t)(chipid>>32), (uint32_t)chipid);
    
    strlcpy(_genericConfig.hwid, deviceId, sizeof(_genericConfig.hwid));
    strlcpy(_genericConfig.model, model, sizeof(_genericConfig.model));
    strlcpy(_genericConfig.group, "UDAWA", sizeof(_genericConfig.group));
    String name = String(model) + String(deviceId);
    strlcpy(_genericConfig.name, name.c_str(), sizeof(_genericConfig.name));
    strlcpy(_genericConfig.tbAddr, tbAddr, sizeof(_genericConfig.tbAddr));
    strlcpy(_genericConfig.wssid, wssid, sizeof(_genericConfig.wssid));
    strlcpy(_genericConfig.wpass, wpass, sizeof(_genericConfig.wpass));
    strlcpy(_genericConfig.dssid, dssid, sizeof(_genericConfig.dssid));
    strlcpy(_genericConfig.dpass, dpass, sizeof(_genericConfig.dpass));
    strlcpy(_genericConfig.upass, upass, sizeof(_genericConfig.upass));
    strlcpy(_genericConfig.accTkn, accTkn, sizeof(_genericConfig.accTkn));
    _genericConfig.provSent = false;
    _genericConfig.tbPort = tbPort;
    strlcpy(_genericConfig.provDK, provDK, sizeof(_genericConfig.provDK));
    strlcpy(_genericConfig.provDS, provDS, sizeof(_genericConfig.provDS));
    _genericConfig.logLev = 5;
    _genericConfig.gmtOff = 28800;

    _genericConfig.fIoT = 1;
    _genericConfig.fWOTA = 1;
    _genericConfig.fWeb = 1;

    strlcpy(_genericConfig.hname, _genericConfig.name, sizeof(_genericConfig.hname));
    strlcpy(_genericConfig.htU, "UDAWA", sizeof(_genericConfig.htU));
    strlcpy(_genericConfig.htP, "defaultkey", sizeof(_genericConfig.htP));
    strlcpy(_genericConfig.logIP, "255.255.255.255", sizeof(_genericConfig.logIP));
    _genericConfig.logPrt = 29514;
}