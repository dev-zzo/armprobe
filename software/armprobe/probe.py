"""
ADI v5 SWJ probe with Python support

Blue Pill pin assignments:
* SWDIO: B12
* SWCLK: B13
* nRST:  B0

"""

import usb1
import struct
from hexdump import hexdump

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

    def __init__(self):
        # TODO: do it right
        self.timeout = 5
        self._context = usb1.USBContext()
        self._handle = self._context.openByVendorIDAndProductID(0xDECA, 0x0002, skip_on_error=True)
        if self._handle is None:
            raise ProbeException("probe not found")
        self._iface = self._handle.claimInterface(0)
        self._handle.setConfiguration(1)
        self.ping()
        
    def ping(self):
        self._handle.controlWrite(0x40, 0, 0x0000, 0x0000, "", self.timeout)
        return True

    def configure_swj(self, enabled=True):
        """Configures the SWJ unit"""
        bits = 0x00
        if enabled:
            bits |= 0x01
        else:
            pass
        self._handle.controlWrite(0x40, 1, bits, 0x0000, "", self.timeout)

    def switch_to_swd(self):
        """Issue the SWJ-DP sequence to switch to SWD"""
        self._handle.controlWrite(0x40, 2, 0x0000, 0x0000, "", self.timeout)

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
        data = self._handle.controlRead(0x40, 3, BluePillProbe._build_request(True, is_ap, a32), 0x0000, 4, self.timeout)
        return struct.unpack("<I", data)[0]

    def _write(self, is_ap, a32, data):
        """Execute a write transaction via SWD"""
        self._handle.controlWrite(0x40, 4, BluePillProbe._build_request(False, is_ap, a32), 0x0000, struct.pack("<I", data), self.timeout)

    def configure_gpio(self, enabled=True):
        """Configure the GPIO unit (currently only enable/disable)"""
        self._handle.controlWrite(0x40, 5, int(enabled), 0x0000, "", self.timeout)

    def set_gpio(self, states):
        """Control the GPIO outputs"""
        self._handle.controlWrite(0x40, 6, states & 0xFFFF, 0x0000, "", self.timeout)

    def get_status(self):
        return self._handle.controlRead(0x40, 7, 0x0000, 0x0000, 1, self.timeout)[0]
