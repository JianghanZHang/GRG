///////////////////////////////////////////////////////////////////////////////
// BSD 3-Clause License
// Copyright (C) 2023, New York University
//
// Copyright note valid unless otherwise stated in individual files.
// All rights reserved.
///////////////////////////////////////////////////////////////////////////////
#include <iostream>
#include <iomanip>

#include <crocoddyl/core/utils/exception.hpp>
#include "grg/grg_adahessian.hpp"

using namespace crocoddyl;

namespace grg {

SolverGRG_ADAHESSIAN::SolverGRG_ADAHESSIAN(boost::shared_ptr<crocoddyl::ShootingProblem> problem)
    : SolverAbstract(problem){

      #ifdef CROCODDYL_WITH_MULTITHREADING
      std::cout << "Built with multithreading" << std::endl;
      #endif  // CROCODDYL_WITH_MULTITHREADING


      if (!problem_) {
        std::cerr << "SolverGRG_ADAHESSIAN constructor received a nullptr for problem." << std::endl;
        throw std::invalid_argument("SolverGRG_ADAHESSIAN constructor requires a non-null problem");
        }

      const std::size_t T = problem_->get_T();
      const std::size_t ndx = problem_->get_ndx();
      xs_try_.resize(T + 1);
      us_try_.resize(T);
      fs_try_.resize(T + 1);
      dx_.resize(T+1);
      du_.resize(T);
      Vx_.resize(T + 1);
      Vu_.resize(T);
      D_square_.resize(T);
      m_corrected_.resize(T);
      v_corrected_.resize(T);
      m_.resize(T);
      v_.resize(T);
      fs_flat_.resize(ndx*(T + 1));
      fs_flat_.setZero();
      gap_list_.resize(filter_size_);
      cost_list_.resize(filter_size_);
      tmpxs_.resize(T + 1);
      Hxx_.resize(T+1);
      Huu_.resize(T);
      FuTHxx_p_.resize(T);
      D_.resize(T);

      const std::vector<boost::shared_ptr<crocoddyl::ActionModelAbstract> >& models = problem_->get_runningModels();
      for (std::size_t t = 0; t < T; ++t) {
        const boost::shared_ptr<crocoddyl::ActionModelAbstract>& model = models[t];
        const std::size_t nu = model->get_nu();

        Hxx_[t] = Eigen::MatrixXd::Zero(ndx, ndx);
        Huu_[t] = Eigen::MatrixXd::Zero(nu, nu);
        Vx_[t] = Eigen::VectorXd::Zero(ndx);
        Vu_[t] = Eigen::VectorXd::Zero(nu);
        m_corrected_[t] = Eigen::VectorXd::Zero(nu);
        v_corrected_[t] = Eigen::VectorXd::Zero(nu);
        m_[t] = Eigen::VectorXd::Zero(nu);
        v_[t] = Eigen::VectorXd::Zero(nu);
        D_[t] = Eigen::VectorXd::Zero(nu);
        D_square_[t] = Eigen::VectorXd::Zero(nu);

        if (t == 0) {
            xs_try_[t] = problem_->get_x0();
        } 
        else {
            xs_try_[t] = model->get_state()->zero();
        }
        us_try_[t] = Eigen::VectorXd::Zero(nu);
        dx_[t] = Eigen::VectorXd::Zero(ndx);
        du_[t] = Eigen::VectorXd::Zero(nu);
        fs_try_[t] = Eigen::VectorXd::Zero(ndx);
        dx_[t] = Eigen::VectorXd::Zero(ndx);
        du_[t] = Eigen::VectorXd::Zero(nu);
        fs_try_[t] = Eigen::VectorXd::Zero(ndx);
        tmpxs_[t] = Eigen::VectorXd::Zero(ndx); 
        FuTHxx_p_[t] = Eigen::MatrixXd::Zero(nu, ndx);
      }
      dx_.back() = Eigen::VectorXd::Zero(ndx);
      dx_.back().setZero();
      fs_try_.back() = Eigen::VectorXd::Zero(ndx);
      fs_try_.back() = Eigen::VectorXd::Zero(ndx);
      xs_try_.back() = problem_->get_terminalModel()->get_state()->zero();
      Vx_.back() = Eigen::VectorXd::Zero(ndx);
      Hxx_.back() = Eigen::MatrixXd::Zero(ndx, ndx);
      FxTHxx_p_ = Eigen::MatrixXd::Zero(ndx, ndx);
      
      const std::size_t n_alphas = 3;
      alphas_.resize(n_alphas);
      for (std::size_t n = 0; n < n_alphas; ++n) {
        alphas_[n] = 2. / pow(4., static_cast<double>(n));
      }
    }

SolverGRG_ADAHESSIAN::~SolverGRG_ADAHESSIAN() {}

bool SolverGRG_ADAHESSIAN::solve(const std::vector<Eigen::VectorXd>& init_xs, const std::vector<Eigen::VectorXd>& init_us,
                       const std::size_t maxiter, const bool is_feasible, const double reginit) {
  // const std::size_t T = problem_->get_T();
  // for (std::size_t t = 0; t < T; ++t) {
  //   m_[t].setZero();
  //   v_[t].setZero(); 
  // }
  START_PROFILER("SolverGRG_ADAHESSIAN::solve");
  (void)is_feasible;
  
  if (problem_->is_updated()) {
    resizeData();
  }
  tmpxs_ = problem_->rollout_us(init_us);
  setCandidate(tmpxs_, init_us, false);
  // setCandidate(init_xs, init_us, false);

  xs_[0] = problem_->get_x0();      // Otherwise xs[0] is overwritten by init_xs inside setCandidate()
  xs_try_[0] = problem_->get_x0();  // it is needed in case that init_xs[0] is infeasible

  bool recalcDiff = true;
  for (iter_ = 0; iter_ < maxiter; ++iter_) {

    recalcDiff = true;

    while (true) {
      try {
        computeDirection(recalcDiff);
      } 
      catch (std::exception& e) {
        std::cout << "Error in computeDirection" << std::endl;
        return false;
      }
      break;
    }

    // KKT termination criteria
    checkKTConditions();
    if(with_callbacks_){
      printCallbacks();
    }
    if (KKT_ <= termination_tol_) {
      if(with_callbacks_){
        printCallbacks();
      }
      STOP_PROFILER("SolverGRG_ADAHESSIAN::solve");
      return true;
    }
      
    gap_list_.push_back(gap_norm_);
    cost_list_.push_back(cost_);

    if(use_line_search_){
        // We need to recalculate the derivatives when the step length passes
        tmp_ub_ = c_ * curvature_;
        tmp_lb_ = (1-c_) * curvature_;
        for (std::vector<double>::const_iterator it = alphas_.begin(); it != alphas_.end(); ++it) {
        steplength_ = *it;
        try {
            merit_try_ = tryStep(steplength_);
            ub_ = merit_+ steplength_ * tmp_ub_;
            // lb_ = merit_ + steplength_ * tmp_lb_;
          } catch (std::exception& e) {
            std::cout << "Error in line search" << std::endl;
            continue;
          }
        // Goldenstein line search criteria
        // if ((merit_try_ <= ub_ && merit_try_ >= lb_) || steplength_ == alphas_.back()) {
        if ((merit_try_ <= ub_) || steplength_ == alphas_.back()) {
            setCandidate(xs_try_, us_try_, false);
            recalcDiff = true;
            break;
          }
        }
    }
    else{
        steplength_ = const_step_length_;
        merit_try_ = tryStep(steplength_);
        setCandidate(xs_try_, us_try_, false);
        recalcDiff = true;
    }
  }
  STOP_PROFILER("SolverGRG_ADAHESSIAN::solve");

  return false;
}

void SolverGRG_ADAHESSIAN::computeDirection(const bool recalcDiff){
  START_PROFILER("SolverGRG_ADAHESSIAN::computeDirection");
  if (recalcDiff) {
      calcDiff();
  }
  gap_norm_ = 0;
  const std::size_t T = problem_->get_T();

  #ifdef CROCODDYL_WITH_MULTITHREADING
  #pragma omp simd reduction(+:gap_norm_)
  #endif
  for (std::size_t t = 0; t < T; ++t) {
    gap_norm_ += fs_[t].lpNorm<1>();   
  }
  gap_norm_ += fs_.back().lpNorm<1>();   
  merit_ = cost_ + mu_*gap_norm_;

  backwardPass();
  forwardPass();

  STOP_PROFILER("SolverGRG_ADAHESSIAN::computeDirection");
}

void SolverGRG_ADAHESSIAN::checkKTConditions(){
  KKT_ = 0.;
  const std::size_t T = problem_->get_T();
  const std::size_t ndx = problem_->get_ndx();
  const std::vector<boost::shared_ptr<ActionDataAbstract> >& datas = problem_->get_runningDatas();
  for (std::size_t t = 0; t < T; ++t) {
    const boost::shared_ptr<ActionDataAbstract>& d = datas[t];
    
    KKT_ = std::max(KKT_, Vx_[t].lpNorm<Eigen::Infinity>());
    KKT_ = std::max(KKT_, Vu_[t].lpNorm<Eigen::Infinity>());
    fs_flat_.segment(t*ndx, ndx) = fs_[t];
  }
  fs_flat_.tail(ndx) = fs_.back();
  const boost::shared_ptr<ActionDataAbstract>& d_ter = problem_->get_terminalData();
  KKT_ = std::max(KKT_, Vx_[T].lpNorm<Eigen::Infinity>());
  KKT_ = std::max(KKT_, fs_flat_.lpNorm<Eigen::Infinity>());
}

double SolverGRG_ADAHESSIAN::stoppingCriteria() {
  stop_ = 0.;
  const std::size_t T = this->problem_->get_T();
  const std::vector<boost::shared_ptr<ActionModelAbstract> >& models = problem_->get_runningModels();

  for (std::size_t t = 0; t < T; ++t) {
    const std::size_t nu = models[t]->get_nu();
    if (nu != 0) {
      stop_ += Vu_[t].squaredNorm();
    }
  }
  return stop_;
}

const Eigen::Vector2d& SolverGRG_ADAHESSIAN::expectedImprovement() {
  d_.fill(0.0);
//   const std::size_t T = this->problem_->get_T();
//   const std::vector<boost::shared_ptr<ActionModelAbstract> >& models = problem_->get_runningModels();
//   for (std::size_t t = 0; t < T; ++t) {
//     const std::size_t nu = models[t]->get_nu();
//     if (nu != 0) {
//       d_[0] += Qu_[t].dot(k_[t]);
//       d_[1] -= k_[t].dot(Quuk_[t]);
//     }
//   }
  return d_;
}

void SolverGRG_ADAHESSIAN::resizeData() {
  START_PROFILER("SolverGRG_ADAHESSIAN::resizeData");
  SolverAbstract::resizeData();

  const std::size_t T = problem_->get_T();
  const std::size_t ndx = problem_->get_ndx();
  const std::vector<boost::shared_ptr<ActionModelAbstract> >& models = problem_->get_runningModels();
  for (std::size_t t = 0; t < T; ++t) {
    const boost::shared_ptr<ActionModelAbstract>& model = models[t];
    const std::size_t nu = model->get_nu();

    Vx_[t].conservativeResize(ndx);
    Vu_[t].conservativeResize(nu);

    m_[t].conservativeResize(nu);
    v_[t].conservativeResize(nu);
  }
  STOP_PROFILER("SolverGRG_ADAHESSIAN::resizeData");
}

double SolverGRG_ADAHESSIAN::calcDiff() {
  START_PROFILER("SolverGRG_ADAHESSIAN::calcDiff");
  if (iter_ == 0) {
    problem_->calc(xs_, us_);
  }
  cost_ = problem_->calcDiff(xs_, us_);

  ffeas_ = computeDynamicFeasibility();
  STOP_PROFILER("SolverGRG_ADAHESSIAN::calcDiff");
  return cost_;
}

void SolverGRG_ADAHESSIAN::backwardPass() {
  START_PROFILER("SolverGRG_ADAHESSIAN::backwardPass");
  const boost::shared_ptr<ActionDataAbstract>& d_T = problem_->get_terminalData();
  Vx_.back() = d_T->Lx;
  Hxx_.back() = d_T->Lxx;

  const std::vector<boost::shared_ptr<ActionModelAbstract> >& models = problem_->get_runningModels();
  const std::vector<boost::shared_ptr<ActionDataAbstract> >& datas = problem_->get_runningDatas();
  for (int t = static_cast<int>(problem_->get_T()) - 1; t >= 0; --t) {
    const boost::shared_ptr<ActionModelAbstract>& m = models[t];
    const boost::shared_ptr<ActionDataAbstract>& d = datas[t];
    const Eigen::VectorXd& Vx_p = Vx_[t + 1];
    const Eigen::MatrixXd& Hxx_p = Hxx_[t + 1];
    const std::size_t nu = m->get_nu();
    START_PROFILER("SolverGRG_ADAHESSIAN::Qxx");
    FxTHxx_p_ = d->Fx.transpose() * Hxx_p;
    Hxx_[t] = d->Lxx;
    Hxx_[t].noalias() += FxTHxx_p_ * d->Fx;
    STOP_PROFILER("SolverGRG_ADAHESSIAN::Qxx");
    
    START_PROFILER("SolverGRG_ADAHESSIAN::Qx");
    Vx_[t] = d->Lx;
    Vx_[t].noalias() += d->Fx.transpose() * Vx_p;
    STOP_PROFILER("SolverGRG_ADAHESSIAN::Qx");
    if (nu != 0) {
      START_PROFILER("SolverGRG_ADAHESSIAN::Qu");
      Vu_[t] = d->Lu;
      Vu_[t].noalias() += d->Fu.transpose() * Vx_p;
      STOP_PROFILER("SolverGRG_ADAHESSIAN::Qu");

      START_PROFILER("SolverGRG_ADAHESSIAN::Quu");
      FuTHxx_p_[t].noalias() = d->Fu.transpose() * Hxx_p;
      Huu_[t] = d->Luu;
      Huu_[t].noalias() += FuTHxx_p_[t] * d->Fu;
      STOP_PROFILER("SolverGRG_ADAHESSIAN::Quu");
    }

    if (raiseIfNaN(Vx_[t].lpNorm<Eigen::Infinity>())) {
      throw_pretty("backward_error");
    }
  }
  STOP_PROFILER("SolverGRG_ADAHESSIAN::backwardPass");
}


void SolverGRG_ADAHESSIAN::forwardPass(){
    START_PROFILER("SolverGRG_ADAHESSIAN::forwardPass");
    x_grad_norm_ = 0; 
    u_grad_norm_ = 0;
    curvature_ = 0;

    const std::size_t T = problem_->get_T();
    const std::vector<boost::shared_ptr<ActionDataAbstract> >& datas = problem_->get_runningDatas();

    if(correct_bias_){
        #ifdef CROCODDYL_WITH_MULTITHREADING
        #pragma omp parallel for num_threads(problem_->get_nthreads())
        #endif
        for (std::size_t t = 0; t < T; ++t) {
        const boost::shared_ptr<ActionDataAbstract>& d = datas[t];
        // D_[t] = Huu_[t].diagonal();
        // D_square_[t] = D_[t].cwiseProduct(D_[t]);
        D_square_[t] = Vu_[t].cwiseProduct(Vu_[t]);

        m_[t] = beta1_ * m_[t] + (1 - beta1_) * Vu_[t];
        v_[t] = beta2_ * v_[t] + (1 - beta2_) * D_square_[t];

        m_corrected_[t] = m_[t] / (1 - pow(beta1_, iter_+1));
        v_corrected_[t] = v_[t] / (1 - pow(beta2_, iter_+1));

        du_[t] = - m_corrected_[t].cwiseQuotient(v_corrected_[t].cwiseSqrt());
        // du_[t].noalias() = - Vu_[t]; //vanilla gradient descent
        // du_[t].noalias() = - Vu_[t];
        u_grad_norm_ += du_[t].lpNorm<1>();
        curvature_ += du_[t].dot(Vu_[t]);
        }
        
        for (std::size_t t = 0; t < T; ++t) {
        const boost::shared_ptr<ActionDataAbstract>& d = datas[t];
        dx_[t+1] = fs_[t+1];
        dx_[t+1].noalias() += d->Fu * du_[t];
        dx_[t+1].noalias() += d->Fx * dx_[t];
        x_grad_norm_ += dx_[t].lpNorm<1>(); // assuming that there is no gap in the initial state
        }
    }

    else{

        #ifdef CROCODDYL_WITH_MULTITHREADING
        #pragma omp parallel for num_threads(problem_->get_nthreads())
        #endif
        for (std::size_t t = 0; t < T; ++t) {
        const boost::shared_ptr<ActionDataAbstract>& d = datas[t];
        D_[t] = Huu_[t].diagonal();
        D_square_[t] = D_[t].cwiseProduct(D_[t]);

        m_[t] = (1 - beta1_) * Vu_[t] + beta1_ * m_[t];
        v_[t] = (1 - beta2_) * D_square_[t] + beta2_ * v_[t];
        
        du_[t] = - m_[t].cwiseQuotient(v_[t].cwiseSqrt());
        u_grad_norm_ += du_[t].lpNorm<1>();
        }
        
        for (std::size_t t = 0; t < T; ++t) {
        const boost::shared_ptr<ActionDataAbstract>& d = datas[t];
        dx_[t+1] = fs_[t+1];
        dx_[t+1].noalias() += d->Fu * du_[t];
        dx_[t+1].noalias() += d->Fx * dx_[t];
        x_grad_norm_ += dx_[t].lpNorm<1>(); // assuming that there is no gap in the initial state
        }
    }

    x_grad_norm_ += dx_.back().lpNorm<1>(); // assuming that there is no gap in the initial state
    x_grad_norm_ = x_grad_norm_/(double)(T+1);
    u_grad_norm_ = u_grad_norm_/(double)T; 
    STOP_PROFILER("SolverGRG_ADAHESSIAN::forwardPass");

}

double SolverGRG_ADAHESSIAN::tryStep(const double steplength) {
    // if (steplength > 1. || steplength < 0.) {
    //     throw_pretty("Invalid argument: "
    //                 << "invalid step length, value is between 0. to 1.");
    // }
    if (steplength < 0.) {
        throw_pretty("Invalid argument: "
                    << "invalid step length, value is between 0. to 1.");
    }
    START_PROFILER("SolverGRG_ADAHESSIAN::tryStep");
    cost_try_ = 0.;
    merit_try_ = 0;
    gap_norm_try_ = 0;

    const std::size_t T = problem_->get_T();
    const std::vector<boost::shared_ptr<ActionModelAbstract> >& models = problem_->get_runningModels();
    const std::vector<boost::shared_ptr<ActionDataAbstract> >& datas = problem_->get_runningDatas();

    #ifdef CROCODDYL_WITH_MULTITHREADING
    #pragma omp parallel for num_threads(problem_->get_nthreads())
    #endif
    for (std::size_t t = 0; t < T; ++t) {
        const boost::shared_ptr<ActionModelAbstract>& m = models[t];
        const boost::shared_ptr<ActionDataAbstract>& d = datas[t];
        const std::size_t nu = m->get_nu();

        m->get_state()->integrate(xs_[t], steplength * dx_[t], xs_try_[t]); 
        if (nu != 0) {
            us_try_[t] = us_[t];
            us_try_[t].noalias() += steplength * du_[t];
        }      
        m->calc(d, xs_try_[t], us_try_[t]);        
        

        if (raiseIfNaN(cost_try_)) {
          STOP_PROFILER("SolverGRG_ADAHESSIAN::tryStep");
          throw_pretty("step_error");
        }   
    }

    #ifdef CROCODDYL_WITH_MULTITHREADING
    #pragma omp simd reduction(+:gap_norm_try_, cost_try_)
    #endif
    for (std::size_t t = 0; t < T; ++t) {
        const boost::shared_ptr<ActionModelAbstract>& m = models[t];
        const boost::shared_ptr<ActionDataAbstract>& d = datas[t];
        cost_try_ += d->cost;
        if (t > 0){
          const boost::shared_ptr<ActionDataAbstract>& d_prev = datas[t-1];
          m->get_state()->diff(xs_try_[t], d_prev->xnext, fs_try_[t-1]);
          gap_norm_try_ += fs_try_[t-1].lpNorm<1>(); 
        } 
    }

    // Terminal state update
    const boost::shared_ptr<crocoddyl::ActionModelAbstract>& m_ter = problem_->get_terminalModel();
    const boost::shared_ptr<crocoddyl::ActionDataAbstract>& d_ter = problem_->get_terminalData();
    m_ter->get_state()->integrate(xs_.back(), steplength * dx_.back(), xs_try_.back()); 
    m_ter->calc(d_ter, xs_try_.back());
    cost_try_ += d_ter->cost;

    const boost::shared_ptr<crocoddyl::ActionModelAbstract>& m = models[T-1];
    const boost::shared_ptr<crocoddyl::ActionDataAbstract>& d = datas[T-1];
    
    m->get_state()->diff(xs_try_.back(), d->xnext, fs_try_[T-1]);
    gap_norm_try_ += fs_try_[T-1].lpNorm<1>(); 

    merit_try_ = cost_try_ + mu_ * gap_norm_try_;

    if (raiseIfNaN(cost_try_)) {
        STOP_PROFILER("SolverGRG_ADAHESSIAN::tryStep");
        throw_pretty("step_error");
    }

    STOP_PROFILER("SolverGRG_ADAHESSIAN::tryStep");

    return merit_try_;
}

void SolverGRG_ADAHESSIAN::printCallbacks(){
  if (this->get_iter() % 10 == 0) {
    std::cout << "iter     merit         cost         grad      step    ||gaps||        KKT";
    std::cout << std::endl;
  }
  if(KKT_ < termination_tol_){
    std::cout << std::setw(4) << "END" << "  ";
  } else {
    std::cout << std::setw(4) << this->get_iter() << "  ";
  }
  std::cout << std::scientific << std::setprecision(5) << this->get_merit() << "  ";
  std::cout << std::scientific << std::setprecision(5) << this->get_cost() << "  ";
  std::cout << this->get_ugrad_norm() << "  ";
  if(KKT_ < termination_tol_){
    std::cout << std::fixed << std::setprecision(4) << " ---- " << "  ";
  } else {
    std::cout << std::fixed << std::setprecision(4) << this->get_steplength() << "  ";
  }
  std::cout << std::scientific << std::setprecision(5) << this->get_gap_norm() << "  ";
  std::cout << std::scientific << std::setprecision(5) << KKT_;
  std::cout << std::scientific << std::setprecision(5) << KKT_;
  std::cout << std::endl;
  std::cout << std::flush;
}

void SolverGRG_ADAHESSIAN::setCallbacks(bool inCallbacks){
  with_callbacks_ = inCallbacks;
}

bool SolverGRG_ADAHESSIAN::getCallbacks(){
  return with_callbacks_;
}
// double SolverSQP::get_th_acceptnegstep() const { return th_acceptnegstep_; }

// void SolverSQP::set_th_acceptnegstep(const double th_acceptnegstep) {
//   if (0. > th_acceptnegstep) {
//     throw_pretty("Invalid argument: "
//                  << "th_acceptnegstep value has to be positive.");
//   }
//   th_acceptnegstep_ = th_acceptnegstep;
// }

}  // namespace mim_solvers
