#ifndef I3D_LINE3D_PP_VIEW_H_
#define I3D_LINE3D_PP_VIEW_H_

/*
Line3D++ - Line-based Multi View Stereo
Copyright (C) 2015  Manuel Hofer

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// check libs
#include "configLIBS.h"

// std
#include <map>

// external
#include "eigen3/Eigen/Eigen"
#include "boost/thread/mutex.hpp"

// opencv
#ifndef L3DPP_OPENCV3
#include "opencv/cv.h"
#else
#include "opencv2/imgproc.hpp"
#include "opencv2/core.hpp"
#endif //L3DPP_OPENCV3

// internal
#include "commons.h"
#include "dataArray.h"
#include "segment3D.h"
#include "cudawrapper.h"

/**
 * Line3D++ - View Class
 * ====================
 * Holds all relevant data for one
 * specific image.
 * ====================
 * Author: M.Hofer, 2016
 */

namespace L3DPP
{
    class View
    {
    public:
        View(const unsigned int id, L3DPP::DataArray<float4>* lines,
             const Eigen::Matrix3d K, const Eigen::Matrix3d R,
             const Eigen::Vector3d t,
             const unsigned int width, const unsigned int height,
             const float median_depth);
        ~View();

        // compute spatial regularizer
        void computeSpatialRegularizer(const float r);

        // find collinear segments
        void findCollinearSegments(const float dist_t, bool useGPU);

        // draws lines into image
        void drawLineImage(cv::Mat& img);
        void drawSingleLine(const unsigned int id, cv::Mat& img,
                            const cv::Scalar color);
        void drawEpipolarLine(const Eigen::Vector3d epi, cv::Mat& img);

        // access to collinear segments
        std::list<unsigned int> collinearSegments(const unsigned int segID);

        // get ray from 2D point (normalized)
        Eigen::Vector3d getNormalizedRay(const Eigen::Vector3d p);
        Eigen::Vector3d getNormalizedRay(const Eigen::Vector2d p);
        Eigen::Vector3d getNormalizedLinePointRay(const unsigned int lID,
                                                  const bool pt1);

        // unproject 2D segment to 3D
        L3DPP::Segment3D unprojectSegment(const unsigned int segID, const float depth1,
                                          const float depth2);

        // projects a 3D point into image
        Eigen::Vector2d project(const Eigen::Vector3d P);
        Eigen::Vector3d projectWithCheck(const Eigen::Vector3d P);

        // get optical axis
        Eigen::Vector3d getOpticalAxis();

        // angle between views or view and segment (in rad)
        double opticalAxesAngle(L3DPP::View* v);
        double segmentQualityAngle(const L3DPP::Segment3D seg3D,
                                   const unsigned int segID);

        // computes a projective visual neighbor score (to ensure bigger baselines)
        float distanceVisualNeighborScore(L3DPP::View* v);

        // baseline between views
        float baseLine(L3DPP::View* v);

        // checks if a projected 3D segment is long enough
        bool projectedLongEnough(const L3DPP::Segment3D seg3D);

        // set new regularization depth
        void update_median_depth(const float d,
                                 const float sigmaP,
                                 const float med_scene_depth)
        {
            median_depth_ = d;

            if(sigmaP > 0.0f)
            {
                // fixed sigma
                k_ = sigmaP/med_scene_depth;
            }

            median_sigma_ = k_*median_depth_;
        }

        // compute k when fixed sigmaP is used
        void update_k(const float sigmaP, const float med_scene_depth)
        {
            k_ = sigmaP/med_scene_depth;
        }

        // compute regularizer with respect to given 3D point
        float regularizerFrom3Dpoint(const Eigen::Vector3d P);

        // get coordinates of a specific line segment
        Eigen::Vector4f getLineSegment2D(const unsigned int id);

        // translate view by a fixed vector
        void translate(const Eigen::Vector3d t);

        // data access
        unsigned int id(){return id_;}
        Eigen::Vector3d C(){return C_;}
        Eigen::Matrix3d K(){return K_;}
        Eigen::Matrix3d Kinv(){return Kinv_;}
        Eigen::Matrix3d R(){return R_;}
        Eigen::Matrix3d Rt(){return Rt_;}
        Eigen::Matrix3d RtKinv(){return RtKinv_;}
        Eigen::Vector3d t(){return t_;}
        Eigen::Vector3d pp(){return pp_;}
        unsigned int width(){return width_;}
        unsigned int height(){return height_;}
        float diagonal(){return diagonal_;}
        L3DPP::DataArray<float4>* lines(){return lines_;}
        size_t num_lines(){return lines_->width();}
        float k(){return k_;}
        float median_depth(){return median_depth_;}
        float median_sigma(){return median_sigma_;}

        // lock/unlock view specific mutex
        void lock_mutex(){mutex_.lock();}
        void unlock_mutex(){mutex_.unlock();}

#ifdef L3DPP_CUDA
        // GPU data
        L3DPP::DataArray<float>* RtKinvGPU(){return RtKinv_DA_;}
        float3 C_GPU(){return C_f3_;}
#endif //L3DPP_CUDA

    private:
        // find collinear segments
        void findCollinGPU();
        void findCollinCPU();

        // checks if a point is on a segment (only approximately!)
        // Note: use only cor collinearity estimation!
        bool pointOnSegment(const Eigen::Vector3d p1, const Eigen::Vector3d p2,
                            const Eigen::Vector3d x);

        // collinearity helper function: point to line distance 2D
        float distance_point2line_2D(const Eigen::Vector3d line, const Eigen::Vector3d p);

        // smaller angle between two lines [0,pi/2]
        float smallerAngle(const Eigen::Vector2d v1, const Eigen::Vector2d v2);

        // lines
        L3DPP::DataArray<float4>* lines_;

        // camera
        unsigned int id_;
        Eigen::Matrix3d K_;
        Eigen::Matrix3d Kinv_;
        Eigen::Matrix3d R_;
        Eigen::Matrix3d Rt_;
        Eigen::Matrix3d RtKinv_;
        Eigen::Vector3d t_;
        Eigen::Vector3d C_;
        Eigen::Vector3d pp_;
        unsigned int width_;
        unsigned int height_;
        float diagonal_;
        float min_line_length_;

        // camera plane
        Eigen::Vector3d plane_n_;
        float width_1_3_;
        float height_1_3_;

        // regularizer
        float k_;
        float initial_median_depth_;
        float median_depth_;
        float median_sigma_;

        // collinearity
        float collin_t_;
        std::vector<std::list<unsigned int> > collin_;

        // mutex
        boost::mutex mutex_;

#ifdef L3DPP_CUDA
        // camera data (GPU)
        L3DPP::DataArray<float>* RtKinv_DA_;
        float3 C_f3_;
#endif //L3DPP_CUDA
    };
}

#endif //I3D_LINE3D_PP_VIEW_H_
