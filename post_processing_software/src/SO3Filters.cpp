/*************************************************************************
*  Software License Agreement (BSD 3-Clause License)
*  
*  Copyright (c) 2019, Yoshiaki Sato
*  All rights reserved.
*  
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions are met:
*  
*  1. Redistributions of source code must retain the above copyright notice, this
*     list of conditions and the following disclaimer.
*  
*  2. Redistributions in binary form must reproduce the above copyright notice,
*     this list of conditions and the following disclaimer in the documentation
*     and/or other materials provided with the distribution.
*  
*  3. Neither the name of the copyright holder nor the names of its
*     contributors may be used to endorse or promote products derived from
*     this software without specific prior written permission.
*  
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
*  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
*  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
*  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
*  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
*  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
*  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
*  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
*  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*************************************************************************/
#include "SO3Filters.h"

void gradientLimit(Eigen::VectorXd &input, double maximum_gradient_)
{
    input = -1 * input;


    if (input.rows() < 2)
        return;
    double limited_value = input.head(1)[0];
    for (int i = 1, e = input.rows(); i < e; ++i)
    {
        if (input(i) > limited_value - maximum_gradient_)
        {
            limited_value = input(i);
        }
        else
        {
            limited_value -= maximum_gradient_;
            input(i) = limited_value;
        }
    }
    limited_value = input.tail(1)[0];
    for (int i = input.rows() - 2; i >= 0; --i)
    {
        if (input(i) > limited_value - maximum_gradient_)
        {
            limited_value = input(i);
        }
        else
        {
            limited_value -= maximum_gradient_;
            input(i) = limited_value;
        }
    }

    input = -1 * input;

}

/**
     * @brief ワープした時に欠けがないかチェックします
     * @retval false:欠けあり true:ワープが良好
     **/
bool isGoodWarp(std::vector<Eigen::Array2d, Eigen::aligned_allocator<Eigen::Array2d>> &contour, VideoPtr video_param)
{
    for (const auto &p : contour)
    {
        // if ((abs(contour[i]) < 1.0) && (abs(contour[i + 1]) < 1.0))
        // std::cout << "p[0]:" << p[0] << std::endl;
        // std::cout << "p[1]:" << p[1] << std::endl;
        if ((0. < p[0]) 
        && ( (video_param->camera_info->width_ - 1.) > p[0])
        && (0. < p[1])
        && ((video_param->camera_info->height_ - 1.) > p[1]))
        {
            return false;
        }
    }
    return true;
}

std::vector<Eigen::Array2d, Eigen::aligned_allocator<Eigen::Array2d>> getSparseContour(VideoPtr video_info, int n)
{
    std::vector<Eigen::Array2d, Eigen::aligned_allocator<Eigen::Array2d>> contour;
    Eigen::Array2d point;
    Eigen::Array2d U_max;
    double u_max = video_info->camera_info->width_ - 1.0;
    double v_max = video_info->camera_info->height_ - 1.0;
    // Top
    for (int i = 0; i <= n; ++i)
    {
        point << (double)i / (double)n * u_max, 0.0;
        contour.push_back(point);
    }
    // Bottom
    for (int i = 0; i <= n; ++i)
    {
        point << (double)i / (double)n * u_max, v_max;
        contour.push_back(point);
    }

    // Left
    for (int i = 1; i < n - 1; ++i)
    {
        point << 0.0, (double)i / (double)n * v_max;
        contour.push_back(point);
    }
    // Right
    for (int i = 0; i < n - 1; ++i)
    {
        point << u_max, (double)i / (double)n * v_max;
        contour.push_back(point);
    }
    return contour;
}

/** @brief 補正前の画像座標から、補正後のポリゴンの頂点を作成
 * @param [in]	Qa	ジャイロの角速度から計算したカメラの方向を表す回転クウォータニオン時系列データ、参照渡し
 * @param [in]	Qf	LPFを掛けて平滑化した回転クウォータニオンの時系列データ、参照渡し
 * @param [in]	m	画面の縦の分割数[ ]
 * @param [in]	n	画面の横の分割数[ ]
 * @param [in]	IK	"逆"歪係数(k1,k2,p1,p2)
 * @param [in]	matIntrinsic	カメラ行列(fx,fy,cx,cy) [pixel]
 * @param [in]	imageSize	フレーム画像のサイズ[pixel]
 * @param [in]  adjustmentQuaternion 画面方向を微調整するクォータニオン[rad]
 * @param [out]	vecPorigonn_uv	OpenGLのポリゴン座標(u',v')座標(-1~1)の組、歪補正後の画面を分割した時の一つ一つのポリゴンの頂点の組
 * @param [in]	zoom	倍率[]。拡大縮小しないなら1を指定すること。省略可
 * @retval true:成功 false:折り返し発生で失敗
 **/
void getUndistortUnrollingContour(
    int frame,
    AngularVelocityPtr angular_velocity,
    std::vector<Eigen::Array2d, Eigen::aligned_allocator<Eigen::Array2d>> &contour,
    std::vector<std::pair<int32_t,double>> sync_table,
    double zoom,
    VideoPtr video_param,
    const Eigen::VectorXd &filter_coeffs)
{

    // std::cout << "fc:" << filter_coeffs << std::endl;

    //手順
    //1.補正前画像を分割した時の分割点の座標(pixel)を計算
    //2.1の座標を入力として、各行毎のW(t1,t2)を計算
    //3.補正後の画像上のポリゴン座標(pixel)を計算、歪み補正も含める
    double &line_delay = video_param->camera_info->line_delay_;
    Eigen::Array2d f, c;
    f << video_param->camera_info->fx_, video_param->camera_info->fy_;
    c << video_param->camera_info->cx_, video_param->camera_info->cy_;
    const double &ik1 = video_param->camera_info->inverse_k1_;
    const double &ik2 = video_param->camera_info->inverse_k2_;
    const double &ip1 = video_param->camera_info->inverse_p1_;
    const double &ip2 = video_param->camera_info->inverse_p2_;

    contour.clear();
    std::vector<Eigen::Array2d, Eigen::aligned_allocator<Eigen::Array2d>> src_contour = getSparseContour(video_param, 9);
    Eigen::MatrixXd R;
    Eigen::Array2d x1;
    Eigen::Vector3d x3, xyz;
    for (auto &p : src_contour)
    {
        // double time_in_row = time + line_delay * (p[1] - video_param->camera_info->height_ * 0.5);
        double frame_in_row = frame + (line_delay * (p[1] - video_param->camera_info->height_ * 0.5))
            * video_param->getFrequency();

        R = angular_velocity->getCorrectionQuaternionFromFrame(frame_in_row, filter_coeffs,sync_table).matrix();
        // std::cout << "R:\r\n" << R << std::endl;
        //↑
        x1 = (p - c) / f;
        double r = x1.matrix().norm();
        Eigen::Array2d x2 = x1 * (1.0 + ik1 * pow(r, 2.0) + ik2 * pow(r, 4.0));
        x2[0] += 2.0 * ip1 * x1[0] * x1[1] + ip2 * (pow(r, 2.0) + 2 * pow(x1[0], 2.0));
        x2[1] += ip1 * (pow(r, 2.0) + 2.0 * pow(x1[1], 2.0)) + 2.0 * ip2 * x1[0] * x1[1];
        //折り返し防止
        if (((x2 - x1).abs() > 1).any())
        {
            printf("Warning: Turn backing.\n");
            x2 = x1;
        }
        x3 << x2[0], x2[1], 1.0;
        xyz = R * x3;
        x2 << xyz[0] / xyz[2], xyz[1] / xyz[2];
        contour.push_back(x2 * f * zoom + c);
    }
    // std::cout << "contour:" << std::endl;
    // for(auto &el:contour){
    //     std::cout << el.transpose() << std::endl;
    // }
}

bool hasBlackSpace(int frame,
                   double zoom,
                   AngularVelocityPtr angular_velocity,
                   VideoPtr video_param,
                   Eigen::VectorXd filter_coefficients,
                   std::vector<std::pair<int32_t,double>> &sync_table)
{
    std::vector<Eigen::Array2d, Eigen::aligned_allocator<Eigen::Array2d>> contour;
    getUndistortUnrollingContour(frame, angular_velocity, contour, sync_table, zoom, video_param, filter_coefficients);
    return !isGoodWarp(contour,video_param);
}

uint32_t bisectionMethod(int frame,
                         double zoom,
                         AngularVelocityPtr angular_velocity,
                         VideoPtr video_param,
                         FilterPtr filter,
                         std::vector<std::pair<int32_t,double>> &sync_table,
                         int32_t minimum_filter_strength,
                         int32_t maximum_filter_strength,
                         int max_iteration, uint32_t eps)
{
    int32_t a = maximum_filter_strength;
    int32_t b = minimum_filter_strength;
    int count = 0;
    int32_t m = 0;
    while (((uint32_t)abs(a - b) > eps) && (count++ < max_iteration))
    {
        m = (a + b) * 0.5;

        if (hasBlackSpace(frame, zoom, angular_velocity, video_param, filter->getFilterCoefficient(a),sync_table) ^ hasBlackSpace(frame, zoom, angular_velocity, video_param, filter->getFilterCoefficient(m),sync_table))
        {
            b = m;
        }
        else
        {
            a = m;
        }
        if (count == max_iteration)
        {
            std::cout << "max_iteration" << std::endl;
        }
    }
    return m;
}

