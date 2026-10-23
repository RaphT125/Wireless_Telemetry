//
// Created by Raphael on 2026-10-21.
//

#ifndef WIRELESS_TELEMETRY_IPARSER_H
#define WIRELESS_TELEMETRY_IPARSER_H

class IParser{
public:
    virtual ~IParser() = default;
    virtual void parseFile() = 0;
};

#endif //WIRELESS_TELEMETRY_IPARSER_H
