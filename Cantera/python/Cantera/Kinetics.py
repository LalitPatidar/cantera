"""
Kinetics managers.
"""

from Cantera.exceptions import CanteraError, getCanteraError
from Cantera.ThermoPhase import ThermoPhase
from Cantera.XML import XML_Node
import Numeric
import _cantera


## def buildKineticsPhases(root=None, id=None):
##     """Return a list of ThermoPhase objects representing the phases
##     involved in a reaction mechanism.

##     root -- XML node contaning a 'kinetics' child
##     id   -- id attribute of the desired 'kinetics' node
##     """
##     kin = root.child(id = id)
##     phase_refs = kin.children("phaseRef")
##     th = None
##     phases = []
##     for p in phase_refs:
##         phase_id = p["id"]
##         try:
##             th = ThermoPhase(root=root, id=phase_id)
##         except:
##             if p["src"]:
##                 pnode = XML_Node(name="root",src=src)
##                 th = ThermoPhase(pnode, phase_id)
##             else:
##                 raise CanteraError("phase "+phase_id+" not found.")
##         phases.append(th)
##     return phases


class Kinetics:
    """
    Kinetics managers. Instances of class Kinetics are responsible for
    evaluating reaction rates of progress, species production rates,
    and other quantities pertaining to a reaction mechanism.

    parameters -
    kintype    - integer specifying the type of kinetics manager to create.
    
    """
    
    def __init__(self, kintype=-1, thrm=0, xml_phase=None, id=None, phases=[]):
        """Build a kinetics manager from an XML specification.

        root     -- root of a CTML tree
        
        id       -- id of the 'kinetics' node within the tree that contains
                    the specification of the parameters.
        """
        np = len(phases)
        self._np = np
        self._sp = []
        self._phnum = {}

        # p0 through p4 are the integer indices of the phase objects
        # corresponding to the input sequence of phases
        
        self._end = [0]
        p0 = phases[0].thermophase()
        p1 = -1
        p2 = -1
        p3 = -1
        p4 = -1
        if np >= 2:
            p1 = phases[1].thermophase()
        if np >= 3:
            p2 = phases[2].thermophase()
        if np >= 4:
            p3 = phases[3].thermophase()
        if np >= 5:
            p4 = phases[4].thermophase()
        if np >= 6:
            raise CanteraError("a maximum of 4 neighbor phases allowed")
        
        self.ckin = _cantera.KineticsFromXML(xml_phase,
                                                 p0, p1, p2, p3, p4)
        
        for nn in range(self._np):
                p = self.phase(nn)
                self._phnum[p.thermophase()] = nn
                self._end.append(self._end[-1]+p.nSpecies())
                for k in range(p.nSpecies()):
                    self._sp.append(p.speciesName(k))

        
    def __del__(self):
        self.clear()
        
    def clear(self):
        """Delete the kinetics manager."""
        if self.ckin > 0:
            _cantera.kin_delete(self.ckin)

    def kin_index(self):
        print "kin_index is deprecated. Use kinetics_hndl."
        return self.ckin

    def kinetics_hndl(self):
        return self.ckin
    
    def kineticsType(self):
        """Kinetics manager type."""
        return _cantera.kin_type(self.ckin)

    def kineticsSpeciesIndex(self, name, phase):
        """The index of a species.
        name  -- species name
        phase -- phase name

        Kinetics managers for heterogeneous reaction mechanisms
        maintain a list of all species in all phases. The order of the
        species in this list determines the ordering of the arrays of
        production rates. This method returns the index for the
        specified species of the specified phase, and is used to
        locate the entry for a particular species in the production
        rate arrays.

        """
        return _cantera.kin_speciesIndex(self.ckin, name, phase)

    def kineticsStart(self, n):
        return _cantera.kin_start(self.ckin, n)

    def phase(self, n):
        return ThermoPhase(index = _cantera.kin_phase(self.ckin, n))
        #return self._ph[_cantera.kin_phase(self.ckin, n)]
    
    def nReactions(self):
        """Number of reactions."""
        return _cantera.kin_nreactions(self.ckin)

    def isReversible(self,i):
        """
        True (1) if reaction number 'i' is reversible,
        and false (0) otherwise.
        """
        return _cantera.kin_isreversible(self.ckin,i)

    def reactionType(self,i):
        """Type of reaction 'i'"""
        return _cantera.kin_rxntype(self.ckin,i)

    #def reactionString(self,i):
    #    return _cantera.kin_getstring(self.ckin,1,i)

    def reactionEqn(self,i):
        try:
            eqs = []
            for rxn in i:
                eqs.append(self.reactionString(rxn))
            return eqs
        except:
            return self.reactionString(i)

    def reactionString(self, i):
        s = ''
        nsp = _cantera.kin_nspecies(self.ckin)
        for k in range(nsp):
            nur = _cantera.kin_rstoichcoeff(self.ckin,k,i)
            if nur <> 0.0:
                if nur <> 1.0:
                    s += `int(nur)`+' '
                s += self._sp[k]+' + '
        s = s[:-2]
        if self.isReversible(i):
            s += ' <=> '
        else:
            s += ' => '
        for k in range(nsp):
            nup = _cantera.kin_pstoichcoeff(self.ckin,k,i)
            if nup <> 0.0:
                if nup <> 1.0:
                    s += `int(nup)`+' '
                s += self._sp[k]+' + '
        s = s[:-2]
        return s
    
    def reactantStoichCoeff(self,k,i):
        return _cantera.kin_rstoichcoeff(self.ckin,k,i)
    
    def reactantStoichCoeffs(self):
        nsp = _cantera.kin_nspecies(self.ckin)
        nr = _cantera.kin_nreactions(self.ckin)
        nu = Numeric.zeros((nsp,nr),'d')
        for i in range(nr):
            for k in range(nsp):
                nu[k,i] = _cantera.kin_rstoichcoeff(self.ckin,k,i)
        return nu

    def productStoichCoeff(self,k,i):
        return _cantera.kin_pstoichcoeff(self.ckin,k,i)    

    def productStoichCoeffs(self):
        nsp = _cantera.kin_nspecies(self.ckin)
        nr = _cantera.kin_nreactions(self.ckin)
        nu = Numeric.zeros((nsp,nr),'d')
        for i in range(nr):
            for k in range(nsp):
                nu[k,i] = _cantera.kin_pstoichcoeff(self.ckin,k,i)
        return nu
    
    def fwdRatesOfProgress(self):
        return _cantera.kin_getarray(self.ckin,10)

    def revRatesOfProgress(self):
        return _cantera.kin_getarray(self.ckin,20)

    def netRatesOfProgress(self):
        return _cantera.kin_getarray(self.ckin,30)

    def equilibriumConstants(self):
        return _cantera.kin_getarray(self.ckin,40)

    def creationRates(self, phase = None):
        c = _cantera.kin_getarray(self.ckin,50)
        if phase:
            kp = phase.thermophase()
            if self._phnum.has_key(kp):
                n = self._phnum[kp]
                return c[self._end[n]:self._end[n+1]]
            else:
                raise CanteraError('unknown phase')
        else:
            return c                
                

    def destructionRates(self, phase = None):
        d = _cantera.kin_getarray(self.ckin,60)
        if phase:
            kp = phase.thermophase()            
            if self._phnum.has_key(kp):
                n = self._phnum[kp]
                return d[self._end[n]:self._end[n+1]]
            else:
                raise CanteraError('unknown phase')
        else:
            return d

    
    def netProductionRates(self, phase = None):
        w = _cantera.kin_getarray(self.ckin,70)        
        if phase:
            kp = phase.thermophase()            
            if self._phnum.has_key(kp):
                n = self._phnum[kp]
                return w[self._end[n]:self._end[n+1]]
            else:
                raise CanteraError('unknown phase')
        else:
            return w
        
    def sourceTerms(self):
        return _cantera.kin_getarray(self.ckin,80)        

    def multiplier(self,i):
        return _cantera.kin_multiplier(self.ckin,i)

    def setMultiplier(self, value = 0.0, reaction = -1):
        if reaction < 0:
            nr = self.nReactions()
            for i in range(nr):
                _cantera.kin_setMultiplier(self.ckin,i,value)
        else:
            _cantera.kin_setMultiplier(self.ckin,reaction,value)    
    
    def advanceCoverages(self,dt):
        return _cantera.kin_advanceCoverages(self.ckin,dt)    









    

    
