#include "core/Report.h"

namespace sst::core {
    void Report::Missing(std::string_view s, const std::string& w, std::string_view r) { _missing.push_back({ std::string(s), w, std::string(r) }); }
    void Report::Skipped(std::string_view s, const std::string& w, std::string_view r) { _skipped.push_back({ std::string(s), w, std::string(r) }); }
    void Report::Error(std::string_view s, const std::string& w, std::string_view r) { _errors.push_back({ std::string(s), w, std::string(r) }); logger::error("[{}] {}: {}", s, w, r); }
    void Report::Verify(std::string_view s, const std::string& w, std::string_view n) { _verify.push_back({ std::string(s), w, std::string(n) }); }
    void Report::Action(std::string_view s, const std::string& w, std::string_view d) { _actions.push_back({ std::string(s), w, std::string(d) }); }
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
        return { { "counts", _counts }, { "missing", list(_missing) }, { "skipped", list(_skipped) }, { "errors", list(_errors) }, { "verify", list(_verify) }, { "actions", list(_actions) } };
    }

    void Report::LogSummary(std::string_view a_title) const
    {
        logger::info("{}: {} actions, {} missing, {} skipped, {} errors, {} to verify", a_title, _actions.size(), _missing.size(), _skipped.size(), _errors.size(), _verify.size());
        for (const auto& [section, counters] : _counts) {
            std::string line;
            for (const auto& [name, n] : counters) {
                line += std::format(" {}={}", name, n);
            }
            logger::info("  {}:{}", section, line);
        }
    }

    void Report::LogMissingByPlugin() const
    {
        std::map<std::string, std::map<std::string, int>> bySection;
        for (const auto& e : _missing) {
            const auto bar = e.what.find('|');
            bySection[e.section][bar == std::string::npos ? e.what : e.what.substr(0, bar)]++;
        }
        for (const auto& [section, plugins] : bySection) {
            std::vector<std::pair<std::string, int>> v(plugins.begin(), plugins.end());
            std::sort(v.begin(), v.end(), [](auto& a, auto& b) { return a.second > b.second; });
            int total = 0;
            std::string line;
            for (std::size_t i = 0; i < v.size(); ++i) {
                total += v[i].second;
                if (i < 6) {
                    line += std::format("{}{} {}", i ? ", " : "", v[i].first, v[i].second);
                }
            }
            logger::info("  {}: {} missing ({}{})", section, total, line, v.size() > 6 ? ", ..." : "");
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
