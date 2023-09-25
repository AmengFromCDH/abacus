#ifndef OPERATOR_H
#define OPERATOR_H

#include<complex>
#include "module_psi/psi.h"
#include "module_base/global_function.h"
#include "module_base/tool_quit.h"

namespace hamilt
{

enum calculation_type
{
    no,
    pw_ekinetic, 
    pw_nonlocal,
    pw_veff,
    pw_meta,
    lcao_overlap,
    lcao_fixed,
    lcao_gint,
    lcao_deepks,
    lcao_exx,
    lcao_dftu,
};

// Basic class for operator module, 
// it is designed for "O|psi>" and "<psi|O|psi>"
// Operator "O" might have several different types, which should be calculated one by one.
// In basic class , function add() is designed for combine all operators together with a chain. 
template<typename T, typename Device = psi::DEVICE_CPU>
class Operator
{
    public:
    Operator();
    virtual ~Operator();

    //this is the core function for Operator
    // do H|psi> from input |psi> , 

    /// as default, different operators donate hPsi independently
    /// run this->act function for the first operator and run all act() for other nodes in chain table 
    /// if this procedure is not suitable for your operator, just override this function.
    /// output of hpsi would be first member of the returned tuple 
    typedef std::tuple<const psi::Psi<T, Device>*, const psi::Range, T*> hpsi_info;
    virtual hpsi_info hPsi(hpsi_info& input)const;

    virtual void init(const int ik_in);

    virtual void add(Operator* next);

    virtual int get_ik() const { return this->ik; }

    ///do operation : |hpsi_choosed> = V|psi_choosed>
    ///V is the target operator act on choosed psi, the consequence should be added to choosed hpsi
    virtual void act(const int nbands,
        const int nbasis,
        const int npol,
        const T* tmpsi_in,
        T* tmhpsi,
        const int ngk_ik = 0)const {};

    /// an developer-friendly interface for act() function
    virtual psi::Psi<T> act(const psi::Psi<T>& psi_in) const { return psi_in; };

    Operator* next_op = nullptr;

    protected:
    int ik = 0;

    mutable bool in_place = false;

    //calculation type, only different type can be in main chain table 
    enum calculation_type cal_type;
    Operator* next_sub_op = nullptr;
    bool is_first_node = true;

    //if this Operator is first node in chain table, hpsi would not be empty
    mutable psi::Psi<T, Device>* hpsi = nullptr;

    /*This function would analyze hpsi_info and choose how to arrange hpsi storage
    In hpsi_info, if the third parameter hpsi_pointer is set, which indicates memory of hpsi is arranged by developer;
    if hpsi_pointer is not set(nullptr), which indicates memory of hpsi is arranged by Operator, this case is rare. 
    two cases would occurred:
    1. hpsi_pointer != nullptr && psi_pointer == hpsi_pointer , psi would be replaced by hpsi, hpsi need a temporary memory
    2. hpsi_pointer != nullptr && psi_pointer != hpsi_pointer , this is the commonly case 
    */
    T* get_hpsi(const hpsi_info& info)const;

    Device *ctx = {};
    using set_memory_op = psi::memory::set_memory_op<T, Device>;

};

}//end namespace hamilt

#endif