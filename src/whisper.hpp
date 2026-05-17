#pragma once
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "whisper.h"

class Whisper {
public:
    Whisper() {
        cparams.use_gpu = true;
        ctx = whisper_init_from_file_with_params("../whisper.cpp/models/ggml-base.en.bin", cparams);

        if (!ctx) {
            std::cerr << "[whisper] failed to load model\n";
            // ctx stays null — transcribe() checks for this
        } else {
            std::cout << "[whisper] model loaded\n";
        }

        wparams.language = "en";
        wparams.n_threads = 4;
        wparams.print_progress = false;
        wparams.print_timestamps = false;
        wparams.single_segment = true;
        wparams.no_context = true;
    };

    ~Whisper() {
        if (ctx) whisper_free(ctx);
    }

    // Returns the transcript, or std::nullopt on failure/empty
    inline std::optional<std::string> transcribe(std::vector<float>& pcmf32) const {
        if (whisper_full(ctx, wparams, pcmf32.data(), pcmf32.size()) != 0) {
            std::cerr << "[whisper] inference failed\n";
            return std::nullopt;
        }

        std::string transcript;
        int n_seg = whisper_full_n_segments(ctx);
        for (int i = 0; i < n_seg; i++) transcript += whisper_full_get_segment_text(ctx, i);

        // Trim whitespace
        size_t start = transcript.find_first_not_of(" \t\n\r");
        size_t end = transcript.find_last_not_of(" \t\n\r");

        if (start == std::string::npos) return std::nullopt;

        transcript = transcript.substr(start, end - start + 1);

        std::cout << "[whisper] \"" << transcript << "\"\n";
        return transcript;
    }

private:
    whisper_context_params cparams = whisper_context_default_params();
    whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    whisper_context* ctx;
};
