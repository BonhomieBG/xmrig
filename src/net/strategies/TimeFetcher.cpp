/* XMRig-M4P
 * Copyright (c) 2025 BonhomieBG <https://github.com/BonhomieBG>
 * 
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
//Require Curl Library to Work!!!
#include "TimeFetcher.h"
#include <curl/curl.h>
#include <iostream>
#include <regex>
#include <sstream>
#include <iomanip>

namespace xmrig {

TimeFetcher::TimeFetcher() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

TimeFetcher::~TimeFetcher() {
    curl_global_cleanup();
}

size_t TimeFetcher::WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::string TimeFetcher::fetchCurrentTime() {
    CURL* curl;
    CURLcode res;
    std::string readBuffer;

    //Exatract time from this website
    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "https://www.utctime.net");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
    }

    std::string timeStr = parseTimeFromHtml(readBuffer);
    return convertToTimePoint(timeStr); // Convert the time string to time_point
}

std::string TimeFetcher::parseTimeFromHtml(const std::string &html){
    std::regex timePattern(R"((\d{2}:\d{2}:d{2}))");
    std::smatch match;

    if (std::regex_search(html, match, timePattern)) {
        return match.str(1); //Return the first matching time string
    }
    return ""; //return an empty string if no match found
}

std::chrono::system_clock::time_point TimeFetcher::convertToTimePoint(const std::string &timeStr) {
    std::tm tm = {};
    std::stringstream ss(timeStr);
    ss >> std::get_time(&tm, "%H:%M:%S");

    std::time_t time = std::mktime(&tm);
    return std::chrono::system_clock::from_time_t(time);
}

} // namespace xmrig
