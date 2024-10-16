/******************************************************************************
 * Copyright 2022 The CIV Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/
#pragma once
#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>

namespace coop {
namespace v2x {
namespace sc = std::chrono;
using tps = sc::time_point<sc::system_clock, sc::seconds>;
using __week = sc::duration<int32_t, std::ratio<7 * 24 * 3600>>;
using __day = sc::duration<int32_t, std::ratio<24 * 3600>>;
using __ms = sc::milliseconds;
using __us = sc::microseconds;
using __ns = sc::nanoseconds;
using __d_second = sc::duration<double, sc::seconds::period>;
using gpst_t = std::pair<int32_t, double>;
using unix_t = int64_t;

/**
 * @brief Get the Current Unix Time object 获得当前的unix时间，单位s
 *
 * @return constexpr double 当前的unix时间
 */

std::string Unix2YMDHSMS(__int64_t unix_time_ms);

inline double getCurrentUnixTime() {
  return sc::system_clock::now().time_since_epoch().count() / 1e9;
}

// 检查当前时间是否同步
inline bool CheckTimeSync(double tp) { return tp > 1.51e9; }

static constexpr int doy[] = {1,   32,  60,  91,  121, 152,
                              182, 213, 244, 274, 305, 335};
static constexpr int Days(const int y, const int m, const int d) {
  return (y - 1970) * 365 + (y - 1969) / 4 + doy[m - 1] + d - 2 +
         (y % 4 == 0 && m >= 3 ? 1 : 0);
}
static constexpr tps GenerateTimeFromYYMMDD(const int y, const int m,
                                            const int d) {
  return tps(__day(Days(y, m, d)));
}
static constexpr tps gpst0 = GenerateTimeFromYYMMDD(1980, 1, 6);
static constexpr sc::seconds delta_gpst0 = gpst0.time_since_epoch();
static constexpr std::pair<int64_t, int32_t> __leaps[] = {
    {1483228800, -18}, {1435708800, -17}, {1341100800, -16}, {1230768000, -15},
    {1136073600, -14}, {915148800, -13},  {867715200, -12},  {820454400, -11},
    {773020800, -10},  {741484800, -9},   {709948800, -8},   {662688000, -7},
    {631152000, -6},   {567993600, -5},   {489024000, -4},   {425865600, -3},
    {394329600, -2},   {362793600, -1}};
static constexpr int Leaps() { return -18; }
inline int GetLeas(const tps &t_) {
  auto pos = std::find_if(std::begin(__leaps), std::end(__leaps),
                          [&t_](const std::pair<int64_t, int32_t> &row) {
                            return t_.time_since_epoch().count() > row.first;
                          });
  return pos == std::end(__leaps) ? 0 : pos->second;
}

template <typename _Dura = sc::microseconds>
constexpr typename _Dura::rep GPST2Unix(const int32_t w_, const double s_) {
  auto other =
      static_cast<typename _Dura::rep>((s_ + Leaps()) * _Dura::period::den);
  return (delta_gpst0 + __week(w_) + _Dura(other)).count();
}

template <typename _Dura = sc::microseconds>
constexpr typename _Dura::rep GPST2Unix(const gpst_t &gpst_) {
  return GPST2Unix<_Dura>(gpst_.first, gpst_.second);
}

// template <typename _Dura = sc::microseconds>
// constexpr gpst_t Unix2GPST(const int64_t t_)
// {
//   int w = sc::duration_cast<__week>(_Dura(t_) - delta_gpst0 -
//   sc::seconds(Leaps())).count(); double s = (_Dura(t_) - delta_gpst0 -
//   sc::seconds(Leaps()) - __week(w)).count() / (1.0 * _Dura::period::den);
//   return gpst_t(w, s);
// }

template <typename _Dura = sc::microseconds>
gpst_t Unix2GPST(const int64_t t_) {
  auto gpst = _Dura(t_) - delta_gpst0 - sc::seconds(Leaps());
  auto w = gpst / __week(1);
  auto s = __d_second(gpst % __week(1)).count();
  return gpst_t(w, s);
}

template <typename _Dura = sc::microseconds>
inline std::string Unix2TimeStr(const uint64_t t_) {
  time_t t2 = sc::duration_cast<sc::seconds>(_Dura(t_)).count();
  std::ostringstream oss;
  oss << std::put_time(std::gmtime(&t2), "%Y-%m-%d %H:%M:%S");
  oss << '.' << std::setw(log10(_Dura::period::den)) << std::setfill('0')
      << (_Dura(t_) - sc::seconds(t2)).count();
  return oss.str();
}

template <typename _Dura = sc::microseconds>
constexpr int64_t Epoch2Unix(const int y = 1970, const int m = 0,
                             const int d = 0, const int hh = 0,
                             const int mm = 0, const double ss = 0) {
  auto other = static_cast<typename _Dura::rep>(ss * _Dura::period::den);
  return (__day(Days(y, m, d)) + sc::hours(hh) + sc::minutes(mm) + _Dura(other))
      .count();
}

/**
 * @brief GPRMC中的年月日时分秒转unix时间
 *
 * @tparam sc::microseconds
 * @param ddmmyy 日月年
 * @param hhmmss.ss 时分秒.秒
 * @return constexpr int64_t
 */
template <typename _Dura = sc::microseconds>
constexpr int64_t Epoch2Unix(const int32_t ddmmyy, const double hhmmss) {
  int32_t dd = (ddmmyy / 10000) % 10000, mon = (ddmmyy / 100) % 100,
          yy = (ddmmyy) % 100 + 2000;
  int32_t hh = (static_cast<int32_t>(hhmmss) / 10000) % 10000,
          mm = (static_cast<int32_t>(hhmmss) / 100) % 100;
  double ss = (static_cast<int32_t>(hhmmss) % 100) +
              (hhmmss - static_cast<int32_t>(hhmmss));
  return Epoch2Unix<_Dura>(yy, mon, dd, hh, mm, ss);
}

template <typename _Dura = sc::microseconds>
inline int64_t Str2Unix(const std::string &str_) {
  int data[5] = {0};
  double sec = 0;
  int n = sscanf(str_.c_str(), "%d-%d-%d %d:%d:%lf", data, data + 1, data + 2,
                 data + 3, data + 4, &sec);
  return n > 0 ? Epoch2Unix<_Dura>(data[0], data[1], data[2], data[3], data[4],
                                   sec)
               : 0;
}

template <typename _Dura = sc::microseconds>
inline std::string GPST2Str(const gpst_t &t_, bool show_week_ = false) {
  std::ostringstream oss;
  if (show_week_) oss << std::setw(6) << t_.first;
  oss << std::setw(16) << std::fixed
      << std::setprecision(log10(_Dura::period::den)) << t_.second;

  return oss.str();
}

template <typename _Dura = sc::microseconds>
inline std::string GPST2Str(const int32_t w_, const double s_,
                            bool show_week_ = false) {
  return GPST2Str<_Dura>(std::make_pair(w_, s_), show_week_);
}

template <typename _Dura = sc::microseconds>
inline std::string Unix2GPSTStr(const unix_t &t_, bool show_week_ = false) {
  return GPST2Str<_Dura>(Unix2GPST<_Dura>(t_), show_week_);
}

template <typename _Dura = sc::microseconds>
inline std::string Unix2Str(const unix_t &t_) {
  std::ostringstream oss;
  oss << std::setw(log10(_Dura::period::den) + 14) << t_;
  return oss.str();
}

/**
 * @brief 输出unix时间,gps时间
 *
 * @param t_s 以秒为单位的unix时间
 * @return std::string
 */
inline std::string FullTimeString(double t_s) {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(6) << std::setw(20) << t_s
      << Unix2GPSTStr(t_s / 1e-6, true);
  return oss.str();
}

inline std::string UnixTimeString(double t_s) {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(6) << std::setw(15) << t_s;
  return oss.str();
}

}  // namespace v2x

// constexpr tools::unix_t t1 = tools::GPST2Unix(2013, 108301.123456);
// constexpr tools::gpst_t t2 = tools::Unix2GPST(1533535483123456l);
// constexpr tools::gpst_t t4 = tools::Unix2GPST(1533535483123456l);
// constexpr tools::unix_t t3 = tools::Epoch2Unix(2017, 1, 1);
// unix_t t1 = Epoch2Unix(2018, 11, 28, 06, 13, 07);
// unix_t t2 = Epoch2Unix(281118, 61307.30);
// int main()
// {
//     tools::unix_t v1 = tools::Str2Unix("2018-08-06 06:04:43.123456");
//     std::cout << tools::Unix2TimeStr(v1) << std::endl;
//     std::cout << tools::Str2Unix(tools::Unix2TimeStr(v1)) << std::endl;

//     // std::cout << (t1 > gpst0) << std::endl;
//     tools::gpst_t gpst = {2013, 108301.123456};
//     std::cout << tools::GPST2Unix(gpst) << std::endl;
//     std::cout << tools::GPST2Unix(2013, 108301.123456) << std::endl;
//     tools::unix_t unixt = tools::GPST2Unix(gpst);
//     tools::gpst_t gpst2 = tools::Unix2GPST(unixt);
//     std::cout << gpst2.first << " , " << std::fixed << std::setprecision(6)
//     << gpst2.second << std::endl;

//     std::cout << "Hello Easy C++ project!" << std::endl;
// }
}  // namespace coop
