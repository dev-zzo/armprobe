"""
Utility functions
"""

import serial
from .probe import BluePillProbe

def setup(portname):
    """Sets up the test probe connected to the given serial port"""
    print "Setting up the probe at port: %s" % (portname)
    port = serial.Serial(portname, baudrate=1125000, timeout=15)
    probe = BluePillProbe(port)
    return probe

def setup_swd(portname):
    """Sets up the test probe in SWD mode connected to the given serial port"""
    probe = setup(portname)
    probe.configure_swj(enabled=True)
    probe.switch_to_swd()
    return probe
