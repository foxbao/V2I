// #include "viz_processor.hpp"
// namespace coop {
// namespace v2x {
// VizProcessor::VizProcessor() {}

// void VizProcessor::Start() {
//   // window
//   viz::Viz3d window("window");
//   window.showWidget("Coordinate", viz::WCoordinateSystem());
//   window.setBackgroundColor(viz::Color(186, 36, 1));

//   // plane
//   viz::WPlane plane(Size2d(2, 2), viz::Color(13, 255, 39));
//   plane.setRenderingProperty(viz::LINE_WIDTH, 5);
//   plane.setPose(Affine3f());
//   window.showWidget("plane", plane);

//   // line
//   viz::WLine line(Point3d(0.0, 0.0, 0.0), Point3d(10.0, 10.0, 10.0),
//                   viz::Color::yellow());
//   line.setRenderingProperty(viz::LINE_WIDTH, 2);
//   // line.setPose(Affine3f());
//   window.showWidget("line", line);

//   // WSphere
//   viz::WSphere sphere(Point3d(1, 4, 3), 2, 1, viz::Color::blue());
//   sphere.setRenderingProperty(viz::LINE_WIDTH, 1);
//   window.showWidget("sphere", sphere);

//   // WArrow
//   viz::WArrow warrow(Point3d(1, 1, 1), Point3d(-2, 8, -2), 0.03,
//                      viz::Color::red());
//   warrow.setRenderingProperty(viz::LINE_WIDTH, 2);
//   window.showWidget("warrow", warrow);

//   // WCircle
//   viz::WCircle wcricle(3.0, 0.02, viz::Color::navy());
//   wcricle.setRenderingProperty(viz::LINE_WIDTH, 2);
//   window.showWidget("wcricle", wcricle);

//   // WPolyLine (将点集自动转为一根线显示,适合SLAM显示轨迹)
//   vector<Point3d> points3d;
//   for (int i = 0; i < 5; i++) {
//     points3d.push_back(Point3d(i, i * i, 2 * i));
//   }

//   viz::WPolyLine wpolyLine(points3d, viz::Color::rose());
//   wpolyLine.setRenderingProperty(viz::LINE_WIDTH, 2);
//   window.showWidget("wpolyLine", wpolyLine);

//   // WPaintedCloud
//   vector<Point3d> pointCloud;
//   // 创建点云
//   pointCloud.resize(points3d.size());
//   for (size_t i = 0; i < points3d.size(); i++) {
//     pointCloud[i] = points3d[i] * 3;
//   }
//   viz::WPaintedCloud wpaintedCloud(pointCloud);
//   wpaintedCloud.setRenderingProperty(viz::POINT_SIZE, 4);
//   window.showWidget("wpaintedCloud", wpaintedCloud);

//   // WGrid
//   viz::WGrid wgrid(Vec2i::all(10), Vec2d::all(1.0), viz::Color::white());
//   window.showWidget("wgrid", wgrid);

//   // WCube
//   viz::WCube wcube(Vec3d::all(-5), Vec3d::all(5), true, viz::Color::cherry());
//   wcube.setRenderingProperty(viz::LINE_WIDTH, 3);
//   window.showWidget("wcube", wcube);

//   // WText(2D)
//   viz::WText wtext("OpenCV", Point2i(100, 100), 20, viz::Color::green());
//   window.showWidget("wtext", wtext);

//   // WText3D
//   viz::WText3D wtext3d("OpenCV", Point3d(6, 5, 5), 4.1, true,
//                        viz::Color::white());
//   window.showWidget("wtext3d", wtext3d);

//   // WTrajectory
//   // Displays a poly line that represents the path. 先要将点进行拟合？？
//   /*viz::WTrajectory wtrajectory(points3d, viz::WTrajectory::PATH, 1.0,
//   viz::Color::green()); wtrajectory.setRenderingProperty(viz::LINE_WIDTH, 3);
//   window.showWidget("wtrajectory", wtrajectory);*/

//   Mat rvec = Mat::zeros(1, 3, CV_32F);
//   while (!window.wasStopped()) {
//     rvec.at<float>(0, 0) = 0.f;
//     rvec.at<float>(0, 1) += CV_PI * 0.01f;
//     rvec.at<float>(0, 2) = 0.f;
//     Mat rmat;
//     Rodrigues(rvec, rmat);
//     Affine3f pose(rmat, Vec3f(0, 0, 0));

//     window.setWidgetPose("plane", pose);

//     window.showWidget("line", line);

//     window.showWidget("sphere", sphere);

//     window.showWidget("warrow", warrow);

//     window.showWidget("wcricle", wcricle);

//     window.showWidget("wpolyLine", wpolyLine);

//     window.showWidget("wpaintedCloud", wpaintedCloud);

//     // window.showWidget("wtrajectory", wtrajectory);

//     window.showWidget("wgrid", wgrid);

//     window.showWidget("wcube", wcube);

//     window.showWidget("wtext", wtext);

//     window.showWidget("wtext3d", wtext3d);

//     window.spinOnce(1, true);
//   }

//   window.removeAllWidgets();
// }
// }  // namespace v2x
// }  // namespace coop