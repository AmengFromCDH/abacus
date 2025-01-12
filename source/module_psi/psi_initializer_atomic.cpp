#include "psi_initializer_atomic.h"
#include "module_hamilt_pw/hamilt_pwdft/soc.h"
// numerical algorithm support
#include "module_base/math_integral.h" // for numerical integration
// numerical algorithm support
#include "module_base/math_polyint.h" // for polynomial interpolation
#include "module_base/math_ylmreal.h" // for real spherical harmonics
// basic functions support
#include "module_base/tool_quit.h"
#include "module_base/timer.h"
// three global variables definition
#include "module_base/global_variable.h"
#include "module_hamilt_pw/hamilt_pwdft/global.h"

psi_initializer_atomic::psi_initializer_atomic(Structure_Factor* sf_in, ModulePW::PW_Basis_K* pw_wfc_in) : psi_initializer(sf_in, pw_wfc_in)
{
    this->set_method("atomic");
    // find correct dimension for ovlp_flzjlq
    int dim1 = GlobalC::ucell.ntype;
    int dim2 = 0; // dim2 should be the maximum number of pseudo atomic orbitals
    for (int it = 0; it < GlobalC::ucell.ntype; it++)
    {
        dim2 = (GlobalC::ucell.atoms[it].ncpp.nchi > dim2) ? GlobalC::ucell.atoms[it].ncpp.nchi : dim2;
    }
    if (dim2 == 0)
    {
        ModuleBase::WARNING_QUIT("psi_initializer_atomic::psi_initializer_atomic", "there is not ANY pseudo atomic orbital read in present system, recommand other methods, quit.");
    }
    int dim3 = GlobalV::NQX;
    // allocate memory for ovlp_flzjlq
    this->ovlp_pswfcjlq.create(dim1, dim2, dim3);
    this->ovlp_pswfcjlq.zero_out();
}


psi_initializer_atomic::~psi_initializer_atomic() {}

void psi_initializer_atomic::set_pseudopot_files(std::string* pseudopot_files)
{
    ModuleBase::timer::tick("psi_initializer_atomic", "set_pseudopot_files");
    for (int itype = 0; itype < GlobalC::ucell.ntype; itype++)
    {
        this->pseudopot_files.push_back(pseudopot_files[itype]);
    }
    ModuleBase::timer::tick("psi_initializer_atomic", "set_pseudopot_files");
}


void psi_initializer_atomic::normalize_pswfc(int n_rgrid, double* pswfc, double* rab)
{
    ModuleBase::timer::tick("psi_initializer_atomic", "normalize_pswfc");
    double* norm_pswfc = new double[n_rgrid];
    for (int ir = 0; ir < n_rgrid; ir++)
    {
        norm_pswfc[ir] = pswfc[ir] * pswfc[ir]; // because in pseudopotential the pswfc already multiplied by r
    }
    double norm = ModuleBase::Integral::simpson(n_rgrid, norm_pswfc, rab);
    delete[] norm_pswfc;
    for (int ir = 0; ir < n_rgrid; ir++)
    {
        pswfc[ir] /= sqrt(norm);
    }
    ModuleBase::timer::tick("psi_initializer_atomic", "normalize_pswfc");
}


void psi_initializer_atomic::cal_ovlp_pswfcjlq()
{
    ModuleBase::timer::tick("psi_initializer_atomic", "cal_ovlp_pswfcjlq");
    int maxn_rgrid = 0;
    double* qgrid = new double[GlobalV::NQX];
    for (int iq = 0; iq < GlobalV::NQX; iq++)
    {
        qgrid[iq] = GlobalV::DQ * iq;
    }
    for (int it=0; it<GlobalC::ucell.ntype; it++)
    {
        maxn_rgrid = (GlobalC::ucell.atoms[it].ncpp.msh > maxn_rgrid) ? GlobalC::ucell.atoms[it].ncpp.msh : maxn_rgrid;
    }
	ModuleBase::GlobalFunc::OUT(GlobalV::ofs_running,"max mesh points in Pseudopotential",maxn_rgrid);
    
    const double pref = ModuleBase::FOUR_PI / sqrt(GlobalC::ucell.omega);

	ModuleBase::GlobalFunc::OUT(GlobalV::ofs_running,"dq(describe PAO in reciprocal space)",GlobalV::DQ);
	ModuleBase::GlobalFunc::OUT(GlobalV::ofs_running,"max q",GlobalV::NQX);

    for (int it=0; it<GlobalC::ucell.ntype; it++)
    {
		Atom* atom = &GlobalC::ucell.atoms[it];

		GlobalV::ofs_running<<"\n number of pseudo atomic orbitals for "<<atom->label<<" is "<< atom->ncpp.nchi << std::endl;

        for (int ic = 0; ic < atom->ncpp.nchi ;ic++)
        {
            int n_rgrid;
            if(GlobalV::PSEUDO_MESH)
                n_rgrid = atom->ncpp.mesh;
            else
                n_rgrid = atom->ncpp.msh;
            double* pswfc = new double[n_rgrid];
            for (int ir=0; ir<n_rgrid; ir++)
            {
                pswfc[ir] = atom->ncpp.chi(ic, ir); // copy pswfc from atom->ncpp.chi to pswfc
            }
            this->normalize_pswfc(n_rgrid, pswfc, atom->ncpp.rab);
            if (atom->ncpp.oc[ic] >= 0.0) // reasonable occupation number, but is it always true?
            {
                const int l = atom->ncpp.lchi[ic];
                double* ovlp_pswfcjlq_q = new double[GlobalV::NQX];
                this->sbt.direct(l, atom->ncpp.msh, atom->ncpp.r, pswfc, GlobalV::NQX, qgrid, ovlp_pswfcjlq_q, 1);
                for (int iq = 0; iq < GlobalV::NQX; iq++)
                {
                    this->ovlp_pswfcjlq(it, ic, iq) = pref * ovlp_pswfcjlq_q[iq];
                }
                delete [] ovlp_pswfcjlq_q;
            }
            delete [] pswfc;
        }
    }
    delete [] qgrid;
    ModuleBase::timer::tick("psi_initializer_atomic", "cal_ovlp_pswfcjlq");
}

std::complex<double> psi_initializer_atomic::phase_factor(double arg, int mode)
{
    if(mode == 1) return std::complex<double>(cos(arg),0);
    else if (mode == -1) return std::complex<double>(0, sin(arg));
    else if (mode == 0) return std::complex<double>(cos(arg), sin(arg));
    else return std::complex<double>(1,0);
}

psi::Psi<std::complex<double>>* psi_initializer_atomic::cal_psig(int ik)
{
    ModuleBase::timer::tick("psi_initializer_atomic", "initialize");
    this->psig->fix_k(ik);
    //this->print_status(psi);
    const int npw = this->pw_wfc->npwk[ik];
    int lmax = GlobalC::ucell.lmax_ppwf;
    const int total_lm = (lmax + 1) * (lmax + 1);
    ModuleBase::matrix ylm(total_lm, npw);
    std::complex<double> *aux = new std::complex<double>[npw];
    double *chiaux = nullptr;

    ModuleBase::Vector3<double> *gk = new ModuleBase::Vector3 <double> [npw];
    for (int ig = 0; ig < npw; ig++)
    {
        gk[ig] = this->pw_wfc->getgpluskcar(ik, ig);
    }
    ModuleBase::YlmReal::Ylm_Real(total_lm, npw, gk, ylm);
    int index = 0;
    double *ovlp_pswfcjlg = new double[npw];
    for (int it = 0; it < GlobalC::ucell.ntype; it++)
    {
        for (int ia = 0; ia < GlobalC::ucell.atoms[it].na; ia++)
        {
/* FOR EVERY ATOM */
            std::complex<double> *sk = this->sf->get_sk(ik, it, ia, this->pw_wfc);
            for (int ipswfc = 0; ipswfc < GlobalC::ucell.atoms[it].ncpp.nchi; ipswfc++)
            {
/* FOR EVERY PSWFC OF ATOM */
                if (GlobalC::ucell.atoms[it].ncpp.oc[ipswfc] >= 0.0)
                {
/* IF IS OCCUPIED, GET L */
                    const int l = GlobalC::ucell.atoms[it].ncpp.lchi[ipswfc];
                    std::complex<double> lphase = pow(ModuleBase::NEG_IMAG_UNIT, l);

                    for (int ig=0; ig<npw; ig++)
                    {
                        ovlp_pswfcjlg[ig] = ModuleBase::PolyInt::Polynomial_Interpolation(
                            this->ovlp_pswfcjlq, it, ipswfc, 
                            GlobalV::NQX, GlobalV::DQ, gk[ig].norm() * GlobalC::ucell.tpiba );
                    }
/* NSPIN == 4 */
                    if(GlobalV::NSPIN == 4)
                    {
                        if(GlobalC::ucell.atoms[it].ncpp.has_so)
                        {
                            Soc soc; soc.rot_ylm(l + 1);
                            const double j = GlobalC::ucell.atoms[it].ncpp.jchi[ipswfc];
    /* NOT NONCOLINEAR CASE, rotation matrix become identity */
                            if (!(GlobalV::DOMAG||GlobalV::DOMAG_Z))
                            {
                                double cg_coeffs[2];
                                for(int m = -l-1; m < l+1; m++)
                                {
                                    cg_coeffs[0] = soc.spinor(l, j, m, 0);
                                    cg_coeffs[1] = soc.spinor(l, j, m, 1);
                                    if (fabs(cg_coeffs[0]) > 1e-8 || fabs(cg_coeffs[1]) > 1e-8)
                                    {
                                        for(int is = 0; is < 2; is++)
                                        {
                                            if(fabs(cg_coeffs[is]) > 1e-8)
                                            {
        /* GET COMPLEX SPHERICAL HARMONIC FUNCTION */
                                                const int ind = GlobalC::ppcell.lmaxkb + soc.sph_ind(l,j,m,is); // ind can be l+m, l+m+1, l+m-1
                                                ModuleBase::GlobalFunc::ZEROS(aux, npw);
                                                for(int n1 = 0; n1 < 2*l+1; n1++)
                                                {
                                                    const int lm = l*l +n1;
                                                    std::complex<double> umM = soc.rotylm(n1, ind);
                                                    if(std::abs(umM) > 1e-8)
                                                    {
                                                        for(int ig = 0; ig < npw; ig++)
                                                        {
                                                            aux[ig] += umM * ylm(lm, ig);
                                                        }
                                                    }
                                                }
                                                for(int ig = 0; ig < npw; ig++)
                                                {
                                                    (*(this->psig))(index, ig + this->pw_wfc->npwk_max*is ) 
                                                    = lphase * cg_coeffs[is] * sk[ig] * aux[ig] * ovlp_pswfcjlg[ig];
                                                }
                                            }
                                            else
                                            {
                                                for(int ig=0; ig < npw; ig++)
                                                {
                                                    (*(this->psig))(index, ig + this->pw_wfc->npwk_max*is ) 
                                                    = std::complex<double>(0.0, 0.0);
                                                }
                                            }
                                        }
                                        index++;
                                    }
                                }
                            }
                            else
                            {
    /* NONCONLINEAR CASE, will use [[cos(a/2)*exp(-ib/2), sin(a/2)*exp(ib/2)], [-sin(a/2)*exp(-ib/2), cos(a/2)*exp(ib/2)]] to rotate */
                                int ipswfc_noncolin_soc;
        /* J = L - 1/2 -> continue */
        /* J = L + 1/2 */
                                if(fabs(j - l + 0.5) < 1e-4) continue;
                                delete[] chiaux; chiaux = new double [npw];
        /* L == 0 */
                                if(l == 0)
                                {
                                    for(int ig = 0; ig < npw; ig++)
                                    {
                                        chiaux[ig] = ovlp_pswfcjlg[ig];
                                    }
                                }
                                else
                                {
        /* L != 0, scan pswfcs that have the same L and satisfy J(pswfc) = L - 0.5 */
                                    for(int jpsiwfc = 0; jpsiwfc < GlobalC::ucell.atoms[it].ncpp.nchi; jpsiwfc++)
                                    {
                                        if(
                                            (GlobalC::ucell.atoms[it].ncpp.lchi[jpsiwfc] == l)
                                          &&(fabs(GlobalC::ucell.atoms[it].ncpp.jchi[jpsiwfc] - l + 0.5) < 1e-4))
                                        {
                                            ipswfc_noncolin_soc = jpsiwfc;
                                            break;
                                        }
                                    }
                                    for(int ig=0;ig<npw;ig++)
                                    {
            /* average <pswfc_a|jl(q)> and <pswfc_b(j=l-1/2)|jl(q)>, a and b seem not necessarily to be equal */
                                        chiaux[ig] =  l *
                                            ModuleBase::PolyInt::Polynomial_Interpolation(
                                                this->ovlp_pswfcjlq, it, ipswfc_noncolin_soc, 
                                                GlobalV::NQX, GlobalV::DQ, gk[ig].norm() * GlobalC::ucell.tpiba);
                                        chiaux[ig] += ovlp_pswfcjlg[ig] * (l + 1.0) ;
                                        chiaux[ig] *= 1/(2.0*l+1.0);
                                    }
                                }
            /* ROTATE ACCORDING TO NONCOLINEAR */
                                double alpha, gamma;
                                std::complex<double> fup, fdw;
                                alpha = GlobalC::ucell.atoms[it].angle1[ia];
                                gamma = -1 * GlobalC::ucell.atoms[it].angle2[ia] + 0.5 * ModuleBase::PI;

                                for(int m = 0; m < 2*l+1; m++)
                                {
                                    const int lm = l*l +m;
                                    if(index+2*l+1>GlobalC::ucell.natomwfc) ModuleBase::WARNING_QUIT("GlobalC::wf.atomic_wfc()","error: too many wfcs");
                                    for(int ig = 0;ig<npw;ig++)
                                    {
                                        aux[ig] = sk[ig] * ylm(lm,ig) * chiaux[ig];
                                    }
                                    //rotate wfc as needed
                                    //first rotation with angle alpha around (OX)
                                    for(int ig = 0;ig<npw;ig++)
                                    {
                                        fup = this->phase_factor(0.5*alpha,  1)*aux[ig];
                                        fdw = this->phase_factor(0.5*alpha, -1)*aux[ig];
                                        //build the orthogonal wfc
                                        //first rotation with angle (alpha + ModuleBase::PI) around (OX)
                                        (*(this->psig))(index, ig                       ) = this->phase_factor( 0.5*gamma)*fup;
                                        (*(this->psig))(index, ig+this->pw_wfc->npwk_max) = this->phase_factor(-0.5*gamma)*fdw;
                                        //second rotation with angle gamma around(OZ)
                                        fup = this->phase_factor(0.5*(alpha + ModuleBase::PI),  1)*aux[ig];
                                        fdw = this->phase_factor(0.5*(alpha + ModuleBase::PI), -1)*aux[ig];
                                        (*(this->psig))(index+2*l+1, ig                       ) = this->phase_factor( 0.5*gamma)*fup;
                                        (*(this->psig))(index+2*l+1, ig+this->pw_wfc->npwk_max) = this->phase_factor(-0.5*gamma)*fdw;
                                    }
                                    index++;
                                }
                                index += 2*l +1;
                            }
                        }
                        else
                        {//atomic_wfc_nc
                            double alpha, gamman;
                            std::complex<double> fup, fdown;
                            //alpha = GlobalC::ucell.magnet.angle1_[it];
                            //gamman = -GlobalC::ucell.magnet.angle2_[it] + 0.5*ModuleBase::PI;
                            alpha = GlobalC::ucell.atoms[it].angle1[ia];
                            gamman = -1 * GlobalC::ucell.atoms[it].angle2[ia] + 0.5 * ModuleBase::PI;
                            for(int m = 0; m < 2*l+1; m++)
                            {
                                const int lm = l*l +m;
                                if(index+2*l+1>GlobalC::ucell.natomwfc) ModuleBase::WARNING_QUIT("GlobalC::wf.atomic_wfc()","error: too many wfcs");
                                for(int ig = 0;ig<npw;ig++)
                                {
                                     aux[ig] = sk[ig] * ylm(lm,ig) * ovlp_pswfcjlg[ig];
                                }
                                //rotate function
                                //first, rotation with angle alpha around(OX)
                                for(int ig = 0; ig<npw; ig++)
                                {
                                     fup = cos(0.5*alpha) * aux[ig];
                                     fdown = ModuleBase::IMAG_UNIT * sin(0.5* alpha) * aux[ig];
                                     //build the orthogonal wfc
                                     //first rotation with angle(alpha+ModuleBase::PI) around(OX)
                                     (*(this->psig))(index,ig) = (cos(0.5 * gamman) + ModuleBase::IMAG_UNIT * sin(0.5*gamman)) * fup;
                                     (*(this->psig))(index,ig+ this->pw_wfc->npwk_max) = (cos(0.5 * gamman) - ModuleBase::IMAG_UNIT * sin(0.5*gamman)) * fdown;
                                     //second rotation with angle gamma around(OZ)
                                     fup = cos(0.5 * (alpha + ModuleBase::PI)) * aux[ig];
                                     fdown = ModuleBase::IMAG_UNIT * sin(0.5 * (alpha + ModuleBase::PI)) * aux[ig];
                                     (*(this->psig))(index+2*l+1,ig) = (cos(0.5*gamman) + ModuleBase::IMAG_UNIT*sin(0.5*gamman))*fup;
                                     (*(this->psig))(index+2*l+1,ig+ this->pw_wfc->npwk_max) = (cos(0.5*gamman) - ModuleBase::IMAG_UNIT*sin(0.5*gamman))*fdown;
                                }
                                index++;
                            }
                            index += 2*l+1;
                        }
                    }
                    else
                    {
                        for (int m = 0; m < 2*l+1; m++)
                        {
                            const int lm = l * l + m;
                            for (int ig = 0; ig < npw; ig++)
                            {
                                (*(this->psig))(index, ig) = lphase * sk [ig] * ylm(lm, ig) * ovlp_pswfcjlg[ig];
                            }
                            index++;
                        }
                    }
                }
            }
			delete [] sk;
        }
    }

    delete[] ovlp_pswfcjlg;
    delete[] gk;
    delete[] aux;
    delete[] chiaux;
	/* complement the rest of bands if there are */
	if(this->get_nbands_complem() > 0)
	{
		this->random_t(this->psig->get_pointer(), index, this->psig->get_nbands(), ik, this->pw_wfc);
	}
    ModuleBase::timer::tick("psi_initializer_atomic", "initialize");
#ifdef PSI_INITIALIZER_TEST
	this->write_psig();
#endif
    return this->psig;
}