#pragma once
#include <iostream>
#include <string>
#include <opencv2/opencv.hpp>

// #include "modules/common/inner_types.hpp"
// #include "modules/common/math/vec2dloc.h"

// using civ::civloc::Vec2dloc;
namespace civ
{
    namespace view
    {
        class IMGPROCESSOR
        {
        public:
            IMGPROCESSOR();
            ~IMGPROCESSOR();
            void PlotPositionCovariance();
            void PlotMap();
            void PlotHmmResult();
            void PlotPos();

        private:
            //   void PlotPosLLH(cv::Mat &img, const Vec3d &pos_llh,
            //                   cv::Scalar color = cv::Scalar(0, 255, 255),
            //                   bool is_deg = false);
            //   Vec3d Convert2IMG(const Vec3d &pos_enu);
            //   Vec2dloc Convert2IMG(const Vec2dloc &pt);

        private:
            cv::Mat result_img_;
            int scale_;
            int H_;
            int W_;
            std::string img_folder_;
            double pos_ori_deg_[3];
            double pos_ori_rad_[3];
            void PlotAxis(cv::Mat &img);
        };
        // DEFINE_EXTEND_TYPE(IMGPROCESSOR);
    } // namespace civloc
} // namespace civ