/*
  ThingsBoard.h - Library API for sending data to the ThingsBoard
  Based on PubSub MQTT library.
  Created by Olender M. Oct 2018.
  Modified by Narin Laboratory. Nov 2021.
  Released into the public domain.
*/

#include "thingsboard.h"

/*----------------------------------------------------------------------------*/

bool Telemetry::serializeKeyval(JsonVariant &jsonObj) const {
  if (m_key) {
    switch (m_type) {
      case TYPE_BOOL:
        jsonObj[m_key] = m_value.boolean;
      break;
      case TYPE_INT:
        jsonObj[m_key] = m_value.integer;
      break;
      case TYPE_REAL:
        jsonObj[m_key] = m_value.real;
      break;
      case TYPE_STR:
        jsonObj[m_key] = m_value.str;
      break;
      default:
      break;
    }
  } else {
    switch (m_type) {
      case TYPE_BOOL:
        return jsonObj.set(m_value.boolean);
      break;
      case TYPE_INT:
        return jsonObj.set(m_value.integer);
      break;
      case TYPE_REAL:
        return jsonObj.set(m_value.real);
      break;
      case TYPE_STR:
        return jsonObj.set(m_value.str);
      break;
      default:
      break;
    }
  }
  return true;
}