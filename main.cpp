#include <iostream>
#include <opencv2/opencv.hpp>
#include <trellis.h>
#include <map.h>

#include <map>
#include <vector>


#define Maxsize_N 100
#define Maxsize_M 100
#define Maxsize_T 100
int V2I_test()
{


    using namespace std;
    map<string, int> Q;
    Q["Fair"] = 0;
    Q["Loaded"] = 1;
    map<string, int> V;
    V["Head"] = 0;
    V["Tail"] = 1;

    vector<string> O;
    int N, M;
    double p;
    // cout << "please input M N:"<<endl;
    // cin>>M>>N;
    N = 2;
    M = 2;
    double A[Maxsize_N][Maxsize_N] = {.0};
    double B[Maxsize_N][Maxsize_M] = {.0};
    double PI[Maxsize_N] = {.0};
    double alpha[Maxsize_T][Maxsize_N] = {.0};
    double beta[Maxsize_T][Maxsize_N] = {.0};
    A[0][0] = 0.9;
    A[0][1] = 0.1;
    A[1][0] = 0.1;
    A[1][1] = 0.9;
    B[0][0] = 0.5;
    B[0][1] = 0.5;
    B[1][0] = 0.75;
    B[1][1] = 0.25;

    PI[0] = 0.5;
    PI[1] = 0.5;
    int T;
    T = 3;

    O.push_back("Head");
    O.push_back("Tail");
    O.push_back("Head");
    O.push_back("end");

    cout << "forward" << endl;

    string o1 = O[0];
    for (int i = 0; i < N; i++)
    {
        alpha[1][i] = PI[i] * B[i][V[o1]];
        cout << "alpha" << i << ":" << alpha[1][i] << endl;
    }

    for (int t = 2; t <= T; t++)
    {
        string o = O[t - 1];
        for (int i = 0; i < N; i++)
        {
            double a = .0;
            for (int k = 0; k < N; k++)
            {
                a += alpha[t - 1][k] * A[k][i];
            }
            alpha[t][i] = a * B[i][V[o]];
            cout << "alpha " << t << "    " << i << ":" << alpha[t][i] << endl;
        }
    }

    double P = .0;
    for (int i = 0; i < N; i++)
    {
        P += alpha[T][i];
    }
    cout << "Result:" << P << endl;

    cout << "backward" << endl;
    string oT = O[T - 1];
    for (int i = 0; i < N; i++)
    {
        beta[T][i] = 1.0;
        cout << "beta1 " << i << ":" << beta[T][i] << endl;
    }

    for (int t = T - 1; t >= 1; t--)
    {
        string o = O[t];
        for (int i = 0; i < N; i++)
        {
            double b = .0;
            for (int k = 0; k < N; k++)
            {
                b += A[i][k] * B[k][V[o]] * beta[t + 1][k];
            }
            beta[t][i] = b;
            cout << "beta " << t << "  " << i << ":" << beta[t][i] << endl;
        }
    }

    double P1 = .0;
    string o = O[0];
    for (int i = 0; i < N; i++)
        P1 += PI[i] * B[i][V[o]] * beta[1][i];
    cout << "Result : " << P1 << endl;
    return 0;
}

int main()
{
    std::cout << "Hello" << std::endl;
    V2I::trellis trel;
    
    V2I::Map map;
    cv::Mat a;
    V2I_test();
}