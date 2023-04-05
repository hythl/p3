#ifndef ROUTING_PROTOCOL_PACKET_H
#define ROUTING_PROTOCOL_PACKET_H

#include <arpa/inet.h>
#include "global.h"

class Packet {
  public:
    uint8_t type;
    uint16_t src;
    uint16_t dst;
    uint16_t size;
    void* payload;
    uint16_t payloadSize;
    void* buffer;

    Packet() {};
    Packet(uint16_t src, uint16_t dst) {
      this->src = src;
      this->dst = dst;
      payload = NULL;
      buffer = NULL;
    };

    ~Packet() {
      free(payload);
    };

    void setType(ePacketType type) { this->type = (uint8_t)type;}
    void setSrc(uint16_t src) { this->src = src;}
    void setDst(uint16_t dst) { this->dst = dst;}
    void setSize(uint16_t size) { this->size = size;}

    void* serialize() {
      uint8_t nType = htons(type);
      uint16_t nSrc = htonl(src);
      uint16_t nDst = htonl(dst);
      uint16_t nSize = htonl(size);

      // PACKET FORMAT
      // type (1 byte) + Reserved (1 byte) + size(2 bytes) + src (2 bytes) + dst (2 bytes) + payload
      size = 8 + payloadSize;
      void* buffer = calloc(size);
      memcpy(buffer, &nType, 1);
      memcpy(buffer + 2, &nSize, 2);
      memcpy(buffer + 4, &nSrc, 2);
      memcpy(buffer + 6, &nDst, 2);
      memcpy(buffer + 8, payload, payloadSize);
      return buffer;
    }

    void deserialize(void* bufferIn) {
      buffer = bufferIn;
      memcpy(&type, buffer, 1);
      memcpy(&size, buffer + 2, 2);
      memcpy(&src, buffer + 4, 2);
      memcpy(&dst, buffer + 6, 2);
      type = ntohs(type);
      size = ntohl(size);
      src = ntohl(src);
      dst = ntohl(dst);
      payloadSize = size - 8;
      payload = payload == NULL ? malloc(payloadSize) : realloc(payload, payloadSize);
      memcpy(payload, buffer + 8, payloadSize);
    }

    void destory() {
      free(buffer);
    }
}

class PongPacket : public Packet {
  public:
    PongPacket(uint16_t src, uint16_t dst) : Packet(src, dst) {
      type = PONG;
      payloadSize = 4;
      payload = malloc(payloadSize);
      memcpy(payload, &timeStamp, payloadSize);
    };

    ~PongPacket();
}

class PingPacket : public Packet {
  public:
    PingPacket(uint32_t src, unsigned int timeStamp) : Packet(src, 0) {
      type = PING;
      payloadSize = 4;
      payload = malloc(payloadSize);
      memcpy(payload, &timeStamp, payloadSize);
    };

    ~PingPacket();

    void* toPongPacket(uint32_t src) {
      // buffer must be valid
      u
      memcpy(buffer, &type, 1);
      memcpy(buffer + 4, &src, 2);
      PongPacket* pong = new PongPacket(src, dst);
      pong->payload = payload;
      pong->payloadSize = payloadSize;
      return pong;
    }
    
}