/**
 * @file server_main.cpp
 * @brief Entry point of the headless server.
 *
 * No graphics, only networking and the engine: it starts, listens, and hosts one
 * match at a time under its own authority.
 *
 *     HEX_Server [port] [board_size]
 */

#include <charconv>
#include <iostream>
#include <string_view>

#include "server/game_server.h"
#include "core/save_format.h"

namespace {

    /**
     * @brief Parses an integer command-line argument.
     * @param text Argument text.
     * @param min Lowest accepted value.
     * @param max Highest accepted value.
     * @param out Receives the value only when the argument is valid.
     * @return false for a non-numeric or out-of-range argument, so the server
     * refuses to start rather than running with a nonsensical setting.
     */
    bool parseInt(const std::string_view text, const long min, const long max, long& out) {
        long value = 0;
        const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);

        if (ec != std::errc{} || end != text.data() + text.size()) return false;
        if (value < min || value > max) return false;

        out = value;
        return true;
    }
}

int main(const int argc, char** argv) {
    // The log is the only window into a headless process. Redirected to a pipe or
    // a file, stdout is block-buffered and would show nothing until the server
    // exits, which is precisely when it is no longer useful.
    std::cout << std::unitbuf;

    long port = hexnet::DEFAULT_PORT;
    long board_size = 11;

    if (argc > 1 && !parseInt(argv[1], 1024, 65535, port)) {
        std::cerr << "porta non valida: " << argv[1] << " (attesa fra 1024 e 65535)\n";
        return 1;
    }
    if (argc > 2 && !parseInt(argv[2], hexsave::MIN_BOARD_SIZE, hexsave::MAX_BOARD_SIZE, board_size)) {
        std::cerr << "lato della scacchiera non valido: " << argv[2] << "\n";
        return 1;
    }

    hexnet::GameServer server(static_cast<unsigned short>(port), static_cast<int>(board_size));
    if (!server.listen()) return 1;

    std::cout << "[server] scacchiera " << board_size << "x" << board_size
              << ", in attesa di due giocatori (Ctrl+C per fermare)\n";

    server.run();
    return 0;
}
