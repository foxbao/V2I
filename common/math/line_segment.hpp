/*
 * Converted to C# by John Burns from C++ sourced from:
 * http://softsurfer.com/Archive/algorithm_0106/algorithm_0106.htm
 *
 * Original C++ Copyright Notice
 * ===================
 * Copyright 2001, softSurfer (www.softsurfer.com)
 * This code may be freely used and modified for any purpose
 * providing that this copyright notice is included with it.
 * SoftSurfer makes no warranty for this code, and cannot be held
 * liable for any real or imagined damage resulting from its use.
 * Users of this code must verify correctness for their application.
 */

// https://www.john.geek.nz/2009/03/code-shortest-distance-between-any-two-line-segments/
#pragma once
#include <math.h>
#include <eigen3/Eigen/Core>
namespace civ
{
    namespace common
    {
        namespace math
        {
            struct point
            {
                double x;
                double y;
                double z;
            };

            double closestDistanceBetweenLines(Eigen::Vector3d a0,Eigen::Vector3d a1,Eigen::Vector3d b0,Eigen::Vector3d b1);



        }
    }
}