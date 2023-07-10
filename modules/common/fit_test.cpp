#include <iostream>
#include <eigen3/Eigen/Core>
#include <eigen3/Eigen/Dense>

int main() {
    // Sample data points
    Eigen::VectorXd x(5);
    x << 1.0, 2.0, 3.0, 4.0, 5.0;
    Eigen::VectorXd y(5);
    y << 1.0, 8.0, 27.0, 64.0, 125.0;

    // Construct the Vandermonde matrix
    Eigen::MatrixXd A(x.size(), 4);
    for (int i = 0; i < x.size(); i++) {
        A(i, 0) = x(i) * x(i) * x(i);
        A(i, 1) = x(i) * x(i);
        A(i, 2) = x(i);
        A(i, 3) = 1.0;
    }

    // Solve the linear system
    Eigen::VectorXd coefficients = A.colPivHouseholderQr().solve(y);

    // Display the coefficients
    std::cout << "Cubic Equation: y = " << coefficients(0) << "x^3 + " << coefficients(1) << "x^2 + " << coefficients(2) << "x + " << coefficients(3) << std::endl;

    // Calculate and display the fitted values
    std::cout << "Fitted Values:" << std::endl;
    for (int i = 0; i < x.size(); i++) {
        double fittedValue = coefficients(0) * x(i) * x(i) * x(i) + coefficients(1) * x(i) * x(i) + coefficients(2) * x(i) + coefficients(3);
        std::cout << "x = " << x(i) << ", y = " << fittedValue << std::endl;
    }
    return 0;
}
