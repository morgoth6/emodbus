
#include <emodbus/server/server.h>
#include <emodbus/server/regs.h>
#include <emodbus/base/byte-word.h>
#include <emodbus/base/modbus_errno.h>
#include <emodbus/base/calc_pdu_size.h>
#include <stdint.h>

uint8_t emb_srv_write_regs(struct emb_super_server_t* _ssrv,
                           struct emb_server_t* _srv) {

    struct emb_srv_regs_t* r;
    uint8_t* rx_data = _ssrv->rx_pdu->data;
    uint8_t* tx_data = _ssrv->tx_pdu->data;
    uint8_t i;

    const uint16_t start_addr = GET_BIG_END16(rx_data + 0),
                   quantity = GET_BIG_END16(rx_data + 2);

    const uint8_t byte_count = rx_data[4];

    if(!(0x0001 <= quantity && quantity <= 0x007B) || (byte_count != (quantity*2)))
        return MBE_ILLEGAL_DATA_VALUE;

    if(!_srv->get_holding_regs)
        return MBE_SLAVE_FAILURE;

    r = _srv->get_holding_regs(_srv, start_addr);

    if(!r)
        return MBE_ILLEGAL_DATA_ADDR;

    if((start_addr+quantity) > (r->start+r->size))
        return MBE_ILLEGAL_DATA_ADDR;

    if(!r->write_regs)
        return MBE_ILLEGAL_DATA_ADDR;

    if(WRITE_REGISTERS_ANS_SIZE() > _ssrv->tx_pdu->max_size)
        return MBE_SLAVE_FAILURE;

    rx_data += 5;

    for(i=0; i<quantity; ++i) {
        uint16_t* p = ((uint16_t*)rx_data) + i;
        SWAP_BYTES_PTR(p);
    }

    i = r->write_regs(r,
                      start_addr - r->start,
                      quantity,
                      (uint16_t*)rx_data);
    if(i)
        return i;

    BIG_END_MK16(tx_data, start_addr);
    tx_data += 2;
    BIG_END_MK16(tx_data, quantity);

    _ssrv->tx_pdu->function = 0x10;
    _ssrv->tx_pdu->data_size = WRITE_REGISTERS_ANS_SIZE();

    return 0;
}
