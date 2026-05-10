#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/beast.hpp>
#include <iostream>
#include <thread>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
using tcp = asio::ip::tcp;

void websocket_session(tcp::socket socket) {
    try {
        websocket::stream<tcp::socket> ws(std::move(socket));
        ws.accept();
        std::cout << "Client connected!\n";

        beast::flat_buffer buffer;
        while (true) {
            ws.read(buffer);
            std::string msg = beast::buffers_to_string(buffer.data());

            std::cout << "\nReceived: " << msg << std::endl;

            // Echo message back to client
            ws.text(ws.got_text());
            ws.write(buffer.data());

            buffer.consume(buffer.size()); // Clear buffer for next message
        }
    } catch (std::exception& e) {
        std::cerr << "WebSocket session error: " << e.what() << "\n";
    }
}

void runServer(asio::io_context& ioc, unsigned short port) {
    tcp::acceptor acceptor(ioc, tcp::endpoint(tcp::v4(), port));

    std::cout << "WebSocket Server running on ws://127.0.0.1:" << port << "\n\n";

    while (true) {
        tcp::socket socket(ioc);
        acceptor.accept(socket);
        std::thread(websocket_session, std::move(socket)).detach(); // Handle client in new thread
    }
}

void run_client(const std::string& host, const std::string& port) {
    try {
        asio::io_context ioc;
        tcp::resolver resolver(ioc);
        websocket::stream<tcp::socket> ws(ioc);

        auto const results = resolver.resolve(host, port);
        asio::connect(ws.next_layer(), results);
        ws.handshake(host, "/");

        std::cout << "Connected to WebSocket server!\n";

        std::string message;
        while (true) {
            std::cout << "\nEnter message (or 'exit' to quit): ";
            std::getline(std::cin, message);

            if (message == "exit") break;

            ws.write(asio::buffer(message));

            beast::flat_buffer buffer;
            ws.read(buffer);
            std::cout << "Server Response: " << beast::buffers_to_string(buffer.data()) << "\n";
        }

        ws.close(websocket::close_code::normal);
    } catch (std::exception& e) {
        std::cerr << "Client error: " << e.what() << "\n";
    }
}

int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "-s" || std::string(argv[i]) == "--server") {
            std::cout << "creating a server...\n";
            try {
                asio::io_context io_context;
                runServer(io_context, 9002);
            } catch (std::exception& e) {
                std::cerr << "Server error: " << e.what() << "\n";
            }
        } else if (std::string(argv[i]) == "-c" || std::string(argv[i]) == "--client") {
            std::cout << "client\n";
            run_client("127.0.0.1", "9002");
        }

        std::cout << "no arguments were given, quitting!\n";
        return 0;
    }
}
