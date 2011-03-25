// copyright 2009-2011 t. schneider tes@mit.edu
// 
// this file is part of the Dynamic Compact Control Language (DCCL),
// the goby-acomms codec. goby-acomms is a collection of libraries 
// for acoustic underwater networking
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This software is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this software.  If not, see <http://www.gnu.org/licenses/>.

#ifndef DCCL20091211H
#define DCCL20091211H

#include <string>
#include <set>
#include <map>
#include <ostream>
#include <stdexcept>
#include <vector>

#include <google/protobuf/descriptor.h>
#include <google/protobuf/dynamic_message.h>

#include <boost/shared_ptr.hpp>

#include "goby/util/time.h"
#include "goby/util/logger.h"
#include "goby/core/core_constants.h"
#include "goby/protobuf/dccl.pb.h"
#include "goby/protobuf/modem_message.pb.h"
#include "goby/acomms/acomms_helpers.h"
#include "protobuf_cpp_type_helpers.h"

#include "dccl_exception.h"
#include "dccl_field_codec.h"

/// The global namespace for the Goby project.
namespace goby
{
    namespace util
    { class FlexOstream; }
    

    /// Objects pertaining to acoustic communications (acomms)
    namespace acomms
    {
        class DCCLFieldCodec;
        
        /// \class DCCLCodec dccl.h goby/acomms/dccl.h
        /// \brief provides an API to the Dynamic CCL Codec.
        /// \ingroup acomms_api
        /// \sa  dccl.proto and modem_message.proto for definition of Google Protocol Buffers messages (namespace goby::acomms::protobuf).
        class DCCLCodec  // TODO(tes): Make singleton class
        {
          public:
            
            /// \name Constructors/Destructor
            //@{
            /// \brief Instantiate optionally with a ostream logger (for human readable output)
            /// \param log std::ostream object or FlexOstream to capture all humanly readable runtime and debug information (optional).
            DCCLCodec(std::ostream* log = 0);

            /// destructor
            ~DCCLCodec() {}
            //@}
        
            /// \name Initialization Methods.
            ///
            /// These methods are intended to be called before doing any work with the class. However,
            /// they may be called at any time as desired.
            //@{

            /// \brief Set (and overwrite completely if present) the current configuration. (protobuf::DCCLConfig defined in dccl.proto)
            void set_cfg(const protobuf::DCCLConfig& cfg);

            /// \brief Set (and merge "repeat" fields) the current configuration. (protobuf::DCCLConfig defined in dccl.proto)
            void merge_cfg(const protobuf::DCCLConfig& cfg);
            

            class FieldCodecManager
            {
              public:
                template<typename T, template <typename T> class Codec>
                    void add_field_codec(const char* name)
                {
                
                    const google::protobuf::FieldDescriptor::CppType cpp_type = ToProtoCppType<T>::as_enum();
    
                    if(!codecs_[cpp_type].count(name))
                        codecs_[cpp_type][name] = boost::shared_ptr<DCCLFieldCodec>(new Codec<T>());
                    else
                    {
                        if(log_)
                            *log_ << warn << "Ignoring duplicate codec " << name << " for CppType " << cpptype_helper_[cpp_type]->as_str() << std::endl;
                    }
                }                

              private:
                std::map<google::protobuf::FieldDescriptor::CppType,
                    std::map<std::string, boost::shared_ptr<DCCLFieldCodec> > > codecs_;
            };
            
            
            /// \brief Messages must be validated before they can be encoded/decoded
            bool validate(const google::protobuf::Descriptor* desc);
            
            /// Registers the group names used for the FlexOstream logger
            static void add_flex_groups(util::FlexOstream* tout);
            
            //@}
        
            /// \name Codec functions.
            ///
            /// This is where the real work happens.
            //@{
            bool encode(std::string* bytes, const google::protobuf::Message& msg);
            bool decode(const std::string& bytes, google::protobuf::Message* msg); 
            //@}

            /// \example acomms/chat/chat.cpp
            
          private:
            void encrypt(std::string* s, const std::string& nonce);
            void decrypt(std::string* s, const std::string& nonce);
            void process_cfg();
            
            void bitset2string(const Bitset& bits, std::string* str)
            {
                str->resize(bits.num_blocks()); // resize the string to fit the bitset
                to_block_range(bits, str->rbegin());
            }
            
            void string2bitset(Bitset* bits, const std::string& str)
            {
                bits->resize(str.size() * BITS_IN_BYTE);
                from_block_range(str.rbegin(), str.rend(), *bits);
            }

            void encode_recursive(Bitset* bits,
                                  const google::protobuf::Message& msg,
                                  bool is_header);
            void decode_recursive(Bitset* bits,
                                  google::protobuf::Message* msg,
                                  bool is_header);
            
            void validate_recursive(const google::protobuf::Descriptor* desc);
            
            unsigned size_recursive(const google::protobuf::Message& msg, bool is_header);

            // more efficient way to do ceil(total_bits / 8)
            // to get the number of bytes rounded up.
            enum { BYTE_MASK = 7 }; // 00000111
            unsigned ceil_bits2bytes(unsigned bits)
            {
                return (bits& BYTE_MASK) ?
                    floor_bits2bytes(bits) + 1 :
                    floor_bits2bytes(bits);
            }
            unsigned floor_bits2bytes(unsigned bits)
            { return bits >> 3; }
            
            
                   
          private:
            static const char* DEFAULT_CODEC_NAME;

            std::ostream* log_;
            protobuf::DCCLConfig cfg_;
            // SHA256 hash of the crypto passphrase
            std::string crypto_key_;

            // maps protocol buffers CppTypes to a map of field codec names & codecs themselves
            static FieldCodecManager codec_manager_;
            
            std::map<google::protobuf::FieldDescriptor::CppType, boost::shared_ptr<FromProtoCppTypeBase> >
                cpptype_helper_;
            
            google::protobuf::DynamicMessageFactory message_factory_;
        };
    }
}


#endif
