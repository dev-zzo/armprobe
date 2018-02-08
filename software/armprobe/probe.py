"""
ADI v5 SWJ probe with Python support

Blue Pill pin assignments:
* SWDIO: B12
* SWCLK: B13
* nRST:  B0
* Host IF USART1-TX
* Host IF USART1-RX

Notes:

* In case of excessive latency on the serial port:
  http://store.chipkin.com/articles/rs232-how-do-i-reduce-latency-on-com-ports

"""

import struct

class ProbeException(Exception):
    pass

class SWDException(Exception):
    def __init__(self, response):
        message = "SWD response returned: %d" % response
        if response == 4:
            message += " -- FAULT"
        elif response == 2:
            message += " -- WAIT"
        super(SWDException, self).__init__(message)
        self.response = response

class BluePillProbe(object):
    """ADI version 5 probe wrapper"""

    def __init__(self, port):
        self._port = port
        self.ping()

    def ping(self):
        self._port.write('?')
        rsp = self._port.read(1)
        if rsp != '?':
            raise ProbeException("out of sync!")
        return True

    def configure_swj(self, enabled=True):
        """Configures the SWJ unit"""
        bits = 0x00
        if enabled:
            bits |= 0x01
        else:
            pass
        cmd = struct.pack('<BB', ord('c'), bits)
        self._port.write(cmd)
        status, = struct.unpack('B', self._port.read(1))
        if status:
            raise ProbeException("bleh")

    def switch_to_swd(self):
        """Issue the SWJ-DP sequence to switch to SWD"""
        cmd = struct.pack('<B', ord('y'))
        self._port.write(cmd)
        status, = struct.unpack('B', self._port.read(1))
        if status:
            raise ProbeException("bleh")

    @staticmethod
    def _build_request(is_read, is_ap, a32):
        return bool(is_ap) | (bool(is_read) << 1) | ((a32 & 3) << 2)

    def dp_read(self, a32):
        """Execute a DP read transaction via SWD"""
        return self._read(False, a32)

    def dp_write(self, a32, data):
        """Execute a DP write transaction via SWD"""
        self._write(False, a32, data)

    def ap_read(self, a32):
        """Execute an AP read transaction via SWD"""
        return self._read(True, a32)

    def ap_write(self, a32, data):
        """Execute an AP write transaction via SWD"""
        self._write(True, a32, data)

    def _read(self, is_ap, a32):
        """Execute a read transaction via SWD"""
        cmd = struct.pack('<BBB', ord('t'), 0, BluePillProbe._build_request(True, is_ap, a32))
        self._port.write(cmd)
        status, response = struct.unpack('BB', self._port.read(2))
        if response != 0x1:
            raise SWDException(response)
        data, = struct.unpack('<I', self._port.read(4))
        return data

    def _write(self, is_ap, a32, data):
        """Execute a write transaction via SWD"""
        cmd = struct.pack('<BBBI', ord('T'), 0, BluePillProbe._build_request(False, is_ap, a32), data)
        self._port.write(cmd)
        status, response = struct.unpack('BB', self._port.read(2))
        if response != 0x1:
            raise SWDException(response)

    def configure_gpio(self, enabled=True):
        """Configure the GPIO unit (currently only enable/disable)"""
        cmd = struct.pack('<BB', ord('G'), int(enabled))
        self._port.write(cmd)
        status, = struct.unpack('B', self._port.read(1))
        if status:
            raise ProbeException("bleh")

    def set_gpio(self, states):
        cmd = struct.pack('<BB', ord('g'), int(states))
        self._port.write(cmd)
        status, = struct.unpack('B', self._port.read(1))
        if status:
            raise ProbeException("bleh")
