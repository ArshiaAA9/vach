#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/beast.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "whisper.hpp"

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
using tcp = boost::asio::ip::tcp;

// const std::string LOCAL_IP = "192.168.0.103"; // FOR DEBUG PURPOSES
const unsigned short PORT = 9002;
const size_t CHUNK_SAMPLES = 16000 * 3;

enum command {
    NEXT_SONG,
    PREV_SONG,
    STOP_SONG,
    PLAY_SONG,
};

asio::ip::address getLocalIp() {
    boost::asio::io_context io;

    boost::asio::ip::udp::socket socket(io);
    socket.open(boost::asio::ip::udp::v4());

    // Doesn't actually send any packets
    socket.connect(boost::asio::ip::udp::endpoint(boost::asio::ip::make_address("8.8.8.8"), 53));
    asio::ip::address localIP = socket.local_endpoint().address();

    std::cout << "Local IP: " << localIP.to_string() << '\n';

    return localIP;
}

std::string parse_command(const std::string& text) {
    std::string lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower.find("next") != std::string::npos) return "next_song";
    if (lower.find("previous") != std::string::npos) return "prev_song";
    if (lower.find("back") != std::string::npos) return "prev_song";
    if (lower.find("pause") != std::string::npos) return "pause";
    if (lower.find("play") != std::string::npos) return "play";
    if (lower.find("stop") != std::string::npos) return "stop";
    return "";
}

// Optionally dump received PCM to disk so you can inspect it with Audacity:
//   File → Import → Raw Data → Signed 16-bit PCM, Little-Endian, Mono, 16000 Hz
static void save_pcm_chunk(const void* data, std::size_t bytes, const std::string& filename = "received.pcm") {
    std::ofstream file(filename, std::ios::binary | std::ios::app);
    if (file) file.write(static_cast<const char*>(data), bytes);
}

void websocket_session(tcp::socket socket, const Whisper& whisper) {
    try {
        websocket::stream<tcp::socket> ws(std::move(socket));
        ws.binary(true);
        ws.accept();
        std::cout << "[Server] Client connected!\n";

        beast::flat_buffer buffer;
        std::vector<int16_t> accumulator;
        std::size_t total_bytes = 0;
        auto t_start = std::chrono::steady_clock::now();

        while (true) {
            buffer.consume(buffer.size()); // clear before each read
            ws.read(buffer);

            auto payload = buffer.data();
            std::size_t n = beast::buffer_bytes(payload);
            total_bytes += n;

            const uint8_t* data = static_cast<const uint8_t*>(payload.data());
            size_t bytes = beast::buffer_bytes(payload);

            for (size_t i = 0; i + 1 < bytes; i += 2) {
                int16_t sample = (int16_t)(data[i] | (data[i + 1] << 8));
                accumulator.push_back(sample);
            }

            // Bandwidth stats every 1 second
            auto now = std::chrono::steady_clock::now();
            double secs = std::chrono::duration<double>(now - t_start).count();
            if (secs >= 1.0) {
                double kbps = (total_bytes / 1024.0) / secs;
                std::cout << "[audio] received " << total_bytes << " bytes  (" << kbps << " KB/s)\n";
                total_bytes = 0;
                t_start = now;
            }

            if (accumulator.size() < CHUNK_SAMPLES) continue;
            // 1. convert to float
            std::vector<float> pcmf32(accumulator.size());
            for (size_t i = 0; i < accumulator.size(); i++) {
                pcmf32[i] = accumulator[i] / 32768.0f;
            }
            accumulator.clear();

            auto result = whisper.transcribe(pcmf32);
            if (!result.has_value()) continue;

            std::string transcript = result.value();
            std::string cmd = parse_command(transcript);

            if (!cmd.empty()) std::cout << "[command] -> " << cmd << "\n";

            // Send response back to phone
            std::string response = cmd.empty() ? "{\"command\":null,\"transcript\":\"" + transcript + "\"}"
                                               : "{\"command\":\"" + cmd + "\",\"transcript\":\"" + transcript + "\"}";

            // write to file
            for (auto it = asio::buffer_sequence_begin(payload); it != asio::buffer_sequence_end(payload); ++it) {
                save_pcm_chunk(it->data(), it->size());
            }
            // // Sends a small JSON text frame so the phone knows the server is alive.
            // std::string ack = "{\"status\":\"ok\",\"bytes\":" + std::to_string(n) + "}";
            ws.binary(true);
            ws.write(asio::buffer(response));
        }
    }

    catch (beast::system_error const& se) {
        if (se.code() != websocket::error::closed) std::cerr << "[session] error: " << se.what() << "\n";
        else
            std::cout << "[session] Client disconnected cleanly.\n";
    }

    catch (std::exception& e) {
        std::cerr << "[session] exception: " << e.what() << "\n";
    }
}

void runServer(
    asio::io_context& ioc, const asio::ip::address& serverIp, const unsigned short port, const Whisper& whisper) {
    std::cout << "[server] trying to create a socket on " << serverIp << ':' << port << '\n';
    tcp::endpoint endpoint(serverIp, port);
    tcp::acceptor acceptor(ioc, endpoint);

    std::cout << "[server] listening on ws://" << serverIp << ':' << port << "\n\n";

    while (true) {
        tcp::socket socket(ioc);
        acceptor.accept(socket);
        std::thread(websocket_session, std::move(socket), std::ref(whisper)).detach(); // Handle client in new thread
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
                  << "  " << argv[0] << " -c   (run test client for debugging)\n";
        return 0;
    }

    std::string arg = argv[1];

    if (arg == "-s" || arg == "--server") {
        std::cout << "creating a server...\n";
        try {
            Whisper whisper{};
            asio::io_context io_context;
            runServer(io_context, getLocalIp(), PORT, whisper);
        } catch (std::exception& e) {
            std::cerr << "Server error: " << e.what() << "\n";
        }
    } else if (arg == "-c" || arg == "--client") {
        std::cout << "client\n";
        run_client(getLocalIp().to_string(), std::to_string(PORT));
    } else {
        std::cerr << "Unknown argument: " << arg << "\n";
        return 1;
    }

    return 0;
}
