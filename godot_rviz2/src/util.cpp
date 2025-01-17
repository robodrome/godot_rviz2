//
//  Copyright 2022 Yukihiro Saito. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//

//
// Copyright 2022 TIER IV, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "util.hpp"

#include <tf2/LinearMath/Quaternion.h>
#include <tf2/utils.h>

namespace
{
/**
 * @brief Determines if a 2D polygon is defined in a clockwise manner.
 *
 * @param polygon_2d The 2D polygon to check.
 * @return True if the polygon is clockwise, false otherwise.
 */
bool is_clockwise(const std::vector<Vector2> & polygon_2d)
{
  // Get the number of points in the polygon
  const int n = polygon_2d.size();

  // Use the first point of the polygon as a reference point
  const double x_offset = polygon_2d.at(0).x;
  const double y_offset = polygon_2d.at(0).y;

  // Calculate the sum for determining the winding of the polygon
  double sum = 0.0;
  for (std::size_t i = 0; i < polygon_2d.size(); ++i) {
    sum += (polygon_2d.at(i).x - x_offset) * (polygon_2d.at((i + 1) % n).y - y_offset) -
           (polygon_2d.at(i).y - y_offset) * (polygon_2d.at((i + 1) % n).x - x_offset);
  }

  // If the area is negative, the polygon is clockwise
  return sum < 0.0;
}

/**
 * @brief Inverts the order of vertices in a 2D polygon to change its winding.
 *
 * @param polygon_2d The 2D polygon to invert.
 * @return A new polygon with vertices in the inverse order.
 */
std::vector<Vector2> inverse_clockwise(const std::vector<Vector2> & polygon_2d)
{
  // Create a copy of the original polygon
  std::vector<Vector2> inversed_polygon = polygon_2d;

  // Reverse the order of vertices
  std::reverse(inversed_polygon.begin(), inversed_polygon.end());

  return inversed_polygon;
}
}  // namespace

std::optional<geometry_msgs::msg::Transform> get_transform(
  const tf2_ros::Buffer & tf_buffer, const std::string & source_frame_id,
  const std::string & target_frame_id, const rclcpp::Time & time)
{
  try {
    geometry_msgs::msg::TransformStamped transform_stamped;
    if (!tf_buffer.canTransform(
          target_frame_id, source_frame_id, time, rclcpp::Duration::from_seconds(0.5)))
      return std::nullopt;
    transform_stamped = tf_buffer.lookupTransform(
      target_frame_id, source_frame_id, time, rclcpp::Duration::from_seconds(0.5));
    return transform_stamped.transform;
  } catch (...) {
    return std::nullopt;
  }
}

Eigen::Vector3f cross_product(const Eigen::Vector3f & a, const Eigen::Vector3f & b)
{
  return Eigen::Vector3f(
    a.y() * b.z() - a.z() * b.y(), a.z() * b.x() - a.x() * b.z(), a.x() * b.y() - a.y() * b.x());
}

void generate_boundingbox3d(
  float width, float height, float length, const Eigen::Translation3f & translation,
  const Eigen::Quaternionf & quaternion, std::vector<Vector3> & vertices,
  std::vector<Vector3> & normals)
{
  // Create the 2D rectangle vertices
  std::vector<Vector2> polygon_2d = {
    {width / 2, length / 2},
    {width / 2, -length / 2},
    {-width / 2, -length / 2},
    {-width / 2, length / 2}};

  // Call the function to convert the 2D polygon into a 3D polygon
  generate_polygon3d(polygon_2d, height, translation, quaternion, vertices, normals);
}

void generate_cylinder3d(
  float radius, float height, const Eigen::Translation3f & translation,
  const Eigen::Quaternionf & quaternion, std::vector<Vector3> & vertices,
  std::vector<Vector3> & normals)
{
  constexpr int n = 12;
  // Generate the vertices of the circular base of the cylinder
  std::vector<Vector2> polygon_2d;
  for (int i = 0; i < n; ++i) {
    Vector2 point;
    point.x = std::cos(
                (static_cast<float>(n - i) / static_cast<float>(n)) * 2.0 * M_PI +
                M_PI / static_cast<float>(n)) *
              radius;
    point.y = std::sin(
                (static_cast<float>(n - i) / static_cast<float>(n)) * 2.0 * M_PI +
                M_PI / static_cast<float>(n)) *
              radius;
    polygon_2d.push_back(point);
  }

  // Call the function to convert the 2D circular polygon into a 3D cylinder
  generate_polygon3d(polygon_2d, height, translation, quaternion, vertices, normals);
}

void generate_polygon3d(
  const geometry_msgs::msg::Polygon & polygon_2d, float height,
  const Eigen::Translation3f & translation, const Eigen::Quaternionf & quaternion,
  std::vector<Vector3> & vertices, std::vector<Vector3> & normals)
{
  std::vector<Vector2> polygon_2d_vector;
  for (const auto & point : polygon_2d.points) {
    polygon_2d_vector.push_back({point.x, point.y});
  }
  generate_polygon3d(polygon_2d_vector, height, translation, quaternion, vertices, normals);
}

void generate_polygon3d(
  const std::vector<Vector2> & polygon_2d, float height, const Eigen::Translation3f & translation,
  const Eigen::Quaternionf & quaternion, std::vector<Vector3> & vertices,
  std::vector<Vector3> & normals)
{
  // Check if the polygon is defined in a clockwise direction
  if (!is_clockwise(polygon_2d)) {
    std::cerr << "Polygon is not clockwise" << std::endl;
    return;
  }

  // Check if the polygon has at least three vertices
  if (polygon_2d.size() < 3) {
    std::cerr << "Polygon size is less than 3" << std::endl;
    return;
  }

  // Combine translation and rotation to form the transformation matrix
  Eigen::Matrix4f transform = (translation * quaternion).matrix();

  // Initialize the vectors for vertices and normals
  vertices.clear();
  normals.clear();

  // Process the top face
  std::optional<Eigen::Vector3f> top_normal = std::nullopt;
  for (size_t i = 2; i < polygon_2d.size(); ++i) {
    std::array<Eigen::Vector4f, 3> local_vertices{
      Eigen::Vector4f{polygon_2d.at(0).x, polygon_2d.at(0).y, height / 2, 1},
      Eigen::Vector4f{polygon_2d.at(i - 1).x, polygon_2d.at(i - 1).y, height / 2, 1},
      Eigen::Vector4f{polygon_2d.at(i).x, polygon_2d.at(i).y, height / 2, 1}};

    std::array<Eigen::Vector4f, 3> global_vertices{
      transform * local_vertices[0], transform * local_vertices[1], transform * local_vertices[2]};

    vertices.push_back({global_vertices[0][0], global_vertices[0][1], global_vertices[0][2]});
    vertices.push_back({global_vertices[1][0], global_vertices[1][1], global_vertices[1][2]});
    vertices.push_back({global_vertices[2][0], global_vertices[2][1], global_vertices[2][2]});

    if (!top_normal.has_value()) {
      top_normal = cross_product(
        global_vertices[2].head<3>() - global_vertices[0].head<3>(),
        global_vertices[1].head<3>() - global_vertices[0].head<3>());
    }
    const auto & normal = top_normal.value();
    normals.push_back({normal[0], normal[1], normal[2]});
    normals.push_back({normal[0], normal[1], normal[2]});
    normals.push_back({normal[0], normal[1], normal[2]});
  }

  // Process the side faces
  for (size_t i = 0; i < polygon_2d.size(); ++i) {
    size_t j = (i + 1) % polygon_2d.size();

    std::array<Eigen::Vector4f, 4> local_vertices{
      Eigen::Vector4f{polygon_2d.at(i).x, polygon_2d.at(i).y, height / 2, 1},
      Eigen::Vector4f{polygon_2d.at(i).x, polygon_2d.at(i).y, -height / 2, 1},
      Eigen::Vector4f{polygon_2d.at(j).x, polygon_2d.at(j).y, -height / 2, 1},
      Eigen::Vector4f{polygon_2d.at(j).x, polygon_2d.at(j).y, height / 2, 1}};

    std::array<Eigen::Vector4f, 4> global_vertices{
      transform * local_vertices[0], transform * local_vertices[1], transform * local_vertices[2],
      transform * local_vertices[3]};

    vertices.push_back({global_vertices[0][0], global_vertices[0][1], global_vertices[0][2]});
    vertices.push_back({global_vertices[1][0], global_vertices[1][1], global_vertices[1][2]});
    vertices.push_back({global_vertices[2][0], global_vertices[2][1], global_vertices[2][2]});
    vertices.push_back({global_vertices[0][0], global_vertices[0][1], global_vertices[0][2]});
    vertices.push_back({global_vertices[2][0], global_vertices[2][1], global_vertices[2][2]});
    vertices.push_back({global_vertices[3][0], global_vertices[3][1], global_vertices[3][2]});

    const Eigen::Vector3f normal = cross_product(
      global_vertices[2].head<3>() - global_vertices[0].head<3>(),
      global_vertices[1].head<3>() - global_vertices[0].head<3>());
    normals.push_back({normal[0], normal[1], normal[2]});
    normals.push_back({normal[0], normal[1], normal[2]});
    normals.push_back({normal[0], normal[1], normal[2]});
    normals.push_back({normal[0], normal[1], normal[2]});
    normals.push_back({normal[0], normal[1], normal[2]});
    normals.push_back({normal[0], normal[1], normal[2]});
  }

  // Process the bottom face
  Eigen::Vector3f bottom_normal = -top_normal.value();
  for (size_t i = 2; i < polygon_2d.size(); ++i) {
    std::array<Eigen::Vector4f, 3> local_vertices{
      Eigen::Vector4f{polygon_2d.at(0).x, polygon_2d.at(0).y, -height / 2, 1},
      Eigen::Vector4f{polygon_2d.at(i).x, polygon_2d.at(i).y, -height / 2, 1},
      Eigen::Vector4f{polygon_2d.at(i - 1).x, polygon_2d.at(i - 1).y, -height / 2, 1}};

    std::array<Eigen::Vector4f, 3> global_vertices{
      transform * local_vertices[0], transform * local_vertices[1], transform * local_vertices[2]};

    vertices.push_back({global_vertices[0][0], global_vertices[0][1], global_vertices[0][2]});
    vertices.push_back({global_vertices[1][0], global_vertices[1][1], global_vertices[1][2]});
    vertices.push_back({global_vertices[2][0], global_vertices[2][1], global_vertices[2][2]});

    const auto & normal = bottom_normal;
    normals.push_back({normal[0], normal[1], normal[2]});
    normals.push_back({normal[0], normal[1], normal[2]});
    normals.push_back({normal[0], normal[1], normal[2]});
  }
}
