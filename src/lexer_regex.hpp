#pragma once
#include "include/ctre-unicode.hpp"

namespace NeoMIPS
{
    namespace Regex
    {
        constexpr auto immPattern = ctll::fixed_string{
            U"(?:(?:\\b0[xX][\\da-fA-F]{1,8})|(?:[+\\-]?\\d+))" };

        constexpr auto labelPattern = ctll::fixed_string{
            U"((?:\\p{Emoji}+|\\w+)+)" };

        constexpr auto regPattern = ctll::fixed_string{
            U"(\\$(?:(?:t[0-9])|(?:s[0-7])|(?:k[01])|(?:v[01])|(?:a[0-3])|(?:[gpf]p)|(?:[r][a])|(?:[z][e][r][o])|(?:\\d{1,2})|(?:[a][t])))" };

        constexpr auto regImmPattern = ctll::fixed_string{
            U"(?:\\$((?:t[0-9])|(?:s[0-7])|(?:k[01])|(?:v[01])|(?:a[0-3])|(?:[gpf]p)|(?:[r][a])|(?:[z][e][r][o])|(?:\\d{1,2})|(?:[a][t]))[\\s,]+((?:0[xX][\\da-fA-F]{1,8})|(?:[+\\-]?\\d+)))" };

        constexpr auto regLabelPattern = ctll::fixed_string{
            U"(?:(\\$(?:(?:t[0-9])|(?:s[0-7])|(?:k[01])|(?:v[01])|(?:a[0-3])|(?:[gpf]p)|(?:[r][a])|(?:[z][e][r][o])|(?:\\d{1,2})|(?:[a][t])))[\\s,]+((?:\\p{Emoji}+|\\w+)+))" };

        constexpr auto immLabelPattern = ctll::fixed_string{
            U"(?:((?:\\b0[xX][\\da-fA-F]{1,8})|(?:[+\\-]?\\d+))[\\s,]+((?:\\p{Emoji}+|\\w+)+))" };

        constexpr auto regRegImmPattern = ctll::fixed_string{
            U"(?:(\\$(?:(?:t[0-9])|(?:s[0-7])|(?:k[01])|(?:v[01])|(?:a[0-3])|(?:[gpf]p)|(?:[r][a])|(?:[z][e][r][o])|(?:\\d{1,2})|(?:[a][t])))[\\s,]+(\\$(?:(?:t[0-9])|(?:s[0-7])|(?:k[01])|(?:v[01])|(?:a[0-3])|(?:[gpf]p)|(?:[r][a])|(?:[z][e][r][o])|(?:\\d{1,2})|(?:[a][t])))[\\s,]+((?:\\b0[xX][\\da-fA-F]{1,8})|(?:[+\\-]?\\d+)))" };

        constexpr auto regRegLabelPattern = ctll::fixed_string{
            U"(?:(\\$(?:(?:t[0-9])|(?:s[0-7])|(?:k[01])|(?:v[01])|(?:a[0-3])|(?:[gpf]p)|(?:[r][a])|(?:[z][e][r][o])|(?:\\d{1,2})|(?:[a][t])))[\\s,]+(\\$(?:(?:t[0-9])|(?:s[0-7])|(?:k[01])|(?:v[01])|(?:a[0-3])|(?:[gpf]p)|(?:[r][a])|(?:[z][e][r][o])|(?:\\d{1,2})|(?:[a][t])))[\\s,]+((?:\\p{Emoji}+|\\w+)+))" };

        constexpr auto regImmLabelPattern = ctll::fixed_string{
            U"(?:(\\$(?:(?:t[0-9])|(?:s[0-7])|(?:k[01])|(?:v[01])|(?:a[0-3])|(?:[gpf]p)|(?:[r][a])|(?:[z][e][r][o])|(?:\\d{1,2})|(?:[a][t])))[\\s,]+((?:\\b0[xX][\\da-fA-F]{1,8})|(?:[+\\-]?\\d+))[\\s,]+((?:\\p{Emoji}+|\\w+)+))" };

        constexpr auto immRegRegPattern = ctll::fixed_string{
            U"(?:((?:\\b0[xX][\\da-fA-F]{1,8})|(?:[+\\-]?\\d+))[\\s,]+(\\$(?:(?:t[0-9])|(?:s[0-7])|(?:k[01])|(?:v[01])|(?:a[0-3])|(?:[gpf]p)|(?:[r][a])|(?:[z][e][r][o])|(?:\\d{1,2})|(?:[a][t])))[\\s,]+(\\$(?:(?:t[0-9])|(?:s[0-7])|(?:k[01])|(?:v[01])|(?:a[0-3])|(?:[gpf]p)|(?:[r][a])|(?:[z][e][r][o])|(?:\\d{1,2})|(?:[a][t]))))" };

        constexpr auto regMemRegPattern = ctll::fixed_string{
            U"(?:(\\$(?:(?:t[0-9])|(?:s[0-7])|(?:k[01])|(?:v[01])|(?:a[0-3])|(?:[gpf]p)|(?:[r][a])|(?:[z][e][r][o])|(?:\\d{1,2})|(?:[a][t])))[\\s,]+\\((\\$(?:(?:t[0-9])|(?:s[0-7])|(?:k[01])|(?:v[01])|(?:a[0-3])|(?:[gpf]p)|(?:[r][a])|(?:[z][e][r][o])|(?:\\d{1,2})|(?:[a][t])))\\))" };

        constexpr auto regOffsetForRegPattern = ctll::fixed_string{
            U"(?:(\\$(?:(?:t[0-9])|(?:s[0-7])|(?:k[01])|(?:v[01])|(?:a[0-3])|(?:[gpf]p)|(?:[r][a])|(?:[z][e][r][o])|(?:\\d{1,2})|(?:[a][t])))[\\s,]+((?:\\b0[xX][\\da-fA-F]{1,8})|(?:[+\\-]?\\d+))\\((\\$(?:(?:t[0-9])|(?:s[0-7])|(?:k[01])|(?:v[01])|(?:a[0-3])|(?:[gpf]p)|(?:[r][a])|(?:[z][e][r][o])|(?:\\d{1,2})|(?:[a][t])))\\))" };

        constexpr auto regLabelPlusImmPattern = ctll::fixed_string{
            U"(?:(\\$(?:(?:t[0-9])|(?:s[0-7])|(?:k[01])|(?:v[01])|(?:a[0-3])|(?:[gpf]p)|(?:[r][a])|(?:[z][e][r][o])|(?:\\d{1,2})|(?:[a][t])))[\\s,]+((?:\\p{Emoji}+|\\w+)+)[\\s,]*[+\\-][\\s,]*((?:\\b0[xX][\\da-fA-F]{1,8})|(?:[+\\-]?\\d+)))" };

        constexpr auto regLabelPlusImmOffsetForRegPattern = ctll::fixed_string{
            U"(?:(\\$(?:(?:t[0-9])|(?:s[0-7])|(?:k[01])|(?:v[01])|(?:a[0-3])|(?:[gpf]p)|(?:[r][a])|(?:[z][e][r][o])|(?:\\d{1,2})|(?:[a][t])))[\\s,]+((?:\\p{Emoji}+|\\w+)+)[\\s,]*[+\\-][\\s,]*((?:\\b0[xX][\\da-fA-F]{1,8})|(?:[+\\-]?\\d+))[\\s]*\\((\\$(?:(?:t[0-9])|(?:s[0-7])|(?:k[01])|(?:v[01])|(?:a[0-3])|(?:[gpf]p)|(?:[r][a])|(?:[z][e][r][o])|(?:\\d{1,2})|(?:[a][t])))\\))" };

        constexpr auto regLabelAsOffsetRegPattern = ctll::fixed_string{
            U"(?:(\\$(?:(?:t[0-9])|(?:s[0-7])|(?:k[01])|(?:v[01])|(?:a[0-3])|(?:[gpf]p)|(?:[r][a])|(?:[z][e][r][o])|(?:\\d{1,2})|(?:[a][t])))[\\s,]+((?:\\p{Emoji}+|\\w+)+)[\\s]*\\((\\$(?:(?:t[0-9])|(?:s[0-7])|(?:k[01])|(?:v[01])|(?:a[0-3])|(?:[gpf]p)|(?:[r][a])|(?:[z][e][r][o])|(?:\\d{1,2})|(?:[a][t])))\\))" };

        constexpr auto instructionPattern = ctll::fixed_string{
            U"[\\s]*((?:[a-zA-Z.])+)" };
    }
}