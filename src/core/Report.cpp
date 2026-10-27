#include "core/Report.h"

namespace sst::core {
    void Report::Missing(std::string_view s, const std::string& w, std::string_view r) { _missing.push_back({ std::string(s), w, std::string(r) }); }
    void Report::Skipped(std::string_view s, const std::string& w, std::string_view r) { _skipped.push_back({ std::string(s), w, std::string(r) }); }
    void Report::Error(std::string_view s, const std::string& w, std::string_view r) { _errors.push_back({ std::string(s), w, std::string(r) }); logger::error("[{}] {}: {}", s, w, r); }
    void Report::Verify(std::string_view s, const std::string& w, std::string_view n) { _verify.push_back({ std::string(s), w, std::string(n) }); }
    void Report::Count(std::string_view s, std::string_view c, int d) { _counts[std::string(s)][std::string(c)] += d; }

    nlohmann::json Report::ToJson() const
    {
        auto list = [](const std::vector<Entry>& v) {
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& e : v) {
                arr.push_back({ { "section", e.section }, { "what", e.what }, { "reason", e.reason } });
            }
            return arr;
        };
        return { { "counts", _counts }, { "missing", list(_missing) }, { "skipped", list(_skipped) }, { "errors", list(_errors) }, { "verify", list(_verify) } };
    }

    void Report::LogSummary(std::string_view a_title) const
    {
        logger::info("{}: {} missing, {} skipped, {} errors, {} to verify", a_title, _missing.size(), _skipped.size(), _errors.size(), _verify.size());
        for (const auto& [section, counters] : _counts) {
            std::string line;
            for (const auto& [name, n] : counters) {
                line += std::format(" {}={}", name, n);
            }
            logger::info("  {}:{}", section, line);
        }
    }

    bool Report::Write(const std::filesystem::path& a_path) const
    {
        std::ofstream out(a_path);
        if (!out) {
            logger::error("could not write report {}", a_path.string());
            return false;
        }
        out << ToJson().dump(2);
        return true;
    }
}
