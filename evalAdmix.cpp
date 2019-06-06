#include <math.h>
#include <fstream>


double **allocDouble2(size_t x,size_t y){
  double **ret= new double*[x];
  for(size_t i=0;i<x;i++)
    ret[i] = new double[y];
  return ret;
}


void adaptF(double **F, int  **G, int K, double **Q, int nInd, int nSites, int withoutInd, int nIts, int **isMissing){


  for(int it=0; it<nIts; it++){
    
    for(int j=0; j<nSites; j++){

	double sumAG[K];
	double sumBG[K];
    
	for(int k=0; k<K;k++){
	  sumAG[k] = 0;
	  sumBG[k] = 0;
	}

	for(int i=0; i<nInd;i++){
	
	  if(i == withoutInd)
	    continue;

	  if(!isMissing[j][i]){
	    double fpart=0;
	    double aNorm;
	    double bNorm;

	    for(int k=0;k<K;k++){
	      fpart+=F[j][k]*Q[i][k];
	    }
	
	    aNorm = 1.0/fpart;
	    bNorm = 1.0/(1-fpart);

	    for(int k=0; k<K; k++){
	    sumAG[k] += G[j][i]*F[j][k]*Q[i][k]*aNorm; // Minor allele
	    sumBG[k] += (2-G[j][i])*Q[i][k]*(1-F[j][k])*bNorm; // Major allele
	 }
	
	  }
	}

	for(int k=0;k<K;k++)
	  F[j][k] = sumAG[k]/(sumAG[k]+sumBG[k]);
	
    }
  }
}


void calcRes(double **r, double *mean_r, int **G, double **Q, double **F, int K, int nSites, int nInd, int **isMissing){

  double sum_r[nInd];
  int usedSites[nInd];


  for(int i=0; i<nInd;i++){
    sum_r[i] = 0;
    usedSites[i] = 0;
  }

  for(int j=0; j<nSites;j++){
    for(int i=0; i<nInd;i++){
      
      if(!isMissing[j][i]){
	
	double fpart=0; // individual freq pi
	
	for(int k=0; k<K; k++)
	  fpart += Q[i][k]*F[j][k];

	r[i][j] = G[j][i] - 2 * fpart;
	sum_r[i] += r[i][j];
	usedSites[i]++;
      }
      
    }
  }

  for(int i=0; i<nInd;i++)
    mean_r[i] = sum_r[i]/usedSites[i];

}



 
double correlateRes(int nSites, double *r1, double *r2, double mean_r1, double mean_r2, int **isMissing, int ind1, int ind2){

  double cor_num=0;
  double cor_den1=0;
  double cor_den2=0;
  double d_r1=0;
  double d_r2=0;
  

  for(int j=0; j<nSites;j++){

    if(!isMissing[j][ind1]&!isMissing[j][ind2]){
      d_r1 = (r1[j] - mean_r1);
      d_r2 = (r2[j] - mean_r2);
      cor_num += d_r1*d_r2;
      cor_den1 += d_r1*d_r1;
      cor_den2 += d_r2*d_r2;
    }
    
  }
  double cor;
  cor = cor_num/(sqrt(cor_den1)*sqrt(cor_den2));
  
  //  fprintf(stderr, "Finished calculating pis without error");
  return cor;

}

 

void evalAdmix(double *cor, int ind1, double **r, double *mean_r, int **G, int K, double **Q, double **F, int nInd, int nSites,int nIts, int **isMissing){

  double **rAdapted=allocDouble2(nInd, nSites);
  double *mean_rAdapted = new double[nInd];
  double **fAdapted=allocDouble2(nSites,K);


   
  for(int j=0; j<nSites;j++){ // initial values for adapted F are normal F
    for(int k=0; k<K;k++){
      fAdapted[j][k] = F[j][k];
      //newF[j][k] = (F[j][k]*2*init_Nk[k]-G[withoutInd][j]*Q[withoutInd][k])/(2*init_Nk[k]-Q[withoutInd][k]);
    }
  }

    
  // Estimate new f and calculate new residuals
  adaptF(fAdapted, G, K, Q, nInd, nSites, ind1, nIts,isMissing);
  calcRes(rAdapted, mean_rAdapted, G, Q, fAdapted, K, nSites, nInd, isMissing);

  for(int ind2=(ind1+1); ind2<nInd; ind2++){
    cor[ind2] =  correlateRes(nSites, r[ind1], rAdapted[ind2], mean_r[ind1], mean_rAdapted[ind2], isMissing, ind1, ind2);
  }
  /*
  // Index from which to start writing correlation
  int start=ind1*(nInd-1)-(ind1-1)*ind1/2;

  for(int ind2=(ind1+1); ind2<nInd; ind2++){ // Calculate pairwise correlations
    cor[start] = correlateRes(nSites, r[ind1], rAdapted[ind2], mean_r[ind1], mean_rAdapted[ind2], isMissing[ind1], isMissing[ind2]);
    start++;      

  }
  */
  
  // Clean

  for(int j = 0; j < nSites; j++){ 
    delete[] fAdapted[j];

  }

  for(int i = 0; i < nInd; i++){
    delete[] rAdapted[i];

  }

 
  delete[] fAdapted;
  delete[] rAdapted;
  delete[] mean_rAdapted;


}
