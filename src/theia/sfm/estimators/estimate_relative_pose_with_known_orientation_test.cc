// Copyright (C) 2015 The Regents of the University of California (Regents).
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//
//     * Neither the name of The Regents or University of California nor the
//       names of its contributors may be used to endorse or promote products
//       derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Please contact the author of this library if you have any questions.
// Author: Chris Sweeney (cmsweeney@cs.ucsb.edu)

#include <ceres/rotation.h>
#include <Eigen/Core>
#include <Eigen/Geometry>
#include <glog/logging.h>

#include <algorithm>
#include <vector>

#include "gtest/gtest.h"

#include "theia/matching/feature_correspondence.h"
#include "theia/math/util.h"
#include "theia/sfm/estimators/estimate_relative_pose_with_known_orientation.h"
#include "theia/sfm/pose/test_util.h"
#include "theia/sfm/pose/util.h"
#include "theia/solvers/sample_consensus_estimator.h"
#include "theia/test/test_utils.h"
#include "theia/util/random.h"

namespace theia {

using Eigen::AngleAxisd;
using Eigen::Matrix3d;
using Eigen::Vector2d;
using Eigen::Vector3d;

static const int kNumPoints = 100;
static const double kFocalLength = 1000.0;
static const double kReprojectionError = 4.0;
static const double kErrorThreshold =
    (kReprojectionError * kReprojectionError) / (kFocalLength * kFocalLength);

RandomNumberGenerator rng(66);

void ExecuteRandomTest(const RansacParameters& options,
                       const Matrix3d& rotation,
                       const Vector3d& position,
                       const double inlier_ratio,
                       const double noise,
                       const double tolerance) {
  // Create feature correspondences (inliers and outliers) and add noise if
  // appropriate.
  std::vector<FeatureCorrespondence> correspondences;
  for (int i = 0; i < kNumPoints; i++) {
    FeatureCorrespondence correspondence;
    const Eigen::Vector3d world_point(rng.RandDouble(-2.0, 2.0),
                                      rng.RandDouble(-2.0, 2.0),
                                      rng.RandDouble(6.0, 10.0));

    // Add an inlier or outlier.
    if (i < inlier_ratio * kNumPoints) {
      // Make sure the point is in front of the camera.
      correspondence.feature1.point_ = world_point.hnormalized();
      correspondence.feature2.point_ = (world_point - position).hnormalized();
    } else {
      correspondence.feature1.point_ = rng.RandVector2d();
      correspondence.feature2.point_ = rng.RandVector2d();
    }
    correspondences.emplace_back(correspondence);
  }

  if (noise) {
    for (int i = 0; i < kNumPoints; i++) {
      AddNoiseToProjection(noise / kFocalLength, &rng,
                           &correspondences[i].feature1.point_);
      AddNoiseToProjection(noise / kFocalLength, &rng,
                           &correspondences[i].feature2.point_);
    }
  }

  // Estimate the relative pose.
  Eigen::Vector3d estimated_position;
  RansacSummary ransac_summary;
  EXPECT_TRUE(EstimateRelativePoseWithKnownOrientation(options,
                                                       RansacType::RANSAC,
                                                       correspondences,
                                                       &estimated_position,
                                                       &ransac_summary));

  // Expect that the inlier ratio is close to the ground truth.
  EXPECT_GT(static_cast<double>(ransac_summary.inliers.size()), 3);

  // Expect poses are near.
  EXPECT_TRUE(test::ArraysEqualUpToScale(3,
                                         position.data(),
                                         estimated_position.data(),
                                         tolerance));
}

TEST(EstimateRelativePoseWithKnownOrientation, AllInliersNoNoise) {
  RansacParameters options;
  options.rng = std::make_shared<RandomNumberGenerator>(rng);
  options.use_mle = true;
  options.error_thresh = kErrorThreshold;
  options.failure_probability = 0.001;
  const double kInlierRatio = 1.0;
  const double kNoise = 0.0;
  const double kPoseTolerance = 1e-4;

  const std::vector<Matrix3d> rotations = {
    Matrix3d::Identity(),
    AngleAxisd(DegToRad(12.0), Vector3d::UnitY()).toRotationMatrix(),
    AngleAxisd(DegToRad(-9.0), Vector3d(1.0, 0.2, -0.8).normalized())
        .toRotationMatrix()
  };
  const std::vector<Vector3d> positions = { Vector3d(-1.3, 0, 0),
                                            Vector3d(0, 0, 0.5) };
  for (int i = 0; i < rotations.size(); i++) {
    for (int j = 0; j < positions.size(); j++) {
      ExecuteRandomTest(options,
                        rotations[i],
                        positions[j],
                        kInlierRatio,
                        kNoise,
                        kPoseTolerance);
    }
  }
}

TEST(EstimateRelativePoseWithKnownOrientation, AllInliersWithNoise) {
  RansacParameters options;
  options.rng = std::make_shared<RandomNumberGenerator>(rng);
  options.use_mle = true;
  options.error_thresh = kErrorThreshold;
  options.failure_probability = 0.001;
  const double kInlierRatio = 1.0;
  const double kNoise = 1.0;
  const double kPoseTolerance = 1e-2;

  const std::vector<Matrix3d> rotations = {
    Matrix3d::Identity(),
    AngleAxisd(DegToRad(12.0), Vector3d::UnitY()).toRotationMatrix(),
    AngleAxisd(DegToRad(-9.0), Vector3d(1.0, 0.2, -0.8).normalized())
        .toRotationMatrix()
  };
  const std::vector<Vector3d> positions = { Vector3d(-1.3, 0, 0),
                                            Vector3d(0, 0, 0.5) };

  for (int i = 0; i < rotations.size(); i++) {
    for (int j = 0; j < positions.size(); j++) {
      ExecuteRandomTest(options,
                        rotations[i],
                        positions[j],
                        kInlierRatio,
                        kNoise,
                        kPoseTolerance);
    }
  }
}

TEST(EstimateRelativePoseWithKnownOrientation, OutliersNoNoise) {
  RansacParameters options;
  options.rng = std::make_shared<RandomNumberGenerator>(rng);
  options.use_mle = true;
  options.error_thresh = kErrorThreshold;
  options.failure_probability = 0.001;
  const double kInlierRatio = 0.7;
  const double kNoise = 0.0;
  const double kPoseTolerance = 1e-2;

  const std::vector<Matrix3d> rotations = {Matrix3d::Identity(),
                                           RandomRotation(10.0, &rng)};
  const std::vector<Vector3d> positions = { Vector3d(1, 0, 0),
                                            Vector3d(0, 1, 0) };

  for (int i = 0; i < rotations.size(); i++) {
    for (int j = 0; j < positions.size(); j++) {
      ExecuteRandomTest(options,
                        rotations[i],
                        positions[j],
                        kInlierRatio,
                        kNoise,
                        kPoseTolerance);
    }
  }
}

TEST(EstimateRelativePoseWithKnownOrientation, OutliersWithNoise) {
  RansacParameters options;
  options.rng = std::make_shared<RandomNumberGenerator>(rng);
  options.use_mle = true;
  options.error_thresh = kErrorThreshold;
  options.failure_probability = 0.001;
  const double kInlierRatio = 0.7;
  const double kNoise = 1.0;
  const double kPoseTolerance = 1e-2;

  const std::vector<Matrix3d> rotations = {Matrix3d::Identity(),
                                           RandomRotation(10.0, &rng)};
  const std::vector<Vector3d> positions = {Vector3d(1, 0, 0),
                                           Vector3d(0, 1, 0)};

  for (int i = 0; i < rotations.size(); i++) {
    for (int j = 0; j < positions.size(); j++) {
      ExecuteRandomTest(options,
                        rotations[i],
                        positions[j],
                        kInlierRatio,
                        kNoise,
                        kPoseTolerance);
    }
  }
}

}  // namespace theia
