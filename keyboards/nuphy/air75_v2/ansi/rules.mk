include $(KEYBOARD_PATH_1)/qmk-vim/rules.mk

SRC += side.c rf.c sleep.c side_driver.c rf_driver.c user.c utils.c layers.c macros.c \
       vim_macros.c mcu_pwr.c rf_queue.c config_ui.c
UART_DRIVER_REQUIRED = yes

# Link-time optimization. Pulled in from jincao1's port to free up enough
# flash for the rf_queue + mcu_pwr code; also shrinks the runtime image.
LTO_ENABLE = yes
