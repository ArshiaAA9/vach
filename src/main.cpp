#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/beast.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
using tcp = asio::ip::tcp;

const std::string LOCAL_IP = "192.168.0.101";
const std::string LOCAL_IP_ON_HOTSPOT = "10.135.79.7";
const std::string HOST_IP = LOCAL_IP_ON_HOTSPOT;
const unsigned short PORT = 9002;

// Optionally dump received PCM to disk so you can inspect it with Audacity:
//   File → Import → Raw Data → Signed 16-bit PCM, Little-Endian, Mono, 44100 Hz
static void save_pcm_chunk(const void* data, std::size_t bytes, const std::string& filename = "received.pcm") {
    std::ofstream file(filename, std::ios::binary | std::ios::app);
    if (file) file.write(static_cast<const char*>(data), bytes);
}

void websocket_session(tcp::socket socket) {
    try {
        websocket::stream<tcp::socket> ws(std::move(socket));
        ws.binary(true);
        ws.accept();
        std::cout << "[Server] Client connected!\n";

        beast::flat_buffer buffer;
        std::size_t total_bytes = 0;
        auto t_start = std::chrono::steady_clock::now();

        while (true) {
            buffer.consume(buffer.size()); // clear before each read
            ws.read(buffer);

            auto payload = buffer.data();
            std::size_t n = beast::buffer_bytes(payload);
            total_bytes += n;

            // ── Progress log: print stats every ~1 second ──────────────────
            auto now = std::chrono::steady_clock::now();
            double secs = std::chrono::duration<double>(now - t_start).count();
            if (secs >= 1.0) {
                double kbps = (total_bytes / 1024.0) / secs;
                std::cout << "[audio] received " << total_bytes << " bytes  (" << kbps << " KB/s)\n";
                total_bytes = 0;
                t_start = now;
            }

            // ── Optional: save to disk for offline inspection ───────────────
            // Comment this out when you no longer need it.
            const std::string raw = beast::buffers_to_string(payload);
            save_pcm_chunk(raw.data(), n);

            // ── Acknowledge back to phone ───────────────────────────────────
            // Sends a small JSON text frame so the phone knows the server is alive.
            // We switch back to text mode just for this write, then back to binary.
            std::string ack = "{\"status\":\"ok\",\"bytes\":" + std::to_string(n) + "}";
            ws.binary(true);
            ws.write(asio::buffer(ack));
        }
    } catch (beast::system_error const& se) {
        if (se.code() != websocket::error::closed) std::cerr << "[session] error: " << se.what() << "\n";
        else
            std::cout << "[session] Client disconnected cleanly.\n";
    } catch (std::exception& e) {
        std::cerr << "[session] exception: " << e.what() << "\n";
    }
}

void runServer(asio::io_context& ioc, const std::string& hostIp, const unsigned short port) {
    std::cout << "[server] trying to create a socket on " << hostIp << ':' << port << '\n';
    asio::ip::address serverIp = asio::ip::make_address(hostIp);
    tcp::endpoint endpoint(serverIp, port);
    tcp::acceptor acceptor(ioc, endpoint);

    std::cout << "[server] listening on ws://" << hostIp << ':' << port << "\n\n";

    while (true) {
        tcp::socket socket(ioc);
        acceptor.accept(socket);
        std::thread(websocket_session, std::move(socket)).detach(); // Handle client in new thread
    }
}

// for testing
void run_client(const std::string& host, const std::string& port) {
    try {
        asio::io_context ioc;
        tcp::resolver resolver(ioc);
        websocket::stream<tcp::socket> ws(ioc);

        auto const results = resolver.resolve(host, port);
        asio::connect(ws.next_layer(), results);
        ws.handshake(host, "/");

        std::cout << "[client] connected!\n";

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
    if (argc < 2) {
        std::cout << "Usage:\n"
                  << "  " << argv[0] << " -s   (run as server)\n"
                  << "  " << argv[0] << " -c   (run test client)\n";
        return 0;
    }

    std::string arg = argv[1];

    if (arg == "-s" || arg == "--server") {
        std::cout << "creating a server...\n";
        try {
            asio::io_context io_context;
            runServer(io_context, HOST_IP, PORT);
        } catch (std::exception& e) {
            std::cerr << "Server error: " << e.what() << "\n";
        }
    } else if (arg == "-c" || arg == "--client") {
        std::cout << "client\n";
        run_client(HOST_IP, std::to_string(PORT));
    } else {
        std::cerr << "Unknown argument: " << arg << "\n";
        return 1;
    }

    return 0;
}
