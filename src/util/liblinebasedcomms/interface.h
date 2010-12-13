// copyright 2010 t. schneider tes@mit.edu
//
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


#ifndef ASIOLineBasedInterface20100715H
#define ASIOLineBasedInterface20100715H

#include <iostream>
#include <deque>
#include <fstream>
#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include <boost/array.hpp>
#include <boost/asio.hpp>

#include "goby/util/time.h"

#include <string>

namespace goby
{
    namespace util
    {
        /// basic interface class for all the derived serial (and networking mimics) line-based nodes (serial, tcp, udp, etc.)
        class LineBasedInterface
        {
          public:
            // start the connection
            void start();
            // close the connection cleanly
            void close();
            // is the connection alive and well?
            bool active() { return active_; }
            
            // returns line with delimiter
            // returns "" if no data to read  because there is a always
            // at least a non-empty delimiter in a valid line
            std::string readline()
            { return readline_oldest(); }

            bool readline(std::string* s)
            {
                *s = readline_oldest();
                return !s->empty();
            }
            
            
            // write a line to the buffer
            void write(const std::string& msg);

            // allows FIFO access (default)
            std::string readline_oldest();
            // allows FILO access
            std::string readline_newest();

            // empties the read buffer
            void clear();
            
          protected:            
            LineBasedInterface();
            
            // all implementors of this line based interface must provide do_start, do_write, do_close, and put all read data into "in_"
            virtual void do_start() = 0;
            virtual void do_write(const std::string & line) = 0;
            virtual void do_close(const boost::system::error_code& error) = 0;            

            void set_active(bool active) { active_ = active; }
            
            boost::asio::io_service io_service_; // the main IO service that runs this connection
            std::deque<std::string> in_; // buffered read data
            boost::mutex in_mutex_;
            
          private:
            boost::asio::io_service::work work_;
            bool active_; // remains true while this object is still operating
            
        };        

    }
}

#endif
