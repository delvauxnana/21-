#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include "zf_common_headfile.h"

/* System timing */
#define BOARD_MOTOR_COUNT                   (4U)
#define BOARD_ENCODER_COUNT                 (4U)
#define BOARD_OPENART_COUNT                 (2U)

#define BOARD_CONTROL_PIT_CH                (PIT_CH0)
#define BOARD_CONTROL_PIT_IRQn              (PIT_IRQn)
#define BOARD_IMU_PERIOD_MS                 (5U)
#define BOARD_CONTROL_DIVIDER               (4U)
#define BOARD_CONTROL_PERIOD_MS             (20U)
#define BOARD_CONTROL_PERIOD_S              (0.020f)
#define BOARD_DEBUG_PRINT_PERIOD_MS         (40U)
#define BOARD_MENU_PERIOD_MS                (10U)
#define BOARD_DISPLAY_PERIOD_MS             (100U)
#define BOARD_WS2812_TASK_PERIOD_MS         (20U)

/* Motor output */
#define BOARD_MOTOR_PWM_FREQ_HZ             (17000U)
#define BOARD_MOTOR_OUTPUT_LIMIT            (9000)
#define BOARD_MOTOR_MIN_EFFECTIVE_DUTY      (650)
#define BOARD_MOTOR_LF_MIN_EFFECTIVE_DUTY   (650)
#define BOARD_MOTOR_RF_MIN_EFFECTIVE_DUTY   (650)
#define BOARD_MOTOR_LB_MIN_EFFECTIVE_DUTY   (650)
#define BOARD_MOTOR_RB_MIN_EFFECTIVE_DUTY   (650)
#define BOARD_MOTOR_LF_ANTI_STALL_DUTY      (0)
#define BOARD_MOTOR_RF_ANTI_STALL_DUTY      (0)
#define BOARD_MOTOR_LB_ANTI_STALL_DUTY      (0)
#define BOARD_MOTOR_RB_ANTI_STALL_DUTY      (0)

/* Motion control */
#define BOARD_WHEEL_TEST_SPEED_MM_S         (120)
#define BOARD_MOVE_TEST_DISTANCE_MM         (1000.0f)
#define BOARD_MOVE_FINISH_WINDOW_MM         (8.0f)
#define BOARD_MOVE_FINISH_SPEED_MM_S        (20.0f)
#define BOARD_MOVE_RELAX_FINISH_WINDOW_MM   (20.0f)
#define BOARD_MOVE_RELAX_FINISH_CMD_MM_S    (15)
#define BOARD_DISTANCE_MOVE_WARMUP_CYCLES   (5U)
#define BOARD_STARTUP_BOOST_MAX_CYCLES      (4U)
#define BOARD_STARTUP_BOOST_FEEDBACK_MM_S   (25)
#define BOARD_ANTI_STALL_SPEED_RATIO        (60)
#define BOARD_ANTI_STALL_MIN_TARGET_MM_S    (60)
#define BOARD_IMU_CALIBRATE_SAMPLES         (200U)
#define BOARD_IMU_GYRO_DEADBAND_DPS         (0.3f)

/* Keys */
#define BOARD_KEY_RELEASE_LEVEL             (GPIO_HIGH)
#define BOARD_KEY_DEBOUNCE_TICKS            (1U)
#define BOARD_KEY_LONG_PRESS_TICKS          (60U)
#define BOARD_KEY_REPEAT_INTERVAL_TICKS     (4U)

#define BOARD_KEY_UP_PIN                    (C12)
#define BOARD_KEY_DOWN_PIN                  (C13)
#define BOARD_KEY_OK_PIN                    (C14)
#define BOARD_KEY_BACK_PIN                  (C15)

/* Chassis geometry */
#define BOARD_WHEEL_BASE_MM                 (200.0f)
#define BOARD_TRACK_WIDTH_MM                (177.0f)
#define BOARD_WHEEL_RADIUS_MM               (30.0f)
#define BOARD_WHEEL_GEAR_RADIUS_MM          (22.0f)
#define BOARD_ENCODER_GEAR_RADIUS_MM        (9.0f)
#define BOARD_MOTOR_GEAR_RADIUS_MM          (4.0f)
#define BOARD_ENCODER_PULSE_PER_REV         (1080)
#define BOARD_WHEEL_COUNT_PER_REV           (2640)
#define BOARD_WHEEL_CIRCUMFERENCE_MM        (188.49556f)
#define BOARD_WHEEL_MM_PER_COUNT            (0.07139983f)
#define BOARD_WHEEL_DELTA_X100_NUM          (714)
#define BOARD_WHEEL_DELTA_X100_DEN          (100)
#define BOARD_SPEED_FROM_DELTA_X100_GAIN    (5)

/* Motor pins */
#define BOARD_MOTOR_LF_DIR_PIN              (D2)
#define BOARD_MOTOR_LF_PWM_PIN              (PWM2_MODULE3_CHB_D3)
#define BOARD_MOTOR_LF_FORWARD_LEVEL        (GPIO_HIGH)

#define BOARD_MOTOR_RF_DIR_PIN              (C10)
#define BOARD_MOTOR_RF_PWM_PIN              (PWM2_MODULE2_CHB_C11)
#define BOARD_MOTOR_RF_FORWARD_LEVEL        (GPIO_HIGH)

#define BOARD_MOTOR_LB_DIR_PIN              (C9)
#define BOARD_MOTOR_LB_PWM_PIN              (PWM2_MODULE1_CHA_C8)
#define BOARD_MOTOR_LB_FORWARD_LEVEL        (GPIO_LOW)

#define BOARD_MOTOR_RB_DIR_PIN              (C7)
#define BOARD_MOTOR_RB_PWM_PIN              (PWM2_MODULE0_CHA_C6)
#define BOARD_MOTOR_RB_FORWARD_LEVEL        (GPIO_LOW)

/* Encoder pins */
#define BOARD_ENCODER_LF_INDEX              (QTIMER1_ENCODER2)
#define BOARD_ENCODER_LF_CH1                (QTIMER1_ENCODER2_CH1_C2)
#define BOARD_ENCODER_LF_CH2                (QTIMER1_ENCODER2_CH2_C24)
#define BOARD_ENCODER_LF_SIGN               (-1)

#define BOARD_ENCODER_RF_INDEX              (QTIMER1_ENCODER1)
#define BOARD_ENCODER_RF_CH1                (QTIMER1_ENCODER1_CH1_C0)
#define BOARD_ENCODER_RF_CH2                (QTIMER1_ENCODER1_CH2_C1)
#define BOARD_ENCODER_RF_SIGN               (1)

#define BOARD_ENCODER_LB_INDEX              (QTIMER2_ENCODER2)
#define BOARD_ENCODER_LB_CH1                (QTIMER2_ENCODER2_CH1_C5)
#define BOARD_ENCODER_LB_CH2                (QTIMER2_ENCODER2_CH2_C25)
#define BOARD_ENCODER_LB_SIGN               (-1)

#define BOARD_ENCODER_RB_INDEX              (QTIMER2_ENCODER1)
#define BOARD_ENCODER_RB_CH1                (QTIMER2_ENCODER1_CH1_C3)
#define BOARD_ENCODER_RB_CH2                (QTIMER2_ENCODER1_CH2_C4)
#define BOARD_ENCODER_RB_SIGN               (1)

/* Vision UART */
#define BOARD_VISION_UART_INDEX             (UART_4)
#define BOARD_VISION_UART_TX_PIN            (UART4_TX_C16)
#define BOARD_VISION_UART_RX_PIN            (UART4_RX_C17)
#define BOARD_VISION_UART_BAUD              (115200U)

/* WS2812 */
#define BOARD_WS2812_PIN                    (D4)
#define BOARD_WS2812_COUNT                  (4U)
#define BOARD_WS2812_ENABLE                 (1U)
#define BOARD_WS2812_BRIGHTNESS_DIV         (10U)

/* Position loop tuning */
#define BOARD_POSITION_I_ENABLE_WINDOW_MM   (240)
#define BOARD_POSITION_I_RESET_WINDOW_MM    (20)
#define BOARD_POSITION_I_ENABLE_WINDOW_DEG  (30)
#define BOARD_POSITION_I_RESET_WINDOW_DEG   (3)

#endif /* BOARD_CONFIG_H */
