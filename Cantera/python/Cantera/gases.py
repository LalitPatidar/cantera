"""Gas mixtures.

These functions all return instances of class Solution that represent
gas mixtures.

"""
# for pydoc
import solution, constants

from constants import *
from Cantera.solution import Solution

#import _cantera
import os

def IdealGasMix(src="", id = ""):
    """Return a Solution object representing an ideal gas mixture.

    src       --- input file
    root      --- root of an XML tree containing the phase specification.
                  Specify src or root but not both.
    thermo    --- auxiliary thermo database
    transport --- transport model
    trandb    --- transport database
    """
    return Solution(src=src,id=id)


def GRI30(transport = ""):
    """Return a Solution instance implementing reaction mechanism
    GRI-Mech 3.0."""
    if transport == "":
        return Solution(src="gri30.cti", id="gri30")
    elif transport == "Mix":
        return Solution(src="gri30.cti", id="gri30_mix")
    elif transport == "Multi":
        return Solution(src="gri30.cti", id="gri30_multi")    
    

def Air():
    """Return a Solution instance implementing the O/N/Ar portion of
    reaction mechanism GRI-Mech 3.0. The initial composition is set to
    that of air"""    
    return Solution(src="air.cti", id="air")


def Argon():
    """Return a Solution instance representing pure argon."""    
    return Solution(src="argon.cti", id="argon")

