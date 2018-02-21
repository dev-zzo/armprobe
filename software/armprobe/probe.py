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
        self._context = usb1.USBContext()
        self._handle = self._context.openByVendorIDAndProductID(0xDECA, 0x0002, skip_on_error=True)
        if self._handle is None:
            raise ProbeException("probe not found")
        self._iface = self._handle.claimInterface(0)
        self._handle.setConfiguration(1)
        self.ping()
        
    def _execute(self, op, param8=0, param16=0, param32=0):
        cmd = struct.pack('<IBBHI', 0xDEADBABE, op, param8, param16, param32)
        print '>', hexdump(cmd, 's')
        self._handle.bulkWrite(1, cmd)
        rsp = self._handle.bulkRead(2, 12)
        print '<', hexdump(rsp, 'b')
        if len(rsp) < 5:
            raise ProbeException("received response too short")
        tag, status = struct.unpack('<IB', rsp[:5])
        if tag != 0xDEADBABE:
            raise ProbeException("invalid tag received (%08X)" % tag)
        if status != 0:
            raise ProbeException("probe reports an error %d" % status)
        if len(rsp) >= 6:
            swd_response = rsp[5]
            if swd_response != 1:
                raise SWDException(swd_response)
            if len(rsp) >= 12:
                data, = struct.unpack('<I', rsp[8:])
                return data
        return None

    def ping(self):
        self._execute(0)
        return True

    def configure_swj(self, enabled=True):
        """Configures the SWJ unit"""
        bits = 0x00
        if enabled:
            bits |= 0x01
        else:
            pass
        self._execute(1, param8=bits)

    def switch_to_swd(self):
        """Issue the SWJ-DP sequence to switch to SWD"""
        self._execute(2)

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
        return self._execute(3, param8=BluePillProbe._build_request(True, is_ap, a32))

    def _write(self, is_ap, a32, data):
        """Execute a write transaction via SWD"""
        self._execute(4, param8=BluePillProbe._build_request(False, is_ap, a32), param32=data)

    def configure_gpio(self, enabled=True):
        """Configure the GPIO unit (currently only enable/disable)"""
        self._execute(5, param8=enabled)

    def set_gpio(self, states):
        self._execute(6, param8=states)
