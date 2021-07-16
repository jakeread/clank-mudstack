#include "syserror.h"
#include "osape-d51/osape/osap/ts.h"
#include "osape-d51/vertices/ucBusDrop.h"
#include "osape-d51/osape/utils/cobs.h"
#include "config.h"

uint8_t errBuf[1028];
uint8_t errEncoded[1028];

/*
boolean writeString(unsigned char* dest, uint16_t* dptr, String msg){
  uint16_t len = msg.length();
  dest[(*dptr) ++] = TS_STRING_KEY;
  writeLenBytes(dest, dptr, len);
  msg.getBytes(dest, len + 1);
  return true;
}

boolean writeLenBytes(unsigned char* dest, uint16_t* dptr, uint16_t len){
  dest[(*dptr) ++] = len;
  dest[(*dptr) ++] = (len >> 8) & 255;
  return true;
}
*/

#ifdef UCBUS_IS_DROP

//uint8_t escape[512];
//uint8_t escapeHeader[10] = { PK_BUSF_KEY, 0, 0, 0, 0, PK_PTR, PK_PORTF_KEY, 0, 0, PK_DEST };

// config-your-own-ll-escape-hatch
void sysError(String msg){
  //ERRLIGHT_ON;
  /*
  uint32_t len = msg.length();
  errBuf[0] = PK_LLERR; // the ll-errmsg-key
  errBuf[1] = len & 255;
  errBuf[2] = (len >> 8) & 255;
  errBuf[3] = (len >> 16) & 255;
  errBuf[4] = (len >> 24) & 255;
  msg.getBytes(&(errBuf[5]), len + 1);
  // write header, 
  memcpy(escape, escapeHeader, 10);
  // write segsize, checksum 
  uint16_t wptr = 10;
  ts_writeUint16(128, escape, &wptr);
  ts_writeUint16(len + 5, escape, &wptr);
  memcpy(&(escape[wptr]), errBuf, len + 5);
  // transmit on ucbus 
  // potential here to hang-up and do while(!(ucBusDrop->cts())) ... I *think* that would clear on an interrupt
  ucBusDrop->transmit(escape, len + wptr + 5);
  */
}

#else 

// config-your-own-ll-escape-hatch
void sysError(String msg){
  // whatever you want,
  //ERRLIGHT_ON;
  uint32_t len = msg.length();
  errBuf[0] = 0; // serport looks for acks in each msg, this is not one
  errBuf[1] = PK_PTR; 
  errBuf[2] = PK_LLESCAPE_KEY; // the ll-errmsg-key
  errBuf[3] = len & 255;
  errBuf[4] = (len >> 8) & 255;
  errBuf[5] = (len >> 16) & 255;
  errBuf[6] = (len >> 24) & 255;
  msg.getBytes(&(errBuf[7]), len + 1);
  size_t ecl = cobsEncode(errBuf, len + 7, errEncoded);
  // direct escape 
  //if(Serial.availableForWrite() > (int64_t)ecl){
    Serial.write(errEncoded, ecl);
    Serial.flush();
  //} else {
  //  ERRLIGHT_ON;
  //}
}

#endif 

void logPacket(uint8_t* pck, uint16_t len){
  String errmsg;
  errmsg.reserve(1024);
  errmsg = "pck: "; // max 64 
  for(uint8_t i = 0; i < 64; i ++){
    if(i >= len) break;
    errmsg += String(pck[i]);
    errmsg += ", ";
  }
  sysError(errmsg);
}