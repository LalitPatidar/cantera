/**
 * @file Sim1D.h
 */

#ifndef CT_SIM1D_H
#define CT_SIM1D_H

#include "OneDim.h"
#include "../funcs.h"

namespace Cantera {

    /**
     * One-dimensional simulations. Class Sim1D extends class OneDim
     * by storing the solution vector, and by adding a hybrid
     * Newton/time-stepping solver.
     */
    class Sim1D : public OneDim {

    public: 

        /**
         * Default constructor. This constructor is provided to make
         * the class default-constructible, but is not meant to be
         * used in most applications.  Use the next constructor
         * instead.
         */
        Sim1D();


        /**
         * Standard constructor. 
         * @param domains A vector of pointers to the domains to be linked together.
         * The domain pointers must be entered in left-to-right order --- i.e., 
         * the pointer to the leftmost domain is domain[0], the pointer to the
         * domain to its right is domain[1], etc. 
         */ 
        Sim1D(vector<Domain1D*>& domains);

        /// Destructor. Does nothing.
        virtual ~Sim1D(){}


        /**
         * @name Setting initial values
         *
         * These methods are used to set the initial values of 
         * solution components.
         */
        //@{

        /// Set one entry in the solution vector.
        void setValue(int dom, int comp, int localPoint,  doublereal value);

        /// Get one entry in the solution vector.
        doublereal value(int dom, int comp, int localPoint) const;

        doublereal workValue(int dom, int comp, int localPoint) const;

        /// Specify a profile for one component of one domain.
        void setProfile(int dom, int comp, const vector_fp& pos, 
            const vector_fp& values);

        /// Set component 'comp' of domain 'dom' to value 'v' at all points.
        void setFlatProfile(int dom, int comp, doublereal v);

        //@}

        void save(string fname, string id, string desc);

        /// Print to stream s the current solution for all domains.     
        void showSolution(ostream& s);
        void showSolution();

        const doublereal* solution() { return m_x.begin(); }

        void setTimeStep(doublereal stepsize, int n, integer* tsteps);

        //void setMaxTimeStep(doublereal tmax) { m_maxtimestep = tmax; }

        void solve(int loglevel = 0, bool refine_grid = true);

        void eval(doublereal rdt=-1.0, int count = 1) {
            OneDim::eval(-1, m_x.begin(), m_xnew.begin(), rdt, count);
        }

        /// Refine the grid in all domains.
        int refine(int loglevel=0);

        /// Set the criteria for grid refinement.
        void setRefineCriteria(int dom = -1, doublereal ratio = 10.0,
            doublereal slope = 0.8, doublereal curve = 0.8, doublereal prune = -0.1);

        void restore(string fname, string id);
        void getInitialSoln();

        void setSolution(const doublereal* soln) {
            copy(soln, soln + m_x.size(), m_x.begin());
        }

        const doublereal* solution() const { return m_x.begin(); }

    protected:

        vector_fp m_x;          // the solution vector
        vector_fp m_xnew;       // a work array used to hold the residual 
                                //      or the new solution
        doublereal m_tstep;     // timestep
        vector_int m_steps;     // array of number of steps to take before 
                                //      re-attempting the steady-state solution


    private:

        /// Calls method _finalize in each domain.
        void finalize();

        void newtonSolve(int loglevel);


    };

}
#endif


