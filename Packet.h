#ifndef ROUTING_PROTOCOL_PACKET_H
#define ROUTING_PROTOCOL_PACKET_H

#include <arpa/inet.h>
#include <string.h>
#include <iostream>
#include "global.h"

class Packet {
  public:
    unsigned char type;
    uint16_t src;
    uint16_t dst;
    uint16_t size;
    void* payload;
    uint16_t payloadSize;
    char* buffer;

    Packet(){
      payload = NULL;
      buffer = NULL;
    };
    
    Packet(uint16_t src, uint16_t dst) {
      this->src = src;
      this->dst = dst;
      payload = NULL;
      buffer = NULL;
    };

    ~Packet() {
      free(payload);
    };

    void setType(ePacketType type) { this->type = (unsigned char)type;}
    void setSrc(uint16_t src) { this->src = src;}
    void setDst(uint16_t dst) { this->dst = dst;}
    void setSize(uint16_t size) { this->size = size;}

    void* serialize() {
      size = 8 + payloadSize;
      cout << "Packet to be serialized: src = " << src << " dst = " << dst << " size = " << size << " type = " << uint16_t(type)<< endl;
      uint16_t nSize = htons(size);
      uint16_t nSrc = htons(src);
      uint16_t nDst = htons(dst);

      // PACKET FORMAT
      // type (1 byte) + Reserved (1 byte) + size(2 bytes) + src (2 bytes) + dst (2 bytes) + payload
      buffer = (char*)malloc(size);
      *buffer = type;
      memcpy(buffer + 2, (void*)&nSize, sizeof(uint16_t));
      memcpy(buffer + 4, (void*)&nSrc, sizeof(uint16_t));
      memcpy(buffer + 6, (void*)&nDst, sizeof(uint16_t));
      memcpy(buffer + 8, payload, payloadSize);
      return buffer;
    }

    void deserialize(void* bufferIn) {
      buffer = (char*)bufferIn;
      memcpy(&type, buffer, sizeof(uint8_t));
      memcpy(&size, buffer + 2, sizeof(uint16_t));
      memcpy(&src, buffer + 4, sizeof(uint16_t));
      memcpy(&dst, buffer + 6, sizeof(uint16_t));

      size = ntohs(size);
      src = ntohs(src);
      dst = ntohs(dst);
      payloadSize = size - 8;
      if (payload != NULL)
        free(payload);
      payload = malloc(payloadSize);
      memcpy(payload, buffer + 8, payloadSize);
    }

    Packet clone() {
      Packet p;
      p.type = type;
      p.src = src;
      p.dst = dst;
      p.size = size;
      p.payloadSize = payloadSize;
      p.payload = malloc(payloadSize);
      memcpy(p.payload, payload, payloadSize);
      return p;
    }

    void destory() {
      free(buffer);
    }
};

class PingPacket : public Packet {
  public:
    PingPacket(uint16_t src, unsigned int timeStamp) : Packet(src, 0) {
      type = PING;
      this->src = src;
      payloadSize = sizeof(unsigned int);
      payload = malloc(payloadSize);
      memcpy(payload, &timeStamp, payloadSize);
    };
};

class PongPacket : public Packet {
  public:
    PongPacket(uint16_t src, uint16_t dst, void* content, uint16_t contentSize) : Packet(src, 0) {
      type = PONG;
      this->src = src;
      this->dst = dst;
      payloadSize = contentSize;
      payload = malloc(payloadSize);
      memcpy(payload, content, payloadSize);
    };
};

#endif //ROUTING_PROTOCOL_PACKET_H