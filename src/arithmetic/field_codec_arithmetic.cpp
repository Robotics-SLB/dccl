// Copyright 2009-2017 Toby Schneider (http://gobysoft.org/index.wt/people/toby)
//                     GobySoft, LLC (for 2013-)
//                     Massachusetts Institute of Technology (for 2007-2014)
//                     Community contributors (see AUTHORS file)
//
//
// This file is part of the Dynamic Compact Control Language Library
// ("DCCL").
//
// DCCL is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 2.1 of the License, or
// (at your option) any later version.
//
// DCCL is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with DCCL.  If not, see <http://www.gnu.org/licenses/>.
#include "field_codec_arithmetic.h"
#include "dccl/field_codec_manager.h"

using dccl::dlog;
using namespace dccl::logger;

std::map<std::string, dccl::arith::Model> dccl::arith::ModelManager::arithmetic_models_;
const dccl::arith::Model::symbol_type dccl::arith::Model::OUT_OF_RANGE_SYMBOL;
const dccl::arith::Model::symbol_type dccl::arith::Model::EOF_SYMBOL;
const dccl::arith::Model::symbol_type dccl::arith::Model::MIN_SYMBOL;
const int dccl::arith::Model::CODE_VALUE_BITS;
const int dccl::arith::Model::FREQUENCY_BITS;
const dccl::arith::Model::freq_type dccl::arith::Model::MAX_FREQUENCY;
std::map<std::string, std::map<std::string, dccl::Bitset> > dccl::arith::Model::last_bits_map;

// shared library load
extern "C"
{
    void dccl3_load(dccl::Codec* dccl)
    {
        dccl_arithmetic_load(dccl);
    }

    void dccl_arithmetic_load(dccl::Codec* dccl)
    {
        using namespace dccl;
        using namespace dccl::arith;
                    
        FieldCodecManager::add<ArithmeticFieldCodec<int32> >("_arithmetic");
        FieldCodecManager::add<ArithmeticFieldCodec<int64> >("_arithmetic");
        FieldCodecManager::add<ArithmeticFieldCodec<uint32> >("_arithmetic");
        FieldCodecManager::add<ArithmeticFieldCodec<uint64> >("_arithmetic");
        FieldCodecManager::add<ArithmeticFieldCodec<double> >("_arithmetic");
        FieldCodecManager::add<ArithmeticFieldCodec<float> >("_arithmetic");
        FieldCodecManager::add<ArithmeticFieldCodec<bool> >("_arithmetic");
        FieldCodecManager::add<ArithmeticFieldCodec<const google::protobuf::EnumValueDescriptor*> >("_arithmetic");

        FieldCodecManager::add<ArithmeticFieldCodec<int32> >("dccl.arithmetic");
        FieldCodecManager::add<ArithmeticFieldCodec<int64> >("dccl.arithmetic");
        FieldCodecManager::add<ArithmeticFieldCodec<uint32> >("dccl.arithmetic");
        FieldCodecManager::add<ArithmeticFieldCodec<uint64> >("dccl.arithmetic");
        FieldCodecManager::add<ArithmeticFieldCodec<double> >("dccl.arithmetic");
        FieldCodecManager::add<ArithmeticFieldCodec<float> >("dccl.arithmetic");
        FieldCodecManager::add<ArithmeticFieldCodec<bool> >("dccl.arithmetic");
        FieldCodecManager::add<ArithmeticFieldCodec<const google::protobuf::EnumValueDescriptor*> >("dccl.arithmetic");

    }
    void dccl3_unload(dccl::Codec* dccl)
    {
        dccl_arithmetic_unload(dccl);
    }
    
    void dccl_arithmetic_unload(dccl::Codec* dccl)
    {
        using namespace dccl;
        using namespace dccl::arith;
                    
        FieldCodecManager::remove<ArithmeticFieldCodec<int32> >("_arithmetic");
        FieldCodecManager::remove<ArithmeticFieldCodec<int64> >("_arithmetic");
        FieldCodecManager::remove<ArithmeticFieldCodec<uint32> >("_arithmetic");
        FieldCodecManager::remove<ArithmeticFieldCodec<uint64> >("_arithmetic");
        FieldCodecManager::remove<ArithmeticFieldCodec<double> >("_arithmetic");
        FieldCodecManager::remove<ArithmeticFieldCodec<float> >("_arithmetic");
        FieldCodecManager::remove<ArithmeticFieldCodec<bool> >("_arithmetic");
        FieldCodecManager::remove<ArithmeticFieldCodec<const google::protobuf::EnumValueDescriptor*> >("_arithmetic");

        FieldCodecManager::remove<ArithmeticFieldCodec<int32> >("dccl.arithmetic");
        FieldCodecManager::remove<ArithmeticFieldCodec<int64> >("dccl.arithmetic");
        FieldCodecManager::remove<ArithmeticFieldCodec<uint32> >("dccl.arithmetic");
        FieldCodecManager::remove<ArithmeticFieldCodec<uint64> >("dccl.arithmetic");
        FieldCodecManager::remove<ArithmeticFieldCodec<double> >("dccl.arithmetic");
        FieldCodecManager::remove<ArithmeticFieldCodec<float> >("dccl.arithmetic");
        FieldCodecManager::remove<ArithmeticFieldCodec<bool> >("dccl.arithmetic");
        FieldCodecManager::remove<ArithmeticFieldCodec<const google::protobuf::EnumValueDescriptor*> >("dccl.arithmetic");
        
    }
}

dccl::arith::Model::symbol_type dccl::arith::Model::value_to_symbol(value_type value) const
{
    if(value < *user_model_.value_bound().begin() || value > *(user_model_.value_bound().end()-1))
        return Model::OUT_OF_RANGE_SYMBOL;
    

    google::protobuf::RepeatedField<double>::const_iterator upper_it =
        std::upper_bound(user_model_.value_bound().begin(),
                         user_model_.value_bound().end(),
                         value);

        
    google::protobuf::RepeatedField<double>::const_iterator lower_it = 
        (upper_it == user_model_.value_bound().begin()) ? upper_it : upper_it - 1;
        
    double lower_diff = std::abs((*lower_it)*(*lower_it) - value*value);
    double upper_diff = std::abs((*upper_it)*(*upper_it) - value*value);

//    std::cout << "value: " << value << std::endl;
//    std::cout << "lower_value: " << *lower_it << std::endl;
//    std::cout << "upper_value: " << *upper_it << std::endl;
    
    symbol_type symbol = ((lower_diff < upper_diff) ? lower_it : upper_it)
        - user_model_.value_bound().begin();

    
    return symbol;
                          
}
              
                  
dccl::arith::Model::value_type dccl::arith::Model::symbol_to_value(symbol_type symbol) const
{

    if(symbol == EOF_SYMBOL)
        throw(Exception("EOF symbol has no value."));
                          
    value_type value = (symbol == Model::OUT_OF_RANGE_SYMBOL) ?
        std::numeric_limits<value_type>::quiet_NaN() :
        user_model_.value_bound(symbol);

    return value;
}
              

std::pair<dccl::arith::Model::freq_type, dccl::arith::Model::freq_type> dccl::arith::Model::symbol_to_cumulative_freq(symbol_type symbol, ModelState state) const
{
    const boost::bimap<symbol_type, freq_type>& c_freqs = (state == ENCODER) ?
        encoder_cumulative_freqs_ :
        decoder_cumulative_freqs_;

    boost::bimap<symbol_type, freq_type>::left_map::const_iterator c_freq_it =
        c_freqs.left.find(symbol);
    std::pair<freq_type, freq_type> c_freq_range;
    c_freq_range.second = c_freq_it->second;
    if(c_freq_it == c_freqs.left.begin())
    {
        c_freq_range.first  = 0;
    }
    else
    {
        c_freq_it--;
        c_freq_range.first = c_freq_it->second;
    }
    return c_freq_range;
                          
}

std::pair<dccl::arith::Model::symbol_type, dccl::arith::Model::symbol_type> dccl::arith::Model::cumulative_freq_to_symbol(std::pair<freq_type, freq_type> c_freq_pair,  ModelState state) const
{

    const boost::bimap<symbol_type, freq_type>& c_freqs = (state == ENCODER) ?
        encoder_cumulative_freqs_ :
        decoder_cumulative_freqs_;
    
    std::pair<symbol_type, symbol_type> symbol_pair;
    
    // find the symbol for which the cumulative frequency is greater than
    // e.g.
    // symbol: 0   freq: 10   c_freq: 10 [0 ... 10)
    // symbol: 1   freq: 15   c_freq: 25 [10 ... 25)
    // symbol: 2   freq: 10   c_freq: 35 [25 ... 35)
    // searching for c_freq of 30 should return symbol 2     
    // searching for c_freq of 10 should return symbol 1
    symbol_pair.first = c_freqs.right.upper_bound(c_freq_pair.first)->second;
    
    
    if(symbol_pair.first == c_freqs.left.rbegin()->first)
        symbol_pair.second = symbol_pair.first; // last symbol can't be ambiguous on the low end
    else if(c_freqs.left.find(symbol_pair.first)->second > c_freq_pair.second)
        symbol_pair.second = symbol_pair.first; // unambiguously this symbol
    else
        symbol_pair.second = symbol_pair.first + 1;


    
    return symbol_pair;
}


void dccl::arith::Model::update_model(symbol_type symbol, ModelState state)
{
    if(!user_model_.is_adaptive())
        return;

    boost::bimap<symbol_type, freq_type>& c_freqs = (state == ENCODER) ?
        encoder_cumulative_freqs_ :
        decoder_cumulative_freqs_;

    if(dlog.is(DEBUG3))
    {
        dlog << "Model was: " << std::endl;
        for(symbol_type i = MIN_SYMBOL, n = max_symbol(); i <= n; ++i)
        {
            boost::bimap<symbol_type, freq_type>::left_iterator it =
                c_freqs.left.find(i);
            if(it != c_freqs.left.end())
                dlog << "Symbol: " << it->first << ", c_freq: " << it->second << std::endl;
        }
    }
    
                
    for(symbol_type i = max_symbol(), n = symbol; i >= n; --i)
    {
        boost::bimap<symbol_type, freq_type>::left_iterator it =
            c_freqs.left.find(i);
        if(it != c_freqs.left.end())
            c_freqs.left.replace_data(it, it->second + 1);
    }

    if(dlog.is(DEBUG3))
    {
        dlog << "Model is now: " << std::endl;
        for(symbol_type i = MIN_SYMBOL, n = max_symbol(); i <= n; ++i)
        {
            boost::bimap<symbol_type, freq_type>::left_iterator it =
                c_freqs.left.find(i);
            if(it != c_freqs.left.end())
                dlog << "Symbol: " << it->first << ", c_freq: " << it->second << std::endl;
        }
    }
    
    dlog.is(DEBUG3) && dlog << "total freq: " << total_freq(state) << std::endl;
                
}
