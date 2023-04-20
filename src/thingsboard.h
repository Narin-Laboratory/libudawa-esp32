/*
  ThingsBoard.h - Library API for sending data to the ThingsBoard
  Based on PubSub MQTT library.
  Created by Olender M. Oct 2018.
  Modified by Narin Laboratory. Nov 2021.
  Released into the public domain.
*/
#ifndef thingsboard_h
#define thingsboard_h

#include <Update.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "ArduinoJson/Polyfills/type_traits.hpp"
#include <logging.h>

#define Default_Payload 1500
#define Default_Fields_Amt 64

// Telemetry record class, allows to store different data using common interface.
class Telemetry {
    template<size_t PayloadSize, size_t MaxFieldsAmt, typename Logger>
    friend class ThingsBoardSized;

  public:
    inline Telemetry()
      : m_type(TYPE_NONE), m_key(NULL), m_value() { }

    // Constructs telemetry record from integer value.
    // EnableIf trick is required to overcome ambiguous float/integer conversion
    inline Telemetry(const char *key, int val)
      : m_type(TYPE_INT), m_key(key), m_value()   {
      m_value.integer = val;
    }

    // Constructs telemetry record from boolean value.
    inline Telemetry(const char *key, bool val)
      : m_type(TYPE_BOOL), m_key(key), m_value()  {
      m_value.boolean = val;
    }

    // Constructs telemetry record from float value.
    inline Telemetry(const char *key, float val)
      : m_type(TYPE_REAL), m_key(key), m_value()  {
      m_value.real = val;
    }

    // Constructs telemetry record from string value.
    inline Telemetry(const char *key, const char *val)
      : m_type(TYPE_STR), m_key(key), m_value()   {
      m_value.str = val;
    }

  private:
    // Data container
    union data {
        const char  *str;
        bool        boolean;
        int         integer;
        float       real;
      };

    // Data type inside a container
    enum dataType {
      TYPE_NONE,
      TYPE_BOOL,
      TYPE_INT,
      TYPE_REAL,
      TYPE_STR,
    };

    dataType     m_type;  // Data type flag
    const char   *m_key;  // Data key
    data         m_value; // Data value

    // Serializes key-value pair in a generic way.
    bool serializeKeyval(JsonVariant &jsonObj) const;
};

// Convenient aliases

using Attribute = Telemetry;
using callbackResponse = Telemetry;
// JSON object is used to communicate RPC parameters to the client
using callbackData = JsonObject;
using Shared_Attribute_Data = JsonObject;
using Provision_Data = JsonObject;

// Generic Callback wrapper
class GenericCallback {
    template <size_t PayloadSize, size_t MaxFieldsAmt, typename Logger>
    friend class ThingsBoardSized;
  public:

    // Generic Callback signature
    using processFn = callbackResponse (*)(const callbackData &data);

    // Constructs empty callback
    inline GenericCallback()
      : m_name(), m_cb(NULL)                {  }

    // Constructs callback that will be fired upon a RPC request arrival with
    // given method name
    inline GenericCallback(const char *methodName, processFn cb)
      : m_name(methodName), m_cb(cb)        {  }

  private:
    const char  *m_name;    // Method name
    processFn   m_cb;       // Callback to call
};


// ThingsBoardSized client class
template<size_t PayloadSize = Default_Payload,
         size_t MaxFieldsAmt = Default_Fields_Amt,
         typename Logger = LogManager>
class ThingsBoardSized
{
    Logger *log_manager = Logger::GetInstance(LogLevel::VERBOSE);
    bool provision_mode = false;

  public:
    // Initializes ThingsBoardSized class with network client.
    inline ThingsBoardSized(Client &client)
      : m_client(client)
      , m_requestId(0)
      , m_fwVersion("")
      , m_fwTitle("")
      , m_fwChecksum("")
      , m_fwChecksumAlgorithm("")
      , m_fwState("")
      , m_fwSize(0)
      , m_fwChunkReceive(-1)
    { }

    // Destroys ThingsBoardSized class with network client.
    inline ~ThingsBoardSized() { }

    bool beginPublish(const char* topic, unsigned int plength, boolean retained){
      return m_client.beginPublish(topic, plength, retained);
    }
    // Finish off this publish message (started with beginPublish)
    // Returns 1 if the packet was sent successfully, 0 if there was an error
    int endPublish(){
      return m_client.endPublish();
    }
    // Write a single byte of payload (only to be used with beginPublish/endPublish)
    virtual size_t write(uint8_t data){
      return m_client.write(data);
    }

    virtual size_t write(const uint8_t *buffer, size_t size){
      return m_client.write(buffer, size);
    }

    bool setBufferSize(uint16_t size)
    {
      return m_client.setBufferSize(size);
    }
    uint16_t getBufferSize()
    {
      return m_client.getBufferSize();
    }
    uint16_t setKeepAlive(uint16_t keepAlive){
      m_client.setKeepAlive(keepAlive);
      return keepAlive;
    }

    uint16_t setSocketTimeout(uint16_t socketTimeout){
      m_client.setSocketTimeout(socketTimeout);
      return socketTimeout;
    }

    // Connects to the specified ThingsBoard server and port.
    // Access token is used to authenticate a client.
    // Returns true on success, false otherwise.
    bool connect(const char *host, const char *access_token = "provision", int port = 1883, const char *client_id = "TbDev", const char *password = NULL) {
      if (!host) {
        return false;
      }
      this->callbackUnsubscribe(); // Cleanup all RPC subscriptions
      if (!strcmp(access_token, "provision")) {
        provision_mode = true;
      }
      m_client.setServer(host, port);
      bool connection_result = m_client.connect(client_id, access_token, password);
      return connection_result;
    }

    // Disconnects from ThingsBoard. Returns true on success.
    inline void disconnect() {
      m_client.disconnect();
    }

    // Returns true if connected, false otherwise.
    inline bool connected() {
      return m_client.connected();
    }

    // Executes an event loop for PubSub client.
    inline void loop() {
      m_client.loop();
    }

    //----------------------------------------------------------------------------
    // Claiming API
    bool sendClaimingRequest(const char *secretKey, unsigned int durationMs) {
      StaticJsonDocument<JSON_OBJECT_SIZE(1)> requestBuffer;
      JsonObject resp_obj = requestBuffer.to<JsonObject>();

      resp_obj["secretKey"] = secretKey;
      resp_obj["durationMs"] = durationMs;

      uint8_t objectSize = measureJson(requestBuffer) + 1;
      char responsePayload[objectSize];
      serializeJson(resp_obj, responsePayload, objectSize);

      return m_client.publish("v1/devices/me/claim", responsePayload);
    }

    // Provisioning API
    bool sendProvisionRequest(const char* deviceName, const char* provisionDeviceKey, const char* provisionDeviceSecret) {
      // TODO Add ability to provide specific credentials from client side.
      StaticJsonDocument<JSON_OBJECT_SIZE(3)> requestBuffer;
      JsonObject requestObject = requestBuffer.to<JsonObject>();

      requestObject["deviceName"] = deviceName;
      requestObject["provisionDeviceKey"] = provisionDeviceKey;
      requestObject["provisionDeviceSecret"] = provisionDeviceSecret;

      uint8_t objectSize = measureJson(requestBuffer) + 1;
      char requestPayload[objectSize];
      serializeJson(requestObject, requestPayload, objectSize);

      log_manager->info(PSTR(__func__), "Provision request: %s\n", requestPayload);
      return m_client.publish("/provision/request", requestPayload);
    }
    //----------------------------------------------------------------------------
    // Telemetry API

    // Sends telemetry data to the ThingsBoard, returns true on success.
    // Sends custom JSON telemetry string to the ThingsBoard.
    inline bool sendTelemetryJson(const char *json) {
      return m_client.publish("v1/devices/me/telemetry", json);
    }

    inline bool sendTelemetryDoc(StaticJsonDocument<PayloadSize> &doc) {
      char jsonBuffer[PayloadSize];
      serializeJson(doc, jsonBuffer);
      return m_client.publish("v1/devices/me/telemetry", jsonBuffer);
    }

    //----------------------------------------------------------------------------
    // Attribute API

    // Sends an attribute with given name and value.
    // Sends custom JSON with attributes to the ThingsBoard.
    inline bool sendAttributeJSON(const char *json) {
      return m_client.publish("v1/devices/me/attributes", json);
    }

    // Sends custom JSON with attributes to the ThingsBoard.
    inline bool sendAttributeDoc(StaticJsonDocument<PayloadSize> &doc) {
      char jsonBuffer[PayloadSize];
      serializeJson(doc, jsonBuffer);
      return m_client.publish("v1/devices/me/attributes", jsonBuffer);
    }

    // Subscribes multiple Generic Callbacks with given size
    bool callbackSubscribe(const GenericCallback *callbacks, size_t callbacksSize)
    {
      if (callbacksSize > sizeof(m_genericCallbacks) / sizeof(*m_genericCallbacks)){return false;}
      if (ThingsBoardSized::m_subscribedInstance){return false;}
      if (!m_client.subscribe("/provision/response")){return false;}
      if (!m_client.subscribe("v1/devices/me/rpc/request/+")){return false;}
      if (!m_client.subscribe("v1/devices/me/attributes/response/+")){return false;}
      if (!m_client.subscribe("v1/devices/me/attributes")){return false;}
      if (!m_client.subscribe("v2/fw/response/#")){return false;}

      ThingsBoardSized::m_subscribedInstance = this;
      for (size_t i = 0; i < callbacksSize; ++i) {
        m_genericCallbacks[i] = callbacks[i];
      }

      m_client.setCallback(ThingsBoardSized::on_message);

      return true;
    }

    inline bool callbackUnsubscribe()
    {
      bool flag1 = m_client.unsubscribe("v1/devices/me/rpc/request/+\n");
      bool flag2 = m_client.unsubscribe("v1/devices/me/attributes/response/+\n");
      bool flag3 = m_client.unsubscribe("v1/devices/me/attributes\n");
      bool flag4 = m_client.unsubscribe("v2/fw/response/#\n");
      bool flag5 = m_client.unsubscribe("/provision/response\n");
      ThingsBoardSized::m_subscribedInstance = NULL;
      return (flag1 && flag2 && flag3 && flag4 && flag5);
    }

    //----------------------------------------------------------------------------
    // Firmware OTA API
    bool Firmware_Update(const char* currFwTitle, const char* currFwVersion) {
      m_fwState.clear();
      m_fwTitle.clear();
      m_fwVersion.clear();

      // Send current firmware version
      if (!Firmware_Send_FW_Info(currFwTitle, currFwVersion)) {
        return false;
      }

      // Update state
      Firmware_Send_State("CHECKING FIRMWARE\n");

      // Request the firmware informations
      if (!Shared_Attributes_Request("fw_checksum,fw_checksum_algorithm,fw_size,fw_title,fw_version")) {
        return false;
      }

      // Wait receive m_fwVersion and m_fwTitle
      unsigned long timeout = millis() + 3000;
      do {
        delay(5);
        loop();
      } while (m_fwVersion.isEmpty() && m_fwTitle.isEmpty() && (timeout >= millis()));

      // Check if firmware is available for our device
      if (m_fwVersion.isEmpty() || m_fwTitle.isEmpty()) {
        log_manager->info(PSTR(__func__), "No firmware found !\n");
        Firmware_Send_State("NO FIRMWARE FOUND\n");
        return false;
      }

      // If firmware is the same, we do not update it
      if ((String(currFwTitle) == m_fwTitle) and (String(currFwVersion) == m_fwVersion)) {
        log_manager->info(PSTR(__func__), "Firmware is already up to date !\n");
        Firmware_Send_State("UP TO DATE\n");
        return false;
      }

      // If firmware title is not the same, we quit now
      if (String(currFwTitle) != m_fwTitle) {
        log_manager->warn(PSTR(__func__), "Firmware is not for us (title is different) !\n");
        Firmware_Send_State("NO FIRMWARE FOUND\n");
        return false;
      }

      if (m_fwChecksumAlgorithm != "MD5") {
        log_manager->warn(PSTR(__func__), "Checksum algorithm is not supported, please use MD5 only\n");
        Firmware_Send_State("CHKS IS NOT MD5\n");
        return false;
      }

      log_manager->info(PSTR(__func__), "=================================\n");
      log_manager->info(PSTR(__func__), "A new Firmware is available : %s\n", String(String(currFwVersion) + " => " + m_fwVersion).c_str());
      log_manager->info(PSTR(__func__), "Try to download it...\n");

      int chunkSize = 4096;   // maybe less if we don't have enough RAM
      int numberOfChunk = int(m_fwSize / chunkSize) + 1;
      int currChunk = 0;
      int nbRetry = 3;

      // Increase size of receive buffer
      if (!m_client.setBufferSize(chunkSize + 50)) {
        log_manager->warn(PSTR(__func__), "Not enough RAM\n");
        return false;
      }

      // Update state
      Firmware_Send_State("DOWNLOADING\n");

      // Download the firmware
      do {
        m_client.publish(String("v2/fw/request/0/chunk/" + String(currChunk)).c_str(), String(chunkSize).c_str());

        timeout = millis() + 3000;
        do {
          delay(5);
          loop();
        } while ((m_fwChunkReceive != currChunk) && (timeout >= millis()));

        if (m_fwChunkReceive == currChunk) {
          // Check if chunk is not the last
          if (numberOfChunk != (currChunk + 1))
          {
            // Check if state is OK
            if ((m_fwState == "DOWNLOADING")) {
              currChunk++;
            }
            else {
              nbRetry--;
              if (nbRetry == 0) {
                log_manager->warn(PSTR(__func__), "Unable to write firmware\n");
                return false;
              }
            }
          }
          // The last chunk
          else {
            currChunk++;
          }
        }

        // Timeout
        else {
          nbRetry--;
          if (nbRetry == 0) {
            log_manager->warn(PSTR(__func__), "Unable to download firmware\n");
            return false;
          }
        }

      } while (numberOfChunk != currChunk);

      // Update state
      Firmware_Send_State(m_fwState.c_str());

      return m_fwState == "SUCCESS" ? true : false;
    }

    bool Firmware_Send_FW_Info(const char* currFwTitle, const char* currFwVersion) {
      // Send our firmware title and version
      StaticJsonDocument<JSON_OBJECT_SIZE(2)> currentFirmwareInfo;
      JsonObject currentFirmwareInfoObject = currentFirmwareInfo.to<JsonObject>();

      currentFirmwareInfoObject["current_fw_title"] = currFwTitle;
      currentFirmwareInfoObject["current_fw_version"] = currFwVersion;

      int objectSize = measureJson(currentFirmwareInfo) + 1;
      char buffer[objectSize];
      serializeJson(currentFirmwareInfoObject, buffer, objectSize);

      return sendTelemetryJson(buffer);
    }

    bool Firmware_Send_State(const char* currFwState) {
      // Send our firmware title and version
      StaticJsonDocument<JSON_OBJECT_SIZE(1)> currentFirmwareState;
      JsonObject currentFirmwareStateObject = currentFirmwareState.to<JsonObject>();

      currentFirmwareStateObject["current_fw_state"] = currFwState;

      int objectSize = measureJson(currentFirmwareState) + 1;
      char buffer[objectSize];
      serializeJson(currentFirmwareStateObject, buffer, objectSize);

      return sendTelemetryJson(buffer);
    }

    //----------------------------------------------------------------------------
    // Custom function
    bool server_rpc_call(JsonObject &payload) {
      String buffer;
      serializeJson(payload, buffer);

      m_requestId++;

      log_manager->debug(PSTR(__func__), PSTR("Sending RPC to server: %s, msgId: %d\n"), buffer.c_str(), m_requestId);

      return m_client.publish(String("v1/devices/me/rpc/request/" + String(m_requestId)).c_str(), buffer.c_str());
    }

    //----------------------------------------------------------------------------
    // Shared attributes API

    bool Shared_Attributes_Request(const char* attributes) {
      StaticJsonDocument<JSON_OBJECT_SIZE(1)> requestBuffer;
      JsonObject requestObject = requestBuffer.to<JsonObject>();

      requestObject["sharedKeys"] = attributes;

      int objectSize = measureJson(requestBuffer) + 1;
      char buffer[objectSize];
      serializeJson(requestObject, buffer, objectSize);

      m_requestId++;

      return m_client.publish(String("v1/devices/me/attributes/request/" + String(m_requestId)).c_str(), buffer);
    }
    // -------------------------------------------------------------------------------
    // Provisioning API

  private:
    // Sends single key-value in a generic way.
    template<typename T>
    bool sendKeyval(const char *key, T value, bool telemetry = true) {
      Telemetry t(key, value);

      char payload[PayloadSize];
      {
        Telemetry t(key, value);
        StaticJsonDocument<JSON_OBJECT_SIZE(1)>jsonBuffer;
        JsonVariant object = jsonBuffer.template to<JsonVariant>();
        if (t.serializeKeyval(object) == false) {
          log_manager->warn(PSTR(__func__), "unable to serialize data\n");
          return false;
        }

        if (measureJson(jsonBuffer) > PayloadSize - 1) {
          log_manager->warn(PSTR(__func__), "too small buffer for JSON data\n");
          return false;
        }
        serializeJson(object, payload, sizeof(payload));
      }
      return telemetry ? sendTelemetryJson(payload) : sendAttributeJSON(payload);
    }

    // Processes RPC message
    void process_rpc_message(char* topic, uint8_t* payload, unsigned int length) {
      log_manager->debug(PSTR(__func__), "Incomming topic: %s\n", topic);
      callbackResponse r;
      {
        StaticJsonDocument<JSON_OBJECT_SIZE(MaxFieldsAmt)> jsonBuffer;
        DeserializationError error = deserializeJson(jsonBuffer, payload, length);
        if (error) {
          log_manager->warn(PSTR(__func__), "unable to de-serialize RPC\n");
          return;
        }
        const JsonObject &data = jsonBuffer.template as<JsonObject>();
        const char *methodName = data["method"];
        const char *params = data["params"];

        if (methodName) {
          log_manager->info(PSTR(__func__), "received RPC: %s\n", methodName);
        } else {
          log_manager->warn(PSTR(__func__), "RPC method is NULL\n");
          return;
        }

        for (size_t i = 0; i < sizeof(m_genericCallbacks) / sizeof(*m_genericCallbacks); ++i) {
          if (m_genericCallbacks[i].m_cb && !strcmp(m_genericCallbacks[i].m_name, methodName)) {

            log_manager->info(PSTR(__func__), "calling RPC: %s\n", m_genericCallbacks[i].m_name);

            // Do not inform client, if parameter field is missing for some reason
            if (!data.containsKey("params")) {
              log_manager->warn(PSTR(__func__), "no parameters passed with RPC, passing null JSON\n");
            }

            // try to de-serialize params
            StaticJsonDocument<JSON_OBJECT_SIZE(MaxFieldsAmt)> doc;
            DeserializationError err_param = deserializeJson(doc, params);
            //if failed to de-serialize params then send JsonObject instead
            if (err_param) {
              log_manager->warn(PSTR(__func__), "params: %s\n", data["params"].as<String>().c_str());
              r = m_genericCallbacks[i].m_cb(data);
            } else {
              log_manager->info(PSTR(__func__), "JsonObject params: %s\n", params);
              const JsonObject &param = doc.template as<JsonObject>();
              // Getting non-existing field from JSON should automatically
              // set JSONVariant to null
              r = m_genericCallbacks[i].m_cb(param);
            }
            break;
          }
        }

      }
      // Fill in response
      char responsePayload[PayloadSize] = {0};
      StaticJsonDocument<JSON_OBJECT_SIZE(1)> respBuffer;
      JsonVariant resp_obj = respBuffer.template to<JsonVariant>();

      if (r.serializeKeyval(resp_obj) == false) {
        log_manager->warn(PSTR(__func__), "unable to serialize data\n");
        return;
      }

      if (measureJson(respBuffer) > PayloadSize - 1) {
        log_manager->warn(PSTR(__func__), "too small buffer for JSON data\n");
        return;
      }
      serializeJson(resp_obj, responsePayload, sizeof(responsePayload));

      String responseTopic = String(topic);
      responseTopic.replace("request", "response");
      log_manager->info(PSTR(__func__), "response: %s to %s \n", responsePayload, responseTopic.c_str());
      m_client.publish(responseTopic.c_str(), responsePayload);
    }

    // Processes firmware response
    void process_firmware_response(char* topic, uint8_t* payload, unsigned int length) {
      static unsigned int sizeReceive = 0;
      static MD5Builder md5;

      m_fwChunkReceive = atoi(strrchr(topic, '/') + 1);
      log_manager->verbose(PSTR(__func__), "Received chuch %d, size %d bytes.\n", m_fwChunkReceive, length);

      m_fwState = "DOWNLOADING";

      if (m_fwChunkReceive == 0) {
        sizeReceive = 0;
        md5 = MD5Builder();
        md5.begin();

        if(Update.isRunning()){Update.abort();}
        // Initialize Flash
        if (!Update.begin(m_fwSize)) {
          log_manager->warn(PSTR(__func__), "Error during Update.begin\n");
          m_fwState = "UPDATE ERROR";
          return;
        }
      }

      // Write data to Flash
      if (Update.write(payload, length) != length) {
        log_manager->warn(PSTR(__func__), "Error during Update.write\n");
        m_fwState = "UPDATE ERROR";
        return;
      }

      // Update value only if write flash success
      md5.add(payload, length);
      sizeReceive += length;

      // Receive Full Firmware
      if (m_fwSize == sizeReceive) {
        md5.calculate();
        String md5Str = md5.toString();
        log_manager->info(PSTR(__func__), "md5 compute: %s\n", md5Str);
        log_manager->info(PSTR(__func__), "md5 compute: %s\n", m_fwChecksum);
        // Check MD5
        if (md5Str != m_fwChecksum) {
          log_manager->warn(PSTR(__func__), "Checksum verification failed !\n");
          Update.abort();
          m_fwState = "CHECKSUM ERROR";
        }
        else {
          log_manager->info(PSTR(__func__), "Checksum is OK !\n");
          if (Update.end(true)) {
            log_manager->info(PSTR(__func__), "Update Success !\n");
            m_fwState = "SUCCESS";
          }
          else {
            log_manager->warn(PSTR(__func__), "Update Fail !\n");
            m_fwState = "FAILED";
          }
        }
      }
    }

    // Processes shared attribute update message
    void process_shared_attribute_update_message(char* topic, uint8_t* payload, unsigned int length) {
      StaticJsonDocument<JSON_OBJECT_SIZE(MaxFieldsAmt)> jsonBuffer;
      DeserializationError error = deserializeJson(jsonBuffer, payload, length);
      if (error) {
        log_manager->warn(PSTR(__func__), "Unable to de-serialize Shared attribute update request\n");
        return;
      }
      JsonObject data = jsonBuffer.template as<JsonObject>();

      if (data && (data.size() >= 1)) {
        log_manager->info(PSTR(__func__), "Received shared attribute update request\n");
        if (data["shared"]) {
          data = data["shared"];
        }
      } else {
        log_manager->warn(PSTR(__func__), "Shared attribute update key not found.\n");
        return;
      }

      // Save data for firmware update
      if (data["fw_title"])
        m_fwTitle = data["fw_title"].as<String>();

      if (data["fw_version"])
        m_fwVersion = data["fw_version"].as<String>();

      if (data["fw_checksum"])
        m_fwChecksum = data["fw_checksum"].as<String>();

      if (data["fw_checksum_algorithm"])
        m_fwChecksumAlgorithm = data["fw_checksum_algorithm"].as<String>();

      if (data["fw_size"])
        m_fwSize = data["fw_size"].as<int>();

      if(m_genericCallbacks[0].m_cb)
      {
        log_manager->info(PSTR(__func__), "Calling callbacks for updated attribute: %s\n", m_genericCallbacks[0].m_name);
        m_genericCallbacks[0].m_cb(data);
      }
    }

    // Processes provisioning response
    void process_provisioning_response(char* topic, uint8_t* payload, unsigned int length) {
      log_manager->info(PSTR(__func__), "Process provisioning response\n");

      StaticJsonDocument<JSON_OBJECT_SIZE(MaxFieldsAmt)> jsonBuffer;
      DeserializationError error = deserializeJson(jsonBuffer, payload, length);
      if (error) {
        log_manager->warn(PSTR(__func__), "Unable to de-serialize provision response\n");
        return;
      }

      const JsonObject &data = jsonBuffer.template as<JsonObject>();

      log_manager->info(PSTR(__func__), "Received provision response\n");

      if (data["status"] == "SUCCESS" && data["credentialsType"] == "X509_CERTIFICATE") {
        log_manager->warn(PSTR(__func__), "Provision response contains X509_CERTIFICATE credentials, it is not supported yet.\n");
        return;
      }

      if (m_genericCallbacks[1].m_cb) {
        log_manager->info(PSTR(__func__), "Calling callbacks for provisioning response: %s\n", m_genericCallbacks[0].m_name);
        m_genericCallbacks[1].m_cb(data);
      }
    }

    // Sends array of attributes or telemetry to ThingsBoard
    bool sendDataArray(const Telemetry *data, size_t data_count, bool telemetry = true) {
      if (MaxFieldsAmt < data_count) {
        log_manager->warn(PSTR(__func__), "too much JSON fields passed\n");
        return false;
      }
      char payload[PayloadSize];
      {
        StaticJsonDocument<JSON_OBJECT_SIZE(MaxFieldsAmt)> jsonBuffer;
        JsonVariant object = jsonBuffer.template to<JsonVariant>();

        for (size_t i = 0; i < data_count; ++i) {
          if (data[i].serializeKeyval(object) == false) {
            log_manager->warn(PSTR(__func__), "unable to serialize data\n");
            return false;
          }
        }
        if (measureJson(jsonBuffer) > PayloadSize - 1) {
          log_manager->warn(PSTR(__func__), "too small buffer for JSON data\n");
          return false;
        }
        serializeJson(object, payload, sizeof(payload));
      }

      return telemetry ? sendTelemetryJson(payload) : sendAttributeJSON(payload);
    }

    PubSubClient m_client;              // PubSub MQTT client instance.
    GenericCallback m_genericCallbacks[20];     // Generic Callbacks array
    unsigned int m_requestId;

    // For Firmware Update
    String m_fwVersion, m_fwTitle, m_fwChecksum, m_fwChecksumAlgorithm, m_fwState;
    unsigned int m_fwSize;
    int m_fwChunkReceive;

    // PubSub client cannot call a method when message arrives on subscribed topic.
    // Only free-standing function is allowed.
    // To be able to forward event to an instance, rather than to a function, this pointer exists.
    static ThingsBoardSized *m_subscribedInstance;

    // The callback for when a PUBLISH message is received from the server.
    static void on_message(char* topic, uint8_t* payload, unsigned int length)
    {
        Logger *log_manager = Logger::GetInstance(LogLevel::VERBOSE);
        log_manager->info(PSTR(__func__), "Callback from topic: %s\n", topic);
        if (!ThingsBoardSized::m_subscribedInstance){return;}

        if (strncmp("v1/devices/me/rpc", topic, strlen("v1/devices/me/rpc")) == 0)
        {
            ThingsBoardSized::m_subscribedInstance->process_rpc_message(topic, payload, length);
        }
        else if (strncmp("v1/devices/me/attributes", topic, strlen("v1/devices/me/attributes")) == 0)
        {
            ThingsBoardSized::m_subscribedInstance->process_shared_attribute_update_message(topic, payload, length);
        }
        else if(strncmp("/provision/response", topic, strlen("/provision/response")) == 0)
        {
            ThingsBoardSized::m_subscribedInstance->process_provisioning_response(topic, payload, length);
        }
        else if (strncmp("v2/fw/response/", topic, strlen("v2/fw/response/")) == 0)
        {
            ThingsBoardSized::m_subscribedInstance->process_firmware_response(topic, payload, length);
        }
    }

};

template<size_t PayloadSize, size_t MaxFieldsAmt, typename Logger>
ThingsBoardSized<PayloadSize, MaxFieldsAmt, Logger> *ThingsBoardSized<PayloadSize, MaxFieldsAmt, Logger>::m_subscribedInstance;


using ThingsBoard = ThingsBoardSized<>;

#endif // ThingsBoard_h