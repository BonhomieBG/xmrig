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

#ifdef  XMRIG_TIMEFETCHER_H
#define XMRIG_TIMEFETCHER_H

//fuction to get real time from website
#include <string>
#include <chrono>

namespace xmrig {

class TimeFetcher
{
    public:
        TimeFetcher();
        ~TimeFetcher();

        std::chrono::system_clock::time_point fetchCurrentTime(); // Returns the current UTC time as a time_point

private:
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp);
    std::string parseTimeFromHtml(const std::string &html); // Function to parse the time from the HTML response
    std::chrono::system_clock::time_point convertToTimePoint(const std::string &timeStr); // Function to convert string to time_point
};
}// namespace xmrig
#endif // XMRIG_TIMEFETCHER_H