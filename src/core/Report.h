#pragma once

namespace sst::core {
    // Collects what an import/export did, skipped, or could not resolve. Written next to the export as
    // <name>.report.json and summarized in the log.
    class Report {
    public:
        void Missing(std::string_view a_section, const std::string& a_what, std::string_view a_reason);
        void Skipped(std::string_view a_section, const std::string& a_what, std::string_view a_reason);
        void Error(std::string_view a_section, const std::string& a_what, std::string_view a_reason);
        void Verify(std::string_view a_section, const std::string& a_what, std::string_view a_note);
        void Count(std::string_view a_section, std::string_view a_counter, int a_delta = 1);

        nlohmann::json ToJson() const;
        void           LogSummary(std::string_view a_title) const;
        bool           Write(const std::filesystem::path& a_path) const;

    private:
        struct Entry {
            std::string section, what, reason;
        };
        std::vector<Entry>                         _missing, _skipped, _errors, _verify;
        std::map<std::string, std::map<std::string, int>> _counts;
    };
}
