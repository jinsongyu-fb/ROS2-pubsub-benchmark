/**
 * Autogenerated by Thrift Compiler (0.19.0)
 *
 * DO NOT EDIT UNLESS YOU ARE SURE THAT YOU KNOW WHAT YOU ARE DOING
 *  @generated
 */
#include "timing_types.h"

#include <algorithm>
#include <ostream>

#include <thrift/TToString.h>




timing::~timing() noexcept {
}


void timing::__set_msgid(const int64_t val) {
  this->msgid = val;
}

void timing::__set_nanosec(const int64_t val) {
  this->nanosec = val;
}

void timing::__set_source(const std::string& val) {
  this->source = val;
}
std::ostream& operator<<(std::ostream& out, const timing& obj)
{
  obj.printTo(out);
  return out;
}


uint32_t timing::read(::apache::thrift::protocol::TProtocol* iprot) {

  ::apache::thrift::protocol::TInputRecursionTracker tracker(*iprot);
  uint32_t xfer = 0;
  std::string fname;
  ::apache::thrift::protocol::TType ftype;
  int16_t fid;

  xfer += iprot->readStructBegin(fname);

  using ::apache::thrift::protocol::TProtocolException;


  while (true)
  {
    xfer += iprot->readFieldBegin(fname, ftype, fid);
    if (ftype == ::apache::thrift::protocol::T_STOP) {
      break;
    }
    switch (fid)
    {
      case 1:
        if (ftype == ::apache::thrift::protocol::T_I64) {
          xfer += iprot->readI64(this->msgid);
          this->__isset.msgid = true;
        } else {
          xfer += iprot->skip(ftype);
        }
        break;
      case 2:
        if (ftype == ::apache::thrift::protocol::T_I64) {
          xfer += iprot->readI64(this->nanosec);
          this->__isset.nanosec = true;
        } else {
          xfer += iprot->skip(ftype);
        }
        break;
      case 3:
        if (ftype == ::apache::thrift::protocol::T_STRING) {
          xfer += iprot->readString(this->source);
          this->__isset.source = true;
        } else {
          xfer += iprot->skip(ftype);
        }
        break;
      default:
        xfer += iprot->skip(ftype);
        break;
    }
    xfer += iprot->readFieldEnd();
  }

  xfer += iprot->readStructEnd();

  return xfer;
}

uint32_t timing::write(::apache::thrift::protocol::TProtocol* oprot) const {
  uint32_t xfer = 0;
  ::apache::thrift::protocol::TOutputRecursionTracker tracker(*oprot);
  xfer += oprot->writeStructBegin("timing");

  xfer += oprot->writeFieldBegin("msgid", ::apache::thrift::protocol::T_I64, 1);
  xfer += oprot->writeI64(this->msgid);
  xfer += oprot->writeFieldEnd();

  xfer += oprot->writeFieldBegin("nanosec", ::apache::thrift::protocol::T_I64, 2);
  xfer += oprot->writeI64(this->nanosec);
  xfer += oprot->writeFieldEnd();

  xfer += oprot->writeFieldBegin("source", ::apache::thrift::protocol::T_STRING, 3);
  xfer += oprot->writeString(this->source);
  xfer += oprot->writeFieldEnd();

  xfer += oprot->writeFieldStop();
  xfer += oprot->writeStructEnd();
  return xfer;
}

void swap(timing &a, timing &b) {
  using ::std::swap;
  swap(a.msgid, b.msgid);
  swap(a.nanosec, b.nanosec);
  swap(a.source, b.source);
  swap(a.__isset, b.__isset);
}

timing::timing(const timing& other0) {
  msgid = other0.msgid;
  nanosec = other0.nanosec;
  source = other0.source;
  __isset = other0.__isset;
}
timing& timing::operator=(const timing& other1) {
  msgid = other1.msgid;
  nanosec = other1.nanosec;
  source = other1.source;
  __isset = other1.__isset;
  return *this;
}
void timing::printTo(std::ostream& out) const {
  using ::apache::thrift::to_string;
  out << "timing(";
  out << "msgid=" << to_string(msgid);
  out << ", " << "nanosec=" << to_string(nanosec);
  out << ", " << "source=" << to_string(source);
  out << ")";
}


