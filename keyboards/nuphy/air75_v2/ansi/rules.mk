include $(KEYBOARD_PATH_1)/qmk-vim/rules.mk

SRC += side.c rf.c sleep.c side_driver.c rf_driver.c user.c utils.c
UART_DRIVER_REQUIRED = yes
