#ifndef PACKETS_H
#define PACKETS_H

#include <sstream>
#include <stdint.h>

#include "blue_hardware_drivers/comms_defs.h"

class Packet {
  private: 
    uint8_t server_id_;
    comm_fc_t func_code_;
    char* result_;
    size_t result_size_;
  public:
    template <typename T>
    Packet (comm_id_t id, comm_fc_t fc, T result) : server_id_( id ), func_code_ (fc){
      result_ = reinterpret_cast<char*>(result);
      result_size_ = sizeof(result_);
    }

    virtual std::string dump();
    virtual void parse(std::stringstream & stream);
};

class ReadRegPacket : public Packet {
  private:
    comm_addr_t read_start_addr_;
    uint8_t read_count_;
  public:
    template <typename T>
    ReadRegPacket (comm_id_t id, comm_addr_t addr, uint8_t count, T result) : 
      Packet( id, COMM_FC_REG_READ, result ), read_start_addr_( addr ), read_count_( count ){}

    std::string dump();
};

class WriteRegPacket : public Packet {
  private:
    comm_addr_t write_start_addr_;
    uint8_t write_count_;
    std::stringstream write_data_;
  public:
    template <typename T>
    WriteRegPacket (comm_id_t id, comm_addr_t addr, uint8_t count, char* data, T result) : 
      Packet( id, COMM_FC_REG_WRITE, result ), write_start_addr_( addr ), write_count_( count )
    {
      write_data_.write(data, write_count_); 
    }

    std::string dump();
};

class JumpToAddrPacket : public Packet {
  private:
    uint32_t jump_addr_;
  public:
    template <typename T>
    JumpToAddrPacket (comm_id_t id, uint32_t jump_addr, T result) : 
      Packet( id, COMM_FC_JUMP_TO_ADDR, result ), jump_addr_(jump_addr) {}

    std::string dump();
};

class ReadWriteRegPacket : public Packet {
  private:
    comm_addr_t read_start_addr_;
    uint8_t read_count_;
    comm_addr_t write_start_addr_;
    uint8_t write_count_;
    std::stringstream write_data_;
  public:
    template <typename T>
    ReadWriteRegPacket (comm_id_t id, comm_addr_t r_addr, uint8_t r_count, 
                                 comm_addr_t w_addr, uint8_t w_count, char* data, T result) : 
                     Packet( id, COMM_FC_REG_READ_WRITE, result ), 
                        read_start_addr_( r_addr ), read_count_( r_count ), 
                        write_start_addr_( w_addr ), write_count_( w_count )
    {
      write_data_.write(data, write_count_); 
    }

    std::string dump();
};

#endif
