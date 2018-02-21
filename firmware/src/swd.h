#ifndef __swd_h
#define __swd_h

typedef enum _swd_response_t {
    SWD_RESPONSE_OK = 1,
    SWD_RESPONSE_WAIT = 2,
    SWD_RESPONSE_FAULT = 4,
    SWD_PROTOCOL_ERROR = 7,
} swd_response_t;

void swd_enable(int enabled);
void swj_switch_to_swd(void);
void swd_turnaround(int writing);
void swd_idle_cycles(void);
swd_response_t swd_request_response(uint8_t request);
int swd_data_read(uint32_t *value);
void swd_data_write(uint32_t value);
int parity_even_4bit(uint8_t bits);

#endif /* __swd_h */
