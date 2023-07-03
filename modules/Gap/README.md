# Gap Introduction

## 1. Gap Scenario
Gap acceptance calculates the probability of passing before a vehicle. 
The gap parameters are estimated via Maximum Likelihood Estimate (MLE), which is implemented 

<center>
    <img style="border-radius: 0.3125em;
    box-shadow: 0 2px 4px 0 rgba(34,36,38,.12),0 2px 10px 0 rgba(34,36,38,.08);" 
    src="../../imgs/gap.png" width = "60%" alt=""/>
    <br>
    <div style="color:orange; border-bottom: 1px solid #d9d9d9;
    display: inline-block;
    color: #999;
    padding: 2px;">
      Gap Scenario
  	</div>
</center>


## 2. Gap Definition 
Critical gap acceptance is a typical classification problem, which outputs 1 or 0, representing whether the vehicle will accept the gap or not. It can described with a logit model. A general logit model is defined as follows
$$
P\left( y_i=1|x_i \right) =S\left( x_i\beta \right) 
$$

Where
$$
S\left( t \right) =\frac{1}{1+\exp \left( -t \right)}
$$

<center>
    <img style="border-radius: 0.3125em;
    box-shadow: 0 2px 4px 0 rgba(34,36,38,.12),0 2px 10px 0 rgba(34,36,38,.08);" 
    src="../../imgs/logit_function.png" width = "60%" alt=""/>
    <br>
    <div style="color:orange; border-bottom: 1px solid #d9d9d9;
    display: inline-block;
    color: #999;
    padding: 2px;">
      Logit function sample
  	</div>
</center>



In the case of gap acceptance, we will consider the factors of vehicle position and vehicle speed.


## 3. MLE
As long as gap acceptance is a logit model, MLE is a widely used method to estimate the parameters. 
We can refer to the psu manuscript of MLE for more details
https://online.stat.psu.edu/stat415/lesson/1/1.2 and https://www.statlect.com/fundamentals-of-statistics/logistic-classification-model \
Here we give a simple derivation of the problem.

There is no analytical solution for the estimation of parameters of logit functions. 

