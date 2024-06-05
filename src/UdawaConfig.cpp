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


            JsonDocument data;
            DeserializationError err = deserializeJson(data, file);

            if(err == DeserializationError::Ok){
                _logger->debug(PSTR(__func__), PSTR("%s is valid JSON.\n"), _path);
                

                if(data[PSTR("hwid")] != nullptr){strlcpy(state.hwid, data[PSTR("hwid")].as<const char*>(), sizeof(state.hwid));}
                if(data[PSTR("name")] != nullptr){strlcpy(state.name, data[PSTR("name")].as<const char*>(), sizeof(state.name));}
                if(data[PSTR("model")] != nullptr){strlcpy(state.model, data[PSTR("model")].as<const char*>(), sizeof(state.model));}
                if(data[PSTR("group")] != nullptr){strlcpy(state.group, data[PSTR("group")].as<const char*>(), sizeof(state.group));}
                if(data[PSTR("wssid")] != nullptr){strlcpy(state.wssid, data[PSTR("wssid")].as<const char*>(), sizeof(state.wssid));}
                if(data[PSTR("wpass")] != nullptr){strlcpy(state.wpass, data[PSTR("wpass")].as<const char*>(), sizeof(state.wpass));}
                if(data[PSTR("dssid")] != nullptr){strlcpy(state.dssid, data[PSTR("dssid")].as<const char*>(), sizeof(state.dssid));}
                if(data[PSTR("dpass")] != nullptr){strlcpy(state.dpass, data[PSTR("dpass")].as<const char*>(), sizeof(state.dpass));}
                if(data[PSTR("upass")] != nullptr){strlcpy(state.upass, data[PSTR("upass")].as<const char*>(), sizeof(state.upass));}
                if(data[PSTR("hname")] != nullptr){strlcpy(state.hname, data[PSTR("hname")].as<const char*>(), sizeof(state.hname));}
                if(data[PSTR("htU")] != nullptr){strlcpy(state.htU, data[PSTR("htU")].as<const char*>(), sizeof(state.htU));}
                if(data[PSTR("htP")] != nullptr){strlcpy(state.htP, data[PSTR("htP")].as<const char*>(), sizeof(state.htP));}
                if(data[PSTR("logIP")] != nullptr){strlcpy(state.logIP, data[PSTR("logIP")].as<const char*>(), sizeof(state.logIP));}
                if(data[PSTR("htU")] != nullptr){strlcpy(state.htU, data[PSTR("htU")].as<const char*>(), sizeof(state.htU));}
                if(data[PSTR("htP")] != nullptr){strlcpy(state.htP, data[PSTR("htP")].as<const char*>(), sizeof(state.htP));}
                if(data[PSTR("logLev")] != nullptr){state.logLev = data[PSTR("logLev")].as<uint8_t>();}                
                if(data[PSTR("fWOTA")] != nullptr){state.fWOTA = data[PSTR("fWOTA")].as<bool>();}
                if(data[PSTR("fWeb")] != nullptr){state.fWeb = data[PSTR("fWeb")].as<bool>();}
                if(data[PSTR("gmtOff")] != nullptr){state.gmtOff = data[PSTR("gmtOff")].as<int>();}
                if(data[PSTR("logPort")] != nullptr){state.logPort = data[PSTR("logPort")].as<uint16_t>();}
                
                #ifdef USE_IOT
                if(data[PSTR("accTkn")] != nullptr){strlcpy(state.accTkn, data[PSTR("accTkn")].as<const char*>(), sizeof(state.accTkn));}
                if(data[PSTR("provDK")] != nullptr){strlcpy(state.provDK, data[PSTR("provDK")].as<const char*>(), sizeof(state.provDK));}
                if(data[PSTR("provDS")] != nullptr){strlcpy(state.provDS, data[PSTR("provDS")].as<const char*>(), sizeof(state.provDS));}
                if(data[PSTR("tbPort")] != nullptr){state.tbPort = data[PSTR("tbPort")].as<uint16_t>();}
                if(data[PSTR("provSent")] != nullptr){state.provSent = data[PSTR("provSent")].as<bool>();}
                if(data[PSTR("fIoT")] != nullptr){state.fIoT = data[PSTR("fIoT")].as<bool>();}
                if(data[PSTR("tbAddr")] != nullptr){strlcpy(state.tbAddr, data[PSTR("tbAddr")].as<const char*>(), sizeof(state.tbAddr));}
                #endif
            }
            else{
                _logger->error(PSTR(__func__), PSTR("%s is not valid JSON!\n"), _path);
                _logger->debug(PSTR(__func__), PSTR("Trying to load factory config from secret.h & params.h.\n"));

                char* decodedString = new char[16];
                uint64_t chipid = ESP.getEfuseMac();
                sprintf(decodedString, "%04X%08X",(uint16_t)(chipid>>32), (uint32_t)chipid);

                strlcpy(state.hwid, decodedString, sizeof(state.hwid));
                strlcpy(state.name, (String("UDAWA") + String(decodedString)).c_str(), sizeof(state.name));
                strlcpy(state.model, model, sizeof(state.model));
                strlcpy(state.group, group, sizeof(state.group));
                strlcpy(state.wssid, wssid, sizeof(state.wssid));
                strlcpy(state.wpass, wpass, sizeof(state.wpass));
                strlcpy(state.dssid, dssid, sizeof(state.dssid));
                strlcpy(state.dpass, dpass, sizeof(state.dpass));
                strlcpy(state.upass, upass, sizeof(state.upass));
                strlcpy(state.hname, (String(model) + String(decodedString)).c_str(), sizeof(state.hname));
                strlcpy(state.logIP, logIP, sizeof(state.logIP));
                strlcpy(state.htU, htU, sizeof(state.htU));
                strlcpy(state.htP, htP, sizeof(state.htP));
                state.logLev = logLev;
                state.fWOTA = fWOTA;
                state.fWeb = fWeb;
                state.gmtOff = gmtOff;
                state.logPort = logPort;

                #ifdef USE_IOT
                strlcpy(state.accTkn, accTkn, sizeof(state.accTkn));
                strlcpy(state.provDK, provDK, sizeof(state.provDK));
                strlcpy(state.provDS, provDS, sizeof(state.provDS));
                state.provSent = false;
                state.fIoT = fIoT;
                strlcpy(state.tbAddr, tbAddr, sizeof(state.tbAddr));
                state.tbPort = tbPort;
                #endif
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
            _logger->warn(PSTR(__func__),PSTR("Failed to delete the old configFile: %s\n"), _path);
        }
        
        File file = LittleFS.open(_path, FILE_WRITE);
        
        if (!file){
            _logger->error(PSTR(__func__),PSTR("Failed to open the old configFile: %s\n"), _path);
            file.close();
            xSemaphoreGive( xSemaphoreConfig );
            return false;
        }

        JsonDocument data;

        data[PSTR("hwid")] = state.hwid;
        data[PSTR("name")] = state.name;
        data[PSTR("model")] = state.model;
        data[PSTR("group")] = state.group;
        data[PSTR("logLev")] = state.logLev;
        data[PSTR("wssid")] = state.wssid;
        data[PSTR("wpass")] = state.wpass;
        data[PSTR("dssid")] = state.dssid;
        data[PSTR("dpass")] = state.dpass;
        data[PSTR("upass")] = state.upass;
        #ifdef USE_IOT
        data[PSTR("accTkn")] = state.accTkn;
        data[PSTR("provSent")] = state.provSent;
        data[PSTR("provDK")] = state.provDK;
        data[PSTR("provDS")] = state.provDS;
        data[PSTR("fIoT")] = state.fIoT;
        data[PSTR("tbAddr")] = state.tbAddr;
        data[PSTR("tbPort")] = state.tbPort;
        #endif
        data[PSTR("gmtOff")] = state.gmtOff;
        data[PSTR("fWOTA")] = state.fWOTA;
        data[PSTR("fWeb")] = state.fWeb;
        data[PSTR("hname")] = state.hname;
        data[PSTR("htU")] = state.htU;
        data[PSTR("htP")] = state.htP;
        data[PSTR("logIP")] = state.logIP;
        data[PSTR("logPort")] = state.logPort;

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

GenericConfig::GenericConfig(const char* path) : _path(path) {

}

bool GenericConfig::load(JsonDocument &data){
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

bool GenericConfig::save(JsonDocument &data){
    if( xSemaphoreConfig != NULL ){
      if( xSemaphoreTake( xSemaphoreConfig, ( TickType_t ) 5000 ) == pdTRUE )
      {
        if(!LittleFS.remove(_path)){
            Serial.println(_path);
            _logger->warn(PSTR(__func__),PSTR("Failed to delete the old configFile: %s\n"), _path);
        }
        
        File file = LittleFS.open(_path, FILE_WRITE);
        
        if (!file){
            _logger->error(PSTR(__func__),PSTR("Failed to open the old configFile: %s\n"), _path);
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