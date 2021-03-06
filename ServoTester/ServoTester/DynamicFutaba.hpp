#ifndef __DynamicFutaba__
#define __DynamicFutaba__

#include "DynamicServo.hpp"
#include "ByteUnion.hpp"

class DynamicFutaba : public DynamicServo {
    template<size_t length>
    static uint8_t calcChecksum(const std::array<uint8_t, length> data) {
        uint8_t ret = data[0];
        for (int i = 1; i < length; ++i) {
            ret ^= data[i];
        }
        return ret;
    }
    template <uint8_t address, uint8_t value>
    void writeMemory(Serial::Error *error) {
        const std::array<uint8_t, 6> packetForChecksum({
            id,
            0, /* No return packet */
            address,
            1,
            1, /* Count */
            value
        });
        const uint8_t checksum = calcChecksum(packetForChecksum);
        const std::array<uint8_t, 9> packet({
            0xFA, 0xAF, id, 0, address, 1, 1, value, checksum
        });
        if (error) {
            *error = this->serial->transfer(packet);
        } else {
            this->serial->transfer(packet);
        }
    }
    template <uint8_t address, typename T>
    void writeMemory(T data, Serial::Error *error) {
        constexpr std::array<uint8_t, 2> header({
            0xFA, 0xAF, /* Header */
        });
        const std::array<uint8_t, 5> packetForChecksum({
            id,
            0, /* No return packet */
            address,
            sizeof(data),
            1, /* Count */
        });
        const uint8_t checksum = calcChecksum(packetForChecksum);
        uint8_t checksumVar = checksum;
        ByteUnion<T> dataByte(data);
        for (uint8_t v : dataByte.array) {
            checksumVar ^= v;
        }
        auto dataArray = dataByte.arrayObj();
        if (error) {
            *error = this->serial->transfer(header, packetForChecksum,
                                            dataArray, std::array<uint8_t, 1>({checksumVar}));
        } else {
            this->serial->transfer(header, packetForChecksum, dataArray, std::array<uint8_t, 1>({checksumVar}));
        }
    }
    template <uint8_t address, typename T>
    T readMemory(Serial::Error *error) {
        const std::array<uint8_t, 8> packet({
            0xFA, 0xAF, /* Header */
            id,
            0x0F, /* Flag */
            address,
            sizeof(T),
            0, /* Count */
            static_cast<uint8_t>(id ^ 0x0F ^ address ^ sizeof(T)) /* Checksum */
        });
        std::array<uint8_t, 8 + sizeof(T)> response;
        Serial::Error ret = this->serial->transfer(response, packet);
        union ReturnPacket {
            uint8_t raw[8 + sizeof(T)];
            struct __attribute__((packed)) {
                uint16_t header;
                uint8_t identifier;
                uint8_t flags;
                uint8_t registerAddress;
                uint8_t length;
                uint8_t count;
                T data;
                uint8_t checksum;
            };
        };
        ReturnPacket rePacket;
        std::copy(response.begin(), response.end(), std::begin(rePacket.raw));
        if (error) {
            *error = ret;
        }
        return rePacket.data;
    }
    
public:
    DynamicFutaba(Serial *serial, uint8_t id) : DynamicServo(serial, id) {}
    void flashROM() {
        const std::array<uint8_t, 8> flashPacket({
            0xFA, 0xAF, id, 0x40, 0xFF, 0, 0,
            static_cast<uint8_t>(id ^ 0x40 ^ 0xFF)
        });
        this->serial->transfer(flashPacket);
    }
    void reboot() {
        std::array<uint8_t, 8> rebootPacket({
            0xFA, 0xAF, id, 0x20, 0xFF, 0, 0,
            static_cast<uint8_t>(id ^ 0x20 ^ 0xFF)
        });
        this->serial->transfer(rebootPacket);
    }
    void setTorque(bool enable, Serial::Error *error = nullptr) {
        if (enable) {
            writeMemory<0x24, 1>(error);
        } else {
            writeMemory<0x24, 0>(error);
        }
    }
    void setPosition(double position, Serial::Error *error = nullptr) {
        writeMemory<0x1E>(static_cast<int16_t>(position * 10), error);
    }
    void setPosition(int16_t position, Serial::Error *error = nullptr) {
        writeMemory<0x1E>(position, error);
    }
    int16_t intPosition(Serial::Error *error = nullptr) {
        return readMemory<0x2A, int16_t>(error);
    }
    double position(Serial::Error *error = nullptr) {
        return readMemory<0x2A, int16_t>(error) / 10;
    }
    void setRightLimit(double limit, Serial::Error *error = nullptr) {
        if (! (limit > 0 && limit < 150)) return;
        int16_t value = limit * 10;
        if (value > 1500) value = 1500;
        writeMemory<0x08>(value, error);
    }
    void setLeftLimit(double limit, Serial::Error *error = nullptr) {
        if (! (limit < 0 && limit > -150)) return;
        int16_t value = limit * 10;
        if (value < -1500) value = -1500;
        writeMemory<0x0A>(value, error);
    }
    void setID(uint8_t newID, Serial::Error *error = nullptr) {
        writeMemory<0x04>(newID, error);
        id = newID;
        flashROM();
        reboot();
    }
    void setBaud(speed_t baud, Serial::Error *error = nullptr) {
        uint8_t speed = 0;
        switch (baud) {
            case B9600:
                speed = 0;
                break;
            case B14400:
                speed = 1;
                break;
            case B19200:
                speed = 2;
                break;
            case B28800:
                speed = 3;
                break;
            case B38400:
                speed = 4;
                break;
            case B57600:
                speed = 5;
                break;
            case B76800:
                speed = 6;
                break;
            case B115200:
                speed = 7;
                break;
#ifdef B153600
            case B153600:
                speed = 8;
                break;
#endif
            case B230400:
                speed = 9;
                break;
            default:
                return;
        }
        writeMemory<0x06>(speed, error);
        flashROM();
        reboot();
        this->serial->changeBaud(baud);
    }
    void rebootIfNeeded(Serial::Error *error = nullptr) {
        const std::array<uint8_t, 8> packet({
            0xFA, 0xAF, /* Header */
            id,
            0x0F, /* Flag */
            0x04,
            1,
            0, /* Count */
            static_cast<uint8_t>(id ^ 0x0F ^ 0x04 ^ 1) /* Checksum */
        });
        std::array<uint8_t, 9> response;
        Serial::Error ret = this->serial->transfer(response, packet);
        if (ret == Serial::Error::NoError) {
            if (response[3]) reboot();
        } else {
            *error = ret;
        }
    }
    uint16_t readCurrent(Serial::Error *error = nullptr) {
        return readMemory<0x30, uint16_t>(error);
    }
};

#endif
