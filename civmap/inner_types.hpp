#pragma once
#include <bitset>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <iostream>
#include <eigen3/Eigen/Core>
#include <eigen3/Eigen/Dense>
#include <eigen3/Eigen/Geometry>
#include "common/type_define.hpp"
namespace civ
{
    namespace V2I
    {
        namespace map
        {
            using namespace civ::common::util;

            // ! 内部结构体定义 增加类型需要修改 operator<<
            enum class ZFrameType
            {
                None = 0,
                VEHICLE_MODEL = 1, // 车辆运动约束虚拟帧
                // 虚拟帧
                IMU = 5,       // IMU
                WHEEL_ODO = 6, // 车轮里程计
                GNSS_POS = 7,  // GNSS 观测值
                GNSS_VEL = 8,  // GNSS 观测值
                GNSS_ALL = 9,  // GNSS 观测值[vel pos dual_antenna]
                //
                CGI610 = 10,
                Cpt7 = 11,
                STATE = 15,
                PERCEPTION = 20,
                SEMANTIC_MAP = 21,
                LINESEGMENT = 25,
                POLE = 26,
                SIGN = 27,
                SEMANTIC_MATCH = 40,
                // LIDAR
                PointCloud = 50,
                LIDAR_MATCH = 51,           // 激光匹配结果
                PointCloudMapFileInfo = 52, // 地图文件信息
            };

            struct ZFrame
            {
                ZFrame() {}
                virtual ~ZFrame(){};
                double t0_, t1_, t2_;      // 产生时刻, 接收时刻，计算完成时刻
                ZFrameType type_;          // 数据类型
                std::string channel_name_; // 数据所在通道
            };
            DEFINE_EXTEND_TYPE(ZFrame);

            enum class LineSegmentType
            {
                UNKNOWN = 0,   // 无法从地图中得知线段的该项属性
                DASH = 1,      // 虚线
                SOLID = 2,     // 实线
                DOUBLE = 3,    // 双线，目前地图中双线只能是地图DOUBLE_YELLOW，没有DOUBLE_DASH
                               // 和DOUBLE_SOLID 数据来源
                CURB = 4,      // 路牙子
                STOP_LINE = 5, // 这条线段来自于地图中的stop_line
            };
            enum class LineSegmentColor
            {
                UNKNOWN = 0,
                YELLOW,
                WHITE
            };
            enum class PoleType
            {
                UNKNOWN = 0,
                LAMP,
                SIGN
            };
            enum class SignType
            {
                UNKNOWN = 0,
                CIRCLE,
                TRIANGLE,
                RECTANGLE
            };

            // map---------------------------------------------------
            struct ZMapBase : public ZFrame
            {
                unsigned int map_id_;
                std::string id_; // string id
                std::vector<Eigen::Vector3d> points_; // 每一个地图点
            };
            // 注意 ： 这里的linesegment
            // 是指地图中在路面上真实存在的线状结构，来源不仅限于车道线，停止线，马路牙子等等。
            struct ZMapLineSegment : public ZMapBase
            {
                ZMapLineSegment() { type_ = ZFrameType::LINESEGMENT; }

                LineSegmentType mtype_ = LineSegmentType::UNKNOWN;
                LineSegmentColor color_ = LineSegmentColor::UNKNOWN;
            };

            struct ZMapPole : public ZMapBase
            {
                ZMapPole() { type_ = ZFrameType::POLE; }
                PoleType mtype_ = PoleType::UNKNOWN;
            };
            struct ZMapSign : public ZMapBase
            {
                ZMapSign() { type_ = ZFrameType::SIGN; }
                SignType mtype_ = SignType::UNKNOWN;
                float rect_width_;
                float rect_height_;
            };
            DEFINE_EXTEND_TYPE(ZMapBase);
            DEFINE_EXTEND_TYPE(ZMapLineSegment);
            DEFINE_EXTEND_TYPE(ZMapPole);
            DEFINE_EXTEND_TYPE(ZMapSign);
            struct ZSemanticMap : public ZFrame
            {
                ZSemanticMap() { type_ = ZFrameType::SEMANTIC_MAP; }
                void add_sign(spZMapSign zsign)
                {
                    signs_.push_back(zsign);
                    elems_map_[zsign->id_] = zsign;
                }
                void add_line(spZMapLineSegment zline)
                {
                    line_segments_.push_back(zline);
                    elems_map_[zline->id_] = zline;
                }
                void add_pole(spZMapPole zpole)
                {
                    poles_.push_back(zpole);
                    elems_map_[zpole->id_] = zpole;
                }
                //   sp_cState state_;
                std::vector<sp_cZMapLineSegment> line_segments_;
                std::vector<sp_cZMapPole> poles_;
                std::vector<sp_cZMapSign> signs_;
                std::map<std::string, spZMapBase> elems_map_; // 所有元素map
            };
            DEFINE_EXTEND_TYPE(ZSemanticMap);
        }
    } // namespace civ
}
