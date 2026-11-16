/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2000, 2001, 2002, 2003 RiskMap srl
 Copyright (C) 2003, 2004, 2005, 2006 StatPro Italia srl
 Copyright (C) 2004, 2005, 2006 Ferdinando Ametrano
 Copyright (C) 2006 Katiuscia Manzoni
 Copyright (C) 2006 Toyin Akin
 Copyright (C) 2015 Klaus Spanderen
 Copyright (C) 2020 Leonardo Arcari
 Copyright (C) 2020 Kline s.r.l.

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <https://www.quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

/*! \file date.hpp
    \brief date- and time-related classes, typedefs and enumerations
*/

#ifndef quantlib_date_hpp
#define quantlib_date_hpp

#include <ql/errors.hpp>
#include <ql/time/period.hpp>
#include <ql/time/weekday.hpp>
#include <ql/utilities/null.hpp>

#ifdef QL_HIGH_RESOLUTION_DATE
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/date_time/posix_time/posix_time_duration.hpp>
#endif

#include <cstdint>
#include <utility>
#include <functional>
#include <string>


namespace QuantLib {

    //! Day number
    /*! \ingroup datetime */
    typedef Integer Day;

    //! Month names
    /*! \ingroup datetime */
    enum Month { January   = 1,
                 February  = 2,
                 March     = 3,
                 April     = 4,
                 May       = 5,
                 June      = 6,
                 July      = 7,
                 August    = 8,
                 September = 9,
                 October   = 10,
                 November  = 11,
                 December  = 12,
                 Jan = 1,
                 Feb = 2,
                 Mar = 3,
                 Apr = 4,
                 Jun = 6,
                 Jul = 7,
                 Aug = 8,
                 Sep = 9,
                 Oct = 10,
                 Nov = 11,
                 Dec = 12
    };

    /*! \relates Month */
    std::ostream& operator<<(std::ostream&, Month);

    //! Year number
    /*! \ingroup datetime */
    typedef Integer Year;

    //! Day of the month, month and year of a date
    /*! \ingroup datetime */
    struct DayMonthYear {
        Day day;
        Month month;
        Year year;
    };

#ifdef QL_HIGH_RESOLUTION_DATE
    //! Hour number
    /*! \ingroup datetime */
    typedef boost::posix_time::hours::hour_type Hour;

    //! Minute number
    /*! \ingroup datetime */
    typedef boost::posix_time::minutes::min_type Minute;

    //! Second number
    /*! \ingroup datetime */
    typedef boost::posix_time::minutes::sec_type Second;

    //! Millisecond number
    /*! \ingroup datetime */
    typedef boost::posix_time::time_duration::fractional_seconds_type
        Millisecond;

    //! Microsecond number
    /*! \ingroup datetime */
    typedef boost::posix_time::time_duration::fractional_seconds_type
        Microsecond;
#endif

    //! Concrete date class
    /*! This class provides methods to inspect dates as well as methods and
        operators which implement a limited date algebra (increasing and
        decreasing dates, and calculating their difference).

        \ingroup datetime

        \test self-consistency of dates, serial numbers, days of
              month, months, and weekdays is checked over the whole
              date range.
    */

    class Date {
      public:
        //! serial number type
        typedef std::int_fast32_t serial_type;
        //! \name constructors
        //@{
        //! Default constructor returning a null date.
        Date() noexcept;
        //! Constructor taking a serial number as given by Applix or Excel.
        explicit Date(Date::serial_type serialNumber);
        //! More traditional constructor.
        Date(Day d, Month m, Year y);

#ifdef QL_HIGH_RESOLUTION_DATE
        //! Constructor taking boost posix date time object
        explicit Date(const boost::posix_time::ptime& localTime);
        //! More traditional constructor.
        Date(Day d, Month m, Year y,
             Hour hours, Minute minutes, Second seconds,
             Millisecond millisec = 0, Microsecond microsec = 0);
#endif
        //@}

        //! \name inspectors
        //@{
        Weekday weekday() const;
        Day dayOfMonth() const;
        //! One-based (Jan 1st = 1)
        Day dayOfYear() const;
        Month month() const;
        Year year() const;
        //! Day of the month, month and year, computed together
        DayMonthYear dayMonthYear() const;
        Date::serial_type serialNumber() const;

#ifdef QL_HIGH_RESOLUTION_DATE
        Hour hours() const;
        Minute minutes() const;
        Second seconds() const;
        Millisecond milliseconds() const;
        Microsecond microseconds() const;

        Time fractionOfDay() const;
        Time fractionOfSecond() const;

        const boost::posix_time::ptime& dateTime() const;
#endif
        //@}

        //! \name date algebra
        //@{
        //! increments date by the given number of days
        Date& operator+=(Date::serial_type days);
        //! increments date by the given period
        Date& operator+=(const Period&);
        //! decrement date by the given number of days
        Date& operator-=(Date::serial_type days);
        //! decrements date by the given period
        Date& operator-=(const Period&);
        //! 1-day pre-increment
        Date& operator++();
        //! 1-day post-increment
        Date operator++(int );
        //! 1-day pre-decrement
        Date& operator--();
        //! 1-day post-decrement
        Date operator--(int );
        //! returns a new date incremented by the given number of days
        Date operator+(Date::serial_type days) const;
        //! returns a new date incremented by the given period
        Date operator+(const Period&) const;
        //! returns a new date decremented by the given number of days
        Date operator-(Date::serial_type days) const;
        //! returns a new date decremented by the given period
        Date operator-(const Period&) const;
        //@}

        //! \name static methods
        //@{
        //! today's date.
        static Date todaysDate();
        //! earliest allowed date
        static Date minDate();
        //! latest allowed date
        static Date maxDate();
        //! whether the given year is a leap one
        static bool isLeap(Year y);
        //! first day of the month to which the given date belongs
        static Date startOfMonth(const Date& d);
        //! whether a date is the first day of its month
        static bool isStartOfMonth(const Date& d);
        //! last day of the month to which the given date belongs
        static Date endOfMonth(const Date& d);
        //! whether a date is the last day of its month
        static bool isEndOfMonth(const Date& d);
        //! next given weekday following or equal to the given date
        /*! E.g., the Friday following Tuesday, January 15th, 2002
            was January 18th, 2002.

            see http://www.cpearson.com/excel/DateTimeWS.htm
        */
        static Date nextWeekday(const Date& d,
                                Weekday w);
        //! n-th given weekday in the given month and year
        /*! E.g., the 4th Thursday of March, 1998 was March 26th,
            1998.

            see http://www.cpearson.com/excel/DateTimeWS.htm
        */
        static Date nthWeekday(Size n,
                               Weekday w,
                               Month m,
                               Year y);

#ifdef QL_HIGH_RESOLUTION_DATE
        //! local date time, based on the time zone settings of the computer
        static Date localDateTime();
        //! UTC date time
        static Date universalDateTime();

        //! underlying resolution of the  posix date time object
        static boost::posix_time::time_duration::tick_type ticksPerSecond();
#endif

        //@}

      private:
#ifdef QL_HIGH_RESOLUTION_DATE
        static const boost::gregorian::date& serialNumberDateReference() noexcept {
            // Boost 1.74 fixed constexpr construction of dates.
#if BOOST_VERSION >= 107400
            static constexpr
#else
            static const
#endif
                boost::gregorian::date dateReference(1899, boost::gregorian::Dec, 30);
            return dateReference;
        }

        boost::posix_time::ptime dateTime_;
#else
        Date::serial_type serialNumber_;
        static Date advance(const Date& d, Integer units, TimeUnit);
        static Integer monthLength(Month m, bool leapYear);
        static Integer monthOffset(Month m, bool leapYear);
        static Date::serial_type yearOffset(Year y);
        static const Integer MonthLength[12];
        static const Integer MonthLeapLength[12];
        static const Date::serial_type YearOffset[301];
        static Integer lengthOfMonth(Month m, bool leapYear);
        static Date::serial_type offsetOfYear(Year y);
        static Year yearOf(Date::serial_type serialNumber);
#endif
    };

    /*! \relates Date
        \brief Difference in days between dates
    */
    Date::serial_type operator-(const Date&, const Date&);
    /*! \relates Date
        \brief Difference in days (including fraction of days) between dates
    */
    Time daysBetween(const Date&, const Date&);

    /*! \relates Date */
    bool operator==(const Date&, const Date&);
    /*! \relates Date */
    bool operator!=(const Date&, const Date&);
    /*! \relates Date */
    bool operator<(const Date&, const Date&);
    /*! \relates Date */
    bool operator<=(const Date&, const Date&);
    /*! \relates Date */
    bool operator>(const Date&, const Date&);
    /*! \relates Date */
    bool operator>=(const Date&, const Date&);

    /*!
      Compute a hash value of @p d.

      This method makes Date hashable via <tt>boost::hash</tt>.

      Example:

      \code{.cpp}
      #include <unordered_set>

      std::unordered_set<Date> set;
      Date d = Date(1, Jan, 2020); 

      set.insert(d);
      assert(set.count(d)); // 'd' was added to 'set'
      \endcode

      \param [in] d Date to hash
      \return A hash value of @p d
      \relates Date
    */
    std::size_t hash_value(const Date& d);

    /*! \relates Date */
    std::ostream& operator<<(std::ostream&, const Date&);

    namespace detail {

        // serial numbers of Date::minDate() and Date::maxDate()
        inline constexpr Date::serial_type minimumSerialNumber = 367;      // Jan 1st, 1901
        inline constexpr Date::serial_type maximumSerialNumber = 109574;   // Dec 31st, 2199

        // Neri and Schneider's algorithm
        // "Euclidean affine functions and their application to calendar algorithms"
        inline DayMonthYear dayMonthYearOf(Date::serial_type serialNumber) {
            QL_REQUIRE(serialNumber != 0, "null date");
            constexpr std::uint32_t s = 82;
            constexpr std::uint32_t K = 719468 + 146097 * s;
            constexpr std::uint32_t L = 400 * s;
            // serial number 25569 is January 1st, 1970
            const std::uint32_t N = std::uint32_t(serialNumber - 25569) + K;
            const std::uint32_t N_1 = 4 * N + 3;
            const std::uint32_t C = N_1 / 146097;
            const std::uint32_t N_C = N_1 % 146097 / 4;
            const std::uint32_t N_2 = 4 * N_C + 3;
            const std::uint64_t P_2 = std::uint64_t(2939745) * N_2;
            const std::uint32_t Z = std::uint32_t(P_2 / 4294967296);
            const std::uint32_t N_Y = std::uint32_t(P_2 % 4294967296) / 2939745 / 4;
            const std::uint32_t Y = 100 * C + Z;
            const std::uint32_t N_3 = 2141 * N_Y + 197913;
            const std::uint32_t M = N_3 / 65536;
            const std::uint32_t D = N_3 % 65536 / 2141;
            const std::uint32_t J = N_Y >= 306;
            return {Day(D + 1), Month(J ? M - 12 : M), Year(Y - L + J)};
        }

        inline bool isLeapYear(Year y) {
            // 1900 is leap in agreement with Excel's bug
            // 1900 is out of valid date range anyway
            // given y % 100 == 0, y % 400 == 0 is equivalent to y % 16 == 0;
            return y % 4 == 0 && (y % 100 != 0 || y % 16 == 0 || y == 1900);
        }

        struct short_date_holder {
            explicit short_date_holder(const Date d) : d(d) {}
            Date d;
        };
        std::ostream& operator<<(std::ostream&, const short_date_holder&);

        struct long_date_holder {
            explicit long_date_holder(const Date& d) : d(d) {}
            Date d;
        };
        std::ostream& operator<<(std::ostream&, const long_date_holder&);

        struct iso_date_holder {
            explicit iso_date_holder(const Date& d) : d(d) {}
            Date d;
        };
        std::ostream& operator<<(std::ostream&, const iso_date_holder&);

        struct formatted_date_holder {
            formatted_date_holder(const Date& d, std::string f) : d(d), f(std::move(f)) {}
            Date d;
            std::string f;
        };
        std::ostream& operator<<(std::ostream&,
                                 const formatted_date_holder&);

#ifdef QL_HIGH_RESOLUTION_DATE
        struct iso_datetime_holder {
            explicit iso_datetime_holder(const Date& d) : d(d) {}
            Date d;
        };
        std::ostream& operator<<(std::ostream&, const iso_datetime_holder&);
#endif
    }

    namespace io {

        //! output dates in short format (mm/dd/yyyy)
        /*! \ingroup manips */
        detail::short_date_holder short_date(const Date&);

        //! output dates in long format (Month ddth, yyyy)
        /*! \ingroup manips */
        detail::long_date_holder long_date(const Date&);

        //! output dates in ISO format (yyyy-mm-dd)
        /*! \ingroup manips */
        detail::iso_date_holder iso_date(const Date&);

        //! output dates in user defined format using boost date functionality
        /*! \ingroup manips */
        detail::formatted_date_holder formatted_date(const Date&,
                                                     const std::string& fmt);

#ifdef QL_HIGH_RESOLUTION_DATE
        //! output datetimes in ISO format (YYYY-MM-DDThh:mm:ss,SSSSSS)
        /*! \ingroup manips */
        detail::iso_datetime_holder iso_datetime(const Date&);
#endif

    }


    // inline definitions

    inline bool Date::isStartOfMonth(const Date& d) {
       return (d.dayOfMonth() == 1);
    }

#ifndef QL_HIGH_RESOLUTION_DATE
    inline Date::Date() noexcept
    : serialNumber_(Date::serial_type(0)) {}

    inline Weekday Date::weekday() const {
        Integer w = serialNumber_ % 7;
        return Weekday(w == 0 ? 7 : w);
    }

    inline Integer Date::lengthOfMonth(Month m, bool leapYear) {
        return (leapYear? MonthLeapLength[m-1] : MonthLength[m-1]);
    }

    inline Date::serial_type Date::offsetOfYear(Year y) {
        return YearOffset[y-1900];
    }

    inline Year Date::yearOf(Date::serial_type serialNumber) {
        // 1461 / 4 is 365.25
        Year y = std::uint32_t(serialNumber) * 4 / 1461 + 1900;
        // yearOffset(y) is December 31st of the preceding year
        if (serialNumber <= offsetOfYear(y))
            --y;
        return y;
    }

    inline DayMonthYear Date::dayMonthYear() const {
        return detail::dayMonthYearOf(serialNumber_);
    }

    inline Day Date::dayOfMonth() const {
        return dayMonthYear().day;
    }

    inline Month Date::month() const {
        return dayMonthYear().month;
    }

    inline Year Date::year() const {
        QL_REQUIRE(serialNumber_ != 0, "null date");
        return yearOf(serialNumber_);
    }

    inline Day Date::dayOfYear() const {
        QL_REQUIRE(serialNumber_ != 0, "null date");
        return serialNumber_ - offsetOfYear(yearOf(serialNumber_));
    }

    inline Date::serial_type Date::serialNumber() const {
        return serialNumber_;
    }

    inline Date Date::operator+(Date::serial_type days) const {
        return Date(serialNumber_+days);
    }

    inline Date Date::operator-(Date::serial_type days) const {
        return Date(serialNumber_-days);
    }

    inline Date Date::operator+(const Period& p) const {
        return advance(*this,p.length(),p.units());
    }

    inline Date Date::operator-(const Period& p) const {
        return advance(*this,-p.length(),p.units());
    }

    inline Date Date::startOfMonth(const Date& d) {
        // the start of a valid date's month is always a valid date, so no check is needed
        Date result;
        result.serialNumber_ = d.serialNumber_ - d.dayOfMonth() + 1;
        return result;
    }

    inline Date Date::endOfMonth(const Date& d) {
        const auto [day, m, y] = d.dayMonthYear();
        const Integer length = lengthOfMonth(m, detail::isLeapYear(y));
        // the end of a valid date's month is always a valid date, so no check is needed
        Date result;
        result.serialNumber_ = d.serialNumber_ + length - day;
        return result;
    }

    inline bool Date::isEndOfMonth(const Date& d) {
        const auto [day, m, y] = d.dayMonthYear();
        return day == lengthOfMonth(m, detail::isLeapYear(y));
    }

    inline Date::serial_type operator-(const Date& d1, const Date& d2) {
        return d1.serialNumber()-d2.serialNumber();
    }

    inline Time daysBetween(const Date& d1, const Date& d2) {
        return static_cast<Time>(d2-d1);
    }

    inline bool operator==(const Date& d1, const Date& d2) {
        return (d1.serialNumber() == d2.serialNumber());
    }

    inline bool operator!=(const Date& d1, const Date& d2) {
        return (d1.serialNumber() != d2.serialNumber());
    }

    inline bool operator<(const Date& d1, const Date& d2) {
        return (d1.serialNumber() < d2.serialNumber());
    }

    inline bool operator<=(const Date& d1, const Date& d2) {
        return (d1.serialNumber() <= d2.serialNumber());
    }

    inline bool operator>(const Date& d1, const Date& d2) {
        return (d1.serialNumber() > d2.serialNumber());
    }

    inline bool operator>=(const Date& d1, const Date& d2) {
        return (d1.serialNumber() >= d2.serialNumber());
    }
#else
    inline Date::Date() noexcept
    : dateTime_(serialNumberDateReference()) {}

    inline Date Date::startOfMonth(const Date& d) {
        Month m = d.month();
        Year y = d.year();
        return Date(1, m, y);
    }
#endif
}

namespace std {
    template<>
    struct hash<QuantLib::Date> {
        std::size_t operator()(const QuantLib::Date& d) const {
            return QuantLib::hash_value(d);
        }
    };
}

#endif
