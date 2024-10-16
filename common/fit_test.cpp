// #include <iostream>
// // #include <opencv2/core/core.hpp>
// #include <ceres/ceres.h>
// using ceres::AutoDiffCostFunction;
// using ceres::CostFunction;
// using ceres::Problem;
// using ceres::Solve;
// using ceres::Solver;

// using namespace std;
// using namespace cv;

// struct CURVE_FITTING_COST
// {
//     CURVE_FITTING_COST(double x, double y) : _x(x), _y(y) {}

//     template <typename T>
//     double aaa()
//     {
        
//         return 1;
//     }
//     template <typename T>
//     bool operator()(const T *const abc, T *residual) const
//     {
//         residual[0] = _y - ceres::exp(abc[0] * _x * _x + abc[1] * _x + abc[2]);
//         return true;
//     }
//     const double _x, _y;
// };

// int main(int argc, char **argv)
// {
//     // google::InitGoogleLogging(argv[0]);
//     // 参数初始化设置，abc初始化为0，白噪声方差为1（使用OpenCV的随机数产生器）。
//     double a = 3, b = 2, c = 1;
//     double w = 1;
//     RNG rng;
//     double abc[3] = {0, 0, 0};

//     // 生成待拟合曲线的数据散点，储存在Vector里，x_data，y_data。
//     vector<double> x_data, y_data;
//     for (int i = 0; i < 1000; i++)
//     {
//         double x = i / 1000.0;
//         x_data.push_back(x);
//         y_data.push_back(exp(a * x * x + b * x + c) + rng.gaussian(w));
//     }

//     ceres::Problem problem;
//     for (int i = 0; i < 1000; i++)
//     {
//         CostFunction *cost_function = new AutoDiffCostFunction<CURVE_FITTING_COST, 1, 3>(
//             new CURVE_FITTING_COST(x_data[i], y_data[i]));
//         problem.AddResidualBlock(cost_function, nullptr, abc);
//     }
//     //配置并运行求解器
//     ceres::Solver::Options options;     //定义配置项
//     options.linear_solver_type = ceres::DENSE_NORMAL_CHOLESKY;  //配置增量方程的解法
//     options.minimizer_progress_to_stdout = true;    //输出到cout
//     // options.minimizer_progress_to_stdout = false;    //输出到cout

//     ceres::Solver::Summary summary; //定义优化信息
//     chrono::steady_clock::time_point t1 = chrono::steady_clock::now();  //计时：求解开始时间
//     ceres::Solve(options, &problem, &summary);  //开始优化求解！
//     chrono::steady_clock::time_point t2 = chrono::steady_clock::now();  //计时：求解结束时间
//     chrono::duration<double> time_used = chrono::duration_cast<chrono::duration<double>>(t2 - t1);  //计算求解耗时

//     //输出信息
//     cout << "solve time cost = " << time_used.count() << "s." << endl;  //输出求解耗时
//     // cout << summary.BriefReport() << endl;  //输出简要优化信息
//     cout << "estimated a, b, c = ";
//     for (auto a:abc)    //输出优化变量
//         cout << a << " ";
//     cout << endl;
//     return 0;
// }