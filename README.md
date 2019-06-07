# Description

evalAdmix allows to evaluate the results of an admixture analysis. It takes as input the genotype data 
(either called genotypes in plink files or genotype likelihoods beagle files) used in the admixture analysis and the frequency  
and admixture propotions (P and Q files) generated.

The output is a pairwise correlation of residuals matrix between individuals The correlation will be 0 in case of a good fit of 
the data to the admixture model. When something is wrong, individuals from the same population will be positively correlated; and 
individuals from different populationts but that share one or more ancestral populations as admixture sources will have a 
negative correlation. Positive correlation between a pair of individuals might also be due to relatedness.

# Installation

```
git clone https://github.com/GenisGE/evalAdmix.git
cd evalAdmix
make
```

# Usage

```
./evalAdmix
```

```
Arguments:
	-plink name of the binary plink file (excluding the .bed)
	-beagle name of beagle file containing genotype likelihoods (alternative to -plink)
	-fname Ancestral population frequencies
	-qname Admixture proportions
	-o name of the output file
Setup:
	-P Number of threads
	-autosomeMax 23	 autosome ends with this chromsome
	-nIts 5	 number of iterations to do for frequency correction; if set to 0 calculates correlation without correction (fast but biased)
	-useSites 1.0	 Proportion of sites to use to calculate correlation of residuals
	-misTol 0.05 	 Tolerance for considering site as missing when using genotype likelihoods (-beagle)
	-mimMaf 0.05 	 Minimum minor allele frequency to keep site. Use same value as used in NGSadmix to keep compatibility when using genotype likelihoods (-beagle)
```

# Visualization
(maybe add R functions I use to plot?)
