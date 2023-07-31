# HMM project


## Table of Contents

1. [Introduction](#introduction)
2. [HMM Definition](#hmm-definition)

## Introduction
Hidden Malkov Model (HMM) is a model to extract truth from the observation. 

## HMM Definition
In HMM, the two important conceptions are states and observations. 
The observation only depends on the state, and the following state only depends on the previous first state. 
<img src="../../imgs/HMM_definition.png" width="400">


## Baum-Welch algorithm
Also known as the forward-backward algorithm, the Baum-Welch algorithm is a dynamic programming approach and a special case of the expectation-maximization algorithm (EM algorithm). Its purpose is to tune the parameters of the HMM, namely the state transition matrix A, the emission matrix B, and the initial state distribution π₀, such that the model is maximally like the observed data.
