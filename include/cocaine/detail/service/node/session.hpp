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

#ifndef COCAINE_ENGINE_SESSION_HPP
#define COCAINE_ENGINE_SESSION_HPP

#include "cocaine/common.hpp"

#include "cocaine/detail/service/node/event.hpp"
#include "cocaine/detail/service/node/stream.hpp"
#include "cocaine/detail/service/node/messages.hpp"

#include "cocaine/rpc/asio/encoder.hpp"
#include "cocaine/rpc/asio/writable_stream.hpp"
#include "cocaine/rpc/queue.hpp"

#include <boost/asio/local/stream_protocol.hpp>

namespace cocaine { namespace engine {

struct session_t:
    public std::enable_shared_from_this<session_t>
{
    COCAINE_DECLARE_NONCOPYABLE(session_t)

    typedef boost::asio::local::stream_protocol protocol_type;

    session_t(uint64_t id, const api::event_t& event, const api::stream_ptr_t& upstream);

    struct downstream_t:
        public api::stream_t
    {
        downstream_t(const std::shared_ptr<session_t>& parent);

        virtual
       ~downstream_t();

        virtual
        void
        write(const char* chunk, size_t size);

        virtual
        void
        error(int code, const std::string& reason);

        virtual
        void
        close();

    private:
        std::shared_ptr<session_t> parent;
    };

    void
    attach(const std::shared_ptr<io::writable_stream<protocol_type, io::encoder_t>>& downstream);

    void
    detach();

    void
    close();

public:
    // Session ID.
    const uint64_t id;

    // Session event type and execution policy.
    const api::event_t event;

    // Client's upstream for response delivery.
    const std::shared_ptr<api::stream_t> upstream;

private:
    template<class Event, typename... Args>
    void
    send(Args&&... args);

    template<class Event, typename... Args>
    void
    send(std::lock_guard<std::mutex>&, Args&&... args);

private:
    class push_action_t;
    class stream_adapter_t;
    std::shared_ptr<synchronized<io::message_queue<io::rpc_tag, stream_adapter_t>>> m_writer;

    // Session interlocking.
    std::mutex m_mutex;

    struct state {
        enum value: int { open, closed };
    };

    // Session state.
    state::value m_state;
};

template<class Event, typename... Args>
void
session_t::send(Args&&... args) {
    std::lock_guard<std::mutex> lock(m_mutex);
    send<Event>(lock, std::forward<Args>(args)...);
}

template<class Event, typename... Args>
void
session_t::send(std::lock_guard<std::mutex>&, Args&&... args) {
    BOOST_ASSERT(m_writer);

    if(m_state == state::open) {
        m_writer->synchronize()->append<Event>(std::forward<Args>(args)...);
    } else {
        throw cocaine::error_t("the session is no longer valid");
    }
}

}} // namespace cocaine::engine

#endif
