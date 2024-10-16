/******************************************************************************
 * Copyright 2022 The CIV Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/
#pragma once
#include <bitset>
#include <eigen3/Eigen/Core>
#include <eigen3/Eigen/Dense>
#include <eigen3/Eigen/Geometry>
#include <map>
#include <memory>
#include <string>
#include <vector>
namespace coop {
namespace v2x {

// Raysun data type
using OCShort = __int16_t;
using OCByte = unsigned char;
using OCLong = __int64_t;
using OCDouble = double;
using OCFloat = float;
using OCInt = unsigned int;

#define cr(T) T const &
#define sp(T) std::shared_ptr<T>
#define up(T) std::unique_ptr<T>
#define crsp_c(T) std::shared_ptr<T const> const &
#define sp_c(T) std::shared_ptr<T const>
#define crsp(T) std::shared_ptr<T> const &

// #define crsp_c(T) std::shared_ptr<T const> const &
#define DEFINE_EXTEND_TYPE(T)                         \
  using cr##T = T const &;                            \
  using sp##T = std::shared_ptr<T>;                   \
  using up##T = std::unique_ptr<T>;                   \
  using crsp_c##T = std::shared_ptr<T const> const &; \
  using sp_c##T = std::shared_ptr<T const>;           \
  using crsp_c##T = std::shared_ptr<T const> const    \
      &  // using crsp##T = std::shared_ptr<T> const &

using crString = std::string const &;  // std::string const&

// ! 内部结构体定义 增加类型需要修改 operator<<
enum class ZFrameType {
  None = 0,
  VEHICLE_MODEL = 1,  // 车辆运动约束虚拟帧
  // 虚拟帧
  IMU = 5,        // IMU
  WHEEL_ODO = 6,  // 车轮里程计
  GNSS_POS = 7,   // GNSS 观测值
  GNSS_VEL = 8,   // GNSS 观测值
  GNSS_ALL = 9,   // GNSS 观测值[vel pos dual_antenna]
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
  LIDAR_MATCH = 51,            // 激光匹配结果
  PointCloudMapFileInfo = 52,  // 地图文件信息
};

struct ZFrame {
  ZFrame() {}
  virtual ~ZFrame() {}
  double t0_, t1_, t2_;       // 产生时刻, 接收时刻，计算完成时刻
  ZFrameType type_;           // 数据类型
  std::string channel_name_;  // 数据所在通道
};
DEFINE_EXTEND_TYPE(ZFrame);

enum class LineSegmentType {
  UNKNOWN = 0,  // 无法从地图中得知线段的该项属性
  DASH = 1,     // 虚线
  SOLID = 2,    // 实线
  DOUBLE = 3,  // 双线，目前地图中双线只能是地图DOUBLE_YELLOW，没有DOUBLE_DASH
               // 和DOUBLE_SOLID 数据来源
  CURB = 4,    // 路牙子
  STOP_LINE = 5,  // 这条线段来自于地图中的stop_line
};
enum class LineSegmentColor { UNKNOWN = 0, YELLOW, WHITE };
enum class PoleType { UNKNOWN = 0, LAMP, SIGN };
enum class SignType { UNKNOWN = 0, CIRCLE, TRIANGLE, RECTANGLE };

// map---------------------------------------------------
struct ZMapBase : public ZFrame {
  unsigned int map_id_;
  std::string id_;  // string id

  std::vector<Eigen::Vector3d> points_;  // 每一个地图点
};
// 注意 ： 这里的linesegment
// 是指地图中在路面上真实存在的线状结构，来源不仅限于车道线，停止线，马路牙子等等。
struct ZMapLineSegment : public ZMapBase {
  ZMapLineSegment() { type_ = ZFrameType::LINESEGMENT; }

  LineSegmentType mtype_ = LineSegmentType::UNKNOWN;
  LineSegmentColor color_ = LineSegmentColor::UNKNOWN;
};

struct ZMapPole : public ZMapBase {
  ZMapPole() { type_ = ZFrameType::POLE; }
  PoleType mtype_ = PoleType::UNKNOWN;
};
struct ZMapSign : public ZMapBase {
  ZMapSign() { type_ = ZFrameType::SIGN; }
  SignType mtype_ = SignType::UNKNOWN;
  float rect_width_;
  float rect_height_;
};
DEFINE_EXTEND_TYPE(ZMapBase);
DEFINE_EXTEND_TYPE(ZMapLineSegment);
DEFINE_EXTEND_TYPE(ZMapPole);
DEFINE_EXTEND_TYPE(ZMapSign);
struct ZSemanticMap : public ZFrame {
  ZSemanticMap() { type_ = ZFrameType::SEMANTIC_MAP; }
  void add_sign(spZMapSign zsign) {
    signs_.push_back(zsign);
    elems_map_[zsign->id_] = zsign;
  }
  void add_line(spZMapLineSegment zline) {
    line_segments_.push_back(zline);
    elems_map_[zline->id_] = zline;
  }
  void add_pole(spZMapPole zpole) {
    poles_.push_back(zpole);
    elems_map_[zpole->id_] = zpole;
  }
  //   sp_cState state_;
  std::vector<sp_cZMapLineSegment> line_segments_;
  std::vector<sp_cZMapPole> poles_;
  std::vector<sp_cZMapSign> signs_;
  std::map<std::string, spZMapBase> elems_map_;  // 所有元素map
};
DEFINE_EXTEND_TYPE(ZSemanticMap);

typedef struct StateIndex {
  int qnn1_ = 0, v_ = 0, p_ = 0, bg_ = 0, ba_ = 0, gz_scale_ = 0, qvv1_ = 0;
  int tia_ = 0, tva_ = 0, tdg_ = 0;            // GNSS外参
  int kod_ = 0, tdo_ = 0;                      // 轮速外参
  int qcc1_ = 0, tic_ = 0, tdc_ = 0, tr_ = 0;  // camera外参
  int fx_ = 0, fy_ = 0, cx_ = 0, cy_ = 0;      // camera内参
  int NS_ = 0;                                 // 总状态维数
} SI;

typedef struct _RaysunHead RaysunHead;
struct _RaysunHead {
  OCShort flag;       // 数据帧头, 取值为 0x7E7E
  OCByte devtype;     // 设备类型
  OCLong devid;       // 设备 ID
  OCLong timestamp;   // 时间戳,ms
  OCByte frame;       // 传输帧号
  OCShort datalen;    // 数据区长度
  OCShort targetnum;  // 目标数量
  OCDouble lon;  // 经度, 雷达中心点经纬度 XY WGS84 /CGCS2000 坐标系
  OCDouble lat;  // 纬度, 雷达中心点经纬度 XY WGS84 /CGCS2000 坐标系
  OCFloat rdir;  // 雷达朝向,与正北的夹角,0.0 -360.0
};

typedef struct _RaysunTarget RaysunTarget;
struct _RaysunTarget {
  OCInt id;          // ID
  OCByte obj_class;  // object class
  OCDouble lon;  // 经度, 原始坐标为雷达坐标系,障碍物中心点 XY WGS84 /CGCS2000
                 // 坐标系, (物体中心点经纬度)
  OCDouble lat;  // 纬度, 原始坐标为雷达坐标系,障碍物中心点 XY WGS84 /CGCS2000
                 // 坐标系, (物体中心点经纬度)
  OCFloat obj_length;  // 长度, 单位 m
  OCFloat obj_width;   // 宽度, 单位 m
  OCFloat obj_height;  // 高度, 单位 m
  OCFloat sog;         // 速度, 单位 km/h
  OCFloat cog;  // 航向角, 单位 度, WGS84/CGCS2000 坐标系时的航向角
  OCFloat dist;     // 相对距离,单位 m
  OCFloat bearing;  // 相对角度,单位 度
  OCByte
      objpos;  // 目标所处区域,
               // 障碍物为机动车或非机动车时,判断所处第几车道,以雷达观测区域从左到右依次
               // 为 1 –9 车道;
               // 目标为行人时,判断是否在人行道上,是为 10,否为 0
  OCByte stable;              // 稳定度
  uint64_t origin_timestamp;  // T0&T1
};

DEFINE_EXTEND_TYPE(_RaysunTarget);

struct KITTI_RAW {
  int64_t timestamp;
  int id;
  float lat;
  float lon;
  float alt;
  float roll;
  float pitch;
  float yaw;
  float speed;
};
DEFINE_EXTEND_TYPE(KITTI_RAW);

enum {
  RAYSUN_TARGET_TYPE_UNKNOWN = 0,
  RAYSUN_TARGET_TYPE_PEDESTRIAN,
  RAYSUN_TARGET_TYPE_BIKE,
  RAYSUN_TARGET_TYPE_MOTOBIKE,
  RAYSUN_TARGET_TYPE_CAR,
  RAYSUN_TARGET_TYPE_TRUCK,
  RAYSUN_TARGET_TYPE_BUS,
  RAYSUN_TARGET_TYPE_LONGTRUCK,
  RAYSUN_TARGET_TYPE_MAX,
};

enum {
  RAYSUN_DATA_TYPE_UNKNOWN = 0,
  RAYSUN_DATA_TYPE_CLOUDPOINT,
  RAYSUN_DATA_TYPE_TRACKTARGET,
  RAYSUN_DATA_TYPE_TRAFFICTARGET,
  RAYSUN_DATA_TYPE_MAX,
};

enum {
  RAYSUN_DEV_TYPE_UNKNOWN = 0,
  RAYSUN_DEV_TYPE_RADAR,
  RAYSUN_DEV_TYPE_CAMERA,
  RAYSUN_DEV_TYPE_FUSION,
  RAYSUN_DEV_TYPE_MAX,
};

}  // namespace v2x
}  // namespace coop
