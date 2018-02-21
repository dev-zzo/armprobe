"""
Utility functions
"""

from .probe import BluePillProbe

def setup():
    """Sets up the test probe connected to the given serial port"""
    print "Setting up the probe"
    return BluePillProbe()

def setup_swd():
    """Sets up the test probe in SWD mode connected to the given serial port"""
    probe = setup()
    probe.configure_swj(enabled=True)
    probe.switch_to_swd()
    return probe
