#include "cocaine/drivers/server+lsd.hpp"

using namespace cocaine::engine::drivers;
using namespace cocaine::lines;

lsd_response_t::lsd_response_t(const std::string& method, lsd_server_t* server, const route_t& route):
    deferred_t(method),
    m_server(server),
    m_route(route)
{ }

void lsd_response_t::enqueue(engine_t* engine) {
    zmq::message_t request;

    request.copy(&m_request);

    engine->enqueue(
        shared_from_this(),
        boost::make_tuple(
            INVOKE,
            m_method,
            boost::ref(request)));
}

void lsd_response_t::send(zmq::message_t& chunk) {
    zmq::message_t envelope;

    envelope.copy(&m_envelope);

    m_server->respond(m_route, envelope, chunk);
}

void lsd_response_t::abort(const std::string& error) {
    zmq::message_t null;

    Json::Reader reader(Json::Features::strictMode());
    Json::Value root;

    if(reader.parse(
        static_cast<const char*>(m_envelope.data()),
        static_cast<const char*>(m_envelope.data()) + m_envelope.size(),
        root))
    {
        root["error"] = error;
    }

    Json::FastWriter writer;
    std::string response(writer.write(root));

    m_envelope.rebuild(response.size());
    memcpy(m_envelope.data(), response.data(), response.size());

    m_server->respond(m_route, m_envelope, null);
}

zmq::message_t& lsd_response_t::request() {
    return m_request;
}

zmq::message_t& lsd_response_t::envelope() {
    return m_envelope;
}

lsd_server_t::lsd_server_t(engine_t* engine, const std::string& method, const Json::Value& args):
    zmq_server_t(engine, method, args)
{ }

Json::Value lsd_server_t::info() const {
    Json::Value result(Json::objectValue);

    result["type"] = "server+lsd";
    result["endpoint"] = m_socket.endpoint();
    result["route"] = m_socket.route();

    return result;
}

void lsd_server_t::process(ev::idle&, int) {
    if(m_socket.pending()) {
        zmq::message_t message;
        std::vector<std::string> route;

        while(true) {
            m_socket.recv(&message);

#if ZMQ_VERSION > 30000
            if(!m_socket.label()) {
#else
            if(!message.size()) {
#endif
                break;
            }

            route.push_back(std::string(
                static_cast<const char*>(message.data()),
                message.size()));
        }

        boost::shared_ptr<lsd_response_t> deferred(
            new lsd_response_t(m_method, this, route));

        // LSD envelope
#if ZMQ_VERSION < 30000
        m_socket.recv(&deferred->envelope());
#else
        deferred->envelope().copy(&message);
#endif

        // Request
        m_socket.recv(&deferred->request());

        try {
            deferred->enqueue(m_engine);
        } catch(const std::runtime_error& e) {
            syslog(LOG_ERR, "driver [%s:%s]: failed to enqueue the invocation - %s",
                m_engine->name().c_str(), m_method.c_str(), e.what());
            deferred->abort(e.what());
        }
    } else {
        m_watcher.start(m_socket.fd(), ev::READ);
        m_processor.stop();
    }
}

void lsd_server_t::respond(const route_t& route, 
                           zmq::message_t& envelope, 
                           zmq::message_t& chunk)
{
    zmq::message_t message;
    
    // Send the identity
    for(route_t::const_iterator id = route.begin(); id != route.end(); ++id) {
        message.rebuild(id->length());
        memcpy(message.data(), id->data(), id->length());
#if ZMQ_VERSION < 30000
        m_socket.send(message, ZMQ_SNDMORE);
#else
        m_socket.send(message, ZMQ_SNDMORE | ZMQ_SNDLABEL);
#endif
    }

#if ZMQ_VERSION < 30000                
    // Send the delimiter
    message.rebuild(0);
    m_socket.send(message, ZMQ_SNDMORE);
#endif

    // Send the response
    if(chunk.size()) {
        m_socket.send(envelope, ZMQ_SNDMORE);
        m_socket.send(chunk);
    } else {
        m_socket.send(envelope);
    }
}

