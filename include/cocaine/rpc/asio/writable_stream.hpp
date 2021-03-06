/*
    Copyright (c) 2011-2014 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2014 Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef COCAINE_IO_BUFFERED_WRITABLE_STREAM_HPP
#define COCAINE_IO_BUFFERED_WRITABLE_STREAM_HPP

#include "cocaine/rpc/asio/errors.hpp"

#include <functional>

#include <boost/asio/io_service.hpp>
#include <boost/asio/basic_stream_socket.hpp>

#include <deque>

namespace cocaine { namespace io {

namespace ph = std::placeholders;

template<class Protocol, class Encoder>
class writable_stream:
    public std::enable_shared_from_this<writable_stream<Protocol, Encoder>>
{
    COCAINE_DECLARE_NONCOPYABLE(writable_stream)

    typedef boost::asio::basic_stream_socket<Protocol> channel_type;

    typedef Encoder encoder_type;
    typedef typename encoder_type::message_type message_type;

    const std::shared_ptr<channel_type> m_channel;

    typedef std::function<void(const boost::system::error_code&)> handler_type;

    std::deque<boost::asio::const_buffer> m_messages;
    std::deque<handler_type> m_handlers;

    enum class states { idle, flushing } m_state;

public:
    explicit
    writable_stream(const std::shared_ptr<channel_type>& channel):
        m_channel(channel),
        m_state(states::idle)
    { }

    void
    write(const message_type& message, handler_type handle) {
        size_t bytes_written = 0;

        if(m_state == states::idle) {
            boost::system::error_code ec;

            // Try to write some data right away, as we don't have anything pending.
            bytes_written = m_channel->write_some(boost::asio::buffer(message.data(), message.size()), ec);

            if(!ec && bytes_written == message.size()) {
                return m_channel->get_io_service().post(std::bind(handle, ec));
            }
        }

        m_messages.emplace_back(
            message.data() + bytes_written,
            message.size() - bytes_written
        );

        m_handlers.emplace_back(handle);

        if(m_state == states::flushing) {
            return;
        } else {
            m_state = states::flushing;
        }

        m_channel->async_write_some(
            m_messages,
            std::bind(&writable_stream::flush, this->shared_from_this(), ph::_1, ph::_2)
        );
    }

    auto
    pressure() const -> size_t {
        return boost::asio::buffer_size(m_messages);
    }

private:
    void
    flush(const boost::system::error_code& ec, size_t bytes_written) {
        if(ec) {
            if(ec == boost::asio::error::operation_aborted) {
                return;
            }

            while(!m_handlers.empty()) {
                m_channel->get_io_service().post(std::bind(m_handlers.front(), ec));

                m_messages.pop_front();
                m_handlers.pop_front();
            }

            return;
        }

        while(bytes_written) {
            BOOST_ASSERT(!m_messages.empty() && !m_handlers.empty());

            const size_t message_size = boost::asio::buffer_size(m_messages.front());

            if(message_size > bytes_written) {
                m_messages.front() = m_messages.front() + bytes_written;
                break;
            }

            bytes_written -= message_size;

            // Queue this block's handler for invocation.
            m_channel->get_io_service().post(std::bind(m_handlers.front(), ec));

            m_messages.pop_front();
            m_handlers.pop_front();
        }

        if(m_messages.empty() && m_state == states::flushing) {
            m_state = states::idle;
            return;
        }

        m_channel->async_write_some(
            m_messages,
            std::bind(&writable_stream::flush, this->shared_from_this(), ph::_1, ph::_2)
        );
    }
};

}} // namespace cocaine::io

#endif
