
#include "rtu.h"
#include "../base/add/container_of.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "../base/byte-word.h"
#include "../base/add/crc.h"
#include "../base/modbus_errno.h"

#include <stdio.h>

/*!
 * \file
 * \brief RTU Protocol.
 *
 * This file contains an RTU protocol realizations.
 *
 */

/**
 * @brief Print packet contents
 *
 * Function will print to given file descriptor
 * data, pointed by _pkt.
 *
 * @param [in] _f File descriptor to write to. (if is zero, then nothing will be printed)
 * @param [in] _prefix Some prefix to print it before data.
 * @param [in] _pkt The packet.
 * @param [in] _size Size of packet in bytes.
 */
static void dbg_print_packet(FILE* _f, const char* _prefix, const void* _pkt, unsigned int _size) {
    if(_f) {
        int i;
        fprintf(_f, "%s", _prefix);
        for(i=0; i<_size; ++i) {
            fprintf(_f, "%02X ", ((uint8_t*)_pkt)[i]);
        }
        fprintf(_f, "\n");
        fflush(_f);
    }
}

/**
 * @brief Parse packet
 *
 * This function checks validity of received packet.
 * And if it is valid, then sends it to high level.
 *
 * @param [in] _mbt RTU context
 */
static void parse_packet(struct modbus_rtu_t* _mbt) {
    const unsigned char* buf = _mbt->rx_buffer;
    const unsigned int size = _mbt->rx_buf_counter-2;
    if(size >= 4) { // 4 bytes - minimal packet size
        const uint16_t crc1 = crc16(buf, size);
        const uint16_t crc2 = MKWORD(buf[size], buf[size+1]);
        if(crc1 == crc2) {
            _mbt->rx_pdu.function = buf[1];
            _mbt->rx_pdu.data_size = size;
            modbus_proto_recv_packet(&_mbt->proto, (int)buf[0], MB_CONST_PDU(&_mbt->rx_pdu));
        }
        else {
            modbus_proto_error(&_mbt->proto, -modbus_bad_crc);
        }
        dbg_print_packet(stdout, ">>", buf, _mbt->rx_buf_counter);
    }
}

/**
 * @brief input_stream interface
 *
 * This function calls by input_stream, when a new data has been received.
 *
 * @param [in] _this Input stream.
 * @param [in] _data Received data.
 * @param [in] _size Size of received data.
 *
 * @return how much data was received.
 */
static int modbus_rtu_on_write(struct input_stream_t* _this, const void* _data, unsigned int _size) {

    const unsigned char* data = (unsigned char*)_data;
    struct modbus_rtu_t* mbt = container_of(_this, struct modbus_rtu_t, input_stream);

    if((mbt->rx_buf_counter + _size) <= mbt->rx_buf_size) {
        memcpy(mbt->rx_buffer + mbt->rx_buf_counter, _data, _size);
        mbt->rx_buf_counter += _size;
    }

    mbt->modbus_rtu_on_char(mbt->user_data);

    return _size;
}

/**
 * @brief output_stream interface
 *
 * This function calls by output_stream, when a some data has beed sent,
 * and hardware transmitter can send a new part of data.
 *
 * @param [in] _this Output stream.
 * @param [in] _data Place for write data, that will be send.
 * @param [in] _size Size of place, pointed by _data.
 *
 * @return how much data written to _data.
 */
static int modbus_rtu_on_read(struct output_stream_t* _this, void* _data, unsigned int _size) {
    struct modbus_rtu_t* mbt = container_of(_this, struct modbus_rtu_t, output_stream);
    if(mbt->tx_buf_counter < mbt->tx_pkt_size) {
        const unsigned int remainder = mbt->tx_pkt_size - mbt->tx_buf_counter;
        const unsigned int sz = _size > remainder ? remainder : _size;
        memcpy(_data, mbt->tx_buffer + mbt->tx_buf_counter, sz);
        mbt->tx_buf_counter += sz;
        return sz;
    }
    return 0;
}

//


/**
 * @brief Send a PDU.
 *
 * (Protocol interface) This function will send packet (CRC suffix automatically added).
 *
 * @param [in] _mbt RTU context
 * @param [in] _slave_addr Address of slave
 * @param [in] _pdu PDU, that will be sent.
 *
 * @return Zero on success, error on fail.
 */
static int modbus_rtu_send_packet(void *_mbt,
                           int _slave_addr,
                           const struct modbus_const_pdu_t *_pdu) {

    struct modbus_rtu_t* mbt = (struct modbus_rtu_t*)_mbt;

    if((_pdu->data_size + 4) <= mbt->tx_buf_size) {

        const int sz = _pdu->data_size + 2;
        uint16_t crc;
        mbt->tx_buffer[0] = _slave_addr;
        mbt->tx_buffer[1] = _pdu->function;
        if(_pdu != MB_CONST_PDU(&mbt->tx_pdu))
            memcpy(mbt->tx_buffer + 2, _pdu->data, _pdu->data_size);
        crc = crc16(mbt->tx_buffer, sz);
        memcpy(mbt->tx_buffer + sz, &crc, 2);
        mbt->tx_pkt_size = sz + 2;
        // write first part
        mbt->tx_buf_counter = stream_write(&mbt->output_stream,
                                           mbt->tx_buffer,
                                           mbt->tx_pkt_size);
        dbg_print_packet(stdout, "<<", mbt->tx_buffer, mbt->tx_pkt_size);
        return 0;
    }
    else
        return -modbus_buffer_overflow;
}

void modbus_rtu_initialize(struct modbus_rtu_t* _mbt) {
    _mbt->input_stream.on_write = modbus_rtu_on_write;
    _mbt->output_stream.on_read = modbus_rtu_on_read;
    _mbt->rx_buf_counter = 0;
    _mbt->tx_buf_counter = 0;
    _mbt->tx_pkt_size = 0;

    // Setup protocol
    _mbt->proto.send_packet = modbus_rtu_send_packet;
    _mbt->proto.low_level_context = _mbt;
    _mbt->proto.tx_pdu = &_mbt->tx_pdu;

    _mbt->rx_pdu.data = _mbt->rx_buffer + 2;  // skip address and function
    _mbt->rx_pdu.data_size = 0;
    _mbt->rx_pdu.function = 0;

    _mbt->tx_pdu.data = _mbt->tx_buffer + 2;  // skip address and function
    _mbt->tx_pdu.data_size = 0;
    _mbt->tx_pdu.function = 0;
}

void modbus_rtu_on_char_timeout(struct modbus_rtu_t* _mbt) {
    parse_packet(_mbt);
    _mbt->rx_buf_counter = 0;
}

void modbus_rtu_on_error(struct modbus_rtu_t* _mbt,
                         int _errno) {
    modbus_proto_error(&_mbt->proto, _errno);
}

int modbus_rtu_send_packet_sync(struct modbus_rtu_t* _mbt,
                                int _slave_addr,
                                const struct modbus_const_pdu_t *_pdu) {
    int r;
    if((r = modbus_rtu_send_packet(_mbt, _slave_addr, _pdu)) == 0) {
        while(_mbt->tx_buf_counter < _mbt->tx_pkt_size) {
            const unsigned int remainder = _mbt->tx_pkt_size - _mbt->tx_buf_counter;
            _mbt->tx_buf_counter += stream_write(&_mbt->output_stream, _mbt->tx_buffer, remainder);
        }
        return 0;
    }
    else
        return r;
}