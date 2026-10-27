#include "world/World.h"

#include "core/Paths.h"

namespace sst::world {
    namespace {
        std::vector<QuestRule> g_rules;
        bool                   g_loaded = false;

        char Lower(char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }
    }

    bool GlobMatch(std::string_view p, std::string_view t)
    {
        std::size_t pi = 0, ti = 0, star = std::string_view::npos, mark = 0;
        while (ti < t.size()) {
            if (pi < p.size() && (p[pi] == '?' || Lower(p[pi]) == Lower(t[ti]))) {
                ++pi; ++ti;
            } else if (pi < p.size() && p[pi] == '*') {
                star = pi++;
                mark = ti;
            } else if (star != std::string_view::npos) {
                pi = star + 1;
                ti = ++mark;
            } else {
                return false;
            }
        }
        while (pi < p.size() && p[pi] == '*') {
            ++pi;
        }
        return pi == p.size();
    }

    const std::vector<QuestRule>& LoadQuestRules()
    {
        if (g_loaded) {
            return g_rules;
        }
        g_loaded = true;
        std::error_code ec;
        const auto dir = core::Paths::OverridesDir();
        for (const auto& e : std::filesystem::directory_iterator(dir, ec)) {
            if (!e.is_regular_file() || e.path().extension() != ".json") {
                continue;
            }
            try {
                std::ifstream in(e.path());
                const auto j = nlohmann::json::parse(in);
                for (const auto& r : j.value("rules", nlohmann::json::array())) {
                    QuestRule rule;
                    rule.pattern = r.value("match", "");
                    rule.mode = r.value("mode", "skip");
                    rule.reason = r.value("reason", "");
                    rule.sourceFile = e.path().filename().string();
                    if (r.contains("onlyStages")) {
                        r.at("onlyStages").get_to(rule.onlyStages);
                    }
                    if (!rule.pattern.empty()) {
                        g_rules.push_back(std::move(rule));
                    }
                }
            } catch (const std::exception& ex) {
                logger::error("overrides: cannot parse {}: {}", e.path().string(), ex.what());
            }
        }
        logger::info("overrides: {} quest rules loaded from {}", g_rules.size(), dir.string());
        return g_rules;
    }

    const QuestRule* FindQuestRule(std::string_view a_editorId)
    {
        for (const auto& r : LoadQuestRules()) {
            if (GlobMatch(r.pattern, a_editorId)) {
                return &r;
            }
        }
        return nullptr;
    }
}
