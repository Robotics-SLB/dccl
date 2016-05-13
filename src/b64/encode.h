// Copyright 2009-2016 Toby Schneider (http://gobysoft.org/index.wt/people/toby)
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
// :mode=c++:
/*
encode.h - c++ wrapper for a base64 encoding algorithm

This is part of the libb64 project, and has been placed in the public domain.
For details, see http://sourceforge.net/projects/libb64
*/
#ifndef BASE64_ENCODE_H
#define BASE64_ENCODE_H

#include <iostream>

namespace dccl
{
    /// Base64 encoding/decoding namespace
    namespace base64
    {
	extern "C" 
	{
#include "cencode.h"
	}

	struct encoder
	{
            base64_encodestate _state;
            int _buffersize;

        encoder(int buffersize_in = BUFFERSIZE)
        : _buffersize(buffersize_in)
		{}

            int encode(char value_in)
		{
                    return base64_encode_value(value_in);
		}

            int encode(const char* code_in, const int length_in, char* plaintext_out)
		{
                    return base64_encode_block(code_in, length_in, plaintext_out, &_state);
		}

            int encode_end(char* plaintext_out)
		{
                    return base64_encode_blockend(plaintext_out, &_state);
		}

            void encode(std::istream& istream_in, std::ostream& ostream_in)
		{
                    base64_init_encodestate(&_state);
                    //
                    const int N = _buffersize;
                    char* plaintext = new char[N];
                    char* code = new char[2*N];
                    int plainlength;
                    int codelength;

                    do
                    {
                        istream_in.read(plaintext, N);
                        plainlength = istream_in.gcount();
                        //
                        codelength = encode(plaintext, plainlength, code);
                        ostream_in.write(code, codelength);
                    }
                    while (istream_in.good() && plainlength > 0);

                    codelength = encode_end(code);
                    ostream_in.write(code, codelength);
                    //
                    base64_init_encodestate(&_state);

                    delete [] code;
                    delete [] plaintext;
		}
	};

    } // namespace base64
}

#endif // BASE64_ENCODE_H

