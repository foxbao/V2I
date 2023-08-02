
## Table of Contents

- [Table of Contents](#table-of-contents)
- [Introduction](#introduction)
- [HMM Definition](#hmm-definition)
- [Foward-Backward](#foward-backward)
- [Forward](#forward)
- [Backward](#backward)
- [Baum-Welch algorithm](#baum-welch-algorithm)

## Introduction
Hidden Malkov Model (HMM) is a model to extract truth from the observation. 

https://medium.com/analytics-vidhya/baum-welch-algorithm-for-training-a-hidden-markov-model-part-2-of-the-hmm-series-d0e393b4fb86

## HMM Definition
In HMM, the two important conceptions are states and observations. 
The observation only depends on the state, and the following state only depends on the previous first state. 

<img src="../../imgs/HMM_definition.png" width="400">

## Foward-Backward
We make the assumption that 

We want to determine the probability distribution of the state variable at any time k given the whole sequence of observed data. It can be understood mathematically
$$
P\left( X_k|Y_{0:T} \right) =\frac{P\left( X_k,Y_{0:T} \right)}{P\left( Y_{0:T} \right)}=\frac{P\left( X_k,Y_{0:k},Y_{k+1:T} \right)}{P\left( Y_{0:T} \right)}
\\
=\frac{P\left( X_k,Y_{0:k} \right) P\left( Y_{k+1:T}|X_k,Y_{0:k} \right)}{P\left( Y_{0:T} \right)}
$$
As long as the observations are independent of each other

$$
=\frac{P\left( X_k,Y_{0:k} \right) P\left( Y_{k+1:T}|X_k \right)}{P\left( Y_{0:T} \right)}
$$

We name the first term of numerator, which is a joint probability, 
$$
\alpha \left( X_k \right) =P\left( X_k,Y_{0:k} \right) 
$$

and the second term of numerator, the conditional probability

$$
\beta \left( X_k \right) =P\left( Y_{k+1:T}|X_k \right) 
$$

Therefore, the whole joint probability can be represented by a consise format

$$
P\left( X_k|Y_{0:T} \right) =\frac{\alpha \left( X_k \right) \beta \left( X_k \right)}{P\left( Y_{0:T} \right)}\propto \alpha \left( X_k \right) \beta \left( X_k \right) 
$$
The denominator term is a normalization constant and is usually dropped like this because it does not depend on the state, and therefore it is not important when comparing the probability of different states at any time k.


## Forward
We can use the marginalization of previous state to calculate the alpha function
$$
\alpha \left( X_k \right) =P\left( X_k,Y_{0:k} \right) =\sum_{X_{k-1}}{P\left( X_{k-1},X_k,Y_{0:k} \right)}
\\
=\sum_{X_{k-1}}{P\left( X_{k-1},Y_{0:k-1} \right) P\left( X_k|X_{k-1},Y_{0:k-1} \right) P\left( Y_k|X_k \right)}
$$

As stated before, state X does not depend on the observation Y, and we note that 
$$
\alpha \left( X_{k-1} \right) =P\left( X_{k-1},Y_{0:k-1} \right) 
$$

Then we can calculate the forward recursively

$$
\alpha \left( X_k \right) =\sum_{X_{k-1}}{\alpha \left( X_{k-1} \right) P\left( X_k|X_{k-1} \right) P\left( Y_k|X_k \right)}
$$
1. The alpha function is defined as the joint probability of the observed data up to time k and the state at time k
2. It is a recursive function because the alpha function appears in the first term of the right hand side (R.H.S.) of the equation, meaning that the previous alpha is reused in the calculation of the next. This is also why it is called the forward phase.
3. The second term of the R.H.S. is the state transition probability from A, while the last term is the emission probability from B.
4. The R.H.S. is summed over all possible states at time k -1.


We need the following starting alpha to begin the recursion.
$$
\alpha \left( X_0 \right) =P\left( X_0,Y_0 \right) =P\left( Y_0|X_0 \right) =P\left( X_0 \right) 
$$
the starting alpha is the product of probabilities of the emission and the initial state


## Backward

$$
\beta \left( X_k \right) =P\left( Y_{k+1:T}|X_k \right) =\sum_{X_{k+1}}{\left( Y_{k+1:T},X_{k+1}|X_k \right)}
\\
=\sum_{X_{k+1}}{\left( Y_{k+2:T},X_{k+1},Y_{k+1}|X_k \right)}
\\
=\sum_{X_{k+1}}{\left( Y_{k+2:T}|Y_{k+1},X_{k+1},X_k \right) P\left( Y_{k+1}|X_{k+1},X_k \right)}P\left( X_{k+1}|X_k \right) 
$$

Therefore,
$$
\beta \left( X_k \right) =\sum_{X_{k+1}}{\left( Y_{k+2:T}|Y_{k+1} \right) P\left( Y_{k+1}|X_{k+1} \right)}P\left( X_{k+1}|X_k \right) 
\\
\sum_{X_{k+1}}{\beta \left( X_{k+1} \right) P\left( Y_{k+1}|X_{k+1} \right)}P\left( X_{k+1}|X_k \right) 
$$


Similar points could be made here:

1. The beta function is defined as the conditional probability of the observed data from time k+1 given the state at time k
2. It is a recursive function because the beta function appears in first term of the right hand side of the equation, meaning that the next beta is reused in the calculation of the current one. This is also why it is called a backward phase.
3. The second term of the R.H.S. is the state transition probability from A, while the last term is the emission probability from B.
4. The R.H.S. is summed over all possible states at time k +1.


## Baum-Welch algorithm
Also known as the forward-backward algorithm, the Baum-Welch algorithm is a dynamic programming approach and a special case of the expectation-maximization algorithm (EM algorithm). Its purpose is to tune the parameters of the HMM, namely the state transition matrix A, the emission matrix B, and the initial state distribution π₀, such that the model is maximally like the observed data.
