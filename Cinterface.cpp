#include <iostream>
#include <fstream>
#include "Cinterface.h"
#include <string>
#include <cmath>
#include <iomanip>//used for setting number of decimals in posterior
#include <cstdlib>
#include <zlib.h>
#include <sstream>
#include <cstring>
#include <vector>
#include <sys/stat.h>
#ifndef _types_h
#define _types_h
#include "types.h"
#endif
#include <pthread.h>
#include <assert.h>

#ifndef _alloc_h
#define _alloc_h
#include "alloc.h"
#endif





#include "filereader_and_conversions.h"
#include "extractors.h"
#include "asort.h"
#include "evalAdmix.h"
#include "ngsevalAdmix.h"
//#include "mafilter.h"

using namespace std;

pthread_t *threads = NULL;
pthread_t *threads1 = NULL;
pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
int NumJobs;
int jobs;
int *printArray;
FILE *fp;
int cunt;
double misTol=0.05;
#define LENS 100000 //this is the max number of bytes perline, should make bigger
int SIG_COND =1;//if we catch signal then quit program nicely

eachPars *allPars = NULL;
eachNGSPars *allNGSPars=NULL;



//optimazation parameteres for maf estimation
#define MAF_START 0.3
#define MAF_ITER 20






// Function to calculate maf from genotype likelihoods
double emFrequency(double *loglike,int numInds, int iter,double start,char *keep,int keepInd){

  if(keepInd == 0)
    return 0.0;
  
  float W0;
  float W1;
  float W2;
  // fprintf(stderr,"start=%f\n",start);
  float p=(float)start;
  float temp_p=(float)start;
  double accu=0.00001;
  double accu2=0;
  float sum;


  int it=0;
  
  for(it=0;it<iter;it++){
    sum=0;
    for(int i=0;i<numInds;i++){
      if(keep!=NULL && keep[i]==0)
        continue;
      W0=(loglike[i*3+0])*(1-p)*(1-p);
      W1=(loglike[i*3+1])*2*p*(1-p);
      W2=(loglike[i*3+2])*p*p;
      sum+=(W1+2*W2)/(2*(W0+W1+W2));
      //  fprintf(stderr,"%f %f %f\n",W0,W1,W2);
      if(0&&std::isnan(sum)){
	//fprintf(stderr,"PRE[%d]: W %f\t%f\t%f sum=%f\n",i,W0,W1,W2,sum);
	exit(0);
      }
    }

    p=sum/keepInd;
    // fprintf(stderr,"it=%d\tp=%f\tsum=%f\tkeepInd=%d\n",it,p,log(sum),keepInd);
    if((p-temp_p<accu&&temp_p-p<accu)||(p/temp_p<1+accu2&&p/temp_p>1-accu2))
      break;
    temp_p=p;
  }



  if(std::isnan(p)){
    fprintf(stderr,"[%s] caught nan will not exit\n",__FUNCTION__);
    fprintf(stderr,"logLike (3*nInd). nInd=%d\n",numInds);
    //print_array(stderr,loglike,3*numInds);
    fprintf(stderr,"keepList (nInd)\n");
    //print_array(stderr,keep,numInds);
    fprintf(stderr,"used logLike (3*length(keep))=%d\n",keepInd);

    for(int ii=0;0&&ii<numInds;ii++){
      if(keep!=NULL && keep[ii]==1)
	    fprintf(stderr,"1\t");
	for(int gg=0;gg<3;gg++)
	  fprintf(stderr,"%f\t",loglike[ii*3+gg]);
      fprintf(stderr,"\n");
    }
    sum=0;
    for(int i=0;i<numInds;i++){
      if(keep!=NULL && keep[i]==0)
        continue;
      W0=(loglike[i*3+0])*(1-p)*(1-p);
      W1=(loglike[i*3+1])*2*p*(1-p);
      W2=(loglike[i*3+2])*p*p;
      sum+=(W1+2*W2)/(2*(W0+W1+W2));
      //fprintf(stderr,"p=%f W %f\t%f\t%f sum=%f loglike: %f\n",p,W0,W1,W2,sum,exp(loglike[i*3+2])*pow(1-p,2));
    }
    p=-999;
    // exit(0);
  }
  
  return(p);
}



bgl readBeagle(const char* fname) {
  const char *delims = "\t \n";
  gzFile fp = NULL;
  if(Z_NULL==(fp=gzopen(fname,"r"))){
    fprintf(stderr,"Error opening file: %s\n",fname);
    exit(0);
  }
  
  bgl ret;
  char buf[LENS];

  //find number of columns
  gzgets(fp,buf,LENS);
  strtok(buf,delims);
  int ncols=1;
  while(strtok(NULL,delims))
    ncols++;
  if(0!=(ncols-3) %3 ){
    fprintf(stderr,"ncols=%d\n",ncols);
    exit(0);
  }
  ret.nInd = (ncols-3)/3;//this is the number of samples
  
  //read every line into a vector
  std::vector<char*> tmp;
  while(gzgets(fp,buf,LENS))
    tmp.push_back(strdup(buf));
  
  //now we now the number of sites
  ret.nSites=tmp.size();
  ret.major= new char[ret.nSites];
  
  ret.minor= new char[ret.nSites];
  ret.ids = new char*[ret.nSites];
  ret.genos= new double*[ret.nSites];

  //then loop over the vector and parsing every line
  
    
  for(int s=0;SIG_COND&& (s<ret.nSites);s++){
    ret.ids[s] = strdup(strtok(tmp[s],delims));
    ret.major[s] =strtok(NULL,delims)[0];
    ret.minor[s] =strtok(NULL,delims)[0];
    ret.genos[s]= new double[3*ret.nInd];
    for(int i=0;i<ret.nInd*3;i++){
      ret.genos[s][i] = atof(strtok(NULL,delims));
      if(ret.genos[s][i]<0){
	fprintf(stderr,"Likelihoods must be positive\n");
	fprintf(stderr,"site %d ind %d geno %d has value %f\n",s,int(i*1.0/3),i%3,ret.genos[s][i]);
	exit(0);
      }
    }
    for(int i=0;i<ret.nInd;i++){
      double tmpS = 0.0;
      for(int g=0;g<3;g++)
	tmpS += ret.genos[s][i*3+g];
      if(!(tmpS>0)){
	fprintf(stderr,"The sum of likelihoods for a genotypes must be positive\n");
	fprintf(stderr,"individual %d site %d has sum %f\n",i,s,tmpS);
	exit(0);
      } 
    }
    free(tmp[s]);
  }
  
  // Here additional stuff is calculated from the likelihoods
  // this must be done again while filtering later in main
  ret.keeps=new char*[ret.nSites]; // array nSites x nInd 0 if missing info
  ret.keepInd = new int[ret.nSites];
  ret.mafs = new float[ret.nSites];

  for(int s=0;s<ret.nSites;s++){
    ret.keeps[s] = new char[ret.nInd];
    int nKeep =0;
    for(int i=0;i<ret.nInd;i++){
      double mmin=std::min(ret.genos[s][i*3],std::min(ret.genos[s][i*3+1],ret.genos[s][i*3+2]));
      double mmax=std::max(ret.genos[s][i*3],std::max(ret.genos[s][i*3+1],ret.genos[s][i*3+2]));
      if(fabs(mmax-mmin)<misTol)
	ret.keeps[s][i] =0;
      else{
	ret.keeps[s][i] =1;
	nKeep++;
      }
    }
    ret.keepInd[s] = nKeep;
    ret.mafs[s] = emFrequency(ret.genos[s],ret.nInd,MAF_ITER,MAF_START,ret.keeps[s],ret.keepInd[s]);
  }
  //  keeps=ret.keeps;
  gzclose(fp); //clean up filepointer
  return ret;
}



void filterMinMaf(bgl &d,float minMaf){
  //  fprintf(stderr,"WARNING filtering minMaf=%f \n",minMaf);
  int posi =0;
  for(int s=0;s<d.nSites;s++){
    //    fprintf(stderr,"minmaf=%f mafs[%d]=%f lik=%f\n",minMaf,s,d.mafs[s],lik);
    if(d.mafs[s]>minMaf&&d.mafs[s]<1-minMaf){
      d.genos[posi] = d.genos[s];
      d.major[posi] = d.major[s];
      d.minor[posi] = d.minor[s];
      d.ids[posi] = d.ids[s];
      d.keeps[posi] = d.keeps[s];
      d.keepInd[posi] = d.keepInd[s];
      d.mafs[posi] = d.mafs[s];
      posi++;   
    }else{
      //      fprintf(stderr,"skippping\n");
    }
  }
  d.nSites=posi;
}



void dalloc(bgl &b){
  for(int i=0;i<b.nSites;i++){
    delete [] b.genos[i];
    free(b.ids[i]);
  }
  delete [] b.minor;
  delete [] b.major;
  delete [] b.genos;
  delete [] b.ids;
}


void readDoubleGZ(double **d,int x,int y,const char*fname,int neg){
  fprintf(stderr,"opening : %s with x=%d y=%d\n",fname,x,y);
  const char*delims=" \n";
  gzFile fp = NULL;
  if((fp=gzopen(fname,"r"))==NULL){
    fprintf(stderr,"cant open:%s\n",fname);
    exit(0);
  }
  int lens=1000000 ;
  char buf[lens];
  for(int i=0;i<x;i++){
    if(NULL==gzgets(fp,buf,lens)){
 	fprintf(stderr,"Error: Only %d sites in frequency file (maybe increase buffer)\n",i);
	exit(0);
    }
    if(neg)
      d[i][0] = 1-atof(strtok(buf,delims));
    else
      d[i][0] = atof(strtok(buf,delims));
    for(int j=1;j<y;j++){
      //      fprintf(stderr,"i=%d j=%d\n",i,j);
      if(neg)
	d[i][j] = 1-atof(strtok(NULL,delims));
      else
	d[i][j] = atof(strtok(NULL,delims));
    }
  }
  if(NULL!=gzgets(fp,buf,lens)){
    fprintf(stderr,"Error: Too many sites in frequency file. Only %d sites in plink file\n",x);
    exit(0);

  }
  gzclose(fp);
}
 

void readDouble(double **d,int x,int y,const char*fname,int neg){
  fprintf(stderr,"opening : %s with x=%d y=%d\n",fname,x,y);
  const char*delims=" \n";
  FILE *fp = NULL;
  if((fp=fopen(fname,"r"))==NULL){
    fprintf(stderr,"can't open:%s\n",fname);
    exit(0);
  }
  int lens=1000000 ;
  char buf[lens];
  for(int i=0;i<x;i++){
    if(NULL==fgets(buf,lens,fp)){
    	fprintf(stderr,"Error: Only %d individuals in admxiture (-qname) file\n",i);
      exit(0);
    }
    if(neg)
      d[i][0] = 1-atof(strtok(buf,delims));
    else
      d[i][0] = atof(strtok(buf,delims));
    for(int j=1;j<y;j++){
      //      fprintf(stderr,"i=%d j=%d\n",i,j);
      if(neg)
	d[i][j] = 1-atof(strtok(NULL,delims));
      else
	d[i][j] = atof(strtok(NULL,delims));
    }
  }
   if(NULL!=fgets(buf,lens,fp)){
    fprintf(stderr,"Error: Too many individuals in admixture (-qname) file. Only %d individuals in plink file\n",x);
    exit(0);
  }

  fclose(fp); 
}

int getK(const char*fname){
  const char*delims=" \n";
  FILE *fp = NULL;
  if((fp=fopen(fname,"r"))==NULL){
    fprintf(stderr,"can't open:%s\n",fname);
    exit(0);
  }
  int lens=100000 ;
  char buf[lens];
  if(NULL==fgets(buf,lens,fp)){
    fprintf(stderr,"Increase buffer\n");
    exit(0);
  }
  strtok(buf,delims);
  int K=1;
  while(strtok(NULL,delims))
    K++;
  fclose(fp);

  return(K);
}


double **allocDouble(size_t x,size_t y){
  double **ret= new double*[x];
  for(size_t i=0;i<x;i++)
    ret[i] = new double[y];
  return ret;
}


double ***allocDouble3D(size_t x,size_t y,size_t z){
  double ***ret= new double**[x];
  for(size_t i=0;i<x;i++){
    ret[i] = new double*[y];
    for(size_t j=0; j<y;j++)
      ret[i][j] = new double[z];
  }
    return ret;
}



void info(){
  fprintf(stderr,"Arguments:\n");
  fprintf(stderr,"\t-plink name of the binary plink file (excluding the .bed)\n");
  fprintf(stderr,"\t-beagle name of beagle file containing genotype likelihoods (alternative to -plink)\n");
  fprintf(stderr,"\t-fname Ancestral population frequencies\n"); 
  fprintf(stderr,"\t-qname Admixture proportions\n"); 
  fprintf(stderr,"\t-o name of the output file\n"); 

  fprintf(stderr,"Setup:\n"); 
  fprintf(stderr,"\t-P Number of threads\n");
  fprintf(stderr,"\t-autosomeMax 23\t autosome ends with this chromsome\n");
  fprintf(stderr,"\t-nIts 5\t number of iterations to do for frequency correction; if set to 0 calculates correlation without correction (fast but biased)\n");
  fprintf(stderr,"\t-useSites 1.0\t Proportion of sites to use to calculate correlation of residuals\n");
  fprintf(stderr, "\t-misTol 0.05 \t Tolerance for considering site as missing when using genotype likelihoods (-beagle)\n");
    fprintf(stderr, "\t-mimMaf 0.05 \t Minimum minor allele frequency to keep site. Use same value as used in NGSadmix to keep compatibility when using genotype likelihoods (-beagle)\n");



  exit(0);

}



void *functionC(void *a) //the a means nothing
{
  int running_job;

  pthread_mutex_lock(&mutex1); // Protect so only one thread can edit jobs done
  
  while (jobs > 0) {
    running_job = jobs--;
    pthread_mutex_unlock(&mutex1);

    ////////////////////////////////////////////// not protected
    int c = NumJobs-running_job;
    eachPars p=allPars[c];
    myPars *pars=p.pars;
    int i=p.ind1;
evalAdmix(p.cor,p.ind1,pars->r,pars->mean_r,pars->genos,pars->K,pars->Q,pars->F,pars->nInd,pars->nSites,pars->nIts, pars->isMissing);
    
    //////////////////////////////////////////////


  }

  return NULL;
}


void *ngsfunctionC(void *a) //the a means nothing
{
  int running_job;

  pthread_mutex_lock(&mutex1); // Protect so only one thread can edit jobs done
  
  while (jobs > 0) {
    running_job = jobs--;
    pthread_mutex_unlock(&mutex1);

    ////////////////////////////////////////////// not protected
    int c = NumJobs-running_job;
    eachNGSPars p=allNGSPars[c];
    myNGSPars *pars=p.pars;
    int i=p.ind1;

    ngsevalAdmix(p.cor,p.ind1,pars->r,pars->mean_r,pars->genos,pars->K,pars->Q,pars->F,pars->nInd,pars->nSites,pars->nIts, pars->keeps);
    
    //////////////////////////////////////////////


  }

  return NULL;
}
 

void fex(const char* fileName){
  const char*delims=" \n";
  FILE *fp = NULL;
  if((fp=fopen(fileName,"r"))==NULL){
    fprintf(stderr,"can't open:%s\n",fileName);
    exit(0);
  }
  fclose(fp);
}


int main(int argc, char *argv[]){
  clock_t t=clock();//how long time does the run take
  time_t t2=time(NULL);
  
  // print commandline
  for(int i=0;i<argc;i++)
    printf("%s ",argv[i]);
  printf("\n");
  
  // if not arguments are given print information
  if(argc==1){
    info();
    return 0;
 }
  
  const char *outname = "output.corres.txt";
  int autosomeMax = 23;
  int nIts = 5;
  string geno= "";
  string pos = "";
  string chr = "";
  string plink_fam;
  string plink_bim;
  string plink_bed;
  int nThreads =1;
  float minMaf = 0.05;
  double useSites=1.0;
  bArray *plinkKeep = NULL; //added in 0.97;
  const char* fname = NULL;
  const char* qname = NULL;
  const char* bname = NULL;
  bool useGeno = 0;
  bool useGenoLikes = 0;
  double misTol=0.05; // min diff between max and min geno likes to consider site missing

  //parse arguments
  int argPos=1;
  while(argPos <argc){
    if (strcmp(argv[argPos],"-o")==0){
      outname  = argv[argPos+1]; 
    }
    else if(strcmp(argv[argPos],"-fname")==0 || strcmp(argv[argPos],"-f")==0)
      fname=argv[argPos+1];
    else if(strcmp(argv[argPos],"-qname")==0 || strcmp(argv[argPos],"-q")==0)
      qname=argv[argPos+1];
    else if(strcmp(argv[argPos],"-nThreads")==0 || strcmp(argv[argPos],"-P")==0)
      nThreads=atoi(argv[argPos+1]);
    else if(strcmp(argv[argPos],"-nIts")==0)
      nIts=atoi(argv[argPos+1]);
    //    else if(strcmp(argv[argPos],"-useSites")==0)
    // useSites=atof(argv[argPos+1]);
    else if(strcmp(argv[argPos],"-autosomeMax")==0)
      autosomeMax=atoi(argv[argPos+1]);
    else if(strcmp(argv[argPos+1],"-minMaf")==0||strcmp(argv[argPos+1],"-maf")==0)
      minMaf=atof(argv[argPos+1]);
    else if(strcmp(argv[argPos],"-plink")==0){
      useGeno = 1;
      std::string p_str =string( argv[argPos+1]);
      if(p_str.length()>4){
	std::string ext = p_str.substr(p_str.length()-4,p_str.length());
	if (!ext.compare(".bed")||!ext.compare(".bim")||!ext.compare(".fam")){
	  std::string front = p_str.substr(0,p_str.length()-4);
	  plink_bim = (front+".bim");
	  plink_fam = (front+".fam");
	  plink_bed = (front+".bed");
	}else{
	  plink_bim = (p_str+".bim");
	  plink_fam = (p_str+".fam");
	  plink_bed = (p_str+".bed");	
	}}else{
	plink_bim = (p_str+".bim");
	plink_fam = (p_str+".fam");
	plink_bed = (p_str+".bed");	
     	}
    }
    else if(strcmp(argv[argPos],"-beagle")==0){
      useGenoLikes = 1;
      bname=argv[argPos+1];
    }
    else if(strcmp(argv[argPos],"-misTol")==0)
      misTol=atof(argv[argPos+1]);
    else{
      printf("\nArgument unknown will exit: %s \n",argv[argPos]);
      info();
      return 0;
    }
    
    argPos+=2;
  }

  // Input control
  if(!useGeno&!useGenoLikes){
    fprintf(stderr, "Please supply either a plink or a beagle file \n");
    info();
    return 0;
  }
  

  if(useGeno&useGenoLikes){
    fprintf(stderr, "Both plink and beagle files provided; please do one at a time, will exit \n");
    info();
    return 0;

  }
  
  //check if files exists
  // fexists
  fex(qname);
  fex(fname);

  
  if(useGeno){

    
    //////////////////////////////////////////////////
    //read plink data
    printf("\t-> Will assume these are the plink files:\n\t\tbed: %s\n\t\tbim: %s\n\t\tfam: %s\n",plink_bed.c_str(),plink_bim.c_str(),plink_fam.c_str());
    int numInds = numberOfLines(plink_fam.c_str())-1;//the number of individuals is just the number of lines

  
    myPars *pars =  new myPars();
    plinkKeep = doBimFile(pars,plink_bim.c_str()," \t",autosomeMax);  
    fprintf(stdout,"\t-> Plink file contains %d autosomale SNPs\n",plinkKeep->numTrue);
    fprintf(stdout,"\t-> reading genotypes ");
    fflush(stdout);
    iMatrix *tmp = bed_to_iMatrix(plink_bed.c_str(),numInds,plinkKeep->x);
    fprintf(stdout," - done \n");
    fflush(stdout);
    if(tmp->y==plinkKeep->numTrue){
    
      pars->data = tmp;
    }
    else{
      fprintf(stdout,"\t-> extractOK (%d %d) ",tmp->x,plinkKeep->numTrue);
      fflush(stdout);
    
      pars->data = extractOK(plinkKeep,tmp);
      killMatrix(tmp);
      fprintf(stdout," - done \n");
      fflush(stdout);

    }
    killArray(plinkKeep);
    fprintf(stdout,"\t-> sorting ");
    fflush(stdout);
    mysort(pars,0);
    fprintf(stdout," - done \n");
    fflush(stdout);
    // printf("Dimension of genodata:=(%d,%d), positions:=%d, chromosomes:=%d\n",pars->data->x,pars->data->y,pars->position->x,pars->chr->x);
    if(pars->data->y != pars->chr->x || pars->position->x != pars->data->y){
      printf("Dimension of data input doesn't have compatible dimensions, program will exit\n");
      printf("Dimension of genodata:=(%d,%d), positions:=%d, chromosomes:=%d\n",pars->data->x,pars->data->y,pars->position->x,pars->chr->x);
      return 0;
    }
  
  


  
    int K=getK(qname);
    int nSites=pars->data->y;
    int nInd=pars->data->x;
    fprintf(stderr,"\t\t->K=%d\tnSites=%d\tnInd=%d\n",K,nSites,nInd);

    pars->K=K;
    pars->nSites=nSites;
    pars->nInd=nInd;
  
    fp=fopen(outname,"w");
    double **F =allocDouble(nSites,K);
    double **Q =allocDouble(nInd,K);
    pars->F=F;
    pars->Q=Q;
    readDouble(Q,nInd,K,qname,0);
    readDoubleGZ(F,nSites,K,fname,1);

 
    double **cor=allocDouble(nInd,nInd);
    double **r=allocDouble(nInd,nSites);
    double *mean_r=new double[nInd];


    // Create matrix to avoid missing data
    int **isMissing= new int*[nSites];

    // Transposed geno matrix
    int **genosT = new int*[nSites];
    // PROBABLY COULD DO THIS WHEN READING DATA AND THEN ONLY HAVE ONE MATRIX IN MEMORY
    for(int j=0; j<nSites;j++){
      genosT[j] = new int[nInd];
      isMissing[j] = new int[nInd];
      for(int i=0; i<nInd; i++){
	genosT[j][i] = pars->data->matrix[i][j];
			
	if(genosT[j][i]==3)
	  isMissing[j][i] = 1;
	
	else
	  isMissing[j][i] = 0;
      }
    }


    

    pars -> r = r;
    pars -> mean_r = mean_r;
    pars -> nIts = nIts;
    pars -> isMissing = isMissing;
    pars -> genos = genosT;
    
    // Calculate unadapted residuals
    calcRes(r, mean_r, pars->genos, pars-> Q, pars->F, K, nSites, nInd, isMissing);
    fprintf(stderr, "Finished calculating normal residuals\n");

    // without threading
    
    if(nThreads==1){
      // Loop over inds, adapt its freq and calculate cor with the remaining inds
      for(int i=0;i<(nInd-1);i++){

	fprintf(stderr, "Estimating ancestral frequencies without individual %d\r",i);
	
	evalAdmix(cor[i], i, pars -> r, pars -> mean_r, pars-> genos ,pars->K,pars-> Q, pars->F,pars->nInd, pars->nSites,pars->nIts, pars->isMissing);

      }

 
    }
    else{ // with threads (the cool way)
 
      fprintf(stderr, "Re-estimating ancestral frequencies with %d threads...",nThreads);
      
      NumJobs = nInd-1;
      jobs =  nInd-1;
      cunt = 0;
      allPars = new eachPars[NumJobs];

    
      for(int c=0;c<NumJobs;c++){ // fill allPars with each job
    
      
	allPars[c].ind1=c;
	allPars[c].pars=pars;
	allPars[c].cor=cor[c];
      }
      pthread_t thread1[nThreads];
    
      for (int i = 0; i < nThreads; i++)
	pthread_create(&thread1[i], NULL, &functionC, NULL);
    
      // Wait all threads to finish
      for (int i = 0; i < nThreads; i++)
	pthread_join(thread1[i], NULL);
    
     

    }


    fprintf(stderr, "\nFinished, going to write all correlations\n");

    for(int i=0;i<nInd;i++){
      for(int j=0;j<nInd;j++){
	if(i<j)
	  fprintf(fp,"%f\t",cor[i][j]);
	else if(i>j)
	  fprintf(fp,"%f\t",cor[j][i]);
	else if(i==j)
	  fprintf(fp,"NA\t");
      }
      fprintf(fp,"\n");

    }

    fprintf(stderr, "Correlation matrix has been written to %s\n", outname);  


    // clean


    for(int j = 0; j < nSites; j++){ 
      delete[] F[j];
      delete[] isMissing[j];
      delete[] genosT[j];
    }


    for(int i = 0; i < nInd; i++){
      delete[] Q[i];
      delete[] r[i];
      delete[] cor[i];
    }
    delete[] F;
    delete[] Q;
    delete[] r;
    delete[] isMissing;
    delete[] genosT;
    delete[] mean_r;
    delete[] cor;
    fclose(fp);
    delete[] allPars;

  }// End of version with genotypes

  
  // NGS version
  if(useGenoLikes){

    myNGSPars *pars =  new myNGSPars();
    fprintf(stderr,"Going to read beagle file\n");
    bgl d=readBeagle(bname);
    fprintf(stderr,"Input file has dim: nsites=%d nind=%d\n",d.nSites,d.nInd);

    // apply maf filter
    if(minMaf!=0.0)
      filterMinMaf(d,minMaf);
    
    double **cor=allocDouble(d.nInd,d.nInd);
    double **r=allocDouble(d.nInd,d.nSites);
    double *mean_r=new double[d.nInd];

    int K=getK(qname);
    int nSites=d.nSites;
    int nInd=d.nInd;
    fprintf(stderr,"\t\t->K=%d\tnSites=%d\tnInd=%d\n",K,nSites,nInd);
    pars->genos = d.genos;
    pars -> keeps = d.keeps;
    pars->K=K;
    pars->nSites=nSites;
    pars->nInd=nInd;
    pars -> nIts=nIts;
  
    fp=fopen(outname,"w");
    double **F =allocDouble(nSites,K);
    double **Q =allocDouble(nInd,K);
    pars->F=F;
    pars->Q=Q;
    readDouble(Q,nInd,K,qname,0);
    readDoubleGZ(F,nSites,K,fname,1);


    // Calculate unadapted residuals
    ngscalcRes(r, mean_r, pars->F, pars-> Q,pars-> K, pars->nSites,pars-> nInd,pars->genos, pars-> keeps);
    fprintf(stderr, "Finished calculating normal residuals\n");


    pars -> r = r;
    pars -> mean_r = mean_r;
    pars -> nIts = nIts;
    
    // without threading
    if(nThreads==1){
      for(int i=0;i<(nInd-1);i++){
	
       fprintf(stderr, "Estimating freqs without ind %d\r",i);
	
	ngsevalAdmix(cor[i], i, pars->r, pars->mean_r, pars->genos, pars->K,pars->Q,pars->F,pars->nInd,pars->nSites,pars->nIts,pars->keeps);

      }
    }
      else{ // with threads (the cool way)


      fprintf(stderr, "Correcting frequencies with %d threads...",nThreads);
      
      NumJobs = nInd-1;
      jobs = nInd-1;
      cunt = 0;
      allNGSPars = new eachNGSPars[NumJobs];

    
      for(int c=0;c<NumJobs;c++){ // fill allPars with each job
    
      
	allNGSPars[c].ind1=c;
	allNGSPars[c].pars=pars;
	allNGSPars[c].cor=cor[c];
      }
      pthread_t thread1[nThreads];
    
      for (int i = 0; i < nThreads; i++)
	pthread_create(&thread1[i], NULL, &ngsfunctionC, NULL);
    
      // Wait all threads to finish
      for (int i = 0; i < nThreads; i++)
	pthread_join(thread1[i], NULL);
    
     

    }




    fprintf(stderr, "\nFinished, going to write all correlations\n");
    // Prints correlation as nIndxnInd tab-delimited matrix, with NA in diagonal (self-correlation)
    for(int i=0;i<nInd;i++){
      for(int j=0;j<nInd;j++){
	if(i<j)
	  fprintf(fp,"%f\t",cor[i][j]);
	else if(i>j)
	  fprintf(fp,"%f\t",cor[j][i]);
	else if(i==j)
	  fprintf(fp,"NA\t");
      }
      fprintf(fp,"\n");

    }

    fprintf(stderr, "Correlation matrix has been written to %s\n", outname);
    // clean

    dalloc(d);

    for(int j = 0; j < nSites; j++){ 
      delete[] F[j];
    }


    for(int i = 0; i < nInd; i++){
      delete[] Q[i];
      delete[] r[i];
      delete[] cor[i];
    }
    delete[] F;
    delete[] Q;
    delete[] r;
    delete[] mean_r;
    delete[] cor;
    fclose(fp);
    delete[] allNGSPars;

      
    
  }

  fprintf(stderr, "\t[ALL done] cpu-time used =  %.2f sec\n", (float)(clock() - t) / CLOCKS_PER_SEC);
  fprintf(stderr, "\t[ALL done] walltime used =  %.2f sec\n", (float)(time(NULL) - t2));  
  fprintf(stderr, "\t[ALL done] results have been outputted to %s\n",outname);



  return(0);
}

