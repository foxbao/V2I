#include "line_segment.hpp"
namespace civ
{
    namespace common
    {
        namespace math
        {
            double closestDistanceBetweenLines(Eigen::Vector3d a0,Eigen::Vector3d a1,Eigen::Vector3d b0,Eigen::Vector3d b1)
            {
                // Calculate denomitator
                Eigen::Vector3d A=a1-a0;
                Eigen::Vector3d B=b1-b0;
                double magA=A.norm();
                double magB=B.norm();
                Eigen::Vector3d _A=A/magA;
                Eigen::Vector3d _B=B/magB;

                using namespace Eigen;
Eigen::Vector3d v(1, 2, 3);

Eigen::Vector3d w(0, 1, 2);



double vDotw = v.dot(w); // dot product of two vectors

// Eigen::Vector3d vCrossw = v.cross(w); // cross product of two vectors


                int aaa=1;


            }
        }
    }
}