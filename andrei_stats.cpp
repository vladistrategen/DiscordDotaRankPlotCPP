#include <dpp/dpp.h>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <date/date.h>
#include <date/tz.h>
#include <matplotlibcpp.h>
#include <dotenv.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

namespace plt = matplotlibcpp;
using json = nlohmann::json;
using namespace date;
using namespace std::chrono;

struct Message {
    system_clock::time_point dateTime;
    int rank;
};

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::vector<Message> fetchMessages(dpp::cluster& bot, const std::string& channel_id) {
    std::vector<Message> messages;
    bot.message_get(channel_id, [&](const dpp::confirmation_callback_t& event) {
        if (event.is_error()) {
            std::cerr << "Error fetching messages: " << event.http_info.body << std::endl;
            return;
        }
        auto jsonResponse = json::parse(event.http_info.body);
        for (const auto& item : jsonResponse) {
            std::string content = item["content"];
            std::string timestamp = item["timestamp"];

            std::regex rank_regex(R"((\d{2}/\d{2}/\d{4})-(\d{2}:\d{2}:\d{2}) - Rank: (\d+))");
            std::smatch match;
            if (std::regex_search(content, match, rank_regex)) {
                std::string date_str = match[1];
                std::string time_str = match[2];
                int rank = std::stoi(match[3]);

                std::istringstream in(date_str + " " + time_str);
                sys_time<milliseconds> tp;
                in >> parse("%d/%m/%Y %H:%M:%S", tp);
                auto tp_local = make_zoned(current_zone(), tp).get_local_time();

                messages.push_back({ tp_local, rank });
            }
        }
    });
    return messages;
}

void plotRankEvolution(const std::vector<Message>& messages, bool inverted, bool detailed) {
    std::vector<double> x, y;
    for (const auto& msg : messages) {
        x.push_back(duration_cast<seconds>(msg.dateTime.time_since_epoch()).count());
        y.push_back(msg.rank);
    }

    plt::figure_size(1920, 1080);
    plt::plot(x, y);

    if (detailed) {
        std::map<year_month, std::vector<int>> monthlyRanks;
        for (const auto& msg : messages) {
            auto ymd = floor<days>(msg.dateTime);
            monthlyRanks[year_month{ ymd.year(), ymd.month() }].push_back(msg.rank);
        }

        for (const auto& [month, ranks] : monthlyRanks) {
            if (ranks.empty()) continue;
            auto minmax = std::minmax_element(ranks.begin(), ranks.end());
            plt::scatter({ duration_cast<seconds>(month.time_since_epoch()).count() }, { *minmax.first }, 100.0, { "red" });
            plt::scatter({ duration_cast<seconds>(month.time_since_epoch()).count() }, { *minmax.second }, 100.0, { "green" });
        }
    }

    plt::xlabel("Date and Time");
    plt::ylabel("Rank");
    plt::title("Rank Evolution Over Time");
    plt::grid(true);
    plt::ylim(0, 1000);

    if (inverted) {
        plt::gca().invert_yaxis();
    }

    plt::save("rank_evolution.png");
    plt::show();
}

int main(int argc, char* argv[]) {
    dotenv::env.load_dotenv(".env");

    std::string token = getenv("DISCORD_BOT_TOKEN");
    std::string channel_id = getenv("DISCORD_CHANNEL_ID");

    dpp::cluster bot(token);

    bool inverted = false;
    bool detailed = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--inverted" || arg == "-i") {
            inverted = true;
        } else if (arg == "--detailed" || arg == "-d") {
            detailed = true;
        }
    }

    bot.on_log(dpp::utility::cout_logger());

    bot.on_ready([&bot, &channel_id, inverted, detailed](const dpp::ready_t& event) {
        if (dpp::run_once<struct fetch_messages>()) {
            auto messages = fetchMessages(bot, channel_id);
            plotRankEvolution(messages, inverted, detailed);
        }
    });

    bot.start(false);

    return 0;
}
