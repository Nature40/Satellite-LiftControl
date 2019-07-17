#!/usr/bin/env python3

import logging
import struct
import random
import time

from bluepy import btle

logger = logging.getLogger(__name__)

BLE_SERVICE_LIFT_UUID = 35037
BLE_CHAR_TIMEOUT_UUID = 0xFF00
BLE_CHAR_DIRECTION_UUID = 0xFF01
BLE_CHAR_SPEED_UUID = 0xFF02


def hex(bytes_in):
    return "".join("\\x{:02x}".format(c) for c in bytes_in)


class IntegerChar():
    def __init__(self, con: btle.Peripheral, uuid, form, name):
        self.__char = con.getCharacteristics(uuid=uuid)[0]
        self.form = form
        self.name = name

    def __set_value(self, val: int):
        send_bytes = struct.pack(self.form, val)
        logger.debug(f"Writing {self.name} {val}, sending {hex(send_bytes)}.")
        self.__char.write(send_bytes)

    def __get_value(self) -> int:
        ret_bytes = self.__char.read()
        logger.debug(f"Reading {self.name} value, received {hex(ret_bytes)}")
        ret = struct.unpack(self.form, ret_bytes)[0]
        logger.debug(f"{self.name} value decoded to {ret}")
        return ret

    value = property(__get_value, __set_value)


class LiftSystem():
    def __init__(self, con: btle.Peripheral):
        self.con = con
        logger.info(f"Connected to {self.con}")

        self.timeout = IntegerChar(con, BLE_CHAR_TIMEOUT_UUID, "H", "timeout")
        self.direction = IntegerChar(
            con, BLE_CHAR_DIRECTION_UUID, "b", "direction")
        self.speed = IntegerChar(con, BLE_CHAR_SPEED_UUID, "B", "speed")

    def upwards(self, timeout=500):
        self.timeout.value = timeout
        self.speed.value = 255
        for i in range(100):
            self.direction.value = 1
            time.sleep(0.1)


if __name__ == "__main__":
    stderr_formatter = logging.Formatter(
        "%(name)s - %(levelname)s - %(funcName)s - %(message)s")
    stderr_handler = logging.StreamHandler()
    stderr_handler.setFormatter(stderr_formatter)

    logger.addHandler(stderr_handler)
    logger.setLevel(logging.DEBUG)

    con = btle.Peripheral("b4:e6:2d:8e:db:03")
    l = LiftSystem(con)
    l.upwards()

    # timeout_new = random.randint(0, (2**16)-1)
    # l.timeout.value = timeout_new
    # if l.timeout.value != timeout_new:
    #     logger.warn(f"Setting timeout {timeout_new} failed.")

    # l.direction.value
    # direction_new = -1  # random.randint(-1, 1)
    # l.direction.value = direction_new
    # if l.direction.value != direction_new:
    #     logger.warning(f"Setting direction {direction_new} failed.")

    # speed_new = random.randint(0, (2**8)-1)
    # l.speed.value = speed_new
    # if l.speed.value != speed_new:
    #     logger.warn(f"Setting speed {speed_new} failed.")
