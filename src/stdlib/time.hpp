

#pragma once


// includes

#include <chrono>

#include "natives.hpp"

namespace chrono = std::chrono;


// functions

// all utc for now

NATIVE(time_timestamp, "time", "timestamp", {
    params({});
    auto now = chrono::system_clock::now();
    return CaroLong(chrono::duration_cast<chrono::seconds>(now.time_since_epoch()).count());
});
NATIVE(time_timestamp_ms, "time", "timestamp_ms", {
    params({});
    auto now = chrono::system_clock::now();
    return CaroLong(chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()).count());
});

NATIVE(time_year, "time", "year", {
    params({});
    auto now = chrono::system_clock::now();
    chrono::year_month_day ymd{chrono::floor<chrono::days>(now)};
    return CaroInt((int32_t)ymd.year());
});
NATIVE(time_month, "time", "month", {
    params({});
    auto now = chrono::system_clock::now();
    chrono::year_month_day ymd{chrono::floor<chrono::days>(now)};
    return CaroUint((uint32_t)ymd.month());
});
NATIVE(time_day, "time", "day", {
    params({});
    auto now = chrono::system_clock::now();
    chrono::year_month_day ymd{chrono::floor<chrono::days>(now)};
    return CaroUint((uint32_t)ymd.day());
});

NATIVE(time_hour, "time", "hour", {
    params({});
    auto now = chrono::system_clock::now();
    chrono::hh_mm_ss hms{now - floor<chrono::days>(now)};
    return CaroUint((uint32_t)hms.hours().count());
});
NATIVE(time_minute, "time", "minute", {
    params({});
    auto now = chrono::system_clock::now();
    chrono::hh_mm_ss hms{now - floor<chrono::days>(now)};
    return CaroUint((uint32_t)hms.minutes().count());
});
NATIVE(time_second, "time", "second", {
    params({});
    auto now = chrono::system_clock::now();
    chrono::hh_mm_ss hms{now - floor<chrono::days>(now)};
    return CaroUint((uint32_t)hms.seconds().count());
});
NATIVE(time_ms, "time", "ms", {
    params({});
    auto now = chrono::system_clock::now();
    chrono::hh_mm_ss hms{now - floor<chrono::days>(now)};
    return CaroUint((uint32_t)hms.subseconds().count());
});

NATIVE(time_display, "time", "display", {
    // strftime but you don't get to choose
    
    params({
        {{TYPE_BOOL}, false}
    });

    auto now = chrono::system_clock::now();
    chrono::year_month_day ymd{chrono::floor<chrono::days>(now)};
    chrono::hh_mm_ss hms{now - floor<chrono::days>(now)};

    string display;
    if(args.empty() || isFalsey(args[0])) {
        display = format(
            "{:04}/{:02}/{:02} {:02}:{:02}:{:02}",
            (int32_t)ymd.year(), (uint32_t)ymd.month(), (uint32_t)ymd.day(), hms.hours().count(), hms.minutes().count(), hms.seconds().count()
        );
    } else {
        display = format(
            "{:04}/{:02}/{:02} {:02}:{:02}:{:02}.{:03}",
            (int32_t)ymd.year(), (uint32_t)ymd.month(), (uint32_t)ymd.day(), hms.hours().count(), hms.minutes().count(), hms.seconds().count(), hms.subseconds().count()
        );
    }

    return CaroObj(copyString(display));

});