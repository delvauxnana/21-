#ifndef PROTOCOL_H
#define PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "zf_common_typedef.h"

#define PROTO_HEADER_0                  (0x55)
#define PROTO_HEADER_1                  (0xAA)

#define PROTO_CMD_CONTROL               (0x01)
#define PROTO_CMD_HEARTBEAT             (0x02)
#define PROTO_CMD_MODE_SET              (0x03)
#define PROTO_CMD_PARAM_SET             (0x04)

#define PROTO_CONTROL_LEN               (6)
#define PROTO_HEARTBEAT_LEN             (1)
#define PROTO_PARAM_SET_LEN             (5)
#define PROTO_FRAME_MAX_SIZE            (24)
#define PROTO_FAILSAFE_MS               (500)

#define PROTO_PARAM_WHEEL_KP            (0x01)
#define PROTO_PARAM_WHEEL_KI            (0x02)
#define PROTO_PARAM_WHEEL_KD            (0x03)
#define PROTO_PARAM_WHEEL_OUTPUT_LIMIT  (0x04)
#define PROTO_PARAM_SPEED_LIMIT         (0x05)
#define PROTO_PARAM_POSITION_KP         (0x06)
#define PROTO_PARAM_POSITION_KI         (0x07)
#define PROTO_PARAM_POSITION_KD         (0x08)
#define PROTO_PARAM_POSITION_SPEED_LIMIT (0x09)

typedef struct {
    int16_t vx;
    int16_t vy;
    int16_t wz;
} proto_velocity_t;

typedef struct {
    int16_t fl;
    int16_t fr;
    int16_t rl;
    int16_t rr;
} proto_wheel_speed_t;

typedef struct {
    uint8 remote_status;
    bool  connected;
} proto_heartbeat_t;

typedef enum {
    PROTO_STATE_IDLE = 0,
    PROTO_STATE_HEADER,
    PROTO_STATE_CMD,
    PROTO_STATE_LEN,
    PROTO_STATE_PAYLOAD,
    PROTO_STATE_CHECKSUM,
} proto_parse_state_t;

typedef struct {
    proto_parse_state_t state;
    uint8 frame_buf[PROTO_FRAME_MAX_SIZE];
    uint8 frame_idx;
    uint8 expected_payload_len;
    uint8 payload_cnt;

    proto_velocity_t velocity;
    proto_heartbeat_t heartbeat;
    proto_wheel_speed_t wheels;

    volatile uint32_t watchdog_cnt;
    bool failsafe_active;

    uint32_t rx_frame_cnt;
    uint32_t rx_err_cnt;
} proto_ctx_t;

void protocol_init(void);
uint32_t protocol_poll(void);
void protocol_tick(void);

const proto_wheel_speed_t *protocol_get_wheel_speeds(void);
const proto_velocity_t *protocol_get_velocity(void);
const proto_heartbeat_t *protocol_get_heartbeat(void);
const proto_ctx_t *protocol_get_ctx(void);

void protocol_send_heartbeat(uint8 status);
void protocol_send_control(int16_t vx, int16_t vy, int16_t wz);
void protocol_send_param_set(uint8 param_id, int32_t value);
void protocol_send_raw(uint8 cmd, const uint8 *payload, uint8 len);

void protocol_mecanum_inverse(int16_t vx, int16_t vy, int16_t wz,
                              proto_wheel_speed_t *out);

#ifdef __cplusplus
}
#endif

#endif /* PROTOCOL_H */
