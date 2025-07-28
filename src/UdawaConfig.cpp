#include "UdawaConfig.h"

SemaphoreHandle_t xSemaphoreConfig = NULL; 

UdawaConfig::UdawaConfig(const char* path) : _path(path){
    
}

bool UdawaConfig::begin(){
    if(xSemaphoreConfig == NULL){xSemaphoreConfig = xSemaphoreCreateMutex();}

    if( xSemaphoreConfig != NULL ){
      if( xSemaphoreTake( xSemaphoreConfig, ( TickType_t ) 5000 ) == pdTRUE )
      {
        if(!LittleFS.begin(true)){
            _logger->error(PSTR(__func__), PSTR("Problem with the LittleFS file system.\n"));
            
            xSemaphoreGive( xSemaphoreConfig );
            return false;
        }else{
            xSemaphoreGive( xSemaphoreConfig );
            return true;
        }
      }
      else
      {
        _logger->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
        return false;
      }
    }
    else{
        return false;
    }

    return true;
}

bool UdawaConfig::load(){
    if( xSemaphoreConfig != NULL ){
        if( xSemaphoreTake( xSemaphoreConfig, ( TickType_t ) 5000 ) == pdTRUE ){
            _logger->info(PSTR(__func__),PSTR("Loading %s.\n"), _path);
            File file = LittleFS.open(_path, FILE_READ);
            if(file.size() > 1)
            {
                _logger->info(PSTR(__func__),PSTR("%s size is normal: %d.\n"), _path, file.size());
            }
            else
            {
                _logger->warn(PSTR(__func__),PSTR("%s size is abnormal: %d!\n"), _path, file.size());
            }

            DynamicJsonDocument _data(JSON_DOC_SIZE_XLARGE);
            DeserializationError err = deserializeJson(_data, file);

            if(err == DeserializationError::Ok){
                _logger->debug(PSTR(__func__), PSTR("%s is valid JSON.\n"), _path);
                
                if(_data[PSTR("fInit")].is<bool>()){state.fInit = _data[PSTR("fInit")].as<bool>();}
                if(_data[PSTR("hwid")].is<const char*>()){strlcpy(state.hwid, _data[PSTR("hwid")].as<const char*>(), sizeof(state.hwid));}
                if(_data[PSTR("name")].is<const char*>()){strlcpy(state.name, _data[PSTR("name")].as<const char*>(), sizeof(state.name));}
                if(_data[PSTR("model")].is<const char*>()){strlcpy(state.model, _data[PSTR("model")].as<const char*>(), sizeof(state.model));}
                if(_data[PSTR("group")].is<const char*>()){strlcpy(state.group, _data[PSTR("group")].as<const char*>(), sizeof(state.group));}
                if(_data[PSTR("wssid")].is<const char*>()){strlcpy(state.wssid, _data[PSTR("wssid")].as<const char*>(), sizeof(state.wssid));}
                if(_data[PSTR("wpass")].is<const char*>()){strlcpy(state.wpass, _data[PSTR("wpass")].as<const char*>(), sizeof(state.wpass));}
                if(_data[PSTR("dssid")].is<const char*>()){strlcpy(state.dssid, _data[PSTR("dssid")].as<const char*>(), sizeof(state.dssid));}
                if(_data[PSTR("dpass")].is<const char*>()){strlcpy(state.dpass, _data[PSTR("dpass")].as<const char*>(), sizeof(state.dpass));}
                if(_data[PSTR("upass")].is<const char*>()){strlcpy(state.upass, _data[PSTR("upass")].as<const char*>(), sizeof(state.upass));}
                if(_data[PSTR("hname")].is<const char*>()){strlcpy(state.hname, _data[PSTR("hname")].as<const char*>(), sizeof(state.hname));}
                if(_data[PSTR("htU")].is<const char*>()){strlcpy(state.htU, _data[PSTR("htU")].as<const char*>(), sizeof(state.htU));}
                if(_data[PSTR("htP")].is<const char*>()){strlcpy(state.htP, _data[PSTR("htP")].as<const char*>(), sizeof(state.htP));}
                if(_data[PSTR("logIP")].is<const char*>()){strlcpy(state.logIP, _data[PSTR("logIP")].as<const char*>(), sizeof(state.logIP));}
                if(_data[PSTR("logLev")].is<uint8_t>()){state.logLev = _data[PSTR("logLev")].as<uint8_t>();}                
                if(_data[PSTR("fWOTA")].is<bool>()){state.fWOTA = _data[PSTR("fWOTA")].as<bool>();}
                if(_data[PSTR("fWeb")].is<bool>()){state.fWeb = _data[PSTR("fWeb")].as<bool>();}
                if(_data[PSTR("gmtOff")].is<int>()){state.gmtOff = _data[PSTR("gmtOff")].as<int>();}
                if(_data[PSTR("logPort")].is<uint16_t>()){state.logPort = _data[PSTR("logPort")].as<uint16_t>();}
                if(_data[PSTR("LEDOn")].is<bool>()){state.LEDOn = _data[PSTR("LEDOn")].as<bool>();}
                if(_data[PSTR("pinLEDR")].is<uint8_t>()){state.pinLEDR = _data[PSTR("pinLEDR")].as<uint8_t>();}
                if(_data[PSTR("pinLEDG")].is<uint8_t>()){state.pinLEDG = _data[PSTR("pinLEDG")].as<uint8_t>();}
                if(_data[PSTR("pinLEDB")].is<uint8_t>()){state.pinLEDB = _data[PSTR("pinLEDB")].as<uint8_t>();}
                if(_data[PSTR("pinBuzz")].is<uint8_t>()){state.pinBuzz = _data[PSTR("pinBuzz")].as<uint8_t>();}
                
                #ifdef USE_IOT
                if(_data[PSTR("accTkn")].is<const char*>()){strlcpy(state.accTkn, _data[PSTR("accTkn")].as<const char*>(), sizeof(state.accTkn));}
                if(_data[PSTR("provDK")].is<const char*>()){strlcpy(state.provDK, _data[PSTR("provDK")].as<const char*>(), sizeof(state.provDK));}
                if(_data[PSTR("provDS")].is<const char*>()){strlcpy(state.provDS, _data[PSTR("provDS")].as<const char*>(), sizeof(state.provDS));}
                if(_data[PSTR("tbPort")].is<uint16_t>()){state.tbPort = _data[PSTR("tbPort")].as<uint16_t>();}
                if(_data[PSTR("provSent")].is<bool>()){state.provSent = _data[PSTR("provSent")].as<bool>();}
                if(_data[PSTR("fIoT")].is<bool>()){state.fIoT = _data[PSTR("fIoT")].as<bool>();}
                if(_data[PSTR("tbAddr")].is<const char*>()){strlcpy(state.tbAddr, _data[PSTR("tbAddr")].as<const char*>(), sizeof(state.tbAddr));}
                #endif
                if(_data[PSTR("binURL")].is<const char*>()){strlcpy(state.binURL, _data[PSTR("binURL")].as<const char*>(), sizeof(state.binURL));}
            }
            else{
                _logger->error(PSTR(__func__), PSTR("%s is not valid JSON!\n"), _path);
                _logger->debug(PSTR(__func__), PSTR("Trying to load factory config from secret.h & params.h.\n"));

                char* decodedString = new char[16];
                uint64_t chipid = ESP.getEfuseMac();
                sprintf(decodedString, "%04X%08X",(uint16_t)(chipid>>32), (uint32_t)chipid);

                strlcpy(state.hwid, decodedString, sizeof(state.hwid));
                strlcpy(state.name, (String(model) + String(decodedString)).c_str(), sizeof(state.name));
                strlcpy(state.model, model, sizeof(state.model));
                strlcpy(state.group, group, sizeof(state.group));
                strlcpy(state.wssid, wssid, sizeof(state.wssid));
                strlcpy(state.wpass, wpass, sizeof(state.wpass));
                strlcpy(state.dssid, dssid, sizeof(state.dssid));
                strlcpy(state.dpass, dpass, sizeof(state.dpass));
                strlcpy(state.upass, upass, sizeof(state.upass));
                strlcpy(state.hname, hname, sizeof(state.hname));
                strlcpy(state.logIP, logIP, sizeof(state.logIP));
                strlcpy(state.htU, htU, sizeof(state.htU));
                strlcpy(state.htP, htP, sizeof(state.htP));
                state.logLev = logLev;
                state.fWOTA = fWOTA;
                state.fWeb = fWeb;
                state.gmtOff = gmtOff;
                state.logPort = logPort;
                state.fInit = fInit;
                state.LEDOn = LEDOn;
                state.pinLEDR = pinLEDR;
                state.pinLEDG = pinLEDG;
                state.pinLEDB = pinLEDB;
                state.pinBuzz = pinBuzz;

                #ifdef USE_IOT
                strlcpy(state.accTkn, accTkn, sizeof(accTkn));
                strlcpy(state.provDK, provDK, sizeof(provDK));
                strlcpy(state.provDS, provDS, sizeof(provDS));
                state.provSent = false;
                state.fIoT = fIoT;
                strlcpy(state.tbAddr, tbAddr, sizeof(tbAddr));
                state.tbPort = tbPort;
                #endif
                strlcpy(state.binURL, binURL, sizeof(binURL));
                file.close();
                xSemaphoreGive( xSemaphoreConfig );
                return false;
            }
            file.close();
            xSemaphoreGive( xSemaphoreConfig );
            return true;
        }
        else{
            _logger->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
        }
    }
    return false;
}

bool UdawaConfig::save(){
    if( xSemaphoreConfig != NULL ){
      if( xSemaphoreTake( xSemaphoreConfig, ( TickType_t ) 5000 ) == pdTRUE )
      {
        if(!LittleFS.remove(_path)){
            _logger->warn(PSTR(__func__),PSTR("Failed to delete the old file: %s\n"), _path);
        }
        
        File file = LittleFS.open(_path, FILE_WRITE);
        
        if (!file){
            _logger->error(PSTR(__func__),PSTR("Failed to open the old file: %s\n"), _path);
            file.close();
            xSemaphoreGive( xSemaphoreConfig );
            return false;
        }

        DynamicJsonDocument _data(JSON_DOC_SIZE_XLARGE);
        _data[PSTR("fInit")] = state.fInit;
        _data[PSTR("hwid")] = state.hwid;
        _data[PSTR("name")] = state.name;
        _data[PSTR("model")] = state.model;
        _data[PSTR("group")] = state.group;
        _data[PSTR("logLev")] = state.logLev;
        _data[PSTR("wssid")] = state.wssid;
        _data[PSTR("wpass")] = state.wpass;
        _data[PSTR("dssid")] = state.dssid;
        _data[PSTR("dpass")] = state.dpass;
        _data[PSTR("upass")] = state.upass;
        #ifdef USE_IOT
        _data[PSTR("accTkn")] = state.accTkn;
        _data[PSTR("provSent")] = state.provSent;
        _data[PSTR("provDK")] = state.provDK;
        _data[PSTR("provDS")] = state.provDS;
        _data[PSTR("fIoT")] = state.fIoT;
        _data[PSTR("tbAddr")] = state.tbAddr;
        _data[PSTR("tbPort")] = state.tbPort;
        #endif
        _data[PSTR("binURL")] = state.binURL;
        _data[PSTR("gmtOff")] = state.gmtOff;
        _data[PSTR("fWOTA")] = state.fWOTA;
        _data[PSTR("fWeb")] = state.fWeb;
        _data[PSTR("hname")] = state.hname;
        _data[PSTR("htU")] = state.htU;
        _data[PSTR("htP")] = state.htP;
        _data[PSTR("logIP")] = state.logIP;
        _data[PSTR("logPort")] = state.logPort;
        _data[PSTR("LEDOn")] = state.LEDOn;
        _data[PSTR("pinLEDR")] = state.pinLEDR;
        _data[PSTR("pinLEDG")] = state.pinLEDG;
        _data[PSTR("pinLEDB")] = state.pinLEDB;
        _data[PSTR("pinBuzz")] = state.pinBuzz;

        serializeJson(_data, file);

        _logger->debug(PSTR(__func__),PSTR("%s saved successfully.\n"), _path);
        file.close();        
        xSemaphoreGive( xSemaphoreConfig );
        return true;
      }
      else
      {
        _logger->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
      }
    }

    return false;
}

GenericConfig::GenericConfig(const char* path) : _path(path) {

}

bool GenericConfig::load(DynamicJsonDocument &data){
    if( xSemaphoreConfig != NULL ){
        if( xSemaphoreTake( xSemaphoreConfig, ( TickType_t ) 5000 ) == pdTRUE ){
            _logger->info(PSTR(__func__),PSTR("Loading %s.\n"), _path);
            File file = LittleFS.open(_path, FILE_READ);
            if(file.size() > 1)
            {
                _logger->info(PSTR(__func__),PSTR("%s size is normal: %d.\n"), _path, file.size());
            }
            else
            {
                _logger->warn(PSTR(__func__),PSTR("%s size is abnormal: %d!\n"), _path, file.size());
                file.close();
                xSemaphoreGive( xSemaphoreConfig );
                return false;
            }

            DeserializationError err = deserializeJson(data, file);

            if(err == DeserializationError::Ok){
                _logger->debug(PSTR(__func__), PSTR("%s is valid JSON.\n"), _path);                
            }
            
            file.close();
            xSemaphoreGive( xSemaphoreConfig );
            return true;
        }
        else{
            _logger->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
        }
    }
    return false;
}

bool GenericConfig::save(DynamicJsonDocument &data){
    if( xSemaphoreConfig != NULL ){
      if( xSemaphoreTake( xSemaphoreConfig, ( TickType_t ) 5000 ) == pdTRUE )
      {
        if(!LittleFS.remove(_path)){
            Serial.println(_path);
            _logger->warn(PSTR(__func__),PSTR("Failed to delete the old file: %s\n"), _path);
        }
        
        File file = LittleFS.open(_path, FILE_WRITE);
        
        if (!file){
            _logger->error(PSTR(__func__),PSTR("Failed to open the old file: %s\n"), _path);
            file.close();
            xSemaphoreGive( xSemaphoreConfig );
            return false;
        }

        serializeJson(data, file);

        _logger->debug(PSTR(__func__),PSTR("%s saved successfully.\n"), _path);

        file.close();        
        xSemaphoreGive( xSemaphoreConfig );
        return true;
      }
      else
      {
        _logger->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
      }
    }

    return false;
}