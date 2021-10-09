// =====================================================================================
//
//       Filename:  Tiingo.cpp
//
//    Description:  Live stream ticker updates 
//
//        Version:  1.0
//        Created:  08/06/2021 09:28:55 AM
//       Revision:  none
//       Compiler:  g++
//
//         Author:  David P. Riedel (), driedel@cox.net
/// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)/
// =====================================================================================
// the guts of this code comes from the examples distributed by Boost.

#include <date/date.h>
#include <string_view>

#include <fmt/chrono.h>

#include "Tiingo.h"
#include "boost/beast/core/buffers_to_string.hpp"

#include "utilities.h"

using namespace std::string_literals;


//--------------------------------------------------------------------------------------
//       Class:  Tiingo
//      Method:  Tiingo
// Description:  constructor
//--------------------------------------------------------------------------------------

Tiingo::Tiingo ()
    : ctx_{ssl::context::tlsv12_client}, resolver_{ioc_}, ws_{ioc_, ctx_}
{
}  // -----  end of method Tiingo::Tiingo  (constructor)  ----- 

Tiingo::~Tiingo ()
{
    // need to disconnect if still connected.
    
    if (ws_.is_open())
    {
        Disconnect();
    }
}		// -----  end of method Tiingo::~Tiingo  ----- 

Tiingo::Tiingo (const std::string& host, const std::string& port, const std::string& api_key)
    : api_key_{api_key}, host_{host}, port_{port}, ctx_{ssl::context::tlsv12_client},
    resolver_{ioc_}, ws_{ioc_, ctx_}
{
}  // -----  end of method Tiingo::Tiingo  (constructor)  ----- 

Tiingo::Tiingo (const std::string& host, const std::string& port, const std::string& prefix,
            const std::string& api_key, const std::string& symbols)
    : api_key_{api_key}, host_{host}, port_{port}, websocket_prefix_{prefix},
        ctx_{ssl::context::tlsv12_client}, resolver_{ioc_}, ws_{ioc_, ctx_}
{
    // symbols is a string of one or more symbols to monitor.
    // If more than 1 symbol, list is coma delimited.

    symbol_list_ = split_string<std::string>(symbols, ',');

}  // -----  end of method Tiingo::Tiingo  (constructor)  ----- 

void Tiingo::Connect()
{
    // Look up the domain name
    auto const results = resolver_.resolve(host_, port_);

    // Make the connection on the IP address we get from a lookup
    auto ep = net::connect(get_lowest_layer(ws_), results);

    // Set SNI Hostname (many hosts need this to handshake successfully)
    if(! SSL_set_tlsext_host_name(ws_.next_layer().native_handle(), host_.c_str()))
        throw beast::system_error(
            beast::error_code(
                static_cast<int>(::ERR_get_error()),
                net::error::get_ssl_category()),
            "Failed to set SNI Hostname");

    // Update the host_ string. This will provide the value of the
    // Host HTTP header during the WebSocket handshake.
    // See https://tools.ietf.org/html/rfc7230#section-5.4
    auto host = host_ + ':' + std::to_string(ep.port());

    // Perform the SSL handshake
    ws_.next_layer().handshake(ssl::stream_base::client);

    // Perform the websocket handshake
    ws_.handshake(host, websocket_prefix_);


}
void Tiingo::StreamData(bool* time_to_stop)
{

    // put this here for now.
    // need to manually construct to get expected formate when serialized 

    Json::Value connection_request;
    connection_request["eventName"] = "subscribe";
    connection_request["authorization"] = api_key_;
    connection_request["eventData"]["thresholdLevel"] = 5;
    Json::Value tickers(Json::arrayValue);
    for (const auto& symbol : symbol_list_)
    {
        tickers.append(symbol);
    }
    
    connection_request["eventData"]["tickers"] = tickers;

    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";        // compact printing and string formatting 
    const std::string connection_request_str = Json::writeString(builder, connection_request);
//    std::cout << "Jsoncpp connection_request_str: " << connection_request_str << '\n';

    // Send the message
    ws_.write(net::buffer(connection_request_str));

    // This buffer will hold the incoming message
    beast::flat_buffer buffer;

    while(true)
    {
        buffer.clear();
        ws_.read(buffer);
        std::string buffer_content = beast::buffers_to_string(buffer.cdata());
        // The make_printable() function helps print a ConstBufferSequence
//        std::cout << buffer_content << std::endl;
        ExtractData(buffer_content);
        if (*time_to_stop == true)
        {
            StopStreaming();
            break;
        }
    }
}

void Tiingo::ExtractData (const std::string& buffer)
{
    // will eventually need to use locks to access this I think.
    // for now, we just append data.
    JSONCPP_STRING err;
    Json::Value response;

    Json::CharReaderBuilder builder;
    const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
//    if (!reader->parse(buffer.data(), buffer.data() + buffer.size(), &response, nullptr))
    if (! reader->parse(buffer.data(), buffer.data() + buffer.size(), &response, &err))
    {
        throw std::runtime_error("Problem parsing tiingo response: "s + err);
    }
//    std::cout << "\n\n jsoncpp parsed response: " << response << "\n\n";

    auto message_type = response["messageType"];
    if (message_type == "A")
    {
        auto data = response["data"];

        if (data[0] != "T")
        {
            return;
        }
        // extract our data

        PF_Data new_value;
        new_value.subscription_id_ = subscription_id_;
        new_value.time_stamp_ = data[1].asString();
        new_value.time_stamp_seconds_ = data[2].asInt64();
        new_value.ticker_ = data[3].asString();
        new_value.last_price_ = DprDecimal::DDecDouble(data[9].asFloat(), 4);
        new_value.last_size_ = data[10].asInt();

        pf_data_.push_back(std::move(new_value));        

//        std::cout << "new data: " << pf_data_.back().ticker_ << " : " << pf_data_.back().last_price_ << '\n';
    }
    else if (message_type == "I")
    {
        subscription_id_ = response["data"]["subscriptionId"].asInt();
//        std::cout << "json cpp subscription ID: " << subscription_id_ << '\n';
        return;
    }
    else if (message_type == "H")
    {
        // heartbeat , just return

        return;
    }
    else
    {
        throw std::runtime_error("unexpected message type: "s + message_type.asString());
    }

}		// -----  end of method Tiingo::ExtractData  ----- 

void Tiingo::StopStreaming ()
{
    // we need to send the unsubscribe message in a separate connection.

    Json::Value disconnect_request;
    disconnect_request["eventName"] = "unsubscribe";
    disconnect_request["authorization"] = api_key_;
    disconnect_request["eventData"]["subscriptionId"] = subscription_id_;
    Json::Value tickers(Json::arrayValue);
    for (const auto& symbol : symbol_list_)
    {
        tickers.append(symbol);
    }
    
    disconnect_request["eventData"]["tickers"] = tickers;

    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";        // compact printing and string formatting 
    const std::string disconnect_request_str = Json::writeString(builder, disconnect_request);
//    std::cout << "Jsoncpp disconnect_request_str: " << disconnect_request_str << '\n';

    // just grab the code from the example program 

    net::io_context ioc;
    ssl::context ctx{ssl::context::tlsv12_client};

    tcp::resolver resolver{ioc};
    websocket::stream<beast::ssl_stream<tcp::socket>> ws{ioc, ctx};

    auto host = host_;
    auto port = port_;

    auto const results = resolver.resolve(host, port);

    auto ep = net::connect(get_lowest_layer(ws), results);

    if(! SSL_set_tlsext_host_name(ws.next_layer().native_handle(), host.c_str()))
        throw beast::system_error(
            beast::error_code(
                static_cast<int>(::ERR_get_error()),
                net::error::get_ssl_category()),
            "Failed to set SNI Hostname");

    host += ':' + std::to_string(ep.port());

    ws.next_layer().handshake(ssl::stream_base::client);

    ws.set_option(websocket::stream_base::decorator(
        [](websocket::request_type& req)
        {
            req.set(http::field::user_agent,
                std::string(BOOST_BEAST_VERSION_STRING) +
                    " websocket-client-coro");
        }));

    ws.handshake(host, websocket_prefix_);

    ws.write(net::buffer(std::string(disconnect_request_str)));

    beast::flat_buffer buffer;

    ws.read(buffer);

    ws.close(websocket::close_code::normal);

//    std::cout << beast::make_printable(buffer.data()) << std::endl;
 
}		// -----  end of method Tiingo::StopStreaming  ----- 

void Tiingo::Disconnect()
{

    // Close the WebSocket connection
    ws_.close(websocket::close_code::normal);
}

Json::Value Tiingo::GetMostRecentTickerData(std::string_view symbol, date::year_month_day start_from, int how_many_previous)
{
    // we need to do some date arithmetic so we can use our basic 'GetTickerData' method. 

    auto business_days = ConstructeBusinessDayRange(start_from, how_many_previous, UpOrDown::e_Down);
//    std::cout << "business days: " << business_days.first << " : " << business_days.second << '\n';

    // we reverse the dates because we worked backwards from our given starting point and 
    // Tiingo needs the dates in ascending order. 

    return GetTickerData(symbol, business_days.second, business_days.first, UpOrDown::e_Down);

}		// -----  end of method Tiingo::GetMostRecentTickerData  ----- 

Json::Value Tiingo::GetTickerData(std::string_view symbol, date::year_month_day start_date, date::year_month_day end_date, UpOrDown sort_asc)
{
    // if any problems occur here, we'll just let beast throw an exception.

    tcp::resolver resolver(ioc_);
    beast::ssl_stream<beast::tcp_stream> stream(ioc_, ctx_);

    if(! SSL_set_tlsext_host_name(stream.native_handle(), host_.c_str()))
    {
        beast::error_code ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
        throw beast::system_error{ec};
    }

    auto const results = resolver.resolve(host_, port_);
    beast::get_lowest_layer(stream).connect(results);
    stream.handshake(ssl::stream_base::client);

    // we use our custom formatter for year_month_day objects because converting to sys_days 
    // and then formatting changes the date (becomes a day earlier) for some reason (time zone 
    // related maybe?? )

    std::string request = fmt::format("https://{}/tiingo/daily/{}/prices?startDate={}&endDate={}&token={}&format={}&resampleFreq={}&sort={}",
            host_, symbol, start_date, end_date, api_key_, "json", "daily", "-date");

    http::request<http::string_body> req{http::verb::get, request.c_str(), version_};
    req.set(http::field::host, host_);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

    http::write(stream, req);

    beast::flat_buffer buffer;

    http::response<http::string_body> res;

    http::read(stream, buffer, res);
    std::string result = res.body();

    // shutdown without causing a 'stream_truncated' error.

    beast::get_lowest_layer(stream).cancel();
    beast::get_lowest_layer(stream).close();

//    std::cout << result << '\n';

    // now, just convert to JSON 

    JSONCPP_STRING err;
    Json::Value response;

    Json::CharReaderBuilder builder;
    const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
//    if (!reader->parse(buffer.data(), buffer.data() + buffer.size(), &response, nullptr))
    if (! reader->parse(result.data(), result.data() + result.size(), &response, &err))
    {
        throw std::runtime_error("Problem parsing tiingo response: "s + err);
    }
    return response;
}		// -----  end of method Tiingo::GetTickerData  ----- 
