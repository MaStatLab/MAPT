#include "modeltree_node.h"
#include "gbt.h"    
#include "calc.cpp" 

double GBT::make_rho0(int level) {

    if (rho_mode == 0) { // constant rho0
      return rho0;
    }
    if (rho_mode == 1) {
      return 1-(1-rho0)/(level+1)/(level+1);
    }
    if (rho_mode == 2) {
      return 0.1+0.9/pow(rho0,k-level);
    }
}

double GBT::get_log_Ma(double theta0,int n_0,int n_1,int t) {
  double nu,lognu;
  double lb_lognu;
  double ub_lognu;
  double bandwidth;
  double log_Ma_curr = -DBL_MAX;
  double log_Ma_nu;
  int i;
  
  // compute the marginal likelihood for state t with G=n_grid grid points
  lb_lognu = lognu_lowerbound + (1.0*t)/n_s*(lognu_upperbound-lognu_lowerbound);
  ub_lognu = lognu_lowerbound + (1.0*(t+1))/n_s*(lognu_upperbound-lognu_lowerbound);

  bandwidth = (ub_lognu - lb_lognu)/n_grid;
 
  for (i=0; i < n_grid; i++) {
    
    lognu=lb_lognu + (i+0.5)*bandwidth;
    nu = pow(10.0,lognu);

       
    log_Ma_nu = lgamma(theta0*nu+n_0)+lgamma((1-theta0)*nu+n_1) - lgamma(nu+n_0+n_1)
      - (lgamma(theta0*nu)+lgamma((1-theta0)*nu) - lgamma(nu)); 

    if (i == 0) log_Ma_curr = log_Ma_nu;
    else log_Ma_curr = log_exp_x_plus_exp_y(log_Ma_curr,log_Ma_nu);
  }
  
  return(log_Ma_curr-log(n_grid));

}

void GBT::make_prior_logrho_mat(int level) { // logrho_mat is the n_s*(n_s+1) log transition matrix
 
  double cumulative_rho = 0;
  for (int s = 0; s < n_s; s++) {
    make_prior_logrho_vec(logrho_mat[s],level, s);
  }


  return ;
}


GBT::GBT(uint *Xwork,int nobs, int k, int p, double rho0, int rho_mode, int tran_mode, double lognu_lowerbound, double lognu_upperbound, int n_grid = 1, int n_s=4, double beta=0.1): k(k), p(p), nobs(nobs), rho0(rho0),rho_mode(rho_mode),tran_mode(tran_mode), beta(beta),n_s(n_s),lognu_lowerbound(lognu_lowerbound),lognu_upperbound(lognu_upperbound),n_grid(n_grid) {


    init(Xwork);
    
    
    loglambda0 = (-1.0) * log((double) p); // noninformative prior seleciton probabilities      
 
    logrho_mat = new double * [n_s];
    for (int s = 0; s < n_s; s++) {
      logrho_mat[s] = new double [n_s+1];
    }

    logrho_vec = new double[n_s+1];
}

GBT::~GBT() {
    clear();
}

int GBT::update_node(double *NODE_CURR, int level, INDEX_TYPE I) {
  
  double *CHILD_0; 
  double *CHILD_1;

  double Z[n_s+1]; // Z is a vector of length n_s + 1, that stores log Z(A,t,x) for the current node A
  double phi[n_s+1]; // phi is a vector of length n_s, that stores log phi(A,s,x) for the current node A for s=0,1,2,...,n_s
  // note that for s = stopping state, by design phi(A,s,x) = Z(A,s,x), which is stored in Z[n_s].
  
  double Zi;
  int t,s; // nonstopping state
  
  if (level == k) { // deepest level allowed, these nodes have prior rho=1
    
    NODE_CURR[1+n_s] = phi[n_s] = Z[n_s] = 0; // = 0
    // NODE_CURR[1+n_s] = phi[n_s] = Z[n_s] = NODE_CURR[0]*(level-k)*log(2.0); // = 0
    // NODE_CURR[1+t] stores log Z(A,t,x) for t = 0,1,2,...,n_s
    // NODE_CURR[2+n_s+s] stores log phi(A,s,x) for s = 0,1,2,...,n_s-1
    
    for (s=0; s<n_s; s++) {
      t = s;
      NODE_CURR[1+t] = Z[t] = 0; // -DBL_MAX; // NODE_CURR[1+t] stores log Z(A,t,x) for t = 0,1,2,...,n_s 
      // In this case Z(A,t,x) for t nonstopping is -inf
      NODE_CURR[2+n_s+s] = Z[n_s];	    // NODE_CURR[2+n_s+s] stores log phi(A,s,x) for s = 0,1,2,...,n_s-1
    }
    
  } else if (NODE_CURR[0] <= 1) { // if the node contains no more than 1 data point
    
    
    if (NODE_CURR[0] == 1) { // if it contains one data point
      NODE_CURR[1+n_s] = Z[n_s] = (level-k)*log(2.0);
    }
    
    else NODE_CURR[1+n_s] = Z[n_s] = 0; // if it contains no data point
    
    for (t=0; t< n_s; t++) {
      // in either case, phi(A,s,x) and Z(A,t,x) all correspond to the uniform likelihood
      s=t;
      NODE_CURR[2+n_s+s] = NODE_CURR[1+t] = Z[t] = Z[n_s];
    }
    
  } else { // other models need recursion to compute phi and rho
    
    NODE_CURR[1+n_s] = Z[n_s] = NODE_CURR[0] * (level-k)*log(2.0); // Z(A,infty,x) can be computed directly
    
    for (t=0; t < n_s; t++) { // non-stopping states
      
      Z[t] = -DBL_MAX; 
	      
      for (int i=0; i < p; i++) { // for each dimension i
	
	CHILD_0 = get_child(I,i,level,0);
	CHILD_1 = get_child(I,i,level,1);
	
	
	Zi = loglambda0 
	  + get_log_Ma(0.5,CHILD_0[0],CHILD_1[0],t)
	  + CHILD_0[2+n_s+t] + CHILD_1[2+n_s+t]; 
	
	
	if (Z[t] == -DBL_MAX) {
	  Z[t] = Zi;
	} else {
	  Z[t] = log_exp_x_plus_exp_y (Z[t], Zi);
	}    
      }
      
      
      // Now Z[t] stores Z(A,t,x) for all states t
      NODE_CURR[1+t] = Z[t]; // NODE[1+t] stores Z(A,t,x)

      
      // Next we compute the phi(A,s,x)
      
      for (s = 0; s < n_s; s++) { // for the non-stopping states
	
	if (t == 0) {
	  phi[s] = logrho_mat[s][t] + Z[t];
	}
	
	else {
	  phi[s] = log_exp_x_plus_exp_y(phi[s], logrho_mat[s][t] + Z[t]);
	}
	
	
      }  	      
    }
    
    for (s=0; s < n_s; s++) {
      phi[s] = log_exp_x_plus_exp_y(phi[s],logrho_mat[s][n_s]+Z[n_s]);
    }
	    
    phi[n_s] = Z[n_s]; // phi(A,infty,x) = Z(A,infty,x)
    
    for (s=0; s<=n_s; s++) {
      
      NODE_CURR[2+n_s+s] = phi[s];
      
    }
    
  }
  return 0;
}


int GBT::update() {
  
  
    double *NODE_CURR; 
    INDEX_TYPE I;  
    
    for (int level=k; level>=0;level--) { //do it from the largest models;

      make_prior_logrho_mat(level);

      unsigned count = 0;
      I = init_index(p,level);
      
      while (count < modelscount[level]) {
	
	NODE_CURR = get_node(I,level);



	for (uint j = 0; j < pow2(level) ; j++) {
	  
		
	  I.var[MAXVAR] = j;
 	  update_node(NODE_CURR,level,I);	  
	  NODE_CURR += NUMNODEVAR;
	  
	}
	
	I = get_next_node(I,p,level); count++;

	
      }
    }
    
 
    
 
    return 0;
}
    


void GBT::find_hmap_subtree(INDEX_TYPE& I, int level) {

    int j,t;
    double *node = get_node(I,level);
    double logrho0 = log(make_rho0(level));
    int curr_max_state, curr_max_dim;
    double curr_phi_max, curr_Zj_max, curr_Zj;

    if (level == k) { // stopping?
      hmap_nodes.push_back(make_pair(I,make_pair(level,n_s+1)));
    }
    
    else {
      curr_max_state = n_s;
      curr_phi_max = node[2+n_s+n_s];
      for(t = 0; t < n_s; t++) {
	if (node[2+n_s+t] > curr_phi_max) {
	  curr_max_state = t;
	  curr_phi_max = node[2+n_s+t];
	}
      }
      
      // so now the hmap state is curr_max_state
      hmap_nodes.push_back(make_pair(I, make_pair(level,curr_max_state+1)));

      if (curr_max_state < n_s) {
    
	curr_max_dim = 0;
	curr_Zj_max = get_add_prob(I,0,curr_max_state,level);
      
	for (j = 1; j < p; j++) {
	  curr_Zj = get_add_prob(I,j,curr_max_state,level);
	  
	  if (curr_Zj > curr_Zj_max) {
	    curr_max_dim = j;
	    curr_Zj_max = curr_Zj;
	  }
	}
	
	
	INDEX_TYPE child_index_0 = make_child_index(I, curr_max_dim, level, 0);
	INDEX_TYPE child_index_1 = make_child_index(I, curr_max_dim, level, 1);

	find_hmap_subtree(child_index_0,level+1);
	find_hmap_subtree(child_index_1,level+1);
      }
    }
    
}

void GBT::make_prior_logrho_vec(double *logrho_vec, int level, int s) {
  double cumulative_rho = 0;
  
  int t;
  double gam[n_s+1]; // total number of states must be no more than 50
  double unnormalized_rho[n_s+1];
  
  if (s == n_s) { // if the parent is in the pruned state then the child must also be pruned
    for (t = 0; t < n_s; t++) {
	  
      logrho_vec[t] = 0;
    }
    logrho_vec[n_s] = 1;
  }

  else {
    if (tran_mode != 2) {
     
      logrho_vec[n_s] = log(make_rho0(level));
      
      if (level == 0 || tran_mode== 0 ) {   // tran mode 0 --- no stochastic ordering. uniform over non-pruning states
	for (t = 0; t < n_s; t++) {
	  
	  gam[t] = 1.0/n_s;
	}
      }
      
      if (tran_mode == 1 && level > 0) { // tran mode 1 --- with stochastic ordering. kernel specification over non-pruning states
    	
	cumulative_rho = 0;	
	for (t = 0; t < n_s; t++) {
	  if (t < s) unnormalized_rho[t] = 0;
	  else unnormalized_rho[t] = exp(-beta*abs(s-t)); 
	  cumulative_rho += unnormalized_rho[t];
	}
	
	for (t = 0; t < n_s; t++) {
	  if (t<s) gam[t] = 0;
	  else gam[t] = unnormalized_rho[t]/cumulative_rho;	
	}
      }
      
      for (t = 0; t < n_s; t++) {
	
	logrho_vec[t] = log((1-exp(logrho_vec[n_s])) * gam[t]);
	

      }    
    }

    else { // if tran_mode 2 ---  with stochastic ordering. kernel specification for both pruning and non-pruning states. no differentiation between pruning and non-pruning states. in this case rho0 and rho0_mode are not used.
      if (level == 0) {
	
	for (t = 0; t < n_s+1; t++) {
	  
	  logrho_vec[t] = -log(n_s+1);
	}
      }
      

      else {
	
	cumulative_rho = 0;
	
	for (t = 0; t < n_s+1; t++) {
	  if (t < s) unnormalized_rho[t] = 0;
	  else unnormalized_rho[t] = exp(-beta*abs(s-t)); 
	  cumulative_rho += unnormalized_rho[t];
	}
	
	for (t = 0; t < n_s+1; t++) {
	  if (t<s) gam[t] = 0;
	  else gam[t] = unnormalized_rho[t]/cumulative_rho;	
	}
      

	for (t = 0; t < n_s+1; t++) {
	  
	  logrho_vec[t] = log(gam[t]);
	

	}   
      }
    }
  }
  
  return ;
}

void GBT::make_posterior_logrho_vec(double *logrho_vec, INDEX_TYPE& I,int level,int s) {
  
  int i,t;
  double *node = get_node(I,level);

  make_prior_logrho_vec(logrho_vec,level,s); // initialization of logrho_vec with prior values

  for (t = 0; t <= n_s; t++) {
    logrho_vec[t] = logrho_vec[t] + node[1+t]-node[2+n_s+s];
  }
  
}

void GBT::sample_subtree(INDEX_TYPE& I,int level,int s,double log_prob) {

    int i,t;
    double *node = get_node(I,level);
    double u, v, cum_prob_curr, log_cum_Ma_curr;
    double nu,log_Ma;
    double *CHILD_0;
    double *CHILD_1;
    double lb_lognu, ub_lognu, bandwidth;
    double theta0, theta, log_Ma_nu;
    int n0, n1;

    make_posterior_logrho_vec(logrho_vec,I,level,s);
    
    // first sample state
    u = unifRand();

    if (level == k || log(u) <= logrho_vec[n_s]) { // pruned?

      t = n_s;
      sample_nodes.push_back(make_pair(make_pair(I,level),make_pair(1.0/0,log_prob)));

    }
    
    else {
      
      cum_prob_curr = exp(logrho_vec[n_s]);

      for (t = 0; t < n_s && cum_prob_curr < u; t++) {
	cum_prob_curr += exp(logrho_vec[t]); // + node[2+t]-node[1]);
      }
      // after the loop (t - 1) will be equal to the state
      
      t--; // t now is the sampled state

      // next sample partition
      v = unifRand();
      cum_prob_curr = 0;
  
      for(i = 0; i < p && cum_prob_curr < v; i++  ) {
	cum_prob_curr += get_add_prob(I,i,t,level);
      }
     
      // i - 1 is the dimension to divide

      INDEX_TYPE child_index_0 = make_child_index(I, i-1, level, 0);
      INDEX_TYPE child_index_1 = make_child_index(I, i-1, level, 1);

      CHILD_0 = get_node(child_index_0,level+1);
      CHILD_1 = get_node(child_index_1,level+1);

      // sample nu given the state t
      lb_lognu = lognu_lowerbound + (1.0*t)/n_s*(lognu_upperbound-lognu_lowerbound);
      ub_lognu = lognu_lowerbound + (1.0*(t+1))/n_s*(lognu_upperbound-lognu_lowerbound);
      
      bandwidth = (ub_lognu - lb_lognu)/n_grid;
 
      theta0 = 0.5;
      n0 = CHILD_0[0];
      n1 = CHILD_1[0];
      log_Ma = get_log_Ma(theta0,n0,n1,t);
      
      v = unifRand();
      log_cum_Ma_curr = -DBL_MAX;

      for (i = 0; i < n_grid && (log_cum_Ma_curr - (log_Ma + log(n_grid)) < log(v)); i++) {
       
	nu = pow(10.0,lb_lognu + (i+0.5)*bandwidth);
	

	log_Ma_nu = lgamma(theta0*nu+n0)+lgamma((1-theta0)*nu+n1) - lgamma(nu+n0+n1)
		    - (lgamma(theta0*nu)+lgamma((1-theta0)*nu) - lgamma(nu)); 
	log_cum_Ma_curr = log_exp_x_plus_exp_y(log_cum_Ma_curr, log_Ma_nu);
      }
      
 
      theta = rbeta(theta0*nu+n0,(1-theta0)*nu+n1);
      sample_nodes.push_back(make_pair(make_pair(I,level),make_pair(nu,log_prob)));
      sample_subtree(child_index_0,level+1,t,log_prob+log(theta));
      sample_subtree(child_index_1,level+1,t,log_prob+log(1-theta));	
    }
}
  

void GBT::sample() {
  INDEX_TYPE I_root = init_index(p,0);
  sample_nodes.clear();
  
  sample_subtree(I_root,0,0,0);   // the sampled nodes are stored in sample_nodes
  
}

void GBT::find_hmap(int print = 0) {
  INDEX_TYPE I_root = init_index(p,0);
  hmap_nodes.clear();
  find_hmap_subtree(I_root,0);   // the sampled nodes are stored in sample_nodes
  if (print) { // print the sampled nodes
    vector< pair<INDEX_TYPE, pair<int,int> > >::iterator it;
    for (it = hmap_nodes.begin(); it < hmap_nodes.end(); it++) {
      print_index(it->first, it->second.first);
      cout << endl;
      print_index_2(it->first, it->second.first, k);
      cout << endl;
    }
  }
}

vector< vector<ushort> > GBT::find_part() {
  vector< vector<ushort> > part_points;
  vector< pair<INDEX_TYPE, pair<int,int> > >::iterator it;
  int i;
  vector<ushort> v(2*p+2);
  
  INDEX_TYPE I;
  int level;
  int state;

  ushort x_curr = 0;
  ushort index_prev_var = 0;
  ushort lower = 0;
  ushort x_curr_count = -1;

  find_hmap(0);
  
  for (it = hmap_nodes.begin(); it < hmap_nodes.end(); it++) {

    I = it->first;
    level = it->second.first;
    state = it->second.second;
    v[2*p] = level;
    v[2*p+1] = state;
    
    for (i = 0; i < p; i++) {
      v[2*i] = 0;
      v[2*i+1] = ((ushort) 1 << k) - 1;
    }

    x_curr = 0;
    index_prev_var = 0;
    lower = 0;
    x_curr_count = -1;
    
    for (i = 0; i < level; i++) {
      if ( I.var[i] - index_prev_var - 1 > 0 ) { // next variable

    	v[2*x_curr] = lower;
    	v[2*x_curr+1] = lower + ((ushort) 1 << (k-x_curr_count-1)) - 1;
	
    	lower = 0;
    	x_curr_count = 0;
      }  
      else { 
    	x_curr_count++;
      }
      
      x_curr += I.var[i] - index_prev_var - 1;
      lower |= (((I.var[MAXVAR] >> i) & (ushort) 1)) << (k-x_curr_count-1);  
      
      index_prev_var = I.var[i];
    }
    
    v[2*x_curr] = lower;
    v[2*x_curr+1] = lower + ((ushort) 1 << (k-x_curr_count-1)) - 1;
    
    part_points.push_back(v);
  }

  return part_points;
}

int GBT::find_sample_part(vector< vector< ushort> > &part_points, vector< vector< double> > &nu_and_probs) {
  
  vector< pair< pair< INDEX_TYPE, int >, pair< double, double > > >::iterator it;
  int i;
  vector<ushort> v(2*p+1);
  vector<double> nu_and_prob(2);

  INDEX_TYPE I;
  int level;

  ushort x_curr = 0;
  ushort index_prev_var = 0;
  ushort lower = 0;
  ushort x_curr_count = -1;

  part_points.clear();
  nu_and_probs.clear();
  
  for (it = sample_nodes.begin(); it < sample_nodes.end(); it++) {
    
    nu_and_prob[0] = it->second.first;
    nu_and_prob[1] = it->second.second;
    nu_and_probs.push_back(nu_and_prob);


    I = it->first.first;
    level = it->first.second;
    v[2*p] = level;
    
    for (i = 0; i < p; i++) {
      v[2*i] = 0;
      v[2*i+1] = ((ushort) 1 << k) - 1;
    }

    x_curr = 0;
    index_prev_var = 0;
    lower = 0;
    x_curr_count = -1;
    
    for (i = 0; i < level; i++) {
      if ( I.var[i] - index_prev_var - 1 > 0 ) { // next variable

    	v[2*x_curr] = lower;
    	v[2*x_curr+1] = lower + ((ushort) 1 << (k-x_curr_count-1)) - 1;
	
    	lower = 0;
    	x_curr_count = 0;
      }  
      else { 
    	x_curr_count++;
      }
      
      x_curr += I.var[i] - index_prev_var - 1;
      lower |= (((I.var[MAXVAR] >> i) & (ushort) 1)) << (k-x_curr_count-1);  
      
      index_prev_var = I.var[i];
    }
    
    v[2*x_curr] = lower;
    v[2*x_curr+1] = lower + ((ushort) 1 << (k-x_curr_count-1)) - 1;
    
    part_points.push_back(v);

  }

  return part_points.size();
}


double GBT::get_add_prob(INDEX_TYPE& I,int i, int t, int level) { //get the splitting probability of dimension i
    double *node = get_node(I,level);
    double loglambda0 = (-1.0) * log((double) p);
    double *CHILD_0 = get_child(I,i,level,0); 
    double *CHILD_1 = get_child(I,i,level,1);

    return exp( loglambda0 + get_log_Ma(0.5,CHILD_0[0],CHILD_1[0],t)
		+ CHILD_0[2+n_s+t] + CHILD_1[2+n_s+t] - node[1+t]);
}


double *GBT::get_child(INDEX_TYPE& I, int i,int level,ushort which){
  INDEX_TYPE child_index = make_child_index(I,i,level,which);
  return &models[level+1][(get_node_index(child_index,level+1))];
}

double *GBT::get_node(INDEX_TYPE& I, int level) {
  return &models[level][(get_node_index(I,level))];
}


int GBT::convert_obs_to_uint(int *obs,INDEX_TYPE I,int level) {
  uint res = 0;
  for (int j = 0; j < level; j++) {
    int i = I.var[j] - 1;
    res += ((uint) (obs[i])) << j;
  }
  return res;
}

void GBT::add_data_to_subtree(INDEX_TYPE I, int level, int x_curr, int part_count, uint *obs) {
  double *NODE_CURR;
  INDEX_TYPE I_child;
  NODE_CURR = get_node(I,level);

  NODE_CURR[0] += 1;

  int i = 0;

  if (level < k) { // not maximum (leaf) node
    
    i = x_curr - 1;
    I_child = make_child_index(I, i, level, (obs[i] >> part_count) & 1);
    add_data_to_subtree(I_child, level+1, x_curr, part_count+1, obs);
    

    for (i = x_curr; i < p; i++) {
      I_child = make_child_index(I,i,level,obs[i] & 1);
      add_data_to_subtree(I_child, level+1, i+1, 1, obs);
    }
  }
}

void GBT::remove_data_from_subtree(INDEX_TYPE I, int level, int x_curr, int part_count, uint *obs) {
  double *NODE_CURR;
  INDEX_TYPE I_child;
  NODE_CURR = get_node(I,level);
  NODE_CURR[0] -= 1;

  int i = 0;

  if (level < k) { // not maximum (leaf) node
  
    i = x_curr - 1;
    I_child = make_child_index(I, i, level, (obs[i] >> part_count) & 1);
    remove_data_from_subtree(I_child, level+1, x_curr, part_count+1, obs);
    

    for (i = x_curr; i < p; i++) {
      I_child = make_child_index(I,i,level,obs[i] & 1);
      remove_data_from_subtree(I_child, level+1, i+1, 1, obs);
    }
  }
}


int GBT::update_subtree_add_new_data(INDEX_TYPE I, int level, int x_curr, int part_count, uint *new_obs) {
  double *NODE_CURR;
  INDEX_TYPE I_child;

  NODE_CURR = get_node(I,level);
  NODE_CURR[0] += 1;

  int i;

  if (level < k) { // not maximum (leaf) node
   
    i = x_curr - 1;
    I_child = make_child_index(I, i, level, (new_obs[i] >> part_count) & 1);
    update_subtree_add_new_data(I_child, level+1, x_curr, part_count+1, new_obs);
    

    for (i = x_curr; i < p; i++) {

      I_child = make_child_index(I,i,level,new_obs[i] & 1);
      update_subtree_add_new_data(I_child, level+1, i+1, 1, new_obs);
    }

  }
  
  make_prior_logrho_mat(level);
  update_node(NODE_CURR,level,I);
  
  return 0;

}

int GBT::update_subtree_remove_new_data(INDEX_TYPE I, int level, int x_curr, int part_count, uint *new_obs) {
  double *NODE_CURR;
  INDEX_TYPE I_child;

  NODE_CURR = get_node(I,level);
  NODE_CURR[0] -= 1;

  int i;

  if (level < k) { // not maximum (leaf) node
   
    i = x_curr - 1;
    I_child = make_child_index(I, i, level, (new_obs[i] >> part_count) & 1);
    update_subtree_remove_new_data(I_child, level+1, x_curr, part_count+1, new_obs);
    

    for (i = x_curr; i < p; i++) {

      I_child = make_child_index(I,i,level,new_obs[i] & 1);
      update_subtree_remove_new_data(I_child, level+1, i+1, 1, new_obs);
    }

  }
  
  make_prior_logrho_mat(level);
  update_node(NODE_CURR,level,I);
  
  return 0;

}



vector<double> GBT::compute_predictive_density(uint *Xnew, int nobs_new) {
  
  INDEX_TYPE I_root = init_index(p,0);
  vector<double> predictive_density(nobs_new);
  double new_root_logphi;
  double original_root_logphi;

  original_root_logphi = get_root_logphi();

  for (int i=0; i < nobs_new; i++) { //since we do it only once, do this for now

    update_subtree_add_new_data(I_root,0,1,0,&Xnew[p*i]);
    new_root_logphi = get_root_logphi();
   
    predictive_density[i] = exp(new_root_logphi - original_root_logphi);
    update_subtree_remove_new_data(I_root,0,1,0,&Xnew[p*i]);
  }
  
  return predictive_density;
  
}


void GBT::init(uint *Xwork) {
  unsigned long long i, j;
  unsigned long long l;

    models = new double*[k+1];
    modelscount = new unsigned int[k+1];
   
    for (i=0; i <= k; i++) {
        modelscount[i] = Choose(p + i - 1, i);
        models[i] = new double[(unsigned long long) (modelscount[i]*NUMNODEVAR) << i];

	for (j=0; j < modelscount[i] ; j++) {
	  for (l=0; l < pow2(i); l++) {
	    models[i][(j*pow2(i)+l)*NUMNODEVAR] = 0; // Initialize NODE_CURR[0] to 0
	  } 
	}
    }


    
   // step 1: add data into the models
    
    INDEX_TYPE I_root = init_index(p,0);

    for (i=0; i < nobs; i++) { //since we do it only once, do this for now
      add_data_to_subtree(I_root,0,1,0,&Xwork[p*i]); // code the data matrix such that each observation is in contiguity
    }


}


void GBT::clear() {
    for (int i =0; i <= k; i++) {
        delete [] models[i];
    }
    delete [] models; models = NULL;
    delete [] modelscount;modelscount= NULL;

    for (int s = 0; s < n_s; s++) {
      delete [] logrho_mat[s];
    }
    
    delete [] logrho_mat;
   
    delete [] logrho_vec;
    logrho_mat=NULL;
}

double GBT::get_root_logrho() { return log(rho0) + models[0][1+n_s] - (get_root_logphi()-models[0][0]*k*log(2.0));}

double GBT::get_root_logphi() { 

  double log_root_phi;
  for (int s = 0; s < n_s; s++) {
    if (s==0) log_root_phi = models[0][2+n_s];
    else log_root_phi = log_exp_x_plus_exp_y(log_root_phi, models[0][2+n_s+s]); 
  }
  log_root_phi -= log(n_s);

  return log_root_phi + models[0][0] * k * log(2.0);


} 

double GBT::get_node_logphi(vector<double> &node_INFO) { // this function only applies with uniform prior over all shrinkage states

  double log_node_phi;
  for (int s = 0; s < n_s; s++) {
    if (s==0) log_node_phi = node_INFO[2+n_s];
    else log_node_phi = log_exp_x_plus_exp_y(log_node_phi, node_INFO[2+n_s+s]); 
  }
  log_node_phi -= log(n_s);

  return log_node_phi + node_INFO[0] * k * log(2.0);


}
